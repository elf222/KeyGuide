// platform/fallback/platform.cpp
#include "platform/platform.hpp"
#include <filesystem>
#include <nlohmann/json.hpp>
#include "platform/preview_input.hpp"
#include "raylib.h"
#include "settings/app_paths.hpp"
#include "settings/defaults.hpp"
#include "settings/settings.hpp"

#if defined(PLATFORM_DESKTOP)
extern "C" void glfwPostEmptyEvent(void);
#endif

namespace
{
    // Without a native credential store, secrets live in an owner-only file.
    std::filesystem::path SecretsPath()
    {
        return AppDataDirectory() / "secrets.json";
    }

    nlohmann::json ReadSecrets()
    {
        std::string text;
        if (!ReadWholeFile(SecretsPath(), text))
        {
            return nlohmann::json::object();
        }
        auto secrets = nlohmann::json::parse(text, nullptr, false);
        return secrets.is_object() ? secrets : nlohmann::json::object();
    }

    bool WriteSecrets(const nlohmann::json& secrets)
    {
        return WriteFileAtomically(SecretsPath(), secrets.dump(), true);
    }
} // namespace

namespace Platform
{
    void PrepareApplication(bool)
    {
    }

    void Initialise()
    {
    }

    void Shutdown()
    {
    }

    void ConfigureOverlayWindow()
    {
    }

    void ResizeOverlayWindow(int width, int height)
    {
        SetWindowSize(width, height);
    }

    void PumpEvents(bool wait)
    {
#if defined(PLATFORM_DESKTOP)
        if (wait)
        {
            EnableEventWaiting();
        }
#elif !defined(PLATFORM_WEB)
        // Mobile/direct-display backends do not necessarily implement event waiting.
        if (wait)
        {
            WaitTime(1.0 / Default::TARGET_FPS);
        }
#endif
        PollInputEvents();
        DisableEventWaiting();
    }

    bool TakeWindowRedrawRequest()
    {
        return false;
    }

    InputBatchInfo ReadInputEvents(std::span<InputEvent> destination)
    {
        return ReadPreviewEvents(destination);
    }

    PointerPosition ReadPointerPosition()
    {
        return ReadPreviewPointer();
    }

    bool IsPreview()
    {
        return true;
    }

    void WakeMainLoop()
    {
#if defined(PLATFORM_DESKTOP)
        glfwPostEmptyEvent();
#endif
    }

    Observation ObservationState()
    {
        return Observation::NotNeeded;
    }

    bool IsPaused()
    {
        return false;
    }

    void RequestObservation()
    {
    }

    void TogglePause()
    {
    }

    void TogglePreview()
    {
    }

    void QueueBaseline(EventType)
    {
    }

    void RequestQuit()
    {
    }

    void ToggleWindowVisibility()
    {
        S::windowVisible = !S::windowVisible;
    }

    void MoveOverlay(Default::WindowPosition position)
    {
        S::windowPosition = position;
    }

    void NudgeOverlay(float, float)
    {
    }

    void MoveOverlayToNextDisplay()
    {
    }

    std::optional<std::string> PromptText(std::string_view, std::string_view, std::string_view,
                                          bool)
    {
        return std::nullopt;
    }

    void ShowMessage(std::string_view, std::string_view)
    {
    }

    bool OpenPath(const std::string& path)
    {
        if (path.empty())
        {
            return false;
        }
        OpenURL(path.c_str());
        return true;
    }

    bool RevealPath(const std::string& path)
    {
        return OpenPath(std::filesystem::path(path).parent_path().string());
    }

    std::string LoadSecret(std::string_view name)
    {
        const auto secrets = ReadSecrets();
        const auto found = secrets.find(std::string(name));
        return found != secrets.end() && found->is_string() ? found->get<std::string>() : "";
    }

    bool StoreSecret(std::string_view name, std::string_view value)
    {
        auto secrets = ReadSecrets();
        secrets[std::string(name)] = std::string(value);
        return WriteSecrets(secrets);
    }

    bool DeleteSecret(std::string_view name)
    {
        auto secrets = ReadSecrets();
        secrets.erase(std::string(name));
        return WriteSecrets(secrets);
    }
} // namespace Platform
