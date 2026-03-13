#include "BiquadFilter.h"
#include <cmath>
#include <algorithm>

static constexpr double PI = 3.14159265358979323846;

void BiquadFilter::setParams (FilterType type, double freq, double gainDb,
                               double q, double sampleRate) noexcept
{
    // Guard against Nyquist overshoot
    freq = std::min (freq, sampleRate * 0.499);
    q    = std::max (q, 0.01);

    const double A     = std::pow (10.0, gainDb / 40.0);
    const double w0    = 2.0 * PI * freq / sampleRate;
    const double cosw0 = std::cos (w0);
    const double sinw0 = std::sin (w0);
    const double alpha = sinw0 / (2.0 * q);
    const double sqrtA = std::sqrt (A);

    double b0_, b1_, b2_, a0_, a1_, a2_;

    switch (type)
    {
        case FilterType::Peak:
            b0_ =  1.0 + alpha * A;
            b1_ = -2.0 * cosw0;
            b2_ =  1.0 - alpha * A;
            a0_ =  1.0 + alpha / A;
            a1_ = -2.0 * cosw0;
            a2_ =  1.0 - alpha / A;
            break;

        case FilterType::LowShelf:
            b0_ =  A * ((A + 1.0) - (A - 1.0) * cosw0 + 2.0 * sqrtA * alpha);
            b1_ =  2.0 * A * ((A - 1.0) - (A + 1.0) * cosw0);
            b2_ =  A * ((A + 1.0) - (A - 1.0) * cosw0 - 2.0 * sqrtA * alpha);
            a0_ =      (A + 1.0) + (A - 1.0) * cosw0 + 2.0 * sqrtA * alpha;
            a1_ = -2.0 * ((A - 1.0) + (A + 1.0) * cosw0);
            a2_ =       (A + 1.0) + (A - 1.0) * cosw0 - 2.0 * sqrtA * alpha;
            break;

        case FilterType::HighShelf:
            b0_ =  A * ((A + 1.0) + (A - 1.0) * cosw0 + 2.0 * sqrtA * alpha);
            b1_ = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosw0);
            b2_ =  A * ((A + 1.0) + (A - 1.0) * cosw0 - 2.0 * sqrtA * alpha);
            a0_ =       (A + 1.0) - (A - 1.0) * cosw0 + 2.0 * sqrtA * alpha;
            a1_ =  2.0 * ((A - 1.0) - (A + 1.0) * cosw0);
            a2_ =       (A + 1.0) - (A - 1.0) * cosw0 - 2.0 * sqrtA * alpha;
            break;

        default:
            b0_ = 1.0; b1_ = 0.0; b2_ = 0.0;
            a0_ = 1.0; a1_ = 0.0; a2_ = 0.0;
            break;
    }

    b0 = b0_ / a0_;
    b1 = b1_ / a0_;
    b2 = b2_ / a0_;
    a1 = a1_ / a0_;
    a2 = a2_ / a0_;
}

float BiquadFilter::processSample (float x) noexcept
{
    const double xd = static_cast<double> (x);
    const double y  = b0 * xd + z1;
    z1 = b1 * xd - a1 * y + z2;
    z2 = b2 * xd - a2 * y;
    return static_cast<float> (y);
}

void BiquadFilter::processBlock (float* data, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
        data[i] = processSample (data[i]);
}

void BiquadFilter::reset() noexcept
{
    z1 = z2 = 0.0;
}
