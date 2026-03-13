#include "PluginEditor.h"

static constexpr int EDITOR_W = 960;
static constexpr int EDITOR_H = 540;
static constexpr int CTRL_H   = 90;

PaAutoEQEditor::PaAutoEQEditor (PaAutoEQProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (EDITOR_W, EDITOR_H);
    setResizable (true, true);
    setResizeLimits (680, 400, 1600, 900);

    // ── Spectrum display ─────────────────────────────────────────────────
    addAndMakeVisible (spectrumDisplay);

    // Wire drag callback to processor
    spectrumDisplay.onBandChanged = [this] (int idx, BandParams bp)
    {
        processor.updateBand (idx, bp);

        // Also update the band editor if open
        if (bandEditorWindow != nullptr && bandEditorWindow->isVisible())
            bandEditorWindow->getPanel()->updateBands (processor.eqBank.getBands());
    };

    // ── Controls ─────────────────────────────────────────────────────────
    addAndMakeVisible (btnEnabled);
    addAndMakeVisible (btnBypass);
    addAndMakeVisible (btnLoad);
    addAndMakeVisible (btnSave);
    addAndMakeVisible (btnClearCurve);
    addAndMakeVisible (btnDeleteCurve);
    addAndMakeVisible (btnFreeze);
    addAndMakeVisible (btnReset);
    addAndMakeVisible (btnDisplayMode);

    cmbBarRes.addItem ("1/3 Oct",  1);
    cmbBarRes.addItem ("1/6 Oct",  2);
    cmbBarRes.addItem ("1/12 Oct", 3);
    cmbBarRes.addItem ("Fine",     4);
    cmbBarRes.setSelectedId (4, juce::dontSendNotification);  // Fine default
    cmbBarRes.onChange = [this]
    {
        const int id = cmbBarRes.getSelectedId();
        const int n  = (id == 1) ? 31 : (id == 2) ? 61 : (id == 3) ? 121 : 200;
        spectrumDisplay.setBarResolution (n);
    };
    addAndMakeVisible (cmbBarRes);

    addAndMakeVisible (btnEditBands);

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

    // Avg time slider
    sliderAvgTime.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    sliderAvgTime.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 16);
    sliderAvgTime.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xFFFF8844));
    addAndMakeVisible (sliderAvgTime);
    lblAvgTime.setText ("Avg Time", juce::dontSendNotification);
    lblAvgTime.setJustificationType (juce::Justification::centred);
    lblAvgTime.setFont (11.0f);
    addAndMakeVisible (lblAvgTime);

    // Display offset slider (visual only — shifts live spectrum up/down)
    sliderOffset.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    sliderOffset.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 16);
    sliderOffset.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xFFAA88FF));
    sliderOffset.setRange (-40.0, 40.0, 0.5);
    sliderOffset.setValue (0.0);
    sliderOffset.setDoubleClickReturnValue (true, 0.0);
    sliderOffset.onValueChange = [this] {
        spectrumDisplay.setDisplayOffset ((float) sliderOffset.getValue());
    };
    addAndMakeVisible (sliderOffset);
    lblOffset.setText ("Offset", juce::dontSendNotification);
    lblOffset.setJustificationType (juce::Justification::centred);
    lblOffset.setFont (11.0f);
    addAndMakeVisible (lblOffset);

    // Status / curve label
    lblCurve.setFont (11.0f);
    lblCurve.setColour (juce::Label::textColourId, juce::Colour (0xFFFFAA00));
    addAndMakeVisible (lblCurve);

    lblStatus.setFont (13.0f);
    addAndMakeVisible (lblStatus);

    // Button callbacks
    btnLoad.onClick        = [this] { loadCurveClicked(); };
    btnSave.onClick        = [this] { saveCurveClicked(); };
    btnClearCurve.onClick  = [this] { clearCurveClicked(); };
    btnDeleteCurve.onClick = [this] { deleteCurveClicked(); };
    btnReset.onClick       = [this] { resetClicked(); };
    btnDisplayMode.onClick = [this] { toggleDisplayMode(); };
    btnEditBands.onClick   = [this] { openBandEditor(); };

    // Default curves dir
    juce::File defaultCurvesDir = juce::File::getSpecialLocation (
                                      juce::File::userDocumentsDirectory)
                                      .getChildFile ("pa-tuning-tool/curves");
    lastCurvesDir = defaultCurvesDir.isDirectory()
                        ? defaultCurvesDir
                        : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    // ── APVTS attachments ─────────────────────────────────────────────────
    attEnabled   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                       (processor.apvts, "enabled",   btnEnabled);
    attBypass    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                       (processor.apvts, "bypass",    btnBypass);
    attFreeze    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                       (processor.apvts, "frozen",    btnFreeze);
    attThreshold = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                       (processor.apvts, "threshold", sliderThreshold);
    attSpeed     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                       (processor.apvts, "stepSize",  sliderSpeed);
    attMaxBands  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                       (processor.apvts, "maxBands",  sliderMaxBands);
    attAvgTime   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                       (processor.apvts, "avgTime",   sliderAvgTime);

    startTimerHz (20);
}

