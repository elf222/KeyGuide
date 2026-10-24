// tests/platform_stub.cpp
#include "platform_stub.hpp"
#include <algorithm>

namespace TestPlatform
{
    Platform::InputBatchInfo status{
        .foregroundGeneration = 1, .foreground = true, .available = true};
    std::vector<Platform::InputEvent> events;
    Platform::PointerPosition pointer{};
} // namespace TestPlatform

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

    void ResizeOverlayWindow(int, int)
    {
    }

    void SetPreviewProfile(AppProfile)
    {
    }

    bool IsPreview()
    {
        return false;
    }

    void WakeMainLoop()
    {
    }

    std::string LoadSecret(std::string_view)
    {
        return {};
    }

    PointerPosition ReadPointerPosition()
    {
        return TestPlatform::pointer;
    }

    InputBatchInfo ReadInputEvents(std::span<InputEvent> destination)
    {
        auto result = TestPlatform::status;
        result.count = std::min(destination.size(), TestPlatform::events.size());
        std::copy_n(TestPlatform::events.begin(), result.count, destination.begin());
        TestPlatform::events.erase(TestPlatform::events.begin(),
                                   TestPlatform::events.begin() + result.count);
        return result;
    }
} // namespace Platform
