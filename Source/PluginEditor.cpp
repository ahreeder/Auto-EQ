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

    spectrumDisplay.onBandChanged = [this] (int idx, BandParams bp)
    {
        processor.updateBand (idx, bp);
        if (bandEditorWindow != nullptr && bandEditorWindow->isVisible())
            bandEditorWindow->getPanel()->updateBands (processor.eqBank.getBands());
    };

    // ── Controls ─────────────────────────────────────────────────────────
    addAndMakeVisible (btnEnabled);
    addAndMakeVisible (btnBypass);

    // Curve dropdown ▼ button + name label
    btnCurveMenu.onClick = [this]
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Load Curve");
        menu.addItem (2, "Save Curve");
        menu.addSeparator();
        menu.addItem (3, "Clear Curve");
        menu.addItem (4, "Delete Curve");

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btnCurveMenu),
            [this] (int result)
            {
                switch (result)
                {
                    case 1: loadCurveClicked();   break;
                    case 2: saveCurveClicked();   break;
                    case 3: clearCurveClicked();  break;
                    case 4: deleteCurveClicked(); break;
                    default: break;
                }
            });
    };
    addAndMakeVisible (btnCurveMenu);

    lblCurveName.setFont (11.0f);
    lblCurveName.setColour (juce::Label::textColourId, juce::Colour (0xFFFFAA00));
    lblCurveName.setText ("No curve", juce::dontSendNotification);
    addAndMakeVisible (lblCurveName);

    addAndMakeVisible (btnFreeze);
    addAndMakeVisible (btnReset);
    addAndMakeVisible (btnDisplayMode);

    cmbBarRes.addItem ("1/3 Oct",  1);
    cmbBarRes.addItem ("1/6 Oct",  2);
    cmbBarRes.addItem ("1/12 Oct", 3);
    cmbBarRes.addItem ("Fine",     4);
    cmbBarRes.setSelectedId (4, juce::dontSendNotification);
    cmbBarRes.onChange = [this]
    {
        const int id = cmbBarRes.getSelectedId();
        const int n  = (id == 1) ? 31 : (id == 2) ? 61 : (id == 3) ? 121 : 200;
        spectrumDisplay.setBarResolution (n);
    };
    addAndMakeVisible (cmbBarRes);
    addAndMakeVisible (btnEditBands);

    // Colour picker squares — coloured to match their curve, tooltip to identify
    auto setupColBtn = [this] (juce::TextButton& btn, juce::Colour col,
                                const juce::String& tip, int target)
    {
        btn.setColour (juce::TextButton::buttonColourId,   col.withAlpha (1.0f));
        btn.setColour (juce::TextButton::buttonOnColourId, col.withAlpha (1.0f).brighter (0.2f));
        btn.setTooltip (tip);
        btn.onClick = [this, &btn, target] { showColourPicker (target, btn); };
        addAndMakeVisible (btn);
    };
    setupColBtn (btnColTarget, spectrumDisplay.colTarget, "Target curve colour", 1);
    setupColBtn (btnColLive,   spectrumDisplay.colLive,   "Live curve colour",   0);
    setupColBtn (btnColDiff,   spectrumDisplay.colDiff,   "Reduction fill colour", 2);

    auto setupColLabel = [this] (juce::Label& lbl, const juce::String& text)
    {
        lbl.setText (text, juce::dontSendNotification);
        lbl.setFont (9.0f);
        lbl.setJustificationType (juce::Justification::centred);
        lbl.setColour (juce::Label::textColourId, juce::Colour (0xFFAAAAAA));
        addAndMakeVisible (lbl);
    };
    setupColLabel (lblColTarget, "Target");
    setupColLabel (lblColLive,   "Live");
    setupColLabel (lblColDiff,   "Reduction");

    // Threshold slider
    sliderThreshold.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    sliderThreshold.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 16);
    sliderThreshold.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xFFFFAA00));
    addAndMakeVisible (sliderThreshold);
    lblThreshold.setText ("Threshold", juce::dontSendNotification);
    lblThreshold.setJustificationType (juce::Justification::centred);
    lblThreshold.setFont (11.0f);
    addAndMakeVisible (lblThreshold);

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

    // Display offset slider
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
    lblOffset.setText ("L Offset", juce::dontSendNotification);
    lblOffset.setJustificationType (juce::Justification::centred);
    lblOffset.setFont (11.0f);
    addAndMakeVisible (lblOffset);

    // Target offset slider
    sliderTargetOffset.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    sliderTargetOffset.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 16);
    sliderTargetOffset.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xFFFFAA00));
    sliderTargetOffset.setRange (-40.0, 40.0, 0.5);
    sliderTargetOffset.setValue (0.0);
    sliderTargetOffset.setDoubleClickReturnValue (true, 0.0);
    sliderTargetOffset.onValueChange = [this] {
        spectrumDisplay.setTargetOffset ((float) sliderTargetOffset.getValue());
    };
    addAndMakeVisible (sliderTargetOffset);
    lblTargetOffset.setText ("T Offset", juce::dontSendNotification);
    lblTargetOffset.setJustificationType (juce::Justification::centred);
    lblTargetOffset.setFont (11.0f);
    addAndMakeVisible (lblTargetOffset);

    lblStatus.setFont (13.0f);
    addAndMakeVisible (lblStatus);

    // Button callbacks
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

    const int knobW = 60;
    const int knobH = CTRL_H - 4;
    int x = 8;

    // ── Knobs ────────────────────────────────────────────────────────────
    sliderThreshold.setBounds (x, ctrlY + 2, knobW, knobH - 16);
    lblThreshold.setBounds    (x, ctrlY + knobH - 14, knobW, 14);
    x += knobW + 4;

    sliderMaxBands.setBounds (x, ctrlY + 2, knobW, knobH - 16);
    lblMaxBands.setBounds    (x, ctrlY + knobH - 14, knobW, 14);
    x += knobW + 4;

    sliderAvgTime.setBounds (x, ctrlY + 2, knobW, knobH - 16);
    lblAvgTime.setBounds    (x, ctrlY + knobH - 14, knobW, 14);
    x += knobW + 4;

    sliderOffset.setBounds (x, ctrlY + 2, knobW, knobH - 16);
    lblOffset.setBounds    (x, ctrlY + knobH - 14, knobW, 14);
    x += knobW + 4;

    sliderTargetOffset.setBounds (x, ctrlY + 2, knobW, knobH - 16);
    lblTargetOffset.setBounds    (x, ctrlY + knobH - 14, knobW, 14);
    x += knobW + 10;

    // ── Auto EQ + Bypass + Freeze (three stacked, vertically centered) ─────
    // 3×22 + 2×8 = 76px total  →  top = (90-76)/2 = 7
    btnEnabled.setBounds (x, ctrlY + 7,  72, 22);
    btnBypass .setBounds (x, ctrlY + 37, 72, 22);
    btnFreeze .setBounds (x, ctrlY + 67, 72, 22);
    x += 88;   // extra gap before curve section

    // ── "..." button on left + curve name label to its right ─────────────
    const int curveNameW = 120;
    btnCurveMenu.setBounds (x, ctrlY + (CTRL_H - 22) / 2, 28, 22);
    lblCurveName.setBounds (x + 28 + 2, ctrlY + (CTRL_H - 16) / 2, curveNameW, 16);
    x += 28 + 2 + curveNameW + 8;

    // ── Reset EQ + Edit Bands (two stacked, vertically centered) ──────────
    // 2×22 + 8 = 52px total  →  top = (90-52)/2 = 19
    btnReset    .setBounds (x, ctrlY + 19, 76, 22);
    btnEditBands.setBounds (x, ctrlY + 49, 76, 22);
    x += 92;   // extra gap before display controls

    // ── Display mode + bar resolution (two stacked, vertically centered) ──
    // 22px button + 8px gap + 20px combo = 50px total  →  top = (90-50)/2 = 20
    btnDisplayMode.setBounds (x, ctrlY + 20, 76, 22);
    cmbBarRes     .setBounds (x, ctrlY + 50, 76, 20);
    x += 84;

    // ── Status label (top of right area) + colours below it ──────────────
    lblStatus.setBounds (x, ctrlY + 6, w - x - 8, 22);

    // Colour pickers — below status text, bottom-right aligned
    // 3×20px squares + 2×16px gaps = 92px wide
    const int sq      = 20;
    const int sqGap   = 16;
    const int lblH    = 12;
    const int colX    = w - 10 - (3 * sq + 2 * sqGap);
    const int sqY     = ctrlY + 34;   // below status text row
    const int colLblY = sqY + sq + 3;
    const int lblW    = 52;           // centered under each square

    btnColTarget.setBounds (colX,                   sqY, sq, sq);
    lblColTarget.setBounds (colX - (lblW - sq) / 2, colLblY, lblW, lblH);

    btnColLive  .setBounds (colX + sq + sqGap,                   sqY, sq, sq);
    lblColLive  .setBounds (colX + sq + sqGap - (lblW - sq) / 2, colLblY, lblW, lblH);

    btnColDiff  .setBounds (colX + 2 * (sq + sqGap),                   sqY, sq, sq);
    lblColDiff  .setBounds (colX + 2 * (sq + sqGap) - (lblW - sq) / 2, colLblY, lblW, lblH);
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
        lblCurveName.setText (processor.targetCurve.getName(), juce::dontSendNotification);
    }
    else
    {
        lblCurveName.setText ("No curve", juce::dontSendNotification);
    }

    const auto bands = processor.eqBank.getBands();
    spectrumDisplay.updateBands (bands);

    if (bandEditorWindow != nullptr && bandEditorWindow->isVisible())
        bandEditorWindow->getPanel()->updateBands (bands);

    const bool frozen = *processor.apvts.getRawParameterValue ("frozen") > 0.5f;
    btnFreeze.setButtonText (frozen ? "Thaw EQ" : "Freeze EQ");

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

