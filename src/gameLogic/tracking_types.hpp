// gameLogic/tracking_types.hpp
#pragma once
#include <cstddef>

enum class Confidence
{
    Assumed,
    Inferred,
    Observed,
    Uncertain
};
using HintId = std::size_t;
inline constexpr HintId NO_HINT = static_cast<HintId>(-1);
