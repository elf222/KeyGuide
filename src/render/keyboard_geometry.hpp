// render/keyboard_geometry.hpp
#pragma once
#include <algorithm>
#include "raylib.h"
#include "gameWorld/keyboard_layout.hpp"

inline Camera2D KeyboardBaseCamera(Vector2 window_size)
{
    const auto canvas = KeyboardCanvasSize();
    const float zoom = std::min(window_size.x / canvas.x, window_size.y / canvas.y);
    return {{(window_size.x - canvas.x * zoom) / 2, (window_size.y - canvas.y * zoom) / 2},
            {0, 0},
            0,
            zoom};
}

inline Rectangle KeyboardViewport(Vector2 window_size)
{
    const float margin = Default::KEYBOARD_EDGE_MARGIN;
    return {margin, margin, window_size.x - 2 * margin, window_size.y - 2 * margin};
}

inline Rectangle KeyScreenRectangle(Camera2D camera, Rectangle key)
{
    const auto point = GetWorldToScreen2D({key.x, key.y}, camera);
    return {point.x, point.y, key.width * camera.zoom, key.height * camera.zoom};
}

inline Rectangle ModifierIconBox(Rectangle key, int index, int slots = 4)
{
    const float slot = std::min(12.0f, std::max(0.0f, key.width - 8) / slots);
    const float size = std::min(8.0f, std::max(0.0f, slot - 2));
    return {key.x + 4 + index * slot + (slot - size) / 2, key.y + 6, size, size};
}

// Overlay cameras have no rotation. Fit the expanded key before clamping its projected bounds.
inline Camera2D KeepKeyInViewport(Camera2D camera, Rectangle key, Rectangle viewport)
{
    if (key.width <= 0 || key.height <= 0 || viewport.width <= 0 || viewport.height <= 0)
    {
        return camera;
    }
    camera.zoom = std::min({camera.zoom, viewport.width / key.width, viewport.height / key.height});
    camera.offset.x = std::clamp(
        camera.offset.x, viewport.x - (key.x - camera.target.x) * camera.zoom,
        viewport.x + viewport.width - (key.x + key.width - camera.target.x) * camera.zoom);
    camera.offset.y = std::clamp(
        camera.offset.y, viewport.y - (key.y - camera.target.y) * camera.zoom,
        viewport.y + viewport.height - (key.y + key.height - camera.target.y) * camera.zoom);
    return camera;
}
