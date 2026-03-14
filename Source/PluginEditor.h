#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "SpectrumDisplay.h"
#include "BandEditorPanel.h"

class BandEditorWindow : public juce::DocumentWindow
{
public:
    explicit BandEditorWindow (juce::Component* parent)
        : juce::DocumentWindow ("EQ Band Editor",
                                juce::Colour (0xFF0D0D1A),
                                juce::DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        panel = std::make_unique<BandEditorPanel>();

        viewport = std::make_unique<juce::Viewport>();
        viewport->setViewedComponent (panel.get(), false);
        viewport->setScrollBarsShown (true, false);

        const int vpW = BandEditorPanel::PANEL_W + viewport->getScrollBarThickness();
        const int vpH = std::min (BandEditorPanel::panelHeight(), 500);

        viewport->setSize (vpW, vpH);
        setContentNonOwned (viewport.get(), true);
        setResizable (false, false);
        centreWithSize (vpW, vpH + getTitleBarHeight());
        setVisible (true);
    }

    void closeButtonPressed() override { setVisible (false); }
    BandEditorPanel* getPanel() { return panel.get(); }

private:
    std::unique_ptr<BandEditorPanel> panel;
    std::unique_ptr<juce::Viewport>  viewport;
};

class PaAutoEQEditor final : public juce::AudioProcessorEditor,
                              private juce::Timer,
                              private juce::ChangeListener
{
public:
    explicit PaAutoEQEditor (PaAutoEQProcessor&);
    ~PaAutoEQEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    void loadCurveClicked();
    void saveCurveClicked();
    void clearCurveClicked();
    void deleteCurveClicked();
    void resetClicked();
    void toggleDisplayMode();
    void openBandEditor();
    void showColourPicker (int target, juce::Button& source);

    PaAutoEQProcessor& processor;

    SpectrumDisplay spectrumDisplay;
    std::unique_ptr<BandEditorWindow> bandEditorWindow;

    // Controls
    juce::ToggleButton btnEnabled     { "Auto EQ" };
    juce::ToggleButton btnBypass      { "Bypass" };
    juce::TextButton   btnCurveMenu   { "..." };
    juce::Label        lblCurveName;
    juce::ToggleButton btnFreeze      { "Freeze EQ" };
    juce::TextButton   btnReset       { "Reset EQ" };
    juce::TextButton   btnDisplayMode { "Bars" };
    juce::ComboBox     cmbBarRes;
    juce::TextButton   btnEditBands   { "Edit Bands" };

    // Colour picker squares + labels
    juce::TextButton   btnColLive;
    juce::TextButton   btnColTarget;
    juce::TextButton   btnColDiff;
    juce::Label        lblColTarget, lblColLive, lblColDiff;

    juce::Slider  sliderThreshold, sliderMaxBands, sliderAvgTime, sliderOffset, sliderTargetOffset;
    juce::Label   lblThreshold, lblMaxBands, lblAvgTime, lblOffset, lblTargetOffset;
    juce::Label   lblStatus;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>  attEnabled;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>  attBypass;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>  attFreeze;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  attThreshold;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  attMaxBands;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  attAvgTime;

    // Colour selector state (SafePointer goes null when CallOutBox destroys the selector)
    juce::Component::SafePointer<juce::ColourSelector> activeColourSelector;
    int activeColourTarget { 0 };  // 0=live, 1=target, 2=diff

    juce::File lastCurvesDir;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PaAutoEQEditor)
};
