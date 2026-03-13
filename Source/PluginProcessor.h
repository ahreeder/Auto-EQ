#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "EQBank.h"
#include "FFTAnalyzer.h"
#include "TargetCurve.h"
#include "ConvergenceEngine.h"
#include <atomic>
#include <array>

class PaAutoEQProcessor final : public juce::AudioProcessor,
                                 private juce::Timer
{
public:
    PaAutoEQProcessor();
    ~PaAutoEQProcessor() override;

    // ── AudioProcessor interface ──────────────────────────────────────────
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool                        hasEditor()    const override { return true; }

    const juce::String getName()    const override { return "PA Auto EQ"; }
    bool  acceptsMidi()             const override { return false; }
    bool  producesMidi()            const override { return false; }
    bool  isMidiEffect()            const override { return false; }
    double getTailLengthSeconds()   const override { return 0.0; }

    int  getNumPrograms()                          override { return 1; }
    int  getCurrentProgram()                       override { return 0; }
    void setCurrentProgram (int)                   override {}
    const juce::String getProgramName (int)        override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ── Public state (accessed by editor) ────────────────────────────────
    FFTAnalyzer     fftAnalyzer;
    TargetCurve     targetCurve;
    EQBank          eqBank;
    ConvergenceEngine convergenceEngine;

    std::atomic<float> maxDeviation  { 0.0f };
    std::atomic<bool>  isConverged   { false };
    std::atomic<int>   activeBands   { 0 };

    // Parameters
    juce::AudioProcessorValueTreeState apvts;

    // Reset all EQ bands to zero
    void resetBands();

    // Message-thread call: manually set one band (from drag or band editor).
    // Convergence engine will still nudge unlocked bands each tick,
    // but locked bands will be preserved as-is.
    void updateBand (int idx, const BandParams& p);

    // Load / manage target curve (call from message thread)
    bool         loadTargetCurve   (const juce::File& file);
    void         clearTargetCurve  ();
    juce::File   getLastCurveFile  () const { return lastCurveFile; }

private:
    void timerCallback() override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParams();

    juce::File lastCurveFile;

    // Band state shared between timer and audio thread via EQBank's SpinLock
    std::array<BandParams, MAX_BANDS> currentBands {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PaAutoEQProcessor)
};
