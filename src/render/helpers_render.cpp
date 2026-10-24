// render/helpers_render.cpp
#include "render/helpers_render.hpp"
#include <algorithm>
#include <string>
#include "global/global.hpp"
#include "gameLogic/keyboard_status.hpp"
#include "guide/explanations.hpp"
#include "registry/global.hpp"
#include "render/keyboard_geometry.hpp"
#include "render/shapes.hpp"
#include "render/text_layout.hpp"
#include "settings/ai.hpp"
#include "settings/settings.hpp"

namespace
{
    void DrawHintDescription(const ShortcutHint& binding, Vector2 position, Vector2 size,
                             Color color)
    {
        const auto lines = WrapLabel(HintFont(), binding.fullHint, 11, size.x - 12);
        const int max_lines = std::max(1, static_cast<int>((size.y - 40) / 13) + 1);
        for (int i = 0; i < max_lines && i < static_cast<int>(lines.size()); ++i)
        {
            const auto text = i == max_lines - 1 && i + 1 < static_cast<int>(lines.size())
                                  ? FitLabel(HintFont(), lines[i] + "..", 11, size.x - 12)
                                  : lines[i];
            DrawLabel(text, {position.x + 6, position.y + 29 + i * 13.0f}, 11, color);
        }
    }

    void DrawMnemonic(const ShortcutHint& binding, Vector2 position, float max_width)
    {
        const auto text = binding.shortHint;
        const auto index = binding.mnemonic < 128 ? text.find(static_cast<char>(binding.mnemonic))
                                                  : std::string_view::npos;
        const auto width_full = MeasureTextEx(HintFont(), std::string(text).c_str(), 12, 1).x;
        const auto font_size = std::min(12.0f, 12.0f * max_width / std::max(1.0f, width_full));
        if (index == std::string_view::npos)
        {
            DrawLabel(text, position, font_size, Default::TEXT);
            return;
        }
        const auto before = text.substr(0, index);
        const auto character = text.substr(index, 1);
        const auto width = TextAdvance(HintFont(), before, font_size);
        const auto character_width = TextAdvance(HintFont(), character, font_size, 0);
        DrawLabel(before, position, font_size, Default::TEXT);
        DrawLabel(character, {position.x + width, position.y}, font_size, Default::ACCENT);
        DrawLabel(text.substr(index + 1), {position.x + width + character_width + 1, position.y},
                  font_size, Default::TEXT);
        DrawLineV({position.x + width, position.y + 13},
                  {position.x + width + character_width, position.y + 13}, Default::ACCENT);
    }

