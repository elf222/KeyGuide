// global/asset_path.hpp
#pragma once
#include <string>
#include <string_view>
#include "raylib.h"

// macOS bundles keep data in Contents/Resources, which code signing requires; other builds keep
// assets beside the executable.
inline std::string AssetPath(std::string_view relative)
{
    const std::string directory = GetApplicationDirectory();
    const auto bundled = directory + "../Resources/assets/" + std::string(relative);
    return FileExists(bundled.c_str()) ? bundled : directory + "assets/" + std::string(relative);
}
