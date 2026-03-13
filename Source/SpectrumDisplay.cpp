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

float SpectrumDisplay::xToFreq (float x, float w) const noexcept
{
    constexpr float logMin = 1.47712f;
    constexpr float logMax = 4.20412f;
    const float t = x / w;
    return std::pow (10.0f, logMin + t * (logMax - logMin));
}

float SpectrumDisplay::yToDb (float y, float h) const noexcept
{
    const float t = y / h;
    return DB_MAX + t * (DB_MIN - DB_MAX);
}

// ── Hit testing ───────────────────────────────────────────────────────────

int SpectrumDisplay::hitTestBand (int mx, int my) const
{
    const float w = (float) getWidth();
    const float h = (float) getHeight();

    juce::SpinLock::ScopedLockType sl (dataLock);

    int   bestBand = -1;
    float bestDist = HIT_RADIUS;

    for (int b = 0; b < MAX_BANDS; ++b)
    {
        if (!bands[b].active)
            continue;

        const float bx   = freqToX (bands[b].freq,   w);
        const float by   = dbToY   (bands[b].gainDb, h);
        const float dist = std::hypot ((float) mx - bx, (float) my - by);

        if (dist < bestDist)
        {
            bestDist = dist;
            bestBand = b;
        }
    }

    return bestBand;
}

// ── Mouse interaction ─────────────────────────────────────────────────────

void SpectrumDisplay::mouseMove (const juce::MouseEvent& e)
{
    const bool overBand = (hitTestBand (e.x, e.y) >= 0);
    setMouseCursor (overBand ? juce::MouseCursor::DraggingHandCursor
                             : juce::MouseCursor::NormalCursor);
}

void SpectrumDisplay::mouseDown (const juce::MouseEvent& e)
{
    dragBandIdx = hitTestBand (e.x, e.y);
}

void SpectrumDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragBandIdx < 0 || !onBandChanged)
        return;

    const float w = (float) getWidth();
    const float h = (float) getHeight();

    const float newFreq = std::clamp (xToFreq ((float) e.x, w),
                                      FFTAnalyzer::F_MIN, FFTAnalyzer::F_MAX);
    const float newGain = std::clamp (yToDb   ((float) e.y, h), -24.0f, 24.0f);

    BandParams p;
    {
        juce::SpinLock::ScopedLockType sl (dataLock);
        p          = bands[dragBandIdx];
        p.freq     = newFreq;
        p.gainDb   = newGain;
        bands[dragBandIdx] = p;
    }

    onBandChanged (dragBandIdx, p);
    repaint();
}

void SpectrumDisplay::mouseUp (const juce::MouseEvent&)
{
    dragBandIdx = -1;
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
        const float y = dbToY   (db[i],    h);

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

    const float freqLines[] = { 31.5f, 63.0f, 125.0f, 250.0f, 500.0f,
                                  1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f };
    g.setColour (juce::Colour (0xFF2A2A3E));
    for (float f : freqLines)
    {
        const float x = freqToX (f, w);
        g.drawVerticalLine (juce::roundToInt (x), 0.0f, h);
    }

    for (float db = DB_MIN + 10.0f; db <= DB_MAX; db += 10.0f)
    {
        const float y = dbToY (db, h);
        g.setColour (db == 0.0f ? juce::Colour (0xFF3A3A5E)
                                : juce::Colour (0xFF2A2A3E));
        g.drawHorizontalLine (juce::roundToInt (y), 0.0f, w);
    }

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

    for (float db = -60.0f; db <= 0.0f; db += 20.0f)
    {
        const float y = dbToY (db, h);
        g.drawText (juce::String (juce::roundToInt (db)) + "dB",
                    2, juce::roundToInt (y) - 6, 32, 12,
                    juce::Justification::left);
    }
}

// ── Bar RTA drawing ───────────────────────────────────────────────────────

void SpectrumDisplay::drawBars (juce::Graphics& g,
                                  const std::vector<float>& freqs,
                                  const std::vector<float>& db,
                                  float w, float h) const
{
    const float bottomY = h;
    const int   n       = (int) freqs.size();
    // Stride so we draw approximately barResolution bars across the range
    const int   stride  = std::max (1, n / std::max (1, barResolution));

    for (int i = 0; i < n; i += stride)
    {
        if (freqs[i] < FFTAnalyzer::F_MIN || freqs[i] > FFTAnalyzer::F_MAX)
            continue;

        const float x    = freqToX (freqs[i], w);
        const float topY = dbToY   (db[i],    h);

        const float prevX = (i >= stride)
            ? freqToX (freqs[i - stride], w) : x - 2.0f;
        const float nextX = (i + stride < n)
            ? freqToX (freqs[i + stride], w) : x + 2.0f;

        const float halfLeft  = std::max ((x - prevX) * 0.5f, 0.5f);
        const float halfRight = std::max ((nextX - x) * 0.5f, 0.5f);

        const float barLeft = x - halfLeft;
        const float barW    = halfLeft + halfRight;
        const float barH    = bottomY - topY;

        if (barH <= 0.0f)
            continue;

        const float normLevel = 1.0f - (topY / h);
        const juce::Colour barColour = colLive
            .withAlpha (0.55f + normLevel * 0.30f)
            .darker    (0.4f * (1.0f - normLevel));

        g.setColour (barColour);
        g.fillRect  (barLeft, topY, barW, barH);
    }
}

