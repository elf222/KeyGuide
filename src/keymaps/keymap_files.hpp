// keymaps/keymap_files.hpp
#pragma once
#include <cstddef>
#include <string>
#include <vector>

// User keymap files that replace KeyGuide's built-in shortcut tables.
struct KeymapFileStatus
{
    std::string aerospacePath, aerospaceError;
    std::vector<std::string> aerospaceWarnings;
    std::size_t aerospaceBindings = 0;
    bool aerospaceFromFile = false;
};

// Reads the files and swaps the active bindings. The caller rebuilds keyboard hints and the
// explanation catalog afterwards, because HintIds of replaced bindings move.
void LoadKeymapFiles();
const KeymapFileStatus& KeymapFiles();