    void DrawSpace(Rectangle rect, const Key_Display& display)
    {
        const auto* tracked = registry.getComponent<Tracked_State>(Global::trackerEID);
        if (!tracked)
        {
            return;
        }
        const auto input = tracked->displayedInput(Global::Input::status);
        const auto status =
            BuildKeyboardStatus(tracked->displayedState(), input, Global::Input::aerospaceService);
        const auto tint = [&](Color color)
        {
            return input.foreground || AeroSpaceHintsShown(input) ? color : Fade(color, 0.55f);
        };
        DrawRectangleRounded(rect, 0.16f, 6,
                             display.pressed ? Color{47, 82, 79, 255}
                                             : tint(Default::KEY_BACKGROUND));
        DrawRectangleRoundedLinesEx(rect, 0.16f, 6, display.hovered ? 1.6f : 1,
                                    tint(display.hovered ? Default::ACCENT : Default::KEY_BORDER));
        const auto color = status.kind == StatusKind::Inactive ? Default::MUTED : WHITE;
        const auto mode_width = LabelWidth(HintFont(), status.label, 14);
        const auto pending = FitLabel(HintFont(), status.pending, 11,
                                      std::max(0.0f, rect.width - 12 - mode_width - 10));
        const float pending_width = pending.empty() ? 0 : LabelWidth(HintFont(), pending, 11) + 10;
        const float x = rect.x + (rect.width - mode_width - pending_width) / 2;
        DrawLabel(status.label, {x, rect.y + 14}, 14, color);
        if (!pending.empty())
        {
            DrawLabel(pending, {x + mode_width + 10, rect.y + 16}, 11, Default::TEXT);
        }
        if (display.hint != NO_HINT)
        {
            DrawLabel(GetShortcutHint(display.hint).shortHint, {rect.x + 6, rect.y + 31}, 9,
                      Default::MUTED);
        }
        if (display.hovered && status.kind != StatusKind::Resync)
        {
            DrawLabel("Space", {rect.x + 6, rect.y + 3}, 8, Default::MUTED);
        }
    }
    // The hovered key's AI tip in its own panel, below or to the right of the key (flipped to
    // the other side near the window edge). Drawn in window points, so it is not zoomed.
    void DrawTipPanel(bool dimmed)
    {
        const auto entity = Global::Render::hoveredEID;
        const auto* display = registry.getComponent<Key_Display>(entity);
        const auto* values = registry.getComponent<Key_Values>(entity);
        if (!HoverTipsShown() || !display || !values || display->hint == NO_HINT ||
            values->key == Platform::KeyId::Space)
        {
            return;
        }
        const auto tip = Guide::Explanation(display->hint);
        const bool pending = tip.empty() && Guide::Pending(display->hint);
        if (tip.empty() && !pending)
        {
            return;
        }
        const auto pos = registry.getComponent<Entity_Position>(entity)->pos;
        const auto size = registry.getComponent<Entity_Size>(entity)->size;
        const auto top_left = GetWorldToScreen2D(pos, Global::Render::camera);
        const auto bottom_right =
            GetWorldToScreen2D({pos.x + size.x, pos.y + size.y}, Global::Render::camera);
        const Rectangle key{top_left.x, top_left.y, bottom_right.x - top_left.x,
                            bottom_right.y - top_left.y};

        constexpr float padding = 8, arrow = 7, text_size = Default::TIP_TEXT_SIZE;
        constexpr float line_height = text_size + 3, margin = Default::KEYBOARD_EDGE_MARGIN;
        const float window_width = static_cast<float>(S::screenWidth);
        const float window_height = static_cast<float>(S::screenHeight);
        const std::string_view text = pending ? "explaining..." : tip;
        const float max_width = std::min(Default::TIP_PANEL_WIDTH, window_width - 2 * margin);
        const float width =
            std::min(max_width, LabelWidth(HintFont(), text, text_size) + 2 * padding);
        const auto lines = WrapLabel(HintFont(), text, text_size, width - 2 * padding);
        const float height = 2 * padding + line_height * static_cast<float>(lines.size()) - 3;

        const float gap = Default::TIP_PANEL_GAP;
        const bool below = S::tipPlacement == Default::TipPlacement::Below;
        Rectangle panel{0, 0, width, height};
        bool flipped = false;
        if (below)
        {
            panel.x = key.x + (key.width - width) / 2;
            panel.y = key.y + key.height + gap;
            flipped = panel.y + height > window_height - margin;
            panel.y = flipped ? key.y - gap - height : panel.y;
        }
        else
        {
            panel.x = key.x + key.width + gap;
            flipped = panel.x + width > window_width - margin;
            panel.x = flipped ? key.x - gap - width : panel.x;
            panel.y = key.y + (key.height - height) / 2;
        }
        panel.x = std::max(margin, std::min(panel.x, window_width - margin - width));
        panel.y = std::max(margin, std::min(panel.y, window_height - margin - height));

        const float alpha = Global::Render::hoverAmount * (dimmed ? 0.55f : 1.0f);
        const auto background = Fade(Default::KEY_BACKGROUND, alpha);
        const auto border = Fade(Default::TIP_TEXT, 0.7f * alpha);
        Camera2D points{};
        points.zoom = Global::Render::pixelScale;
        BeginMode2D(points);
        // Arrow toward the key, along the panel edge that faces it (counter-clockwise points).
        if (below)
        {
            // Inset at most half the panel, so very short tips keep ordered clamp bounds.
            const float inset = std::min(2 * arrow, width / 2);
            const float x =
                std::clamp(key.x + key.width / 2, panel.x + inset, panel.x + width - inset);
            const float edge = flipped ? panel.y + height : panel.y;
            const float point = flipped ? edge + arrow : edge - arrow;
            if (flipped)
            {
                DrawTriangle({x - arrow, edge - 1}, {x, point}, {x + arrow, edge - 1}, border);
            }
            else
            {
                DrawTriangle({x + arrow, edge + 1}, {x, point}, {x - arrow, edge + 1}, border);
            }
        }
        else
        {
            const float inset = std::min(2 * arrow, height / 2);
            const float y =
                std::clamp(key.y + key.height / 2, panel.y + inset, panel.y + height - inset);
            const float edge = flipped ? panel.x + width : panel.x;
            const float point = flipped ? edge + arrow : edge - arrow;
            if (flipped)
            {
                DrawTriangle({edge - 1, y + arrow}, {point, y}, {edge - 1, y - arrow}, border);
            }
            else
            {
                DrawTriangle({edge + 1, y - arrow}, {point, y}, {edge + 1, y + arrow}, border);
            }
        }
        DrawRectangleRounded(panel, 0.18f, 6, background);
        DrawRectangleRoundedLinesEx(panel, 0.18f, 6, 1.2f, border);
        const auto colour = Fade(pending ? Default::MUTED : Default::TIP_TEXT, alpha);
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            DrawLabel(lines[i],
                      {panel.x + padding, panel.y + padding + line_height * static_cast<float>(i)},
                      text_size, colour);
        }
        EndMode2D();
    }
} // namespace

