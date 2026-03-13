#include "EQBank.h"

EQBank::EQBank()
{
    activeParams.fill ({});
    pendingParams.fill ({});
}

void EQBank::prepare (double sr, int /*maxBlockSize*/)
{
    sampleRate = sr;
    reset();

    // Recompute coefficients for any already-configured bands
    for (int i = 0; i < MAX_BANDS; ++i)
        if (activeParams[i].active)
            recomputeCoeffs (i);
}

void EQBank::processBlock (float* left, float* right, int numSamples) noexcept
{
    applyPendingIfDirty();

    for (int i = 0; i < MAX_BANDS; ++i)
    {
        if (!activeParams[i].active)
            continue;

        if (left  != nullptr) filtersL[i].processBlock (left,  numSamples);
        if (right != nullptr) filtersR[i].processBlock (right, numSamples);
    }
}

void EQBank::reset()
{
    for (auto& f : filtersL) f.reset();
    for (auto& f : filtersR) f.reset();
}

void EQBank::setBands (const std::array<BandParams, MAX_BANDS>& newParams)
{
    juce::SpinLock::ScopedLockType sl (paramsLock);
    pendingParams = newParams;
    paramsDirty   = true;
}

std::array<BandParams, MAX_BANDS> EQBank::getBands() const
{
    juce::SpinLock::ScopedLockType sl (paramsLock);
    return activeParams;
}

void EQBank::applyPendingIfDirty() noexcept
{
    // Try-lock: if message thread holds it, skip — we'll catch it next block
    if (paramsLock.tryEnter())
    {
        if (paramsDirty)
        {
            // Find bands whose params actually changed, reset their filter state
            for (int i = 0; i < MAX_BANDS; ++i)
            {
                const auto& np = pendingParams[i];
                const auto& ap = activeParams[i];

                if (np.active != ap.active
                    || np.freq   != ap.freq
                    || np.gainDb != ap.gainDb
                    || np.q      != ap.q
                    || np.type   != ap.type)
                {
                    activeParams[i] = np;
                    filtersL[i].reset();
                    filtersR[i].reset();
                    if (np.active)
                        recomputeCoeffs (i);
                }
            }
            paramsDirty = false;
        }
        paramsLock.exit();
    }
}

void EQBank::recomputeCoeffs (int idx) noexcept
{
    const auto& p = activeParams[idx];
    filtersL[idx].setParams (p.type, p.freq, p.gainDb, p.q, sampleRate);
    filtersR[idx].setParams (p.type, p.freq, p.gainDb, p.q, sampleRate);
}
