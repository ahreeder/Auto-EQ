#include "TargetCurve.h"
#include <cmath>
#include <algorithm>

bool TargetCurve::loadFromFile (const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    const auto text = file.loadFileAsString();
    auto json = juce::JSON::parse (text);

    if (!json.isObject())
        return false;

    auto* freqsVar = json.getDynamicObject()->getProperties().getVarPointer ("freqs");
    auto* dbVar    = json.getDynamicObject()->getProperties().getVarPointer ("db");

    if (freqsVar == nullptr || dbVar == nullptr)
        return false;

    auto& freqsArr = *freqsVar->getArray();
    auto& dbArr    = *dbVar->getArray();

    if (freqsArr.size() != dbArr.size() || freqsArr.size() < 2)
        return false;

    points.clear();
    points.reserve (freqsArr.size());

    for (int i = 0; i < freqsArr.size(); ++i)
        points.push_back ({ static_cast<float> (freqsArr[i]),
                            static_cast<float> (dbArr[i]) });

    // Ensure sorted by frequency
    std::sort (points.begin(), points.end(),
               [] (const CurvePoint& a, const CurvePoint& b) { return a.freq < b.freq; });

    name = file.getFileNameWithoutExtension();
    return true;
}

float TargetCurve::getDbAtFreq (float freq) const
{
    if (points.empty())
        return 0.0f;

    if (freq <= points.front().freq) return points.front().db;
    if (freq >= points.back().freq)  return points.back().db;

    // Binary search for surrounding points
    auto it = std::lower_bound (points.begin(), points.end(), freq,
                                [] (const CurvePoint& p, float f) { return p.freq < f; });

    if (it == points.begin()) return it->db;
    const auto& hi = *it;
    const auto& lo = *std::prev (it);

    // Log-frequency interpolation
    const float logLo = std::log10 (lo.freq);
    const float logHi = std::log10 (hi.freq);
    const float logF  = std::log10 (freq);

    if (std::abs (logHi - logLo) < 1e-6f)
        return lo.db;

    const float t = (logF - logLo) / (logHi - logLo);
    return lo.db + t * (hi.db - lo.db);
}

void TargetCurve::interpolateTo (const float* freqs, float* outDb, int n) const
{
    for (int i = 0; i < n; ++i)
        outDb[i] = getDbAtFreq (freqs[i]);
}
