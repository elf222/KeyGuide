// menu/menu_model.hpp
#pragma once
#include <string>
#include <vector>

// Platform-independent menu: built fresh from app state every time the menu opens, then
// rendered by a thin native layer (NSStatusItem on macOS). Actions run on the main thread.
enum class MenuAction
{
    None,
    ToggleWindow,
    TogglePause,
    Baseline, // argument: Platform::EventType
    TipPlacement, // argument: Default::TipPlacement
    Generate, // argument: Guide::Domain
    StopGenerating,
    ExportGuide, // argument: Guide::Domain
    SelectProvider, // argument: Default::AIProvider
    SelectModel,    // payload: model id
    RefreshModels,
    TestConnection,
    SetDigitalOceanKey,
    RemoveDigitalOceanKey,
    SetOllamaServer,
    ClearTips,
    ReloadKeybindings,
    OpenAeroSpaceConfig,
    ToggleBlenderState,
    RevealBlenderAddon,
    Transparency, // argument: Default::TransparencyMode
    ToggleFunctionRow,
    ToggleNumpad,
    Position, // argument: Default::WindowPosition
    NextDisplay,
    Nudge, // argument: index into the nudge table
    RequestObservation,
    Preview, // argument: -1 for the overlay, otherwise Platform::AppProfile
    OpenDataFolder,
    ResetSettings,
    Quit
};

enum class MenuStyle
{
    Normal,
    Title,  // Bold status line.
    Detail, // Small secondary text.
    Warning
};

struct MenuItem
{
    std::string title, payload;
    MenuAction action = MenuAction::None;
    int argument = 0;
    MenuStyle style = MenuStyle::Normal;
    bool enabled = true, checked = false, separator = false;
    char keyEquivalent = 0; // Lowercase letter used with Command.
    std::vector<MenuItem> children;
};

std::vector<MenuItem> BuildMenu();
// Indented text dump of a menu tree (`--print-menu`), for checks without clicking.
std::string DescribeMenu(const std::vector<MenuItem>& items, int depth = 0);
void PerformMenuAction(MenuAction action, int argument, const std::string& payload);
