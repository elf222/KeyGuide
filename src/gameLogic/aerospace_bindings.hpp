// gameLogic/aerospace_bindings.hpp
#pragma once
#include <span>
#include <string_view>
#include "keymaps/aerospace_config.hpp"
#include "platform/input_event.hpp"

// AeroSpace hotkeys are global, so they are shown while no supported editor is focused.
// AeroSpace matches physical keys and exact modifiers. The default config (qwerty preset) is
// built in; LoadKeymapFiles() replaces it with the user's ~/.aerospace.toml when present.
struct AeroSpaceBinding
{
    Platform::KeyId key = Platform::KeyId::Unknown;
    std::uint8_t modifiers = 0;
    bool service = false;
    std::string_view shortHint, fullHint;
    char32_t mnemonic = 0;
    bool entersService = false, returnsToMain = false;
};

std::span<const AeroSpaceBinding> AeroSpaceBindings();
// Replaces the active bindings with exactly these (an empty config means no AeroSpace hotkeys).
void SetAeroSpaceBindings(std::span<const AeroSpaceConfigBinding> bindings);
void RestoreDefaultAeroSpaceBindings();
bool AeroSpaceUsesDefaults();

inline bool AeroSpaceHintsShown(const Platform::InputBatchInfo& input)
{
    return !input.foreground && !input.paused && input.available && !input.preview;
}

// Bound keys follow their binding's mode switch; unbound keys pass through and keep the mode.
bool AeroSpaceServiceAfter(bool service, Platform::KeyId key, std::uint8_t modifiers);