// ── Colour picker ─────────────────────────────────────────────────────────

void PaAutoEQEditor::showColourPicker (int target, juce::Button& source)
{
    activeColourTarget = target;

    juce::Colour initial;
    if      (target == 0) initial = spectrumDisplay.colLive;
    else if (target == 1) initial = spectrumDisplay.colTarget;
    else                  initial = spectrumDisplay.colDiff;

    auto* cs = new juce::ColourSelector (
        juce::ColourSelector::showColourAtTop  |
        juce::ColourSelector::showSliders      |
        juce::ColourSelector::showColourspace  |
        juce::ColourSelector::showAlphaChannel);

    cs->setCurrentColour (initial);
    cs->addChangeListener (this);
    cs->setSize (280, 280);

    activeColourSelector = cs;

    juce::CallOutBox::launchAsynchronously (
        std::unique_ptr<juce::Component> (cs),
        source.getScreenBounds(), nullptr);
}

void PaAutoEQEditor::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (activeColourSelector == nullptr) return;
    if (source != static_cast<juce::ChangeBroadcaster*> (activeColourSelector.getComponent())) return;

    const juce::Colour c = activeColourSelector->getCurrentColour();

    if      (activeColourTarget == 0) spectrumDisplay.colLive   = c;
    else if (activeColourTarget == 1) spectrumDisplay.colTarget = c;
    else                              spectrumDisplay.colDiff   = c;

    // Keep button backgrounds in sync
    auto syncBtn = [] (juce::TextButton& btn, juce::Colour col)
    {
        btn.setColour (juce::TextButton::buttonColourId,   col.withAlpha (1.0f));
        btn.setColour (juce::TextButton::buttonOnColourId, col.withAlpha (1.0f).brighter (0.2f));
    };
    syncBtn (btnColLive,   spectrumDisplay.colLive);
    syncBtn (btnColTarget, spectrumDisplay.colTarget);
    syncBtn (btnColDiff,   spectrumDisplay.colDiff);

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
