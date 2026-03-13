#pragma once
#include "BiquadFilter.h"

struct BandParams
{
    FilterType type   = FilterType::Peak;
    float      freq   = 1000.0f;
    float      gainDb = 0.0f;
    float      q      = 1.41f;
    bool       active = false;
    bool       locked = false;
};
