// render/text_layout.hpp
#pragma once
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include "raylib.h"
#include "registry/ecs_components.hpp"

inline std::string KeyName(const Key_Values& values, std::uint8_t modifiers)
{
    using K = Platform::KeyId;
    if (values.key >= K::F1 && values.key <= K::F12)
    {
        return "F" + std::to_string(static_cast<int>(values.key) - static_cast<int>(K::F1) + 1);
    }
    if (values.key >= K::NumpadZero && values.key <= K::NumpadNine)
    {
        return "Num " +
               std::to_string(static_cast<int>(values.key) - static_cast<int>(K::NumpadZero));
    }
    switch (values.key)
    {
        case K::Shift:
        case K::RightShift:
            return "Shift";
        case K::Control:
        case K::RightControl:
            return "Control";
        case K::Option:
        case K::RightOption:
            return "Option";
        case K::Command:
        case K::RightCommand:
            return "Command";
        case K::NumpadPeriod:
            return "Num .";
        case K::NumpadDivide:
            return "Num /";
        case K::NumpadMultiply:
            return "Num *";
        case K::NumpadMinus:
            return "Num -";
        case K::NumpadPlus:
            return "Num +";
        case K::NumpadEnter:
            return "Num Enter";
        case K::NumpadEqual:
            return "Num =";
        default:
            break;
    }
    const auto value = values.values[(modifiers & Platform::SHIFT) ? 1 : 0];
    if (value.character == U' ')
    {
        return "Space";
    }
    if (value.character && value.character < 128)
    {
        return std::string(1, static_cast<char>(value.character));
    }
    switch (value.special)
    {
        case Platform::SpecialKey::Escape:
            return "Esc";
        case Platform::SpecialKey::Enter:
            return "Enter";
        case Platform::SpecialKey::Tab:
            return "Tab";
        case Platform::SpecialKey::Backspace:
            return "Backspace";
        case Platform::SpecialKey::Left:
            return "Left";
        case Platform::SpecialKey::Right:
            return "Right";
        case Platform::SpecialKey::Up:
            return "Up";
        case Platform::SpecialKey::Down:
            return "Down";
        default:
            return "";
    }
}

inline std::string KeyChord(const Key_Values& values, std::uint8_t modifiers)
{
    std::string chord;
    if (modifiers & Platform::CONTROL)
    {
        chord += "Ctrl-";
    }
    if (modifiers & Platform::OPTION)
    {
        chord += "Opt-";
    }
    if (modifiers & Platform::COMMAND)
    {
        chord += "Cmd-";
    }
    return chord + KeyName(values, modifiers);
}

inline std::string PhysicalChord(const Key_Values& values, std::uint8_t modifiers)
{
    return std::string(modifiers & Platform::SHIFT ? "Shift-" : "") +
           KeyChord(values, modifiers & ~Platform::SHIFT);
}

inline Key_Values DisplayedKeyValues(const Key_Values& values, bool function_layer)
{
    return function_layer ? Key_Values{values.functionKey, {}} : values;
}

inline std::string HintChord(const Key_Values& values, const ShortcutHint& binding,
                             bool function_layer = false)
{
    const auto shown_values = DisplayedKeyValues(values, function_layer);
    auto chord = binding.physical ? PhysicalChord(shown_values, binding.modifiers)
                                  : KeyChord(shown_values, binding.modifiers);
    if (function_layer)
    {
        chord = "Fn-" + PhysicalChord(values, binding.modifiers) + " / " + chord;
    }
    return binding.exContinuation ? chord + " -> " + std::string(binding.chord) : chord;
}

inline float LabelWidth(Font font, std::string_view text, float size)
{
    const auto owned = std::string(text);
    return MeasureTextEx(font, owned.c_str(), size, 1).x;
}

inline std::vector<std::string> WrapLabel(Font font, std::string_view text, float size,
                                          float max_width)
{
    std::vector<std::string> lines;
    std::istringstream paragraphs{std::string(text)};
    std::string paragraph;
    while (std::getline(paragraphs, paragraph))
    {
        std::istringstream words(paragraph);
        std::string word, line;
        while (words >> word)
        {
            const auto joined = line.empty() ? word : line + " " + word;
            if (LabelWidth(font, joined, size) <= max_width)
            {
                line = joined;
                continue;
            }
            if (!line.empty())
            {
                lines.push_back(line);
                line.clear();
            }
            for (std::size_t i = 0; i < word.size();)
            {
                int bytes = 0;
                GetCodepointNext(word.c_str() + i, &bytes);
                const auto character = word.substr(i, bytes);
                if (!line.empty() && LabelWidth(font, line + character, size) > max_width)
                {
                    lines.push_back(line);
                    line.clear();
                }
                line += character;
                i += bytes;
            }
        }
        if (!line.empty() || paragraph.empty())
        {
            lines.push_back(line);
        }
    }
    return lines;
}

inline std::string FitLabel(Font font, std::string_view text, float size, float max_width)
{
    if (LabelWidth(font, text, size) <= max_width)
    {
        return std::string(text);
    }
    std::string result;
    for (std::size_t i = 0; i < text.size();)
    {
        int bytes = 0;
        GetCodepointNext(text.data() + i, &bytes);
        const auto next = result + std::string(text.substr(i, bytes));
        if (LabelWidth(font, next + "..", size) > max_width)
        {
            break;
        }
        result = next;
        i += bytes;
    }
    return LabelWidth(font, "..", size) <= max_width ? result + ".." : "";
}
