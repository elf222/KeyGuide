// tests/mac_input_observer_tests.mm
#include "test_helpers.hpp"
#include <atomic>
#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

namespace
{
    bool permission_allowed = false, create_fails = false, source_fails = false;
    bool secure_input_enabled = false;
    bool preview_function_held = false, preview_function_seen = false;
    int creation_attempts = 0, permission_requests = 0;
    std::atomic<bool> tap_enabled{false}, finish_run_loop{false};
    CGEventTapCallBack observed_callback = nullptr;

    bool FixturePreflight()
    {
        return permission_allowed;
    }

    bool FixtureRequestAccess()
    {
        ++permission_requests;
        return permission_allowed;
    }

    bool FixtureSecureInput()
    {
        return secure_input_enabled;
    }

    bool FixtureKeyState(CGEventSourceStateID state, CGKeyCode key)
    {
        CHECK(state == kCGEventSourceStateCombinedSessionState && key == kVK_Function);
        return preview_function_held;
    }

    CFMachPortRef FixtureCreateTap(CGEventTapLocation location, CGEventTapPlacement placement,
                                   CGEventTapOptions options, CGEventMask mask,
                                   CGEventTapCallBack callback, void*)
    {
        CHECK(location == kCGSessionEventTap && placement == kCGTailAppendEventTap);
        CHECK(options == kCGEventTapOptionListenOnly);
        CHECK(mask & CGEventMaskBit(kCGEventKeyDown));
        CHECK(mask & CGEventMaskBit(kCGEventKeyUp));
        ++creation_attempts;
        observed_callback = callback;
        if (create_fails)
        {
            return nullptr;
        }
        // A local Mach port exercises the real CF run-loop lifecycle. It is not an event tap.
        return CFMachPortCreate(
            kCFAllocatorDefault,
            [](CFMachPortRef, void*, CFIndex, void*)
            {
            },
            nullptr, nullptr);
    }

    void FixtureEnableTap(CFMachPortRef, bool enabled)
    {
        tap_enabled.store(enabled);
    }

    bool FixtureTapEnabled(CFMachPortRef)
    {
        return tap_enabled.load();
    }

    CFRunLoopSourceRef FixtureCreateSource(CFAllocatorRef allocator, CFMachPortRef port,
                                           CFIndex order)
    {
        return source_fails ? nullptr : CFMachPortCreateRunLoopSource(allocator, port, order);
    }

    SInt32 FixtureRunLoop(CFStringRef mode, CFTimeInterval interval, Boolean return_after_source)
    {
        if (finish_run_loop.load())
        {
            return kCFRunLoopRunFinished;
        }
        return CFRunLoopRunInMode(mode, interval, return_after_source);
    }

    TISInputSourceRef FixtureInputSource()
    {
        return reinterpret_cast<TISInputSourceRef>(
            const_cast<void*>(CFRetain(CFSTR("fixture-input-source"))));
    }

    CFStringRef FixtureSourceProperty(TISInputSourceRef, CFStringRef)
    {
        return CFSTR("com.apple.keylayout.ABC");
    }
} // namespace

// Replace only the native API boundary. Exercise the actual observer implementation without
// registering a global tap, asking for privacy permission or posting any OS input.
#define CGPreflightListenEventAccess FixturePreflight
#define CGRequestListenEventAccess FixtureRequestAccess
#define IsSecureEventInputEnabled FixtureSecureInput
#define CGEventTapCreate FixtureCreateTap
#define CGEventTapEnable FixtureEnableTap
#define CGEventTapIsEnabled FixtureTapEnabled
#define CFMachPortCreateRunLoopSource FixtureCreateSource
#define CFRunLoopRunInMode FixtureRunLoop
#define TISCopyCurrentKeyboardInputSource FixtureInputSource
#define TISGetInputSourceProperty FixtureSourceProperty
#define CGEventSourceKeyState FixtureKeyState
#include "platform/mac/input_observer.mm"
#undef CGPreflightListenEventAccess
#undef CGRequestListenEventAccess
#undef IsSecureEventInputEnabled
#undef CGEventTapCreate
#undef CGEventTapEnable
#undef CGEventTapIsEnabled
#undef CFMachPortCreateRunLoopSource
#undef CFRunLoopRunInMode
#undef TISCopyCurrentKeyboardInputSource
#undef TISGetInputSourceProperty
#undef CGEventSourceKeyState

