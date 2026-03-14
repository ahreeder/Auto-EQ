#include "TargetCurve.h"
#include <cmath>
#include <algorithm>

static bool loadFromJson (const juce::String& text, std::vector<CurvePoint>& points)
{
    auto json = juce::JSON::parse (text);
    if (!json.isObject()) return false;

    auto* freqsVar = json.getDynamicObject()->getProperties().getVarPointer ("freqs");
    auto* dbVar    = json.getDynamicObject()->getProperties().getVarPointer ("db");

    if (freqsVar == nullptr || dbVar == nullptr) return false;

    auto& freqsArr = *freqsVar->getArray();
    auto& dbArr    = *dbVar->getArray();

    if (freqsArr.size() != dbArr.size() || freqsArr.size() < 2) return false;

    points.reserve (freqsArr.size());
    for (int i = 0; i < freqsArr.size(); ++i)
        points.push_back ({ static_cast<float> (freqsArr[i]),
                            static_cast<float> (dbArr[i]) });
    return true;
}

// Parses Smaart .trf (Transfer Function Reference) files.
// Format: plain-text header block, optional [Data] section marker,
// then whitespace-separated columns: frequency  magnitude(dB)  [phase — ignored].
// Header lines are skipped; any line whose first token parses as a positive
// float is treated as a data row.
static bool loadFromTrf (const juce::String& text, std::vector<CurvePoint>& points)
{
    const auto lines = juce::StringArray::fromLines (text);
    bool inData = false;

    for (const auto& raw : lines)
    {
        const auto line = raw.trim();
        if (line.isEmpty()) continue;

        // Section markers like [Data] or [Smaart Transfer Function Reference]
        if (line.startsWith ("["))
        {
            inData = line.equalsIgnoreCase ("[data]") ||
                     line.startsWithIgnoreCase ("[data");
            continue;
        }

        // Skip key=value header lines
        if (line.contains ("=") && !line.startsWithChar ('-') &&
            !juce::CharacterFunctions::isDigit (line[0]))
            continue;

        // Try to parse first token as frequency
        const auto tokens = juce::StringArray::fromTokens (line, " \t,", "");
        if (tokens.size() < 2) continue;

        const float freq = tokens[0].getFloatValue();
        const float db   = tokens[1].getFloatValue();

        if (freq <= 0.0f) continue;  // skip malformed / header rows

        // Once we hit a valid data row we're implicitly in data mode
        inData = true;
        points.push_back ({ freq, db });
    }

    return points.size() >= 2;
}

bool TargetCurve::loadFromFile (const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    const auto text = file.loadFileAsString();
    const auto ext  = file.getFileExtension().toLowerCase();

    points.clear();

    bool ok = false;
    if (ext == ".trf")
        ok = loadFromTrf (text, points);
    else
        ok = loadFromJson (text, points);

    if (!ok) { points.clear(); return false; }

    std::sort (points.begin(), points.end(),
               [] (const CurvePoint& a, const CurvePoint& b) { return a.freq < b.freq; });

    name = file.getFileNameWithoutExtension();
    return true;
}

void TargetCurve::setPoints (const std::vector<float>& freqs,
                              const std::vector<float>& db,
                              const juce::String& curveName)
{
    jassert (freqs.size() == db.size());
    points.clear();
    points.reserve (freqs.size());
    for (size_t i = 0; i < freqs.size(); ++i)
        points.push_back ({ freqs[i], db[i] });

    std::sort (points.begin(), points.end(),
               [] (const CurvePoint& a, const CurvePoint& b) { return a.freq < b.freq; });

    name = curveName;
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
