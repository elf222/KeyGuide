// elfTypesRL/bit_masks.hpp

#pragma once

#include <array>
#include <cstddef>

namespace BitMask
{
    template <std::size_t N> constexpr std::array<std::size_t, N> MakeBitMasks()
    {
        std::array<std::size_t, N> masks{};

        for (std::size_t i = 0; i < N; ++i)
        {
            masks[i] = 1 << i;
        }

        return masks;
    }

    constexpr auto BitAt = MakeBitMasks<32>();
} // namespace BitMask