PaAutoEQEditor::~PaAutoEQEditor()
{
    stopTimer();
    bandEditorWindow.reset();
}

// ── Layout ────────────────────────────────────────────────────────────────

void PaAutoEQEditor::resized()
{
    const int w     = getWidth();
    const int h     = getHeight();
    const int ctrlY = h - CTRL_H;

    spectrumDisplay.setBounds (0, 0, w, ctrlY);

    const int knobW = 64;
    const int knobH = CTRL_H - 4;
    int x = 8;

    sliderThreshold.setBounds (x, ctrlY + 2, knobW, knobH - 16);
    lblThreshold.setBounds    (x, ctrlY + knobH - 14, knobW, 14);
    x += knobW + 4;

    sliderSpeed.setBounds (x, ctrlY + 2, knobW, knobH - 16);
    lblSpeed.setBounds    (x, ctrlY + knobH - 14, knobW, 14);
    x += knobW + 4;

    sliderMaxBands.setBounds (x, ctrlY + 2, knobW, knobH - 16);
    lblMaxBands.setBounds    (x, ctrlY + knobH - 14, knobW, 14);
    x += knobW + 4;

    sliderAvgTime.setBounds (x, ctrlY + 2, knobW, knobH - 16);
    lblAvgTime.setBounds    (x, ctrlY + knobH - 14, knobW, 14);
    x += knobW + 4;

    sliderOffset.setBounds (x, ctrlY + 2, knobW, knobH - 16);
    lblOffset.setBounds    (x, ctrlY + knobH - 14, knobW, 14);
    x += knobW + 16;

    btnEnabled.setBounds (x, ctrlY + 10, 80, 24);
    btnBypass.setBounds  (x, ctrlY + 38, 80, 24);
    x += 92;

    btnLoad.setBounds        (x,       ctrlY + 6,  80, 22);
    btnSave.setBounds        (x,       ctrlY + 32, 80, 22);
    btnClearCurve.setBounds  (x,       ctrlY + 58, 80, 22);
    btnDeleteCurve.setBounds (x + 86,  ctrlY + 58, 80, 22);
    btnFreeze.setBounds      (x + 86,  ctrlY + 6,  80, 22);
    btnReset.setBounds       (x + 86,  ctrlY + 32, 80, 22);
    x += 178;

    btnDisplayMode.setBounds (x, ctrlY + 10, 80, 22);
    cmbBarRes     .setBounds (x, ctrlY + 34, 80, 20);
    btnEditBands  .setBounds (x, ctrlY + 58, 80, 22);
    x += 88;

    lblCurve .setBounds (x, ctrlY + 8,  w - x - 8, 18);
    lblStatus.setBounds (x, ctrlY + 30, w - x - 8, 22);
}

// ── Paint ─────────────────────────────────────────────────────────────────

void PaAutoEQEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF0D0D1A));

    const int ctrlY = getHeight() - CTRL_H;
    g.setColour (juce::Colour (0xFF14141F));
    g.fillRect (0, ctrlY, getWidth(), CTRL_H);

    g.setColour (juce::Colour (0xFF2A2A4A));
    g.drawHorizontalLine (ctrlY, 0.0f, static_cast<float> (getWidth()));
}

// ── Timer (UI refresh) ────────────────────────────────────────────────────

void PaAutoEQEditor::timerCallback()
{
    float freqs[FFTAnalyzer::N_POINTS];
    float liveDb[FFTAnalyzer::N_POINTS];
    processor.fftAnalyzer.getSpectrum (freqs, liveDb);
    spectrumDisplay.updateLive (freqs, liveDb, FFTAnalyzer::N_POINTS);

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

    const auto bands = processor.eqBank.getBands();
    spectrumDisplay.updateBands (bands);

    // Update band editor if open
    if (bandEditorWindow != nullptr && bandEditorWindow->isVisible())
        bandEditorWindow->getPanel()->updateBands (bands);

    const bool frozen = *processor.apvts.getRawParameterValue ("frozen") > 0.5f;
    btnFreeze.setButtonText (frozen ? "Thaw EQ" : "Freeze EQ");

    // Display mode button label
    btnDisplayMode.setButtonText (
        spectrumDisplay.getDisplayMode() == SpectrumDisplay::DisplayMode::Bars
            ? "Line" : "Bars");

    const float  dev       = processor.maxDeviation.load();
    const bool   converged = processor.isConverged.load();
    const int    nBands    = processor.activeBands.load();

    juce::String statusText;
    juce::Colour statusColour;

    if (!processor.targetCurve.isLoaded())
    {
        statusText   = "Load a target curve to begin";
        statusColour = juce::Colour (0xFF888888);
    }
    else if (frozen)
    {
        statusText   = juce::String::formatted ("Frozen  |  max dev: %.1f dB  |  bands: %d", dev, nBands);
        statusColour = juce::Colour (0xFF00C8FF);
    }
    else if (converged)
    {
        statusText   = juce::String::formatted ("Converged  |  max dev: %.1f dB  |  bands: %d", dev, nBands);
        statusColour = juce::Colour (0xFF00FF80);
    }
    else
    {
        statusText   = juce::String::formatted ("Converging...  |  max dev: %.1f dB  |  bands: %d", dev, nBands);
        statusColour = juce::Colour (0xFFFFAA00);
    }

    lblStatus.setColour (juce::Label::textColourId, statusColour);
    lblStatus.setText (statusText, juce::dontSendNotification);

    spectrumDisplay.repaint();
}

