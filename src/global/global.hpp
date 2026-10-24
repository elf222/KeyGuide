// global/global.hpp
#pragma once
#include <array>
#include "raylib.h"
#include "registry/ecs_components.hpp"

namespace Global
{
    inline float dt = 0, lostTimeTotal = 0;
    inline long long frameCounter = 0;
    inline Entity trackerEID = -1;
    inline bool quit = false;

    namespace Render
    {
        inline bool frameDrawn = false, animating = false, continuousRendering = false;
        inline RenderTexture2D texture_world{};
        inline Font font{};
        inline float pixelScale = 1;
        inline Camera2D camera{{0, 0}, {0, 0}, 0, 1};
        inline Entity hoveredEID = -1;
        inline float hoverAmount = 0, expandedWidth = 0, expandedHeight = 0;
        inline Vector2 hoverScreenPosition{}, hoverWorldPosition{};
    } // namespace Render

    namespace Input
    {
        inline Platform::InputBatchInfo status{};
        inline Platform::PointerPosition pointer{};
        inline std::array<bool, Platform::KEY_COUNT> pressed{};
        inline std::uint8_t modifiers = 0;
        inline bool capsLock = false;
        inline bool functionHeld = false;
        // Fn typed a number-row key, so this keyboard's Fn does not produce F1-F12.
        inline bool functionTypesNumbers = false;
        // Latest F1-F12 press until its release or the next key press, so lost releases recover.
        inline Platform::KeyId functionKey = Platform::KeyId::Unknown;
        // AeroSpace's service binding mode, entered with Option-Shift-;.
        inline bool aerospaceService = false;
    } // namespace Input
} // namespace Global
