#include "ConvergenceEngine.h"
#include <cmath>
#include <algorithm>
#include <numeric>

ConvergenceState ConvergenceEngine::tick (const float* currentDb,
                                          const float* targetDb,
                                          const float* freqs,
                                          std::array<BandParams, MAX_BANDS>& bands,
                                          const ConvergenceSettings& settings)
{
    // ── 1. Compute diff (shape only — remove level offset) ────────────────
    float diff[N];
    float sumDiff = 0.0f;
    for (int i = 0; i < N; ++i)
    {
        diff[i]  = targetDb[i] - currentDb[i];
        sumDiff += diff[i];
    }
    // Subtract mean so we only correct spectral shape, not absolute level
    const float meanDiff = sumDiff / N;
    for (int i = 0; i < N; ++i)
        diff[i] -= meanDiff;

    // ── 2. Find max deviation ─────────────────────────────────────────────
    float maxDev = 0.0f;
    for (int i = 0; i < N; ++i)
        maxDev = std::max (maxDev, std::abs (diff[i]));

    ConvergenceState state;
    state.maxDeviation = maxDev;
    state.converged    = (maxDev <= settings.threshold);

    // ── 3. Nudge existing active bands ────────────────────────────────────
    int activeBands = 0;
    for (int b = 0; b < MAX_BANDS; ++b)
    {
        if (!bands[b].active)
            continue;

        ++activeBands;

        // Find the nearest frequency bin to this band's centre
        float minDist = 1e9f;
        int   bestBin = 0;
        for (int i = 0; i < N; ++i)
        {
            const float d = std::abs (std::log10 (freqs[i]) - std::log10 (bands[b].freq));
            if (d < minDist) { minDist = d; bestBin = i; }
        }

        // Nudge gain by up to stepSize toward the diff at that bin
        const float needed    = diff[bestBin];
        const float nudge     = std::clamp (needed, -settings.stepSize, settings.stepSize);
        bands[b].gainDb      += nudge;
        bands[b].gainDb       = std::clamp (bands[b].gainDb, -24.0f, 24.0f);

        // Deactivate band if it has faded to near-zero
        if (std::abs (bands[b].gainDb) < 0.1f)
        {
            bands[b].active = false;
            bands[b].gainDb = 0.0f;
            --activeBands;
        }
    }

    // ── 4. Allocate new bands if needed ──────────────────────────────────
    if (!state.converged && activeBands < settings.maxBands)
    {
        // Build a mask of uncovered frequencies (1 = free, 0 = already covered)
        float mask[N];
        std::fill (mask, mask + N, 1.0f);

        // Mask out regions already covered by active bands
        for (int b = 0; b < MAX_BANDS; ++b)
            if (bands[b].active)
                maskAround (mask, freqs, bands[b].freq);

        // Find the bin with maximum |diff| that is still unmasked
        float biggestDev = 0.0f;
        int   biggestBin = -1;
        for (int i = 0; i < N; ++i)
        {
            const float d = std::abs (diff[i]) * mask[i];
            if (d > biggestDev && d > settings.threshold)
            {
                biggestDev = d;
                biggestBin = i;
            }
        }

        if (biggestBin >= 0)
        {
            // Find a free band slot
            for (int b = 0; b < MAX_BANDS; ++b)
            {
                if (bands[b].active)
                    continue;

                bands[b].freq   = freqs[biggestBin];
                bands[b].gainDb = std::copysign (settings.stepSize, diff[biggestBin]);
                bands[b].q      = estimateQ (diff, biggestBin, freqs);
                bands[b].type   = detectFilterType (diff, biggestBin, N);
                bands[b].active = true;
                break;
            }
        }
    }

    // Count final active bands
    state.activeBands = 0;
    for (int b = 0; b < MAX_BANDS; ++b)
        if (bands[b].active)
            ++state.activeBands;

    return state;
}

// ── Helpers ───────────────────────────────────────────────────────────────

float ConvergenceEngine::estimateQ (const float* diff, int peakIdx,
                                     const float* freqs) const
{
    const float peakVal  = std::abs (diff[peakIdx]);
    const float halfPow  = peakVal - 3.0f;

    int left = peakIdx - 1;
    while (left > 0 && std::abs (diff[left]) > halfPow)
        --left;

    int right = peakIdx + 1;
    while (right < N - 1 && std::abs (diff[right]) > halfPow)
        ++right;

    const float bw = freqs[right] - freqs[left];
    if (bw <= 0.0f)
        return 1.41f;

    return std::clamp (freqs[peakIdx] / bw, 0.3f, 10.0f);
}

void ConvergenceEngine::maskAround (float* mask, const float* freqs,
                                     float centerFreq) const
{
    // Mask ±0.6 octave around centerFreq
    const float lo = centerFreq * std::pow (2.0f, -0.6f);
    const float hi = centerFreq * std::pow (2.0f,  0.6f);

    for (int i = 0; i < N; ++i)
        if (freqs[i] >= lo && freqs[i] <= hi)
            mask[i] = 0.0f;
}

FilterType ConvergenceEngine::detectFilterType (const float* diff, int peakIdx,
                                                 int n) const
{
    // Shelf detection: if the peak is near the top or bottom 15% of the
    // frequency range AND the diff stays elevated on one side, use a shelf.
    const float relPos = static_cast<float> (peakIdx) / n;

    if (relPos < 0.15f)
    {
        // Check that most bins below the peak also deviate in the same direction
        int sameDir = 0;
        const float sign = diff[peakIdx] > 0 ? 1.0f : -1.0f;
        for (int i = 0; i <= peakIdx; ++i)
            if (diff[i] * sign > 0.0f) ++sameDir;

        if (sameDir > (peakIdx + 1) / 2)
            return FilterType::LowShelf;
    }

    if (relPos > 0.85f)
    {
        int sameDir = 0;
        const float sign = diff[peakIdx] > 0 ? 1.0f : -1.0f;
        for (int i = peakIdx; i < n; ++i)
            if (diff[i] * sign > 0.0f) ++sameDir;

        if (sameDir > (n - peakIdx) / 2)
            return FilterType::HighShelf;
    }

    return FilterType::Peak;
}