extern "C" void glfwPostEmptyEvent(void)
{
}

namespace Platform
{
    void InitialiseMenu()
    {
    }

    void ShutdownMenu()
    {
    }

    void ShutdownOverlayWindow()
    {
    }

    void ConfigureOverlayWindow()
    {
    }

    InputBatchInfo ReadPreviewEvents(std::span<InputEvent>, bool function_held)
    {
        preview_function_seen = function_held;
        return {.available = true, .preview = true};
    }
} // namespace Platform

int main()
{
    @autoreleasepool
    {
        using namespace Platform;
        InputEvent events[16]{};
        const auto health = [&]
        {
            health_check_due = true;
            return ReadInputEvents(events);
        };
        const auto expire_retry = [&]
        {
            next_observer_attempt = {};
        };
        Initialise();
        auto status = health();
        CHECK(!status.available && !tap && creation_attempts == 0);
        CHECK(std::string_view(status.description).find("not granted to this build") !=
              std::string_view::npos);
        CHECK(permission_requests == 0);

        // A grant must recover on the existing health tick, without a menu click or restart.
        permission_allowed = true;
        const auto before_grant = losses.load();
        status = health();
        CHECK(status.available && tap && running.load() && creation_attempts == 1);
        CHECK(losses.load() > before_grant && permission_requests == 0);
        for (int i = 0; i < 20; ++i)
        {
            CHECK(health().available);
        }
        CHECK(creation_attempts == 1);

        auto event = CGEventCreateKeyboardEvent(nullptr, 5, true);
        CHECK(event);
        const auto flags = CGEventGetFlags(event);
        // The callback never dereferences its proxy; use a non-null fixture address for the API
        // type.
        const auto proxy = reinterpret_cast<CGEventTapProxy>(&creation_attempts);
        CHECK(observed_callback(proxy, kCGEventKeyDown, event, nullptr) == event);
        CHECK(CGEventGetFlags(event) == flags);
        CHECK(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode) == 5);
        const auto published_sequence = input_sequence.load();
        input_sequence.store(
            0); // Model a consumer running before the producer publishes its count.
        CHECK(ReadInputEvents(events).count == 1 && events[0].key == KeyId::G);
        CHECK(processed_input_sequence >= events[0].sequence &&
              processed_input_sequence >= published_sequence);
        input_sequence.store(published_sequence);
        CFRelease(event);

        const auto observed_key = [&](CGKeyCode code, CGEventType type, CGEventFlags key_flags)
        {
            auto key_event = CGEventCreateKeyboardEvent(nullptr, code, type != kCGEventKeyUp);
            CHECK(key_event);
            CGEventSetFlags(key_event, key_flags);
            CHECK(observed_callback(proxy, type, key_event, nullptr) == key_event);
            CHECK(CGEventGetFlags(key_event) == key_flags);
            CHECK(CGEventGetIntegerValueField(key_event, kCGKeyboardEventKeycode) == code);
            CHECK(ReadInputEvents(events).count == 1);
            const auto result = events[0];
            CFRelease(key_event);
            return result;
        };
        // Function/navigation event flags alone must not invent a held physical Fn key.
        CHECK(!observed_key(120, kCGEventKeyDown, kCGEventFlagMaskSecondaryFn).functionHeld);
        CHECK(!observed_key(120, kCGEventKeyUp, kCGEventFlagMaskSecondaryFn).functionHeld);

        CHECK(observed_key(kVK_Function, kCGEventFlagsChanged, kCGEventFlagMaskSecondaryFn)
                  .functionHeld);
        losses.fetch_add(1);
        CHECK(!observed_key(120, kCGEventKeyDown, kCGEventFlagMaskSecondaryFn).functionHeld);
        CHECK(!observed_key(kVK_Function, kCGEventFlagsChanged, 0).functionHeld);
        CHECK(!observed_key(126, kCGEventKeyDown, kCGEventFlagMaskSecondaryFn).functionHeld);
        CHECK(observed_key(kVK_Function, kCGEventFlagsChanged, kCGEventFlagMaskSecondaryFn)
                  .functionHeld);
        const auto number = observed_key(19, kCGEventKeyDown, kCGEventFlagMaskSecondaryFn);
        CHECK(number.functionHeld && number.key == KeyId::Two && number.modifiers == 0);
        CHECK(observed_key(120, kCGEventKeyDown, kCGEventFlagMaskSecondaryFn).functionHeld);
        CHECK(!observed_key(kVK_Function, kCGEventFlagsChanged, 0).functionHeld);
        CHECK(!observed_key(120, kCGEventKeyUp, kCGEventFlagMaskSecondaryFn).functionHeld);

        // Revocation closes the old observer and invalidates queued state.
        permission_allowed = false;
        const auto before_revocation = losses.load();
        status = health();
        CHECK(!status.available && !tap && !running.load() && !observer_loop.load());
        CHECK(losses.load() > before_revocation);
        RequestObservation();
        CHECK(permission_requests == 1 && creation_attempts == 1);

        // Creation failures are diagnosed distinctly, and retries cannot become a busy loop.
        permission_allowed = true;
        create_fails = true;
        status = health();
        CHECK(!status.available && !tap && creation_attempts == 2);
        CHECK(std::string_view(status.description).find("could not create") !=
              std::string_view::npos);
        for (int i = 0; i < 20; ++i)
        {
            health();
        }
        CHECK(creation_attempts == 2 && permission_requests == 1);
        RequestObservation();
        CHECK(creation_attempts == 3 && permission_requests == 1);
        create_fails = false;
        expire_retry();
        CHECK(health().available && creation_attempts == 4);

        tap_enabled.store(false);
        const auto before_disabled = losses.load();
        expire_retry();
        CHECK(health().available && creation_attempts == 5);
        CHECK(losses.load() > before_disabled);

        StopObserver();
        source_fails = true;
        expire_retry();
        status = health();
        CHECK(!status.available && !tap && !source && creation_attempts == 6);
        CHECK(std::string_view(status.description).find("run-loop source") !=
              std::string_view::npos);
        health();
        CHECK(creation_attempts == 6);
        source_fails = false;
        expire_retry();
        CHECK(health().available && creation_attempts == 7);

        // Secure Input, pause and preview must not repeatedly restart an unavailable observer.
        StopObserver();
        secure_input_enabled = true;
        expire_retry();
        CHECK(!health().available && creation_attempts == 7);
        secure_input_enabled = false;
        paused = true;
        health();
        CHECK(creation_attempts == 7);
        TogglePause();
        CHECK(health().available && creation_attempts == 8);
        TogglePreview();
        preview_function_held = true;
        CHECK(health().preview && !tap && creation_attempts == 8);
        CHECK(preview_function_seen);
        preview_function_held = false;
        CHECK(health().preview && !preview_function_seen);
        TogglePreview();
        CHECK(health().available && creation_attempts == 9);

        // A finished source must terminate its worker instead of spinning at full CPU.
        StopObserver();
        finish_run_loop.store(true);
        expire_retry();
        health();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        while (running.load() && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::yield();
        }
        CHECK(!running.load() && !health().available);
        finish_run_loop.store(false);
        expire_retry();
        CHECK(health().available && creation_attempts == 11);
        Shutdown();
        CHECK(!tap && !source && !observer_loop.load() && !observer_thread.joinable());
        CHECK(permission_requests == 1);
        std::cout << "Native observer permission recovery, retry limits, failure diagnostics, "
                     "passive return and worker lifecycle passed; no global tap or OS input used\n";
    }
}
