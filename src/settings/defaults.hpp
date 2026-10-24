// settings/defaults.hpp
#pragma once
#include "raylib.h"

namespace Default
{
    enum class WindowPosition
    {
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight,
        Center,
        TopCenter,
        BottomCenter
    };
    inline constexpr WindowPosition WINDOW_POSITION = WindowPosition::TopRight;
    inline constexpr Vector2 WINDOW_MARGIN{14, 66};
    inline constexpr float WINDOW_MOVE_STEP = 20, WINDOW_MOVE_LARGE_STEP = 100;
    inline constexpr int SCREEN_WIDTH = 748, SCREEN_HEIGHT = 243, TARGET_FPS = 60;
    inline constexpr unsigned int DEBUG_RANDOMISATION_SEED = 42u;
    inline constexpr float KEY_UNIT = 49, KEY_HEIGHT = 43, KEY_GAP = 3;
    inline constexpr bool ESCAPE_ON_CAPS_LOCK = true; // Display profile; does not remap input.
    inline constexpr bool SHOW_FUNCTION_ROW = false, SHOW_NUMPAD = false;
    enum class TransparencyMode
    {
        Off,
        Always,
        OnHover,
        WhenNotHovered
    };
    inline constexpr bool WINDOW_VISIBLE = true;
    inline constexpr TransparencyMode TRANSPARENCY_MODE = TransparencyMode::Off;
    inline constexpr float SEMITRANSPARENT_KEY_OPACITY = 0.75f,
                           SEMITRANSPARENT_BACKGROUND_OPACITY = 0.20f;

    inline constexpr float HOVER_ZOOM = 1.48f, MAX_KEY_WIDTH = 210, MAX_KEY_HEIGHT = 96;
    // AI tip panel beside the hovered key, in window points (not zoomed with the key).
    inline constexpr float TIP_PANEL_WIDTH = 250, TIP_PANEL_GAP = 8, TIP_TEXT_SIZE = 13;
    inline constexpr float KEYBOARD_EDGE_MARGIN = 8;
    inline constexpr Color BACKGROUND{19, 22, 29, 255};
    inline constexpr Color KEY_BACKGROUND{30, 35, 45, 255};
    inline constexpr Color KEY_BORDER{53, 61, 74, 255};
    inline constexpr Color TEXT{220, 225, 232, 255};
    inline constexpr Color MUTED{105, 117, 134, 255};
    inline constexpr Color ACCENT{120, 210, 190, 255};
    inline constexpr Color WARNING{231, 171, 99, 255};
    inline constexpr Color TIP_TEXT{160, 196, 255, 255};
} // namespace Default
