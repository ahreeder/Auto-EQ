#include "SpectrumDisplay.h"
#include <cmath>
#include <algorithm>

SpectrumDisplay::SpectrumDisplay()
{
    setOpaque (true);
}

void SpectrumDisplay::updateLive (const float* freqs, const float* db, int n)
{
    juce::SpinLock::ScopedLockType sl (dataLock);
    liveFreqs.assign (freqs, freqs + n);
    liveDb.assign    (db,    db    + n);
}

void SpectrumDisplay::updateTarget (const float* freqs, const float* db, int n)
{
    juce::SpinLock::ScopedLockType sl (dataLock);
    targetFreqs.assign (freqs, freqs + n);
    targetDb.assign    (db,    db    + n);
}

void SpectrumDisplay::updateBands (const std::array<BandParams, MAX_BANDS>& b)
{
    juce::SpinLock::ScopedLockType sl (dataLock);
    bands = b;
}

// ── Coordinate helpers ────────────────────────────────────────────────────

float SpectrumDisplay::dbToY (float db, float h) const noexcept
{
    const float t = (db - DB_MAX) / (DB_MIN - DB_MAX);
    return t * h;
}

float SpectrumDisplay::freqToX (float freq, float w) const noexcept
{
    constexpr float logMin = 1.47712f;  // log10(30)
    constexpr float logMax = 4.20412f;  // log10(16000)
    const float t = (std::log10 (std::max (freq, 1.0f)) - logMin) / (logMax - logMin);
    return t * w;
}

// ── Path builder ──────────────────────────────────────────────────────────

juce::Path SpectrumDisplay::buildCurvePath (const std::vector<float>& freqs,
                                             const std::vector<float>& db,
                                             float w, float h) const
{
    juce::Path p;
    bool started = false;

    for (size_t i = 0; i < freqs.size(); ++i)
    {
        if (freqs[i] < FFTAnalyzer::F_MIN || freqs[i] > FFTAnalyzer::F_MAX)
            continue;

        const float x = freqToX (freqs[i], w);
        const float y = dbToY (db[i], h);

        if (!started) { p.startNewSubPath (x, y); started = true; }
        else            p.lineTo (x, y);
    }

    return p;
}

// ── Grid ──────────────────────────────────────────────────────────────────

void SpectrumDisplay::drawGrid (juce::Graphics& g, float w, float h) const
{
    g.setColour (juce::Colour (0xFF1A1A2E));
    g.fillRect (0.0f, 0.0f, w, h);

    // Frequency grid lines
    const float freqLines[] = { 31.5f, 63.0f, 125.0f, 250.0f, 500.0f,
                                 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f };
    g.setColour (juce::Colour (0xFF2A2A3E));
    for (float f : freqLines)
    {
        const float x = freqToX (f, w);
        g.drawVerticalLine (juce::roundToInt (x), 0.0f, h);
    }

    // dB grid lines
    for (float db = DB_MIN + 10.0f; db <= DB_MAX; db += 10.0f)
    {
        const float y = dbToY (db, h);
        g.setColour (db == 0.0f ? juce::Colour (0xFF3A3A5E)
                                : juce::Colour (0xFF2A2A3E));
        g.drawHorizontalLine (juce::roundToInt (y), 0.0f, w);
    }

    // Frequency labels
    g.setColour (juce::Colour (0xFF6666AA));
    g.setFont (10.0f);
    const std::pair<float, const char*> labels[] = {
        { 63.0f, "63" }, { 125.0f, "125" }, { 250.0f, "250" },
        { 500.0f, "500" }, { 1000.0f, "1k" }, { 2000.0f, "2k" },
        { 4000.0f, "4k" }, { 8000.0f, "8k" }, { 16000.0f, "16k" }
    };
    for (auto& [f, label] : labels)
    {
        const float x = freqToX (f, w);
        g.drawText (label, juce::roundToInt (x) - 12, juce::roundToInt (h) - 14,
                    24, 12, juce::Justification::centred);
    }

    // dB labels
    for (float db = -60.0f; db <= 0.0f; db += 20.0f)
    {
        const float y = dbToY (db, h);
        g.drawText (juce::String (juce::roundToInt (db)) + "dB",
                    2, juce::roundToInt (y) - 6, 32, 12,
                    juce::Justification::left);
    }
}