// ── Button handlers ───────────────────────────────────────────────────────

void PaAutoEQEditor::loadCurveClicked()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Load Target Curve",
                      lastCurvesDir, "*.json");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode |
                              juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file != juce::File{})
            {
                lastCurvesDir = file.getParentDirectory();
                if (!processor.loadTargetCurve (file))
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                        "Load Failed", "Could not read curve file.\n"
                        "Expected JSON with \"freqs\" and \"db\" arrays.", "OK");
            }
        });
}

void PaAutoEQEditor::saveCurveClicked()
{
    float freqs[FFTAnalyzer::N_POINTS];
    float liveDb[FFTAnalyzer::N_POINTS];
    processor.fftAnalyzer.getSpectrum (freqs, liveDb);

    fileChooser = std::make_unique<juce::FileChooser> ("Save Curve",
                      lastCurvesDir.getChildFile ("my_curve.json"), "*.json");

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode |
                              juce::FileBrowserComponent::canSelectFiles |
                              juce::FileBrowserComponent::warnAboutOverwriting,
        [this, freqsCopy = std::vector<float>(freqs, freqs + FFTAnalyzer::N_POINTS),
               dbCopy    = std::vector<float>(liveDb, liveDb + FFTAnalyzer::N_POINTS)]
        (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File{})
                return;

            if (file.getFileExtension().toLowerCase() != ".json")
                file = file.withFileExtension ("json");

            lastCurvesDir = file.getParentDirectory();

            juce::String json = "{\n  \"freqs\": [";
            for (int i = 0; i < FFTAnalyzer::N_POINTS; ++i)
            {
                json += juce::String (freqsCopy[i], 2);
                if (i < FFTAnalyzer::N_POINTS - 1) json += ", ";
            }
            json += "],\n  \"db\": [";
            for (int i = 0; i < FFTAnalyzer::N_POINTS; ++i)
            {
                json += juce::String (dbCopy[i], 2);
                if (i < FFTAnalyzer::N_POINTS - 1) json += ", ";
            }
            json += "]\n}\n";

            if (!file.replaceWithText (json))
                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                    "Save Failed", "Could not write to:\n" + file.getFullPathName(), "OK");
        });
}

void PaAutoEQEditor::clearCurveClicked()
{
    processor.clearTargetCurve();
    spectrumDisplay.updateTarget (nullptr, nullptr, 0);
}

void PaAutoEQEditor::deleteCurveClicked()
{
    const juce::File file = processor.getLastCurveFile();
    if (file == juce::File{})
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
            "Delete Curve", "No curve file is currently loaded.", "OK");
        return;
    }

    juce::AlertWindow::showOkCancelBox (
        juce::AlertWindow::WarningIcon,
        "Delete Curve",
        "Permanently delete \"" + file.getFileName() + "\" from disk?",
        "Delete", "Cancel", nullptr,
        juce::ModalCallbackFunction::create ([this, file] (int result)
        {
            if (result == 1)
            {
                file.deleteFile();
                processor.clearTargetCurve();
                spectrumDisplay.updateTarget (nullptr, nullptr, 0);
            }
        }));
}

void PaAutoEQEditor::resetClicked()
{
    processor.resetBands();
}

void PaAutoEQEditor::toggleDisplayMode()
{
    using DM = SpectrumDisplay::DisplayMode;
    const DM next = (spectrumDisplay.getDisplayMode() == DM::Line) ? DM::Bars : DM::Line;
    spectrumDisplay.setDisplayMode (next);
}

void PaAutoEQEditor::openBandEditor()
{
    if (bandEditorWindow == nullptr)
    {
        bandEditorWindow = std::make_unique<BandEditorWindow> (this);

        // Wire the panel's callback to processor.updateBand
        bandEditorWindow->getPanel()->onBandChanged = [this] (int idx, BandParams bp)
        {
            processor.updateBand (idx, bp);
        };
    }
    else if (!bandEditorWindow->isVisible())
    {
        bandEditorWindow->setVisible (true);
    }
    else
    {
        bandEditorWindow->toFront (true);
    }
}
