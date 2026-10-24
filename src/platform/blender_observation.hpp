// platform/blender_observation.hpp
#pragma once
#include <array>
#include <cstdint>

namespace Platform
{
    struct BlenderObservation
    {
        bool connected = false;
        bool current = false;
        bool modalAvailable = false;
        bool interfaceLocked = false;
        std::uint8_t selectionMask = 0;
        std::uint64_t inputSequence = 0;
        std::array<char, 32> mode{}, editor{};
        std::array<char, 64> operation{};

        bool operator==(const BlenderObservation&) const = default;
    };
} // namespace Platform
