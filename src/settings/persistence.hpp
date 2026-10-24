// settings/persistence.hpp
#pragma once
#include <filesystem>

// Menu choices persist in <AppDataDirectory>/settings.json. Command-line flags still override
// them for the current launch.
std::filesystem::path SettingsPath();
void LoadSettings();
void SaveSettings();
void ResetSettings();
