// render/animations.hpp
#pragma once
#include <cmath>

inline float ApproachValue(float current, float target, float dt, float speed = 14,
                           float tolerance = 0.01f)
{
    const auto next = current + (target - current) * (1 - std::exp(-speed * dt));
    return std::abs(target - next) <= tolerance ? target : next;
}
