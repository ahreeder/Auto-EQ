#include "PluginEditor.h"

static constexpr int EDITOR_W = 920;
static constexpr int EDITOR_H = 520;
static constexpr int CTRL_H   = 90;

PaAutoEQEditor::PaAutoEQEditor (PaAutoEQProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (EDITOR_W, EDITOR_H);
    setResizable (true, true);
    setResizeLimits (680, 400, 1600, 900);

    // ── Spectrum display ─────────────────────────────────────────────────
    addAndMakeVisible (spectrumDisplay);

    // ── Controls ─────────────────────────────────────────────────────────
    addAndMakeVisible (btnEnabled);
    addAndMakeVisible (btnBypass);
    addAndMakeVisible (btnLoad);
    addAndMakeVisible (btnReset);

    // Threshold slider
    sliderThreshold.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    sliderThreshold.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 16);
    sliderThreshold.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xFFFFAA00));
    addAndMakeVisible (sliderThreshold);
    lblThreshold.setText ("Threshold", juce::dontSendNotification);
    lblThreshold.setJustificationType (juce::Justification::centred);
    lblThreshold.setFont (11.0f);
    addAndMakeVisible (lblThreshold);

    // Speed slider
    sliderSpeed.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    sliderSpeed.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 16);
    sliderSpeed.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xFF00C8FF));
    addAndMakeVisible (sliderSpeed);
    lblSpeed.setText ("Speed", juce::dontSendNotification);
    lblSpeed.setJustificationType (juce::Justification::centred);
    lblSpeed.setFont (11.0f);
    addAndMakeVisible (lblSpeed);

    // Max bands slider
    sliderMaxBands.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    sliderMaxBands.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 16);
    sliderMaxBands.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xFF80FF80));
    addAndMakeVisible (sliderMaxBands);
    lblMaxBands.setText ("Max Bands", juce::dontSendNotification);
    lblMaxBands.setJustificationType (juce::Justification::centred);
    lblMaxBands.setFont (11.0f);
    addAndMakeVisible (lblMaxBands);

    // Status / curve label
    lblCurve.setFont (11.0f);
    lblCurve.setColour (juce::Label::textColourId, juce::Colour (0xFFFFAA00));
    addAndMakeVisible (lblCurve);

    lblStatus.setFont (13.0f);
    addAndMakeVisible (lblStatus);

    // Button callbacks
    btnLoad.onClick  = [this] { loadCurveClicked(); };
    btnReset.onClick = [this] { resetClicked(); };

    // ── APVTS attachments ─────────────────────────────────────────────────
    attEnabled   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                       (processor.apvts, "enabled",   btnEnabled);
    attBypass    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                       (processor.apvts, "bypass",    btnBypass);
    attThreshold = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                       (processor.apvts, "threshold", sliderThreshold);
    attSpeed     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                       (processor.apvts, "stepSize",  sliderSpeed);
    attMaxBands  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                       (processor.apvts, "maxBands",  sliderMaxBands);

    startTimerHz (20);   // UI refresh at 20 fps
}

PaAutoEQEditor::~PaAutoEQEditor()
{
    stopTimer();
}

// ── Layout ────────────────────────────────────────────────────────────────