// ── Paint ─────────────────────────────────────────────────────────────────

void SpectrumDisplay::paint (juce::Graphics& g)
{
    const float w = static_cast<float> (getWidth());
    const float h = static_cast<float> (getHeight());

    std::vector<float> lf, ld, tf, td;
    std::array<BandParams, MAX_BANDS> bds;
    {
        juce::SpinLock::ScopedLockType sl (dataLock);
        lf  = liveFreqs;   ld = liveDb;
        tf  = targetFreqs; td = targetDb;
        bds = bands;
    }

    drawGrid (g, w, h);

    // Apply display offset to live data copy
    std::vector<float> ldOff = ld;
    for (auto& v : ldOff) v += displayOffsetDb;

    if (displayMode == DisplayMode::Bars)
    {
        // Bar RTA for live spectrum
        if (!lf.empty() && lf.size() == ldOff.size())
            drawBars (g, lf, ldOff, w, h);
    }
    else
    {
        // Diff fill (between live and target)
        if (!lf.empty() && !tf.empty() && lf.size() == ldOff.size() && tf.size() == td.size())
        {
            juce::Path diffFill;
            bool started = false;

            for (size_t i = 0; i < lf.size(); ++i)
            {
                if (lf[i] < FFTAnalyzer::F_MIN || lf[i] > FFTAnalyzer::F_MAX) continue;
                const float x       = freqToX (lf[i], w);
                const float yTarget = dbToY (i < td.size() ? td[i] : 0.0f, h);

                if (!started) { diffFill.startNewSubPath (x, yTarget); started = true; }
                else            diffFill.lineTo (x, yTarget);
            }
            for (int i = (int) lf.size() - 1; i >= 0; --i)
            {
                if (lf[i] < FFTAnalyzer::F_MIN || lf[i] > FFTAnalyzer::F_MAX) continue;
                diffFill.lineTo (freqToX (lf[i], w), dbToY (ldOff[i], h));
            }
            diffFill.closeSubPath();

            g.setColour (colDiff);
            g.fillPath  (diffFill);
        }

        // Live curve
        if (!lf.empty() && lf.size() == ldOff.size())
        {
            auto path = buildCurvePath (lf, ldOff, w, h);
            g.setColour (colLive);
            g.strokePath (path, juce::PathStrokeType (2.0f));
        }
    }

    // Target curve (always drawn)
    if (!tf.empty() && tf.size() == td.size())
    {
        auto path = buildCurvePath (tf, td, w, h);
        g.setColour (colTarget);
        g.strokePath (path, juce::PathStrokeType (1.5f));
    }

    // Band markers (inline using local bds copy)
    for (int b = 0; b < MAX_BANDS; ++b)
    {
        if (!bds[b].active) continue;

        const float x      = freqToX (bds[b].freq,   w);
        const float gainDb = bds[b].gainDb;
        const float y0     = dbToY   (0.0f,    h);
        const float yGain  = dbToY   (gainDb,  h);
        const bool  lk     = bds[b].locked;

        juce::Colour col = gainDb > 0.0f ? juce::Colour (0xFF00FF80)
                                         : juce::Colour (0xFFFF4040);
        if (lk) col = col.withAlpha (0.6f).brighter (0.2f);

        g.setColour (col.withAlpha (0.7f));
        const float lt = std::min (y0, yGain);
        const float lb = std::max (y0, yGain);
        if (lb > lt) g.fillRect (x - 0.5f, lt, 1.5f, lb - lt);

        g.setColour (col);
        g.fillEllipse (x - 6.0f, yGain - 6.0f, 12.0f, 12.0f);

        if (lk)
        {
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.drawEllipse (x - 6.0f, yGain - 6.0f, 12.0f, 12.0f, 1.5f);
        }

        g.setFont (9.0f);
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawText (juce::String (b + 1),
                    juce::roundToInt (x) - 8, juce::roundToInt (yGain) - 20,
                    16, 12, juce::Justification::centred);

        g.setColour (col.brighter (0.2f));
        g.setFont (9.0f);
        g.drawText (juce::String (gainDb, 1) + "dB",
                    juce::roundToInt (x) - 18, juce::roundToInt (yGain) + 8,
                    36, 11, juce::Justification::centred);
    }
}
