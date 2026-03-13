#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_core/juce_core.h>
#include <vector>
#include <array>

// Feeds audio samples, computes 8192-pt FFT, log-smooths onto N_POINTS grid.
// Thread model: pushSamples() called from audio thread,
//               getSpectrum() called from message thread.
class FFTAnalyzer
{
public:
    static constexpr int   FFT_ORDER = 13;          // 8192
    static constexpr int   FFT_SIZE  = 1 << FFT_ORDER;
    static constexpr int   N_POINTS  = 200;
    static constexpr float F_MIN     = 30.0f;
    static constexpr float F_MAX     = 16000.0f;
    FFTAnalyzer();

    void setSampleRate (double sr) { sampleRate = sr; }
    void reset();

    // Set how many seconds the EMA integrates over (converts to alpha internally).
    // Safe to call from any thread.
    void setAveragingTime (float seconds) noexcept;

    // Audio thread: feed one channel of samples
    void pushSamples (const float* data, int numSamples) noexcept;

    // Message thread: copy latest smoothed spectrum
    // outFreqs[N_POINTS], outDb[N_POINTS]
    void getSpectrum (float* outFreqs, float* outDb) const;

private:
    juce::dsp::FFT                          fft;
    juce::dsp::WindowingFunction<float>     window;

    // Lock-free FIFO for audio → FFT handoff
    std::array<float, FFT_SIZE>             fifo {};
    int                                     fifoIndex { 0 };

    // Double-buffered output
    mutable juce::SpinLock                  outputLock;
    std::array<float, N_POINTS>             outFreqs {};
    std::array<float, N_POINTS>             outDb    {};
    std::array<float, N_POINTS>             emaDb    {};
    bool                                    hasData  { false };

    std::atomic<float>  emaAlpha  { 0.15f };   // computed from averaging time
    double sampleRate { 48000.0 };

    std::array<float, FFT_SIZE * 2>         fftBuf {};  // interleaved complex

    void computeAndSmooth() noexcept;
};
