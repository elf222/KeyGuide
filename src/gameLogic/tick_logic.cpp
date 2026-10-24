// gameLogic/tick_logic.cpp
#include "gameLogic/tick_logic.hpp"
#include <algorithm>
#include <string>
#include "global/global.hpp"
#include "guide/explanations.hpp"
#include "registry/global.hpp"
#include "render/animations.hpp"
#include "render/keyboard_geometry.hpp"
#include "render/shapes.hpp"
#include "render/text_layout.hpp"
#include "settings/settings.hpp"

namespace
{
    Camera2D BaseCamera()
    {
        return KeyboardBaseCamera(
            {static_cast<float>(S::screenWidth), static_cast<float>(S::screenHeight)});
    }

    Rectangle KeyboardViewport()
    {
        return ::KeyboardViewport(
            {static_cast<float>(S::screenWidth), static_cast<float>(S::screenHeight)});
    }
} // namespace

void CameraLogic()
{
    auto camera = BaseCamera();
    if (Global::Render::hoverAmount > 0.001f)
    {
        camera.target = Global::Render::hoverWorldPosition;
        camera.offset = Global::Render::hoverScreenPosition;
        camera.zoom *= 1 + (Default::HOVER_ZOOM - 1) * Global::Render::hoverAmount;
        const auto* position = registry.getComponent<Entity_Position>(Global::Render::hoveredEID);
        const auto* size = registry.getComponent<Entity_Size>(Global::Render::hoveredEID);
        if (position && size)
        {
            const Rectangle key{position->pos.x, position->pos.y, size->size.x, size->size.y};
            camera = KeepKeyInViewport(camera, key, KeyboardViewport());
        }
    }
    Global::Render::camera = camera;
}

void TickOnce()
{
    const auto pointer = Global::Input::pointer;
    Entity hovered = -1;
    if (pointer.inside && CheckCollisionPointRec({pointer.x, pointer.y}, KeyboardViewport()))
    {
        // Hover always uses the unzoomed layout, independent of animated drawing bounds.
        const auto point = GetScreenToWorld2D({pointer.x, pointer.y}, BaseCamera());
        for (auto entity : registry.getTag(TagName::Key))
        {
            const auto* layout = registry.getComponent<Key_Layout>(entity);
            if (CheckCollisionPointRec(
                    point, {layout->pos.x, layout->pos.y, layout->size.x, layout->size.y}))
            {
                hovered = entity;
                break;
            }
        }
    }
    if (Global::Render::hoveredEID != hovered)
    {
        Global::Render::expandedWidth = 0;
        Global::Render::expandedHeight = 0;
    }
    Global::Render::hoveredEID = hovered;
    const bool reduce_motion = S::reduceMotion || Global::Input::status.reduceMotion;
    const auto* values = registry.getComponent<Key_Values>(hovered);
    const bool status_key = values && values->key == Platform::KeyId::Space;
    const float target = hovered >= 0 ? 1.0f : 0.0f;
    Global::Render::hoverAmount =
        reduce_motion ? target
                      : ApproachValue(Global::Render::hoverAmount, target, Global::dt, 14, 0.001f);
    if (hovered >= 0)
    {
        Global::Render::hoverScreenPosition = {pointer.x, pointer.y};
        Global::Render::hoverWorldPosition =
            GetScreenToWorld2D({pointer.x, pointer.y}, BaseCamera());
    }
    float width = 0, height = 0;
    if (hovered >= 0 && !status_key)
    {
        const auto* display = registry.getComponent<Key_Display>(hovered);
        const auto* layout = registry.getComponent<Key_Layout>(hovered);
        if (display && display->hint != NO_HINT)
        {
            const auto binding = GetShortcutHint(display->hint);
            // Cheap when the tip is cached, pending or backing off after a failure, so
            // asking on every hovered tick retries once settings or the provider recover.
            if (const auto* tracked = registry.getComponent<Tracked_State>(Global::trackerEID))
            {
                Guide::RequestHover(display->hint, tracked->displayedState(),
                                    Global::Input::aerospaceService);
            }
            width = std::min(
                Default::MAX_KEY_WIDTH,
                std::max(LabelWidth(HintFont(), binding.fullHint, 11),
                         LabelWidth(HintFont(), HintChord(*values, binding, display->functionLayer),
                                    10)) +
                    14);
            const auto lines =
                WrapLabel(HintFont(), binding.fullHint, 11, std::max(layout->size.x, width) - 12);
            height = std::min(Default::MAX_KEY_HEIGHT,
                              layout->size.y +
                                  13 * static_cast<float>(lines.empty() ? 0 : lines.size() - 1));
        }
    }
    Global::Render::expandedWidth =
        reduce_motion ? width : ApproachValue(Global::Render::expandedWidth, width, Global::dt);
    Global::Render::expandedHeight =
        reduce_motion ? height : ApproachValue(Global::Render::expandedHeight, height, Global::dt);
    Global::Render::animating = Global::Render::hoverAmount != target ||
                                Global::Render::expandedWidth != width ||
                                Global::Render::expandedHeight != height;
    cmd_registry.add<CommandProjectKeyboard>(hovered, Global::Render::expandedWidth,
                                             Global::Render::expandedHeight);
}