// ── Band markers ──────────────────────────────────────────────────────────

void SpectrumDisplay::drawBandMarkers (juce::Graphics& g, float w, float h) const
{
    for (int b = 0; b < MAX_BANDS; ++b)
    {
        if (!bands[b].active)
            continue;

        const float x      = freqToX (bands[b].freq, w);
        const float gainDb = bands[b].gainDb;
        const float y      = dbToY (0.0f, h);

        const juce::Colour col = gainDb > 0.0f ? juce::Colour (0xFF00FF80)   // boost = green
                                               : juce::Colour (0xFFFF4040);  // cut   = red
        g.setColour (col);
        g.drawVerticalLine (juce::roundToInt (x), y - 4.0f, y + 4.0f);

        // Small triangle marker at gain position
        const float yGain = dbToY (gainDb, h);
        g.fillEllipse (x - 3.0f, yGain - 3.0f, 6.0f, 6.0f);

        // Gain label
        g.setFont (9.0f);
        g.drawText (juce::String (gainDb, 1) + "dB",
                    juce::roundToInt (x) - 16, juce::roundToInt (yGain) - 16,
                    32, 12, juce::Justification::centred);
    }
}

// ── Paint ─────────────────────────────────────────────────────────────────

void SpectrumDisplay::paint (juce::Graphics& g)
{
    const float w = static_cast<float> (getWidth());
    const float h = static_cast<float> (getHeight());

    // Copy data under lock
    std::vector<float> lf, ld, tf, td;
    std::array<BandParams, MAX_BANDS> bds;
    {
        juce::SpinLock::ScopedLockType sl (dataLock);
        lf  = liveFreqs;   ld = liveDb;
        tf  = targetFreqs; td = targetDb;
        bds = bands;
    }

    drawGrid (g, w, h);

    // Diff fill (between live and target)
    if (!lf.empty() && !tf.empty() && lf.size() == ld.size() && tf.size() == td.size())
    {
        juce::Path diffFill;
        bool started = false;

        for (size_t i = 0; i < lf.size(); ++i)
        {
            if (lf[i] < FFTAnalyzer::F_MIN || lf[i] > FFTAnalyzer::F_MAX) continue;

            const float x     = freqToX (lf[i], w);
            const float yLive = dbToY (ld[i], h);
            float tdb = 0.0f;
            // Nearest target point
            if (i < td.size()) tdb = td[i];
            const float yTarget = dbToY (tdb, h);

            if (!started) { diffFill.startNewSubPath (x, yTarget); started = true; }
            else            diffFill.lineTo (x, yTarget);
        }
        // Trace back along live curve
        for (int i = (int)lf.size() - 1; i >= 0; --i)
        {
            if (lf[i] < FFTAnalyzer::F_MIN || lf[i] > FFTAnalyzer::F_MAX) continue;
            diffFill.lineTo (freqToX (lf[i], w), dbToY (ld[i], h));
        }
        diffFill.closeSubPath();

        g.setColour (colDiff);
        g.fillPath (diffFill);
    }

    // Target curve
    if (!tf.empty() && tf.size() == td.size())
    {
        auto path = buildCurvePath (tf, td, w, h);
        g.setColour (colTarget);
        g.strokePath (path, juce::PathStrokeType (1.5f));
    }

    // Live curve
    if (!lf.empty() && lf.size() == ld.size())
    {
        auto path = buildCurvePath (lf, ld, w, h);
        g.setColour (colLive);
        g.strokePath (path, juce::PathStrokeType (2.0f));
    }

    drawBandMarkers (g, w, h);
}
