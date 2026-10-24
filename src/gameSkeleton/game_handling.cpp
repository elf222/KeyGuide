// gameSkeleton/game_handling.cpp
#include "gameSkeleton/game_handling.hpp"
#include <algorithm>
#include <string>
#include <tuple>
#include "gameLogic/input.hpp"
#include "gameLogic/tick_logic.hpp"
#include "gameWorld/keyboard_layout.hpp"
#include "global/asset_path.hpp"
#include "global/global.hpp"
#include "guide/explanations.hpp"
#include "keymaps/keymap_files.hpp"
#include "platform/platform.hpp"
#include "registry/global.hpp"
#include "render/render.hpp"
#include "settings/settings.hpp"

void UpdateToNewWindowSize()
{
    UpdateSettingsToNewWindowSize();
    if (Global::Render::texture_world.id)
    {
        UnloadRenderTexture(Global::Render::texture_world);
    }
    Global::Render::texture_world = LoadRenderTexture(GetRenderWidth(), GetRenderHeight());
    SetTextureFilter(Global::Render::texture_world.texture, TEXTURE_FILTER_BILINEAR);
    Global::Render::pixelScale = static_cast<float>(GetRenderWidth()) / S::screenWidth;
    CameraLogic();
}

void ConfigureFlags()
{
    unsigned int flags =
        FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT | FLAG_WINDOW_TRANSPARENT;
    if (!Platform::IsPreview())
    {
        flags |= FLAG_WINDOW_HIDDEN | FLAG_WINDOW_UNFOCUSED | FLAG_WINDOW_UNDECORATED |
                 FLAG_WINDOW_TOPMOST | FLAG_WINDOW_MOUSE_PASSTHROUGH;
    }
    SetConfigFlags(flags);
}

void SetFlags()
{
    SetExitKey(KEY_NULL);
    SetWindowMinSize(400, 160);
}

void InitialiseWorld()
{
    cmd_registry.clear();
    registry.clear();
    Global::trackerEID = -1;
    ResetInputInfo();
    cmd_registry.add<CommandInitialiseKeyboard>();
    cmd_registry.executeAll();
    cmd_registry.add<CommandProjectKeyboard>();
    cmd_registry.executeAll();
    CameraLogic();
}

void InitialiseGame()
{
    const auto canvas = KeyboardCanvasSize();
    S::screenWidth = static_cast<int>(canvas.x);
    S::screenHeight = static_cast<int>(canvas.y);
    LoadKeymapFiles();
    ConfigureFlags();
    InitWindow(S::screenWidth, S::screenHeight, "KeyGuide");
    SetFlags();
    Platform::ConfigureOverlayWindow();
    const auto font_path = AssetPath("fonts/ibm_plex_mono.ttf");
    if (FileExists(font_path.c_str()))
    {
        int codepoints[96]{};
        for (int i = 0; i < 95; ++i)
        {
            codepoints[i] = 32 + i;
        }
        codepoints[95] = 183; // Middle dot in status labels.
        Global::Render::font = LoadFontEx(font_path.c_str(), 48, codepoints, 96);
        SetTextureFilter(Global::Render::font.texture, TEXTURE_FILTER_BILINEAR);
    }
    UpdateToNewWindowSize();
    InitialiseWorld();
    Platform::Initialise();
    Guide::Initialise();
}

void UpdateDeltaTime()
{
    static double previous_update = GetTime();
    const double now = GetTime();
    // Idle waits are not animation frames. Start each resumed animation with a small step.
    Global::dt = static_cast<float>(std::min(now - previous_update, 1.0 / Default::TARGET_FPS));
    previous_update = now;
}

void MainLoop()
{
    Global::Render::frameDrawn = false;
    ++Global::frameCounter;
    static bool first_frame = true;
    const auto settings = std::tuple{S::windowVisible, S::reduceMotion, S::transparencyMode};
    static auto previous_settings = settings;
    bool dirty =
        first_frame || settings != previous_settings || Global::Render::continuousRendering;
    first_frame = false;
    previous_settings = settings;
    if (S::keyboardLayoutChanged)
    {
        S::keyboardLayoutChanged = false;
        cmd_registry.add<CommandRebuildKeyboard>();
        cmd_registry.executeAll();
        const auto canvas = KeyboardCanvasSize();
        Platform::ResizeOverlayWindow(static_cast<int>(canvas.x), static_cast<int>(canvas.y));
        UpdateToNewWindowSize();
        dirty = true;
    }
    UpdateDeltaTime();
    dirty |= UpdateInputInfo();
    dirty |= Guide::Update();
    cmd_registry.executeAll();
    dirty |= Platform::TakeWindowRedrawRequest();
    if (IsWindowResized() || Global::Render::texture_world.texture.width != GetRenderWidth() ||
        Global::Render::texture_world.texture.height != GetRenderHeight())
    {
        UpdateToNewWindowSize();
        dirty = true;
    }
    if (!S::windowVisible)
    {
        Global::Render::hoveredEID = -1;
        Global::Render::hoverAmount = 0;
        Global::Render::expandedWidth = 0;
        Global::Render::expandedHeight = 0;
        Global::Render::animating = false;
        return;
    }
    if (!dirty && !Global::Render::animating)
    {
#if defined(PLATFORM_WEB)
        Platform::PumpEvents(false);
#endif
        return;
    }
    if (dirty)
    {
        cmd_registry.add<CommandProjectKeyboard>(Global::Render::hoveredEID,
                                                 Global::Render::expandedWidth,
                                                 Global::Render::expandedHeight);
        cmd_registry.executeAll();
    }
    TickOnce();
    cmd_registry.executeAll();
    CameraLogic();
    DrawFrame();
    Global::Render::frameDrawn = true;
}

void CloseGame()
{
    Guide::Shutdown();
    Platform::Shutdown();
    cmd_registry.clear();
    registry.clear();
    if (Global::Render::texture_world.id)
    {
        UnloadRenderTexture(Global::Render::texture_world);
    }
    Global::Render::texture_world = {};
    if (Global::Render::font.texture.id)
    {
        UnloadFont(Global::Render::font);
    }
    Global::Render::font = {};
    CloseWindow();
}
