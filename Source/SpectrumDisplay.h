#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "FFTAnalyzer.h"
#include "EQBand.h"
#include <array>

// Draws the live spectrum, target curve, difference fill, and EQ band markers.
class SpectrumDisplay : public juce::Component
{
public:
    SpectrumDisplay();

    // Called from message thread (timer) to push new data before repaint
    void updateLive   (const float* freqs, const float* db, int n);
    void updateTarget (const float* freqs, const float* db, int n);
    void updateBands  (const std::array<BandParams, MAX_BANDS>& bands);

    void paint (juce::Graphics& g) override;
    void resized() override {}

    // Colours (can be set from editor)
    juce::Colour colLive   { 0xFF00C8FF };
    juce::Colour colTarget { 0xFFFFAA00 };
    juce::Colour colDiff   { 0x4000FF80 };

private:
    static constexpr float DB_MIN = -80.0f;
    static constexpr float DB_MAX =   6.0f;

    mutable juce::SpinLock dataLock;

    std::vector<float> liveFreqs, liveDb;
    std::vector<float> targetFreqs, targetDb;
    std::array<BandParams, MAX_BANDS> bands {};

    // Map dB value to y pixel
    float dbToY (float db, float height) const noexcept;
    // Map frequency to x pixel (log scale)
    float freqToX (float freq, float width) const noexcept;

    // Draw a curve from parallel freq/db arrays
    juce::Path buildCurvePath (const std::vector<float>& freqs,
                               const std::vector<float>& db,
                               float w, float h) const;

    void drawGrid (juce::Graphics& g, float w, float h) const;
    void drawBandMarkers (juce::Graphics& g, float w, float h) const;
};