void PaAutoEQEditor::resized()
{
    const int w    = getWidth();
    const int h    = getHeight();
    const int ctrlY = h - CTRL_H;

    spectrumDisplay.setBounds (0, 0, w, ctrlY);

    // Controls row
    const int knobW  = 64;
    const int knobH  = CTRL_H - 4;
    int x = 8;

    // Knobs
    sliderThreshold.setBounds (x, ctrlY + 2, knobW, knobH - 16);
    lblThreshold.setBounds    (x, ctrlY + knobH - 14, knobW, 14);
    x += knobW + 4;

    sliderSpeed.setBounds (x, ctrlY + 2, knobW, knobH - 16);
    lblSpeed.setBounds    (x, ctrlY + knobH - 14, knobW, 14);
    x += knobW + 4;

    sliderMaxBands.setBounds (x, ctrlY + 2, knobW, knobH - 16);
    lblMaxBands.setBounds    (x, ctrlY + knobH - 14, knobW, 14);
    x += knobW + 16;

    // Toggle buttons
    btnEnabled.setBounds (x, ctrlY + 10, 80, 24);
    btnBypass.setBounds  (x, ctrlY + 38, 80, 24);
    x += 92;

    // Action buttons
    btnLoad.setBounds  (x, ctrlY + 10, 90, 26);
    btnReset.setBounds (x, ctrlY + 42, 90, 26);
    x += 102;

    // Labels (remaining width)
    lblCurve.setBounds  (x, ctrlY + 8,  w - x - 8, 18);
    lblStatus.setBounds (x, ctrlY + 30, w - x - 8, 22);
}

// ── Paint ─────────────────────────────────────────────────────────────────

void PaAutoEQEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF0D0D1A));

    // Control strip background
    const int ctrlY = getHeight() - CTRL_H;
    g.setColour (juce::Colour (0xFF14141F));
    g.fillRect (0, ctrlY, getWidth(), CTRL_H);

    // Separator line
    g.setColour (juce::Colour (0xFF2A2A4A));
    g.drawHorizontalLine (ctrlY, 0.0f, static_cast<float> (getWidth()));
}

// ── Timer (UI refresh) ────────────────────────────────────────────────────

void PaAutoEQEditor::timerCallback()
{
    // Push latest spectrum data to display
    float freqs[FFTAnalyzer::N_POINTS];
    float liveDb[FFTAnalyzer::N_POINTS];
    processor.fftAnalyzer.getSpectrum (freqs, liveDb);
    spectrumDisplay.updateLive (freqs, liveDb, FFTAnalyzer::N_POINTS);

    // Push target curve if loaded
    if (processor.targetCurve.isLoaded())
    {
        float targetDb[FFTAnalyzer::N_POINTS];
        processor.targetCurve.interpolateTo (freqs, targetDb, FFTAnalyzer::N_POINTS);
        spectrumDisplay.updateTarget (freqs, targetDb, FFTAnalyzer::N_POINTS);
        lblCurve.setText ("Curve: " + processor.targetCurve.getName(),
                          juce::dontSendNotification);
    }
    else
    {
        lblCurve.setText ("No curve loaded", juce::dontSendNotification);
    }

    // Push band markers
    spectrumDisplay.updateBands (processor.eqBank.getBands());

    // Status
    const float  dev       = processor.maxDeviation.load();
    const bool   converged = processor.isConverged.load();
    const int    nBands    = processor.activeBands.load();

    juce::String statusText;
    if (!processor.targetCurve.isLoaded())
        statusText = "Load a target curve to begin";
    else if (converged)
        statusText = juce::String::formatted ("Converged  |  max dev: %.1f dB  |  bands: %d", dev, nBands);
    else
        statusText = juce::String::formatted ("Converging...  |  max dev: %.1f dB  |  bands: %d", dev, nBands);

    lblStatus.setColour (juce::Label::textColourId,
                         converged ? juce::Colour (0xFF00FF80) : juce::Colour (0xFFFFAA00));
    lblStatus.setText (statusText, juce::dontSendNotification);

    spectrumDisplay.repaint();
}

// ── Button handlers ───────────────────────────────────────────────────────

void PaAutoEQEditor::loadCurveClicked()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Load Target Curve",
                      juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                      "*.json");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode |
                              juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file != juce::File{})
            {
                if (!processor.loadTargetCurve (file))
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                        "Load Failed", "Could not read curve file.\n"
                        "Expected JSON with \"freqs\" and \"db\" arrays.", "OK");
            }
        });
}

void PaAutoEQEditor::resetClicked()
{
    processor.resetBands();
}
