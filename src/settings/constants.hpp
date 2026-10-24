// settings/constants.hpp

#pragma once

#include "raylib.h"

#include "elfTypesRL/IntVector2.hpp"

namespace Consts
{
    inline constexpr float FLOAT_BREAKDOWN_CONSTANT = 16380; // calculated for 600 fps
    inline constexpr int ICS = 1000;                         // integer constants scale

    inline constexpr float SQRT3 = 1.7320508075688772f;
    inline constexpr int SQRT3I = SQRT3 * ICS;

    inline constexpr float SEX2RAD = (PI / 3.0f);

    namespace Vec
    {
        inline static constexpr Vector2 VEC2_ZERO{0, 0};
        inline static constexpr IntVector2 INT_VEC2_ZERO{0, 0};
    } // namespace Vec
} // namespace Consts
