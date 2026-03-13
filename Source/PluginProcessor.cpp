#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout PaAutoEQProcessor::createParams()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterBool>   ("enabled",   "Auto EQ",   true));
    layout.add (std::make_unique<juce::AudioParameterBool>   ("bypass",    "Bypass",    false));
    layout.add (std::make_unique<juce::AudioParameterFloat>  ("threshold", "Threshold",
        juce::NormalisableRange<float> (0.5f, 6.0f, 0.1f), 2.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat>  ("stepSize",  "Speed",
        juce::NormalisableRange<float> (0.1f, 2.0f, 0.05f), 0.5f));
    layout.add (std::make_unique<juce::AudioParameterInt>    ("maxBands",  "Max Bands", 1, MAX_BANDS, 10));
    layout.add (std::make_unique<juce::AudioParameterBool>   ("frozen",    "Freeze EQ", false));

    return layout;
}

PaAutoEQProcessor::PaAutoEQProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createParams())
{
    currentBands.fill ({});
}

PaAutoEQProcessor::~PaAutoEQProcessor()
{
    stopTimer();
}

//==============================================================================
void PaAutoEQProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    eqBank.prepare (sampleRate, samplesPerBlock);
    fftAnalyzer.setSampleRate (sampleRate);
    fftAnalyzer.reset();

    startTimerHz (10);   // Convergence ticks at 10 Hz
}

void PaAutoEQProcessor::releaseResources()
{
    stopTimer();
    eqBank.reset();
}

//==============================================================================
void PaAutoEQProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    const bool bypassed = *apvts.getRawParameterValue ("bypass") > 0.5f;

    if (bypassed)
        return;   // Pass audio through unchanged

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    float* left  = numChannels > 0 ? buffer.getWritePointer (0) : nullptr;
    float* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    // Apply EQ
    eqBank.processBlock (left, right, numSamples);

    // Feed post-EQ mono signal into FFT analyzer
    if (left != nullptr)
    {
        if (right != nullptr)
        {
            // Mix to mono for analysis
            juce::AudioBuffer<float> mono (1, numSamples);
            auto* m = mono.getWritePointer (0);
            for (int i = 0; i < numSamples; ++i)
                m[i] = (left[i] + right[i]) * 0.5f;
            fftAnalyzer.pushSamples (m, numSamples);
        }
        else
        {
            fftAnalyzer.pushSamples (left, numSamples);
        }
    }
}

//==============================================================================
void PaAutoEQProcessor::timerCallback()
{
    const bool enabled = *apvts.getRawParameterValue ("enabled") > 0.5f;
    const bool frozen  = *apvts.getRawParameterValue ("frozen")  > 0.5f;
    if (!enabled || !targetCurve.isLoaded() || frozen)
        return;

    // Get current spectrum
    float freqs[FFTAnalyzer::N_POINTS];
    float liveDb[FFTAnalyzer::N_POINTS];
    fftAnalyzer.getSpectrum (freqs, liveDb);

    // Get target at same frequency grid
    float targetDb[FFTAnalyzer::N_POINTS];
    targetCurve.interpolateTo (freqs, targetDb, FFTAnalyzer::N_POINTS);

    // Read current bands from EQ bank
    currentBands = eqBank.getBands();

    // Run convergence
    ConvergenceSettings settings;
    settings.threshold = *apvts.getRawParameterValue ("threshold");
    settings.stepSize  = *apvts.getRawParameterValue ("stepSize");
    settings.maxBands  = static_cast<int> (*apvts.getRawParameterValue ("maxBands"));

    const auto state = convergenceEngine.tick (liveDb, targetDb, freqs,
                                               currentBands, settings);

    // Push updated bands to audio thread
    eqBank.setBands (currentBands);

    // Update atomic status
    maxDeviation.store (state.maxDeviation);
    isConverged.store  (state.converged);
    activeBands.store  (state.activeBands);
}

//==============================================================================
void PaAutoEQProcessor::resetBands()
{
    currentBands.fill ({});
    eqBank.setBands (currentBands);
    maxDeviation.store (0.0f);
    isConverged.store (false);
    activeBands.store (0);
}

bool PaAutoEQProcessor::loadTargetCurve (const juce::File& file)
{
    return targetCurve.loadFromFile (file);
}

//==============================================================================
void PaAutoEQProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PaAutoEQProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
juce::AudioProcessorEditor* PaAutoEQProcessor::createEditor()
{
    return new PaAutoEQEditor (*this);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PaAutoEQProcessor();
}