void DrawKeyboard()
{
    const auto modifiers = Global::Input::modifiers;
    const bool caps_lock = Global::Input::capsLock;
    const auto* tracked = registry.getComponent<Tracked_State>(Global::trackerEID);
    const auto input =
        tracked ? tracked->displayedInput(Global::Input::status) : Global::Input::status;
    const bool resync = tracked && input.foreground &&
                        TrackedConfidence(tracked->displayedState()) == Confidence::Uncertain;
    auto camera = Global::Render::camera;
    camera.offset.x *= Global::Render::pixelScale;
    camera.offset.y *= Global::Render::pixelScale;
    camera.zoom *= Global::Render::pixelScale;
    BeginMode2D(camera);
    const auto draw_key = [&](Entity entity)
    {
        const auto pos = registry.getComponent<Entity_Position>(entity)->pos;
        const auto size = registry.getComponent<Entity_Size>(entity)->size;
        const auto& display = *registry.getComponent<Key_Display>(entity);
        const auto& values = *registry.getComponent<Key_Values>(entity);
        const auto shown_values = DisplayedKeyValues(values, display.functionLayer);
        if (values.key == Platform::KeyId::Space)
        {
            DrawSpace({pos.x, pos.y, size.x, size.y}, display);
            return;
        }
        const auto tint = [&](Color color)
        {
            return input.foreground || AeroSpaceHintsShown(input) ? color : Fade(color, 0.55f);
        };
        const auto color = tint(registry.getComponent<Colour>(entity)->col);
        const Rectangle rect{pos.x, pos.y, size.x, size.y};
        DrawRectangleRounded(rect, 0.16f, 6, display.pressed ? Color{47, 82, 79, 255} : color);
        DrawRectangleRoundedLinesEx(rect, 0.16f, 6, display.hovered ? 1.6f : 1,
                                    tint(display.hovered ? Default::ACCENT : Default::KEY_BORDER));
        if (!resync && !Platform::ModifierBit(values.key) &&
            values.key != Platform::KeyId::CapsLock)
        {
            const bool function_alias = values.functionKey != Platform::KeyId::Unknown;
            const int slots = function_alias ? 5 : 4;
            const auto icon_color = [&](bool available, bool active)
            {
                return tint(available ? (active ? Default::ACCENT : WHITE)
                                      : Color{57, 66, 81, 255});
            };
            for (int i = 0; i < 4; ++i)
            {
                const auto bit = static_cast<std::uint8_t>(1u << i);
                const auto box = ModifierIconBox(rect, i, slots);
                DrawModifierIcon(bit, {box.x, box.y},
                                 icon_color(display.availableModifiers & bit, modifiers & bit),
                                 box.width);
            }
            if (function_alias)
            {
                const auto box = ModifierIconBox(rect, 4, slots);
                DrawLabel("f", {box.x, box.y - 1}, 9,
                          icon_color(display.functionAvailable, display.functionLayer));
            }
        }
        if (values.key == Platform::KeyId::CapsLock)
        {
            const bool escape_alias = values.values[0].special == Platform::SpecialKey::Escape;
            if (!escape_alias && display.hovered && !resync)
            {
                DrawLabel("Caps", {pos.x + 6, pos.y + 25}, 12, Default::MUTED);
                return;
            }
            const auto icon = escape_alias ? Vector2{pos.x + 6, pos.y + 6}
                                           : Vector2{pos.x + size.x / 2 - 4, pos.y + 23};
            const auto color = caps_lock ? Default::ACCENT : tint(Default::MUTED);
            DrawModifierIcon(Platform::SHIFT, icon, color);
            DrawLineV({icon.x, icon.y + 10}, {icon.x + 8, icon.y + 10}, color);
            if (!escape_alias)
            {
                return;
            }
        }
        if (const auto bit = Platform::ModifierBit(values.key))
        {
            const bool details = display.hovered && display.hint != NO_HINT;
            const auto modifier_color = (modifiers & bit)         ? Default::ACCENT
                                        : display.hint != NO_HINT ? tint(WHITE)
                                                                  : tint(Default::MUTED);
            if (bit == Platform::SHIFT)
            {
                const auto width = LabelWidth(HintFont(), "shift", 12);
                DrawLabel("shift", {pos.x + (size.x - width) / 2, pos.y + (details ? 12 : 22)}, 12,
                          modifier_color);
            }
            else
            {
                DrawModifierIcon(bit, {pos.x + size.x / 2 - 4, pos.y + (details ? 14 : 24)},
                                 modifier_color);
            }
            if (display.hint != NO_HINT)
            {
                const auto binding = GetShortcutHint(display.hint);
                if (details)
                {
                    DrawHintDescription(binding, pos, size, tint(Default::TEXT));
                }
                else
                {
                    DrawLabel(FitLabel(HintFont(), binding.shortHint, 8, size.x - 8),
                              {pos.x + 4, pos.y + size.y - 10}, 8, tint(Default::TEXT));
                }
            }
        }
        else if (display.hint != NO_HINT)
        {
            const auto binding = GetShortcutHint(display.hint);
            if (display.hovered)
            {
                DrawLabel(FitLabel(HintFont(), HintChord(values, binding, display.functionLayer),
                                   10, size.x - 12),
                          {pos.x + 6, pos.y + 17}, 10, tint(Default::ACCENT));
                DrawHintDescription(binding, pos, size, tint(Default::TEXT));
            }
            else
            {
                DrawMnemonic(binding, {pos.x + 6, pos.y + 25}, size.x - 12);
            }
        }
        else if (display.hovered && (!resync || display.pressed))
        {
            const auto caps_shift =
                tracked && tracked->displayedState().appProfile != Platform::AppProfile::Blender &&
                        caps_lock && values.key >= Platform::KeyId::A &&
                        values.key <= Platform::KeyId::Z
                    ? Platform::SHIFT
                    : 0;
            DrawLabel(KeyName(shown_values, modifiers ^ caps_shift), {pos.x + 6, pos.y + 25}, 12,
                      Default::MUTED);
        }
    };
    for (auto entity : registry.getTag(TagName::Key))
    {
        if (entity != Global::Render::hoveredEID)
        {
            draw_key(entity);
        }
    }
    if (registry.getComponent<Key_Display>(Global::Render::hoveredEID))
    {
        draw_key(Global::Render::hoveredEID);
    }
    EndMode2D();
    DrawTipPanel(!input.foreground && !AeroSpaceHintsShown(input));
}
