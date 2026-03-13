#include "FFTAnalyzer.h"
#include <cmath>
#include <algorithm>
#include <numeric>

FFTAnalyzer::FFTAnalyzer()
    : fft    (FFT_ORDER),
      window (FFT_SIZE, juce::dsp::WindowingFunction<float>::hann)
{
    outDb.fill (-90.0f);
    emaDb.fill (-90.0f);

    // Pre-compute log-spaced frequency grid
    const float logMin = std::log10 (F_MIN);
    const float logMax = std::log10 (F_MAX);
    for (int i = 0; i < N_POINTS; ++i)
        outFreqs[i] = std::pow (10.0f, logMin + (logMax - logMin) * i / (N_POINTS - 1));
}

void FFTAnalyzer::reset()
{
    fifo.fill (0.0f);
    fifoIndex = 0;
    emaDb.fill (-90.0f);
    juce::SpinLock::ScopedLockType sl (outputLock);
    outDb.fill (-90.0f);
    hasData = false;
}

void FFTAnalyzer::pushSamples (const float* data, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        fifo[fifoIndex++] = data[i];

        if (fifoIndex >= FFT_SIZE)
        {
            fifoIndex = 0;
            computeAndSmooth();
        }
    }
}

void FFTAnalyzer::computeAndSmooth() noexcept
{
    // Copy fifo into fftBuf (real part), zero imaginary
    for (int i = 0; i < FFT_SIZE; ++i)
    {
        fftBuf[2 * i]     = fifo[i];
        fftBuf[2 * i + 1] = 0.0f;
    }

    // Apply Hann window
    window.multiplyWithWindowingTable (fftBuf.data(), FFT_SIZE);

    // Forward FFT (in-place, interleaved complex)
    fft.performFrequencyOnlyForwardTransform (fftBuf.data());

    // fftBuf now holds magnitudes in [0..FFT_SIZE/2]
    const int nBins = FFT_SIZE / 2 + 1;
    const float binWidth = static_cast<float> (sampleRate) / FFT_SIZE;

    // Convert to dB and interpolate onto log grid
    const float logMin = std::log10 (F_MIN);
    const float logMax = std::log10 (F_MAX);

    for (int k = 0; k < N_POINTS; ++k)
    {
        const float targetFreq = outFreqs[k];
        const float binF       = targetFreq / binWidth;
        const int   bin0       = static_cast<int> (binF);
        const int   bin1       = bin0 + 1;

        float mag = 0.0f;
        if (bin0 >= 0 && bin1 < nBins)
        {
            const float frac = binF - bin0;
            mag = fftBuf[bin0] * (1.0f - frac) + fftBuf[bin1] * frac;
        }
        else if (bin0 >= 0 && bin0 < nBins)
        {
            mag = fftBuf[bin0];
        }

        const float db = 20.0f * std::log10 (mag / FFT_SIZE + 1e-10f);

        // EMA smoothing
        emaDb[k] = EMA_ALPHA * db + (1.0f - EMA_ALPHA) * emaDb[k];
    }

    // Gaussian-style smoothing pass (5-tap) in log-frequency space
    std::array<float, N_POINTS> smoothed {};
    const float kernel[5] = { 0.0625f, 0.25f, 0.375f, 0.25f, 0.0625f };
    for (int k = 0; k < N_POINTS; ++k)
    {
        float acc = 0.0f;
        for (int t = -2; t <= 2; ++t)
        {
            int idx = std::clamp (k + t, 0, N_POINTS - 1);
            acc += kernel[t + 2] * emaDb[idx];
        }
        smoothed[k] = acc;
    }

    // Publish to output (briefly held lock)
    juce::SpinLock::ScopedLockType sl (outputLock);
    outDb   = smoothed;
    hasData = true;
}

void FFTAnalyzer::getSpectrum (float* destFreqs, float* destDb) const
{
    juce::SpinLock::ScopedLockType sl (outputLock);
    for (int i = 0; i < N_POINTS; ++i)
    {
        destFreqs[i] = outFreqs[i];
        destDb[i]    = outDb[i];
    }
}
