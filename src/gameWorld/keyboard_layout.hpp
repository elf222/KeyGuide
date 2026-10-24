// gameWorld/keyboard_layout.hpp
#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <string_view>
#include <vector>
#include "platform/input_event.hpp"
#include "settings/defaults.hpp"
#include "settings/settings.hpp"

struct KeyboardKeyLayout
{
    Platform::KeyId key{};
    std::array<Platform::KeyValue, 2> values{};
    Vector2 pos{}, size{};
    int row = 0;
};

inline const std::vector<KeyboardKeyLayout>& KeyboardLayout()
{
    static bool old_function_row = !S::showFunctionRow, old_numpad = !S::showNumpad;
    static std::vector<KeyboardKeyLayout> keys;
    if (old_function_row == S::showFunctionRow && old_numpad == S::showNumpad)
    {
        return keys;
    }
    old_function_row = S::showFunctionRow;
    old_numpad = S::showNumpad;
    keys = []
    {
        std::vector<KeyboardKeyLayout> result;
        using K = Platform::KeyId;
        using Special = Platform::SpecialKey;
        float x = Default::KEYBOARD_EDGE_MARGIN;
        int row = 0;
        const auto add =
            [&](K id, Platform::KeyValue normal, Platform::KeyValue shifted, float units = 1)
        {
            const float width = Default::KEY_UNIT * units - Default::KEY_GAP;
            result.push_back({id,
                              {normal, shifted},
                              {x, Default::KEYBOARD_EDGE_MARGIN +
                                      row * (Default::KEY_HEIGHT + Default::KEY_GAP)},
                              {width, Default::KEY_HEIGHT},
                              row});
            x += Default::KEY_UNIT * units;
        };
        const auto special = [&](K id, Special value, float units)
        {
            add(id, {0, value}, {0, value}, units);
        };
        if (S::showFunctionRow)
        {
            for (int i = 0; i < 12; ++i)
            {
                add(static_cast<K>(static_cast<int>(K::F1) + i), {}, {}, 1.25f);
            }
            ++row;
            x = Default::KEYBOARD_EDGE_MARGIN;
        }
        if (!Default::ESCAPE_ON_CAPS_LOCK)
        {
            special(K::Escape, Special::Escape, 0.75f);
        }
        add(K::Grave, {U'`'}, {U'~'});
        const std::string_view numbers = "1234567890-=", shifted = "!@#$%^&*()_+";
        for (std::size_t i = 0; i < numbers.size(); ++i)
        {
            add(static_cast<K>(numbers[i]), {static_cast<char32_t>(numbers[i])},
                {static_cast<char32_t>(shifted[i])});
        }
        special(K::Backspace, Special::Backspace, Default::ESCAPE_ON_CAPS_LOCK ? 2 : 1.25f);
        ++row;
        x = Default::KEYBOARD_EDGE_MARGIN;
        special(K::Tab, Special::Tab, 1.5f);
        for (char c : std::string_view("qwertyuiop"))
        {
            add(static_cast<K>(c - 32), {static_cast<char32_t>(c)},
                {static_cast<char32_t>(c - 32)});
        }
        add(K::LeftBracket, {U'['}, {U'{'});
        add(K::RightBracket, {U']'}, {U'}'});
        add(K::Backslash, {U'\\'}, {U'|'}, 1.5f);
        ++row;
        x = Default::KEYBOARD_EDGE_MARGIN;
        const Platform::KeyValue caps_value{0, Default::ESCAPE_ON_CAPS_LOCK ? Special::Escape
                                                                            : Special::None};
        add(K::CapsLock, caps_value, caps_value, 1.75f);
        for (char c : std::string_view("asdfghjkl"))
        {
            add(static_cast<K>(c - 32), {static_cast<char32_t>(c)},
                {static_cast<char32_t>(c - 32)});
        }
        add(K::Semicolon, {U';'}, {U':'});
        add(K::Apostrophe, {U'\''}, {U'"'});
        special(K::Enter, Special::Enter, 2.25f);
        ++row;
        x = Default::KEYBOARD_EDGE_MARGIN;
        add(K::Shift, {}, {}, 2.25f);
        for (char c : std::string_view("zxcvbnm"))
        {
            add(static_cast<K>(c - 32), {static_cast<char32_t>(c)},
                {static_cast<char32_t>(c - 32)});
        }
        add(K::Comma, {U','}, {U'<'});
        add(K::Period, {U'.'}, {U'>'});
        add(K::Slash, {U'/'}, {U'?'});
        add(K::RightShift, {}, {}, 1.75f);
        special(K::Up, Special::Up, 1);
        ++row;
        x = Default::KEYBOARD_EDGE_MARGIN;
        add(K::Control, {}, {}, 1);
        add(K::Option, {}, {}, 1);
        add(K::Command, {}, {}, 1.5f);
        add(K::Space, {U' '}, {U' '}, 6);
        add(K::RightCommand, {}, {}, 1.5f);
        add(K::RightOption, {}, {}, 1);
        special(K::Left, Special::Left, 1);
        special(K::Down, Special::Down, 1);
        special(K::Right, Special::Right, 1);
        if (S::showNumpad)
        {
            const float base_x =
                Default::KEYBOARD_EDGE_MARGIN + 15 * Default::KEY_UNIT + Default::KEY_GAP * 2;
            const int base_row = S::showFunctionRow ? 1 : 0;
            const auto pad = [&](K key, int column, int pad_row, float units = 1, int rows = 1)
            {
                result.push_back(
                    {key,
                     {},
                     {base_x + column * Default::KEY_UNIT,
                      Default::KEYBOARD_EDGE_MARGIN +
                          (base_row + pad_row) * (Default::KEY_HEIGHT + Default::KEY_GAP)},
                     {Default::KEY_UNIT * units - Default::KEY_GAP,
                      rows * (Default::KEY_HEIGHT + Default::KEY_GAP) - Default::KEY_GAP},
                     base_row + pad_row});
            };
            pad(K::NumpadEqual, 0, 0);
            pad(K::NumpadDivide, 1, 0);
            pad(K::NumpadMultiply, 2, 0);
            pad(K::NumpadMinus, 3, 0);
            for (int r = 0; r < 3; ++r)
            {
                for (int c = 0; c < 3; ++c)
                {
                    pad(static_cast<K>(static_cast<int>(K::NumpadSeven) - r * 3 + c), c, r + 1);
                }
            }
            pad(K::NumpadPlus, 3, 1, 1, 2);
            pad(K::NumpadEnter, 3, 3, 1, 2);
            pad(K::NumpadZero, 0, 4, 2);
            pad(K::NumpadPeriod, 2, 4);
        }
        return result;
    }();
    return keys;
}

inline Vector2 KeyboardCanvasSize()
{
    Vector2 extent{};
    for (const auto& key : KeyboardLayout())
    {
        extent.x = std::max(extent.x, key.pos.x + key.size.x);
        extent.y = std::max(extent.y, key.pos.y + key.size.y);
    }
    return {std::ceil(extent.x + Default::KEYBOARD_EDGE_MARGIN),
            std::ceil(extent.y + Default::KEYBOARD_EDGE_MARGIN)};
}
