#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "FFTAnalyzer.h"
#include "EQBank.h"
#include <array>
#include <functional>

class SpectrumDisplay : public juce::Component
{
public:
    enum class DisplayMode { Line, Bars };

    SpectrumDisplay();

    // Called from message thread (timer) to push new data before repaint
    void updateLive   (const float* freqs, const float* db, int n);
    void updateTarget (const float* freqs, const float* db, int n);
    void updateBands  (const std::array<BandParams, MAX_BANDS>& bands);

    void setDisplayMode (DisplayMode m) { displayMode = m; repaint(); }
    DisplayMode getDisplayMode() const  { return displayMode; }

    void  setDisplayOffset (float db) { displayOffsetDb = db; repaint(); }
    float getDisplayOffset()  const   { return displayOffsetDb; }

    void  setTargetOffset (float db) { targetOffsetDb = db; repaint(); }
    float getTargetOffset()  const   { return targetOffsetDb; }

    // Number of bars to draw in Bars mode (decimates the 200-pt grid).
    void setBarResolution (int n) { barResolution = std::max (1, n); repaint(); }
    int  getBarResolution()  const { return barResolution; }

    void paint   (juce::Graphics& g) override;
    void resized () override {}

    void mouseMove  (const juce::MouseEvent& e) override;
    void mouseDown  (const juce::MouseEvent& e) override;
    void mouseDrag  (const juce::MouseEvent& e) override;
    void mouseUp    (const juce::MouseEvent& e) override;

    // Fired on message thread when user drags a band marker
    std::function<void (int bandIdx, BandParams)> onBandChanged;

    // Colours
    juce::Colour colLive   { 0xFF00C8FF };
    juce::Colour colTarget { 0xFFFFAA00 };
    juce::Colour colDiff   { 0x4000FF80 };

private:
    static constexpr float DB_MIN = -80.0f;
    static constexpr float DB_MAX =   6.0f;
    static constexpr float HIT_RADIUS = 14.0f;

    mutable juce::SpinLock dataLock;

    std::vector<float> liveFreqs, liveDb;
    std::vector<float> targetFreqs, targetDb;
    std::array<BandParams, MAX_BANDS> bands {};

    DisplayMode displayMode     { DisplayMode::Line };
    float       displayOffsetDb { 0.0f };
    float       targetOffsetDb  { 0.0f };
    int         barResolution   { 200 };

    // Drag state
    int   dragBandIdx { -1 };

    // Coordinate helpers
    float dbToY   (float db,   float height) const noexcept;
    float freqToX (float freq, float width)  const noexcept;
    float xToFreq (float x,    float width)  const noexcept;
    float yToDb   (float y,    float height) const noexcept;

    int  hitTestBand (int mx, int my) const;  // returns band index or -1

    juce::Path buildCurvePath (const std::vector<float>& freqs,
                                const std::vector<float>& db,
                                float w, float h) const;

    void drawGrid (juce::Graphics& g, float w, float h) const;
    void drawBars (juce::Graphics& g,
                   const std::vector<float>& freqs,
                   const std::vector<float>& db,
                   float w, float h) const;
};
