// platform/mac/mac_bridge.hpp
#pragma once
#include "platform/platform.hpp"

namespace Platform
{
    void InitialiseMenu();
    void ShutdownMenu();
    void ShutdownOverlayWindow();
    void UpdatePointerRegion(float left, float top, float width, float height);
} // namespace Platform
