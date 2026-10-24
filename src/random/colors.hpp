// random/colors.hpp

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "raylib.h"

#include "random/random.hpp"
#include "settings/defaults.hpp"

constexpr uint32_t Hash32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;
    return x;
}

inline Color ColorFromInt(int num)
{
    num += 18; // shift for small numbers to pleasant-er colours

    uint32_t h = Hash32(static_cast<uint32_t>(num));

    float hue = (h & 0xFFFF) * (360.0f / 65536.0f);
    float sat = 0.65f + ((h >> 16) & 0xFF) * (0.25f / 255.0f);
    float val = 0.80f + ((h >> 24) & 0xFF) * (0.20f / 255.0f);

    return ColorFromHSV(hue, sat, val);
}

inline unsigned char LerpColourChannel(unsigned char start, int target, float amount)
{
    return static_cast<unsigned char>(
        std::lround(static_cast<float>(start) +
                    (static_cast<float>(target) - static_cast<float>(start)) * amount));
}
