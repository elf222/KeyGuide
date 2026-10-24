// main.cpp
// KeyGuide · Copyright (c) 2026 elf222 · Built with raylib.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <string>
#include <string_view>
#include "raylib.h"
#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif
#include "gameSkeleton/game_handling.hpp"
#include "global/global.hpp"
#include "guide/command_line.hpp"
#include "menu/menu_model.hpp"
#include "platform/platform.hpp"
#include "platform/preview_input.hpp"
#include "settings/ai.hpp"
#include "settings/defaults.hpp"
#include "settings/persistence.hpp"
#include "settings/settings.hpp"

// `--pointer` moves the authored pointer into the window after the startup state settles.
constexpr long long POINTER_ENTRY_FRAME = 10;

#if !defined(PLATFORM_WEB)
namespace
{
    // `--preview --demo-video out.mp4`: a scripted pointer tour, piped frame by frame to ffmpeg.
    struct DemoStop
    {
        float x, y; // Window points of the 748 x 243 preview.
        Default::TipPlacement placement;
    };
    using enum Default::TipPlacement;
    constexpr DemoStop DEMO_STOPS[] = {
        {148, 75, Below},  {197, 75, Below},  {330, 167, Below}, {208, 121, Below},
        {281, 167, Below}, {257, 121, Right}, {393, 75, Right},  {551, 121, Right},
        {404, 121, Right},
    };
    constexpr long long DEMO_INTRO = 45, DEMO_MOVE = 24, DEMO_HOLD = 140, DEMO_OUTRO = 60;
    constexpr long long DEMO_STEP = DEMO_MOVE + DEMO_HOLD;

    long long DemoFrameCount()
    {
        return DEMO_INTRO + static_cast<long long>(std::size(DEMO_STOPS)) * DEMO_STEP + DEMO_OUTRO;
    }

    void DemoPointer(long long frame)
    {
        const auto index = (frame - DEMO_INTRO) / DEMO_STEP;
        if (frame < DEMO_INTRO || index >= static_cast<long long>(std::size(DEMO_STOPS)))
        {
            Platform::SetPreviewPointerOverride(-1, -1);
            return;
        }
        const auto& stop = DEMO_STOPS[index];
        const auto& from = index == 0 ? DemoStop{stop.x, 250, stop.placement} : DEMO_STOPS[index - 1];
        float t = std::min(1.0f, static_cast<float>((frame - DEMO_INTRO) % DEMO_STEP) / DEMO_MOVE);
        t = t * t * (3 - 2 * t);
        S::tipPlacement = stop.placement;
        Platform::SetPreviewPointerOverride(from.x + (stop.x - from.x) * t,
                                            from.y + (stop.y - from.y) * t);
    }

    // Raw RGBA frames at 60 fps; ffmpeg encodes H.264. Returns false once writing fails.
    bool WriteDemoFrame(FILE*& video, const char* path)
    {
        auto image = LoadImageFromScreen();
        if (!video)
        {
            // Single-quoted, with embedded quotes escaped, so the shell expands nothing.
            std::string quoted = "'";
            for (const char c : std::string_view(path))
            {
                quoted += c == '\'' ? std::string("'\\''") : std::string(1, c);
            }
            quoted += "'";
            const auto command = "ffmpeg -y -loglevel error -f rawvideo -pix_fmt rgba -s " +
                                 std::to_string(image.width) + "x" + std::to_string(image.height) +
                                 " -r 60 -i - -c:v libx264 -pix_fmt yuv420p -crf 16 " + quoted;
            video = popen(command.c_str(), "w");
        }
        const auto bytes = static_cast<std::size_t>(image.width) * image.height * 4;
        const bool written = video && std::fwrite(image.data, 1, bytes, video) == bytes;
        UnloadImage(image);
        return written;
    }
} // namespace
#endif

int main(int argc, char** argv)
{
    bool preview = false;
    int frames = 0;
    const char* screenshot = nullptr;
    bool print_menu = false, pointer = false;
    const char* demo_video = nullptr;
    float pointer_x = 0, pointer_y = 0;
    // Saved menu choices first, so command-line flags override them for this launch.
    LoadSettings();
    for (int i = 1; i < argc; ++i)
    {
        const auto arg = std::string_view(argv[i]);
        if (arg == "--preview")
        {
            preview = true;
        }
        else if (arg == "--profile" && i + 1 < argc)
        {
            const auto profile = std::string_view(argv[++i]);
            Platform::SetPreviewProfile(profile == "blender" ? Platform::AppProfile::Blender
                                        : profile == "nvim"  ? Platform::AppProfile::Neovim
                                                             : Platform::AppProfile::Zed);
        }
        else if (arg == "--function-row")
        {
            S::showFunctionRow = true;
        }
        else if (arg == "--numpad")
        {
            S::showNumpad = true;
        }
        else if (arg == "--frames" && i + 1 < argc)
        {
            frames = std::atoi(argv[++i]);
        }
        else if (arg == "--screenshot" && i + 1 < argc)
        {
            screenshot = argv[++i];
        }
        else if (arg == "--pointer" && i + 1 < argc)
        {
            pointer = std::sscanf(argv[++i], "%f,%f", &pointer_x, &pointer_y) == 2;
            if (pointer)
            {
                // Start outside; entering later avoids freezing the startup state in hover lock.
                Platform::SetPreviewPointerOverride(-1, -1);
            }
        }
        else if (arg == "--demo-video" && i + 1 < argc)
        {
            demo_video = argv[++i];
            preview = true;
            frames = static_cast<int>(DemoFrameCount());
            Platform::SetPreviewPointerOverride(-1, -1);
        }
        else if (arg == "--print-menu")
        {
            print_menu = true;
        }
        else if (arg == "--explain")
        {
            return RunExplainCommand(i + 1 < argc ? argv[i + 1] : "");
        }
    }
    SetTraceLogLevel(LOG_WARNING);
    Platform::PrepareApplication(preview);
    InitialiseGame();
    Global::Render::continuousRendering = frames > 0 || screenshot;
    FILE* video = nullptr;
    SetTargetFPS(Default::TARGET_FPS);
#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(MainLoop, Default::TARGET_FPS, 1);
#else
    while (!WindowShouldClose() && !Global::quit)
    {
        if (demo_video)
        {
            DemoPointer(Global::frameCounter);
        }
        MainLoop();
        if (demo_video && Global::Render::frameDrawn && !WriteDemoFrame(video, demo_video))
        {
            std::fputs("--demo-video: could not write frames to ffmpeg\n", stderr);
            break;
        }
        if (pointer && Global::frameCounter == POINTER_ENTRY_FRAME)
        {
            Platform::SetPreviewPointerOverride(pointer_x, pointer_y);
        }
        if (frames > 0 && Global::frameCounter >= frames)
        {
            break;
        }
        // EndDrawing pumps events on rendered frames. Do not poll twice and lose preview key edges.
        if (!Global::Render::frameDrawn)
        {
            Platform::PumpEvents(true);
        }
    }
    if (print_menu)
    {
        std::fputs(DescribeMenu(BuildMenu()).c_str(), stdout);
    }
    if (screenshot)
    {
        const auto image = LoadImageFromScreen();
        ExportImage(image, screenshot);
        UnloadImage(image);
    }
    if (video)
    {
        pclose(video);
    }
    CloseGame();
#endif
    return 0;
}
