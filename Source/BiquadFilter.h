#pragma once

enum class FilterType { Peak = 0, LowShelf = 1, HighShelf = 2 };

// Direct Form II Transposed biquad — RBJ Audio EQ Cookbook formulas.
class BiquadFilter
{
public:
    void setParams (FilterType type, double freq, double gainDb,
                    double q, double sampleRate) noexcept;

    float processSample (float x) noexcept;
    void  processBlock  (float* data, int numSamples) noexcept;
    void  reset()  noexcept;

private:
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;   // TDF-II state
};
