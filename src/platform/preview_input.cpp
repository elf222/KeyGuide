// platform/preview_input.cpp
#include "platform/preview_input.hpp"
#include <array>
#include "raylib.h"
#include "gameWorld/keyboard_layout.hpp"

namespace
{
    Platform::AppProfile preview_profile = Platform::AppProfile::Zed;
    // Authored screenshots pin the pointer and count as focused, since a window launched from a
    // terminal may never receive focus.
    bool pointer_overridden = false;
    Vector2 pointer_override{};

    int RaylibKey(Platform::KeyId key)
    {
        using K = Platform::KeyId;
        if (key >= K::F1 && key <= K::F12)
        {
            return KEY_F1 + static_cast<int>(key) - static_cast<int>(K::F1);
        }
        if (key >= K::NumpadZero && key <= K::NumpadNine)
        {
            return KEY_KP_0 + static_cast<int>(key) - static_cast<int>(K::NumpadZero);
        }
        switch (key)
        {
            case K::Escape:
                return KEY_ESCAPE;
            case K::Enter:
                return KEY_ENTER;
            case K::Tab:
                return KEY_TAB;
            case K::Backspace:
                return KEY_BACKSPACE;
            case K::Delete:
                return KEY_DELETE;
            case K::Right:
                return KEY_RIGHT;
            case K::Left:
                return KEY_LEFT;
            case K::Up:
                return KEY_UP;
            case K::Down:
                return KEY_DOWN;
            case K::PageUp:
                return KEY_PAGE_UP;
            case K::PageDown:
                return KEY_PAGE_DOWN;
            case K::Home:
                return KEY_HOME;
            case K::End:
                return KEY_END;
            case K::NumpadPeriod:
                return KEY_KP_DECIMAL;
            case K::NumpadDivide:
                return KEY_KP_DIVIDE;
            case K::NumpadMultiply:
                return KEY_KP_MULTIPLY;
            case K::NumpadMinus:
                return KEY_KP_SUBTRACT;
            case K::NumpadPlus:
                return KEY_KP_ADD;
            case K::NumpadEnter:
                return KEY_KP_ENTER;
            case K::NumpadEqual:
                return KEY_KP_EQUAL;
            default:
                return static_cast<int>(key);
        }
    }
} // namespace

namespace Platform
{
    void SetPreviewProfile(AppProfile profile)
    {
        preview_profile = profile;
    }

