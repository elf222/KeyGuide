// tests/test_helpers.hpp
#pragma once
#include <cstdlib>
#include <iostream>
#include "platform/input_event.hpp"
#define CHECK(condition)                                                                           \
    do                                                                                             \
    {                                                                                              \
        if (!(condition))                                                                          \
        {                                                                                          \
            std::cerr << __FILE__ << ':' << __LINE__ << ": " #condition "\n";                      \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)

inline Platform::InputEvent Key(char32_t character, double timestamp = 0,
                                std::uint8_t modifiers = 0)
{
    if ((character >= U'A' && character <= U'Z') ||
        std::u32string_view(U"~!@#$%^&*()_+{}|:\"<>?").find(character) != std::u32string_view::npos)
    {
        modifiers |= Platform::SHIFT;
    }
    const auto physical = character >= U'a' && character <= U'z' ? character - 32 : character;
    return {.key = static_cast<Platform::KeyId>(physical),
            .value = {character},
            .modifiers = modifiers,
            .timestamp = timestamp};
}

inline Platform::InputEvent Esc(double time, bool up = false, bool repeat = false)
{
    return {.type = up ? Platform::EventType::KeyUp : Platform::EventType::KeyDown,
            .key = Platform::KeyId::Escape,
            .value = {0, Platform::SpecialKey::Escape},
            .repeat = repeat,
            .timestamp = time};
}
