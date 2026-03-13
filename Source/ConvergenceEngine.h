#pragma once
#include "EQBank.h"
#include "FFTAnalyzer.h"
#include <array>

struct ConvergenceSettings
{
    float threshold { 2.0f };    // dB — bands within this are left alone
    float stepSize  { 0.5f };    // dB nudge per tick
    int   maxBands  { 10 };      // max active bands allowed
};

struct ConvergenceState
{
    float maxDeviation { 0.0f };
    bool  converged    { false };
    int   activeBands  { 0 };
};

// Runs on the message thread (timer, ~10 Hz).
// Reads current spectrum + target, nudges EQBand params toward target.
class ConvergenceEngine
{
public:
    // Call every timer tick.
    // currentDb[N], targetDb[N], freqs[N] — all length FFTAnalyzer::N_POINTS
    // bands — modified in place, then caller pushes to EQBank
    ConvergenceState tick (const float* currentDb,
                           const float* targetDb,
                           const float* freqs,
                           std::array<BandParams, MAX_BANDS>& bands,
                           const ConvergenceSettings& settings);

private:
    static constexpr int N = FFTAnalyzer::N_POINTS;

    // Estimate Q from peak width in the diff array
    float estimateQ (const float* diff, int peakIdx, const float* freqs) const;

    // Zero out ±0.5 octave around a frequency in a diff mask
    void maskAround (float* mask, const float* freqs, float centerFreq) const;

    // Detect whether a region looks like a shelf vs peak
    FilterType detectFilterType (const float* diff, int peakIdx, int n) const;
};
