// platform/platform.hpp
#pragma once
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include "platform/input_event.hpp"

namespace Default
{
    enum class WindowPosition;
}

namespace Platform
{
    void PrepareApplication(bool preview);
    void Initialise();
    void Shutdown();
    void ConfigureOverlayWindow();
    void ResizeOverlayWindow(int width, int height);
    void PumpEvents(bool wait);
    bool TakeWindowRedrawRequest();
    InputBatchInfo ReadInputEvents(std::span<InputEvent> destination);
    PointerPosition ReadPointerPosition();
    bool IsPreview();
    void SetPreviewProfile(AppProfile profile);

    // Thread-safe: wakes a main loop blocked in PumpEvents(true).
    void WakeMainLoop();

    enum class Observation
    {
        NotNeeded, // Local preview / builds without a global observer.
        NotGranted,
        Failed,
        Running
    };
    Observation ObservationState();
    bool IsPaused();
    void RequestObservation();
    void TogglePause();
    void TogglePreview();
    void QueueBaseline(EventType type);
    void RequestQuit();
    void ToggleWindowVisibility();
    void MoveOverlay(Default::WindowPosition position);
    void NudgeOverlay(float x, float y);
    void MoveOverlayToNextDisplay();

    // Native dialogs and shell integration. Builds without native UI return std::nullopt / false.
    std::optional<std::string> PromptText(std::string_view title, std::string_view message,
                                          std::string_view initial, bool secure);
    void ShowMessage(std::string_view title, std::string_view message);
    bool OpenPath(const std::string& path);
    bool RevealPath(const std::string& path);

    // Credentials (macOS Keychain; owner-only file elsewhere).
    std::string LoadSecret(std::string_view name);
    bool StoreSecret(std::string_view name, std::string_view value);
    bool DeleteSecret(std::string_view name);
} // namespace Platform
