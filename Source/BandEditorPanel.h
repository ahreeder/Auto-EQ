#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "EQBank.h"
#include <functional>
#include <array>

// Shows all 16 EQ band slots with freq / Q / gain sliders and a Lock toggle.
// Active bands are highlighted; inactive ones are dimmed.
// Call updateBands() every UI timer tick to keep sliders in sync.
class BandEditorPanel : public juce::Component
{
public:
    // Fired when the user changes a slider or toggles Lock.
    std::function<void (int bandIdx, BandParams)> onBandChanged;

    BandEditorPanel();
    ~BandEditorPanel() override = default;

    // Push new band state (does NOT fire onBandChanged).
    void updateBands (const std::array<BandParams, MAX_BANDS>& bands);

    void resized () override;
    void paint   (juce::Graphics& g) override;

    static constexpr int ROW_H      = 44;
    static constexpr int HEADER_H   = 22;
    static constexpr int PANEL_W    = 540;
    static int           panelHeight() { return HEADER_H + MAX_BANDS * ROW_H; }

private:
    struct BandRow
    {
        juce::Label        numLabel;
        juce::Slider       freqSlider, qSlider, gainSlider;
        juce::ToggleButton lockBtn { "Lock" };
        int                bandIdx { 0 };
    };

    std::array<BandRow, MAX_BANDS> rows;
    std::array<BandParams, MAX_BANDS> currentParams {};

    bool isUpdating { false };

    void setupRow (int rowIndex);
    void rowChanged (int bandIdx);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandEditorPanel)
};
