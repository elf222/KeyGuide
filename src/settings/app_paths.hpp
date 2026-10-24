// settings/app_paths.hpp
#pragma once
#include <filesystem>
#include <string>

// Per-user KeyGuide data directory, created on first use:
// macOS ~/Library/Application Support/KeyGuide, Windows %APPDATA%\KeyGuide,
// elsewhere $XDG_CONFIG_HOME/keyguide or ~/.config/keyguide.
std::filesystem::path AppDataDirectory();
std::filesystem::path HomeDirectory();
// Writes through a temporary file and rename so readers never see a partial file.
// `owner_only` restricts the file to its owner before any content is written.
bool WriteFileAtomically(const std::filesystem::path& path, const std::string& contents,
                         bool owner_only = false);
bool ReadWholeFile(const std::filesystem::path& path, std::string& contents);
