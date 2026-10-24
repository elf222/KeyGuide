// settings/settings.hpp
#pragma once
#include <algorithm>
#include "elfTypesRL/IntVector2.hpp"
#include "settings/defaults.hpp"
#include "settings/blender.hpp"

namespace S
{
    inline bool showFunctionRow = Default::SHOW_FUNCTION_ROW, showNumpad = Default::SHOW_NUMPAD;
    inline bool keyboardLayoutChanged = false;
    inline int screenWidth = Default::SCREEN_WIDTH, screenHeight = Default::SCREEN_HEIGHT;
    inline IntVector2 screenSize{screenWidth, screenHeight};
    inline bool reduceMotion = false;
    inline bool windowVisible = Default::WINDOW_VISIBLE;
    inline Default::WindowPosition windowPosition = Default::WINDOW_POSITION;
    inline Default::TransparencyMode transparencyMode = Default::TRANSPARENCY_MODE;
    inline IntVector2 ScreenHalf = screenSize / 2;
} // namespace S

struct WindowOpacity
{
    float keys = 1, background = 1;
};

inline WindowOpacity GetWindowOpacity(bool hovered)
{
    using enum Default::TransparencyMode;
    const bool semitransparent = S::transparencyMode == Always ||
                                 (S::transparencyMode == OnHover && hovered) ||
                                 (S::transparencyMode == WhenNotHovered && !hovered);
    return semitransparent
               ? WindowOpacity{std::clamp(Default::SEMITRANSPARENT_KEY_OPACITY, 0.0f, 1.0f),
                               std::clamp(Default::SEMITRANSPARENT_BACKGROUND_OPACITY, 0.0f, 1.0f)}
               : WindowOpacity{};
}

inline void UpdateSettingsToNewWindowSize()
{
    S::screenWidth = GetScreenWidth();
    S::screenHeight = GetScreenHeight();
    S::screenSize = {S::screenWidth, S::screenHeight};
    S::ScreenHalf = S::screenSize / 2;
}
