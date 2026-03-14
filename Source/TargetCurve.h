#pragma once
#include <juce_core/juce_core.h>
#include <vector>

struct CurvePoint { float freq; float db; };

// Loads target curves saved by the Python PA Tuning Tool.
// JSON format: { "freqs": [...], "db": [...] }
class TargetCurve
{
public:
    bool loadFromFile (const juce::File& file);
    void setPoints   (const std::vector<float>& freqs, const std::vector<float>& db,
                      const juce::String& curveName = "edited");
    void clear() { points.clear(); name = {}; }

    bool         isLoaded()   const { return !points.empty(); }
    juce::String getName()    const { return name; }

    const std::vector<CurvePoint>& getPoints() const { return points; }

    // Linear interpolation in log-frequency space
    float getDbAtFreq (float freq) const;

    // Fill a pre-allocated array (same grid as FFTAnalyzer::N_POINTS)
    // freqs[n] — input frequency grid, outDb[n] — output, n points
    void interpolateTo (const float* freqs, float* outDb, int n) const;

private:
    std::vector<CurvePoint> points;
    juce::String            name;
};