    InputBatchInfo ReadPreviewEvents(std::span<InputEvent> destination, bool function_held)
    {
        static bool previous_focus = false;
        static std::uint64_t generation = 0, sequence = 0, losses = 0;
        static AppProfile old_profile = preview_profile;
        const auto old_generation = generation;
        const bool focused = IsWindowFocused() || pointer_overridden;
        if (old_profile != preview_profile)
        {
            ++generation;
            old_profile = preview_profile;
        }
        if (focused != previous_focus)
        {
            ++generation;
            previous_focus = focused;
        }
        InputBatchInfo status{.foregroundGeneration = generation,
                              .lossGeneration = losses,
                              .foreground = focused,
                              .available = false,
                              .preview = true,
                              .description = "LOCAL PREVIEW · input belongs to this window",
                              .appProfile = preview_profile};
        static bool caps_lock = false;
        std::uint8_t modifiers = 0;
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
        {
            modifiers |= SHIFT;
        }
        if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))
        {
            modifiers |= CONTROL;
        }
        if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT))
        {
            modifiers |= OPTION;
        }
        if (IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER))
        {
            modifiers |= COMMAND;
        }
        const auto emit = [&](InputEvent event)
        {
            event.capsLock = caps_lock;
            event.functionHeld = function_held;
            event.modifiers = modifiers;
            event.timestamp = GetTime();
            event.sequence = ++sequence;
            event.foregroundGeneration = generation;
            event.lossGeneration = losses;
            if (status.count < destination.size())
            {
                destination[status.count++] = event;
            }
            else
            {
                ++losses;
            }
        };
        static std::uint8_t old_modifiers = 0;
        static bool old_function_held = false;
        // Portable input clears held state for each new generation; resend the current snapshot.
        if (generation != old_generation || modifiers != old_modifiers ||
            function_held != old_function_held)
        {
            emit({.type = EventType::Modifiers});
            old_modifiers = modifiers;
            old_function_held = function_held;
        }
        const auto normalize = [&](int ray_key, EventType type, bool repeated = false)
        {
            if (ray_key == KEY_CAPS_LOCK && type == EventType::KeyDown)
            {
                caps_lock = !caps_lock;
                emit({.type = EventType::Modifiers});
                return;
            }
            // Escape input stays physical even when its displayed hint lives on Caps Lock.
            if (ray_key == KEY_ESCAPE)
            {
                emit({.type = type,
                      .key = KeyId::Escape,
                      .value = {0, SpecialKey::Escape},
                      .repeat = repeated});
                return;
            }
            for (int id = static_cast<int>(KeyId::F1); id < static_cast<int>(KeyId::Count); ++id)
            {
                const auto key = static_cast<KeyId>(id);
                if (RaylibKey(key) == ray_key)
                {
                    emit({.type = type,
                          .key = key,
                          .value = {0, key == KeyId::NumpadEnter ? SpecialKey::Enter
                                                                 : SpecialKey::None},
                          .repeat = repeated});
                    return;
                }
            }
            for (const auto& key : KeyboardLayout())
            {
                if (key.key >= KeyId::Shift || RaylibKey(key.key) != ray_key)
                {
                    continue;
                }
                emit({.type = type,
                      .key = key.key,
                      .value =
                          key.values[((modifiers & SHIFT) !=
                                      0) ^ (caps_lock && key.key >= KeyId::A && key.key <= KeyId::Z)
                                         ? 1
                                         : 0],
                      .repeat = repeated});
                return;
            }
            // Modifier presses are already represented by the snapshot.
            if (ray_key < KEY_LEFT_SHIFT && type == EventType::KeyDown)
            {
                emit({.type = EventType::Unsupported});
            }
        };
        // raylib's press FIFO preserves command order when several keys arrive in one frame.
        for (int ray_key = GetKeyPressed(); ray_key; ray_key = GetKeyPressed())
        {
            normalize(ray_key, EventType::KeyDown);
        }
        if (IsKeyReleased(KEY_ESCAPE))
        {
            normalize(KEY_ESCAPE, EventType::KeyUp);
        }
        else if (IsKeyPressedRepeat(KEY_ESCAPE) && !IsKeyPressed(KEY_ESCAPE))
        {
            normalize(KEY_ESCAPE, EventType::KeyDown, true);
        }
        for (const auto& key : KeyboardLayout())
        {
            if (key.key >= KeyId::Shift || key.key == KeyId::Escape)
            {
                continue;
            }
            const int ray_key = RaylibKey(key.key);
            if (IsKeyReleased(ray_key))
            {
                normalize(ray_key, EventType::KeyUp);
            }
            else if (IsKeyPressedRepeat(ray_key) && !IsKeyPressed(ray_key))
            {
                normalize(ray_key, EventType::KeyDown, true);
            }
        }
        // Hidden function/numpad keys still reach the tracker and release their physical feedback.
        for (int id = static_cast<int>(KeyId::F1); id < static_cast<int>(KeyId::Count); ++id)
        {
            const int ray_key = RaylibKey(static_cast<KeyId>(id));
            if (IsKeyReleased(ray_key))
            {
                normalize(ray_key, EventType::KeyUp);
            }
            else if (IsKeyPressedRepeat(ray_key) && !IsKeyPressed(ray_key))
            {
                normalize(ray_key, EventType::KeyDown, true);
            }
        }
        for (auto [ray_button, button] : {std::pair{MOUSE_BUTTON_LEFT, MouseButton::Left},
                                          {MOUSE_BUTTON_RIGHT, MouseButton::Right},
                                          {MOUSE_BUTTON_MIDDLE, MouseButton::Middle}})
        {
            if (IsMouseButtonPressed(ray_button))
            {
                emit({.type = EventType::MouseAction, .mouseButton = button});
            }
            if (IsMouseButtonReleased(ray_button))
            {
                emit(
                    {.type = EventType::MouseAction, .mouseButton = button, .mousePressed = false});
            }
        }
        while (GetCharPressed())
        {
        }
        status.lossGeneration = losses;
        return status;
    }

    void SetPreviewPointerOverride(float x, float y)
    {
        pointer_overridden = true;
        pointer_override = {x, y};
    }

    PointerPosition ReadPreviewPointer()
    {
        const auto position = pointer_overridden ? pointer_override : GetMousePosition();
        return {position.x, position.y,
                position.x >= 0 && position.y >= 0 && position.x < GetScreenWidth() &&
                    position.y < GetScreenHeight()};
    }
} // namespace Platform
