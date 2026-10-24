// keymaps/aerospace_config.hpp
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "platform/input_event.hpp"

struct AeroSpaceConfigBinding
{
    Platform::KeyId key = Platform::KeyId::Unknown;
    std::uint8_t modifiers = 0;
    bool service = false;        // declared under [mode.service.binding]
    bool entersService = false;  // main-mode binding whose final mode switch is "service"
    bool returnsToMain = false;  // service-mode binding whose final mode switch is "main"
    std::string shortHint, fullHint, command; // command = raw commands joined with "; "
};

struct AeroSpaceConfig
{
    std::vector<AeroSpaceConfigBinding> bindings;
    std::vector<std::string> warnings; // skipped keys / modes, non-qwerty preset, etc.
};

// Parses AeroSpace TOML text. Returns std::nullopt and fills *error when the TOML is invalid.
std::optional<AeroSpaceConfig> ParseAeroSpaceConfig(std::string_view text,
                                                    std::string* error = nullptr);
// Reads and parses a file. std::nullopt (with *error) when it is missing, unreadable or invalid.
std::optional<AeroSpaceConfig> LoadAeroSpaceConfig(const std::string& path,
                                                   std::string* error = nullptr);
// First existing of: $HOME/.aerospace.toml, $XDG_CONFIG_HOME/aerospace/aerospace.toml,
// $HOME/.config/aerospace/aerospace.toml. Empty string when none exists.
std::string FindAeroSpaceConfigPath();
// "alt-shift-h" -> KeyId::H, OPTION|SHIFT. False for unsupported notation.
bool ParseAeroSpaceKey(std::string_view notation, Platform::KeyId& key, std::uint8_t& modifiers);
// Hint text for one binding's command list (without "mode main"/"mode service" bookkeeping
// entries).
void DescribeAeroSpaceCommands(const std::vector<std::string>& commands, bool service,
                               std::string& short_hint, std::string& full_hint);
