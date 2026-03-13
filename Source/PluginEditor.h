#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "SpectrumDisplay.h"

class PaAutoEQEditor final : public juce::AudioProcessorEditor,
                              private juce::Timer
{
public:
    explicit PaAutoEQEditor (PaAutoEQProcessor&);
    ~PaAutoEQEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    void timerCallback() override;
    void loadCurveClicked();
    void saveCurveClicked();
    void resetClicked();

    PaAutoEQProcessor& processor;

    SpectrumDisplay spectrumDisplay;

    // Controls
    juce::ToggleButton btnEnabled  { "Auto EQ" };
    juce::ToggleButton btnBypass   { "Bypass" };
    juce::TextButton   btnLoad     { "Load Curve" };
    juce::TextButton   btnSave     { "Save Curve" };
    juce::TextButton   btnReset    { "Reset EQ" };

    juce::Slider  sliderThreshold, sliderSpeed;
    juce::Slider  sliderMaxBands;
    juce::Label   lblThreshold, lblSpeed, lblMaxBands;
    juce::Label   lblCurve, lblStatus;

    // Parameter attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>  attEnabled;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>  attBypass;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  attThreshold;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  attSpeed;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  attMaxBands;

    juce::File lastCurvesDir;   // remembered across Load/Save dialogs

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PaAutoEQEditor)
};
