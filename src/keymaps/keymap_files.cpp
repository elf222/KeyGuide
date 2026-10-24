// keymaps/keymap_files.cpp
#include "keymaps/keymap_files.hpp"
#include "gameLogic/aerospace_bindings.hpp"
#include "keymaps/aerospace_config.hpp"

namespace
{
    KeymapFileStatus status;
}

void LoadKeymapFiles()
{
    status = {};
    status.aerospacePath = FindAeroSpaceConfigPath();
    std::optional<AeroSpaceConfig> config;
    if (!status.aerospacePath.empty())
    {
        config = LoadAeroSpaceConfig(status.aerospacePath, &status.aerospaceError);
    }
    if (config)
    {
        // A config without binding tables is valid: AeroSpace then has no hotkeys at all.
        SetAeroSpaceBindings(config->bindings);
        status.aerospaceFromFile = true;
        status.aerospaceWarnings = std::move(config->warnings);
    }
    else
    {
        RestoreDefaultAeroSpaceBindings();
    }
    status.aerospaceBindings = AeroSpaceBindings().size();
}

const KeymapFileStatus& KeymapFiles()
{
    return status;
}
