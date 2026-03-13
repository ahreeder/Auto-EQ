#pragma once
#include "EQBand.h"
#include <juce_core/juce_core.h>
#include <array>

static constexpr int MAX_BANDS = 16;

// Thread-safe bank of parametric biquad filters.
// setBands() is called from the message thread (convergence engine).
// processBlock() is called from the audio thread.
class EQBank
{
public:
    EQBank();

    void prepare (double sampleRate, int maxBlockSize);
    void processBlock (float* left, float* right, int numSamples) noexcept;
    void reset();

    // Message thread: push new band configuration
    void setBands (const std::array<BandParams, MAX_BANDS>& newParams);

    // Message thread: read current configuration
    std::array<BandParams, MAX_BANDS> getBands() const;

private:
    mutable juce::SpinLock              paramsLock;
    std::array<BandParams, MAX_BANDS>   pendingParams;
    std::array<BandParams, MAX_BANDS>   activeParams;
    bool                                paramsDirty { false };

    std::array<BiquadFilter, MAX_BANDS> filtersL;
    std::array<BiquadFilter, MAX_BANDS> filtersR;
    double sampleRate { 48000.0 };

    void applyPendingIfDirty() noexcept;
    void recomputeCoeffs (int idx) noexcept;
};
