// gameLogic/shortcut_bindings.hpp
#pragma once
#include <array>
#include <string_view>
#include "gameLogic/tracked_state.hpp"

struct ShortcutHint
{
    std::string_view shortHint, fullHint, chord;
    char32_t mnemonic = 0;
    std::uint8_t modifiers = 0;
    bool exContinuation = false, physical = false;
};

std::size_t ShortcutCount();
ShortcutHint GetShortcutHint(HintId id);
bool ShortcutMatchesKey(HintId id, Platform::KeyId key,
                        const std::array<Platform::KeyValue, 2>& values, std::uint8_t modifiers);
bool ShortcutAvailable(HintId id, const TrackedState& state);
// AeroSpace hints replace editor hints while no supported editor is focused.
bool AeroSpaceShortcutAvailable(HintId id, bool service);
