// platform/mac/input_observer.mm
#include "platform/platform.hpp"
#include <array>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>
#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#include "platform/event_queue.hpp"
#include "platform/mac/mac_bridge.hpp"
#include "platform/mac/blender_bridge.hpp"
#include "platform/preview_input.hpp"
#include "settings/blender.hpp"

// raylib's bundled desktop backend supplies GLFW. This wakes its UI loop only;
// it posts no keyboard or mouse event into the system input stream.
extern "C" void glfwPostEmptyEvent(void);

namespace
{
    struct RawEvent
    {
        CGEventType type{};
        CGKeyCode keyCode = 0;
        CGEventFlags flags = 0;
        std::array<UniChar, 2> text{};
        UniCharCount textLength = 0;
        bool repeat = false;
        Platform::MouseButton mouseButton = Platform::MouseButton::Unknown;
        bool mousePressed = true;
        double timestamp = 0;
        std::uint64_t foregroundToken = 0, lossGeneration = 0, sequence = 0;
    };

    Platform::EventQueue<RawEvent, 1024> queue;
    std::atomic<std::uint64_t> foreground_token{0}, losses{0};
    std::atomic<bool> running{false}, capture_enabled{true};
    std::atomic<CFRunLoopRef> observer_loop{nullptr};
    std::thread observer_thread;
    CFMachPortRef tap = nullptr;
    CFRunLoopSourceRef source = nullptr;
    id workspace_token = nil;
    CFRunLoopSourceRef main_wake_source = nullptr;
    std::atomic<bool> wake_pending{false};
    std::atomic<float> pointer_left{0}, pointer_top{0}, pointer_right{0}, pointer_bottom{0};
    NSTimer* health_timer = nil;
    bool health_check_due = true, observation_allowed = false, reduce_motion = false;
    bool preview = false, paused = false, quitting = false, secure_input = false;
    bool layout_supported = true;
    bool function_held = false;
    std::uint64_t function_loss_generation = 0;
    enum class ObserverFailure
    {
        None,
        TapCreation,
        RunLoopSource
    };
    ObserverFailure observer_failure = ObserverFailure::None;
    std::chrono::steady_clock::time_point next_observer_attempt{};
    constexpr std::chrono::seconds OBSERVER_RETRY_INTERVAL{5};
    Platform::AppProfile app_profile = Platform::AppProfile::Zed;
    std::vector<Platform::InputEvent> menu_events;
    Platform::Mac::BlenderBridge blender_bridge;
    std::atomic<std::uint64_t> input_sequence{0};
    std::uint64_t processed_input_sequence = 0;
    std::uint32_t blender_pid = 0;

    Platform::KeyId PhysicalKey(CGKeyCode code)
    {
        using K = Platform::KeyId;
        switch (code)
        {
            case 0:
                return K::A;
            case 1:
                return K::S;
            case 2:
                return K::D;
            case 3:
                return K::F;
            case 4:
                return K::H;
            case 5:
                return K::G;
            case 6:
                return K::Z;
            case 7:
                return K::X;
            case 8:
                return K::C;
            case 9:
                return K::V;
            case 11:
                return K::B;
            case 12:
                return K::Q;
            case 13:
                return K::W;
            case 14:
                return K::E;
            case 15:
                return K::R;
            case 16:
                return K::Y;
            case 17:
                return K::T;
            case 18:
                return K::One;
            case 19:
                return K::Two;
            case 20:
                return K::Three;
            case 21:
                return K::Four;
            case 22:
                return K::Six;
            case 23:
                return K::Five;
            case 24:
                return K::Equal;
            case 25:
                return K::Nine;
            case 26:
                return K::Seven;
            case 27:
                return K::Minus;
            case 28:
                return K::Eight;
            case 29:
                return K::Zero;
            case 30:
                return K::RightBracket;
            case 31:
                return K::O;
            case 32:
                return K::U;
            case 33:
                return K::LeftBracket;
            case 34:
                return K::I;
            case 35:
                return K::P;
            case 36:
                return K::Enter;
            case 37:
                return K::L;
            case 38:
                return K::J;
            case 39:
                return K::Apostrophe;
            case 40:
                return K::K;
            case 41:
                return K::Semicolon;
            case 42:
                return K::Backslash;
            case 43:
                return K::Comma;
            case 44:
                return K::Slash;
            case 45:
                return K::N;
            case 46:
                return K::M;
            case 47:
                return K::Period;
            case 48:
                return K::Tab;
            case 49:
                return K::Space;
            case 50:
                return K::Grave;
            case 51:
                return K::Backspace;
            case 53:
                return K::Escape;
            case 55:
                return K::Command;
            case 54:
                return K::RightCommand;
            case 56:
                return K::Shift;
            case 60:
                return K::RightShift;
            case 58:
                return K::Option;
            case 61:
                return K::RightOption;
            case 57:
                return K::CapsLock;
            case 59:
                return K::Control;
            case 62:
                return K::RightControl;
            case 115:
                return K::Home;
            case 116:
                return K::PageUp;
            case 117:
                return K::Delete;
            case 119:
                return K::End;
            case 121:
                return K::PageDown;
            case 123:
                return K::Left;
            case 124:
                return K::Right;
            case 125:
                return K::Down;
            case 126:
                return K::Up;
            case 122:
                return K::F1;
            case 120:
                return K::F2;
            case 99:
                return K::F3;
            case 118:
                return K::F4;
            case 96:
                return K::F5;
            case 97:
                return K::F6;
            case 98:
                return K::F7;
            case 100:
                return K::F8;
            case 101:
                return K::F9;
            case 109:
                return K::F10;
            case 103:
                return K::F11;
            case 111:
                return K::F12;
            case 82:
                return K::NumpadZero;
            case 83:
                return K::NumpadOne;
            case 84:
                return K::NumpadTwo;
            case 85:
                return K::NumpadThree;
            case 86:
                return K::NumpadFour;
            case 87:
                return K::NumpadFive;
            case 88:
                return K::NumpadSix;
            case 89:
                return K::NumpadSeven;
            case 91:
                return K::NumpadEight;
            case 92:
                return K::NumpadNine;
            case 65:
                return K::NumpadPeriod;
            case 75:
                return K::NumpadDivide;
            case 67:
                return K::NumpadMultiply;
            case 78:
                return K::NumpadMinus;
            case 69:
                return K::NumpadPlus;
            case 76:
                return K::NumpadEnter;
            case 81:
                return K::NumpadEqual;
            default:
                return K::Unknown;
        }
    }

    Platform::SpecialKey SpecialValue(Platform::KeyId key)
    {
        using K = Platform::KeyId;
        using S = Platform::SpecialKey;
        switch (key)
        {
            case K::Escape:
                return S::Escape;
            case K::Enter:
            case K::NumpadEnter:
                return S::Enter;
            case K::Tab:
                return S::Tab;
            case K::Backspace:
                return S::Backspace;
            case K::Delete:
                return S::Delete;
            case K::Right:
                return S::Right;
            case K::Left:
                return S::Left;
            case K::Up:
                return S::Up;
            case K::Down:
                return S::Down;
            case K::PageUp:
                return S::PageUp;
            case K::PageDown:
                return S::PageDown;
            case K::Home:
                return S::Home;
            case K::End:
                return S::End;
            default:
                return S::None;
        }
    }

    void UpdateForegroundApplication(NSString* bundle, pid_t pid)
    {
        const bool ghostty = [bundle isEqualToString:@"com.mitchellh.ghostty"];
        const bool blender = [bundle isEqualToString:@"org.blenderfoundation.blender"];
        if (blender)
        {
            blender_pid = static_cast<std::uint32_t>(pid);
        }
        const bool active = blender || ghostty || [bundle isEqualToString:@"dev.zed.Zed"] ||
                            [bundle isEqualToString:@"dev.zed.Zed-Preview"];
        const auto profile = blender   ? Platform::AppProfile::Blender
                             : ghostty ? Platform::AppProfile::Neovim
                                       : Platform::AppProfile::Zed;
        const auto previous = foreground_token.load(std::memory_order_relaxed);
        static pid_t previous_pid = -1;
        if (previous_pid != pid || active != ((previous & 1) != 0) ||
            (active && app_profile != profile))
        {
            previous_pid = pid;
            if (active)
            {
                app_profile = profile;
            }
            // Main-thread profile changes advance the token; queued callback records
            // retain this generation and cannot cross editor profiles.
            foreground_token.store(((previous >> 1) + 1) * 2 + (active ? 1 : 0),
                                   std::memory_order_release);
        }
    }

    void UpdateForeground()
    {
        NSRunningApplication* application = [[NSWorkspace sharedWorkspace] frontmostApplication];
        UpdateForegroundApplication([application bundleIdentifier],
                                    [application processIdentifier]);
    }

    void UpdateLayout()
    {
        TISInputSourceRef input = TISCopyCurrentKeyboardInputSource();
        auto id = input ? static_cast<CFStringRef>(
                              TISGetInputSourceProperty(input, kTISPropertyInputSourceID))
                        : nullptr;
        // Physical geometry/hint chords are initially US QWERTY. Do not silently guess an IME or
        // remapped layout.
        const bool supported = id && (CFEqual(id, CFSTR("com.apple.keylayout.US")) ||
                                      CFEqual(id, CFSTR("com.apple.keylayout.ABC")));
        if (input)
        {
            CFRelease(input);
        }
        if (supported != layout_supported)
        {
            layout_supported = supported;
            losses.fetch_add(1, std::memory_order_release);
        }
    }

    CGEventRef Observe(CGEventTapProxy, CGEventType type, CGEventRef event, void*)
    {
        if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput)
        {
            losses.fetch_add(1, std::memory_order_release);
            CGEventTapEnable(tap, true);
            Platform::WakeMainLoop();
            return event;
        }
        if (type == kCGEventMouseMoved || type == kCGEventLeftMouseDragged ||
            type == kCGEventRightMouseDragged || type == kCGEventOtherMouseDragged)
        {
            const auto point = CGEventGetLocation(event);
            const bool inside = point.x >= pointer_left.load(std::memory_order_relaxed) &&
                                point.x < pointer_right.load(std::memory_order_relaxed) &&
                                point.y >= pointer_top.load(std::memory_order_relaxed) &&
                                point.y < pointer_bottom.load(std::memory_order_relaxed);
            static bool previously_inside = false;
            if (inside || previously_inside)
            {
                Platform::WakeMainLoop();
            }
            previously_inside = inside;
            return event;
        }
        const auto token = foreground_token.load(std::memory_order_acquire);
        if (!capture_enabled.load(std::memory_order_relaxed))
        {
            return event;
        }
        if (!(token & 1) && type != kCGEventKeyDown && type != kCGEventKeyUp &&
            type != kCGEventFlagsChanged)
        {
            return event;
        }
        static std::uint64_t sequence = 0;
        RawEvent record{.type = type,
                        .flags = CGEventGetFlags(event),
                        .timestamp = CGEventGetTimestamp(event) * 1e-9,
                        .foregroundToken = token,
                        .lossGeneration = losses.load(std::memory_order_acquire),
                        .sequence = ++sequence};
        if (type == kCGEventKeyDown || type == kCGEventKeyUp || type == kCGEventFlagsChanged)
        {
            record.keyCode =
                static_cast<CGKeyCode>(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
            record.repeat = CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat) != 0;
            if (token & 1)
            {
                CGEventKeyboardGetUnicodeString(event, record.text.size(), &record.textLength,
                                                record.text.data());
            }
        }
        else
        {
            record.mousePressed = type == kCGEventLeftMouseDown || type == kCGEventRightMouseDown ||
                                  type == kCGEventOtherMouseDown;
            const auto button = CGEventGetIntegerValueField(event, kCGMouseEventButtonNumber);
            record.mouseButton = button == 0   ? Platform::MouseButton::Left
                                 : button == 1 ? Platform::MouseButton::Right
                                 : button == 2 ? Platform::MouseButton::Middle
                                               : Platform::MouseButton::Other;
        }
        if (!queue.push(record))
        {
            losses.fetch_add(1, std::memory_order_release);
        }
        input_sequence.store(record.sequence, std::memory_order_release);
        Platform::WakeMainLoop();
        return event; // Listen-only: return the identical OS event on every path.
    }

    void StopObserver()
    {
        function_held = false;
        capture_enabled.store(false, std::memory_order_release);
        running.store(false, std::memory_order_release);
        if (tap)
        {
            CGEventTapEnable(tap, false);
        }
        if (auto loop = observer_loop.load(std::memory_order_acquire))
        {
            CFRunLoopPerformBlock(loop, kCFRunLoopDefaultMode, ^{
                CFRunLoopStop(loop);
            });
            CFRunLoopWakeUp(loop);
        }
        if (observer_thread.joinable())
        {
            observer_thread.join();
        }
        if (auto loop = observer_loop.exchange(nullptr))
        {
            CFRelease(loop);
        }
        if (source)
        {
            CFRelease(source);
            source = nullptr;
        }
        if (tap)
        {
            CFMachPortInvalidate(tap);
            CFRelease(tap);
            tap = nullptr;
        }
    }

    void StartObserver()
    {
        if (preview || paused || !observation_allowed || secure_input)
        {
            return;
        }
        if (tap && CFMachPortIsValid(tap) && CGEventTapIsEnabled(tap) &&
            running.load(std::memory_order_acquire))
        {
            return;
        }
        const auto now = std::chrono::steady_clock::now();
        if (now < next_observer_attempt)
        {
            return;
        }
        next_observer_attempt = now + OBSERVER_RETRY_INTERVAL;
        if (tap)
        {
            StopObserver();
            losses.fetch_add(1, std::memory_order_release);
        }
        observer_failure = ObserverFailure::None;
        const CGEventMask mask =
            CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp) |
            CGEventMaskBit(kCGEventFlagsChanged) | CGEventMaskBit(kCGEventLeftMouseDown) |
            CGEventMaskBit(kCGEventRightMouseDown) | CGEventMaskBit(kCGEventOtherMouseDown) |
            CGEventMaskBit(kCGEventLeftMouseUp) | CGEventMaskBit(kCGEventRightMouseUp) |
            CGEventMaskBit(kCGEventOtherMouseUp) | CGEventMaskBit(kCGEventMouseMoved) |
            CGEventMaskBit(kCGEventLeftMouseDragged) | CGEventMaskBit(kCGEventRightMouseDragged) |
            CGEventMaskBit(kCGEventOtherMouseDragged);
        tap = CGEventTapCreate(kCGSessionEventTap, kCGTailAppendEventTap,
                               kCGEventTapOptionListenOnly, mask, Observe, nullptr);
        if (!tap)
        {
            observer_failure = ObserverFailure::TapCreation;
            return;
        }
        source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0);
        if (!source)
        {
            StopObserver();
            observer_failure = ObserverFailure::RunLoopSource;
            return;
        }
        // The first callback must tag its record with the new observation generation.
        losses.fetch_add(1, std::memory_order_release);
        running.store(true, std::memory_order_release);
        std::promise<void> ready;
        auto started = ready.get_future();
        observer_thread = std::thread(
            [ready = std::move(ready)]() mutable
            {
                @autoreleasepool
                {
                    auto loop = CFRunLoopGetCurrent();
                    CFRetain(loop);
                    observer_loop.store(loop, std::memory_order_release);
                    CFRunLoopAddSource(loop, source, kCFRunLoopDefaultMode);
                    CGEventTapEnable(tap, true);
                    ready.set_value();
                    while (running.load(std::memory_order_acquire))
                    {
                        if (CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0e10, true) ==
                            kCFRunLoopRunFinished)
                        {
                            running.store(false, std::memory_order_release);
                            losses.fetch_add(1, std::memory_order_release);
                            Platform::WakeMainLoop();
                            break;
                        }
                    }
                    CFRunLoopRemoveSource(loop, source, kCFRunLoopDefaultMode);
                }
            });
        started.wait();
    }

    void RefreshObserverHealth()
    {
        const bool allowed = CGPreflightListenEventAccess();
        if (allowed != observation_allowed)
        {
            observation_allowed = allowed;
            losses.fetch_add(1, std::memory_order_release);
            next_observer_attempt = {};
        }
        if (!observation_allowed)
        {
            if (tap)
            {
                StopObserver();
            }
            observer_failure = ObserverFailure::None;
            return;
        }
        StartObserver();
    }

    Platform::InputEvent Normalize(const RawEvent& record)
    {
        using namespace Platform;
        std::uint8_t modifiers = 0;
        if (record.flags & kCGEventFlagMaskShift)
        {
            modifiers |= SHIFT;
        }
        if (record.flags & kCGEventFlagMaskControl)
        {
            modifiers |= CONTROL;
        }
        if (record.flags & kCGEventFlagMaskAlternate)
        {
            modifiers |= OPTION;
        }
        if (record.flags & kCGEventFlagMaskCommand)
        {
            modifiers |= COMMAND;
        }
        const auto key = PhysicalKey(record.keyCode);
        const auto current_loss = losses.load(std::memory_order_acquire);
        if (function_loss_generation != current_loss)
        {
            function_held = false;
            function_loss_generation = current_loss;
        }
        // Navigation/F-key events can carry the Function flag without a physical Fn press.
        // Flag changes and printable keys report the actual layer state instead.
        if (record.lossGeneration == current_loss &&
            (record.type == kCGEventFlagsChanged || (key >= KeyId::Space && key <= KeyId::Grave)))
        {
            function_held = (record.flags & kCGEventFlagMaskSecondaryFn) != 0;
        }
        const bool editor_event = (record.foregroundToken & 1) != 0;
        KeyValue value{.special = editor_event ? SpecialValue(key) : SpecialKey::None};
        if (editor_event && value.special == SpecialKey::None && record.textLength)
        {
            value.character = record.text[0];
            if (record.textLength == 2 && record.text[0] >= 0xd800 && record.text[0] <= 0xdbff &&
                record.text[1] >= 0xdc00 && record.text[1] <= 0xdfff)
            {
                value.character =
                    0x10000 + ((record.text[0] - 0xd800) << 10) + (record.text[1] - 0xdc00);
            }
        }
        if (editor_event && (modifiers & (CONTROL | COMMAND)) && key >= KeyId::A && key <= KeyId::Z)
        {
            value.character = static_cast<char32_t>(key) + ((modifiers & SHIFT) ? 0 : 32);
        }
        if (editor_event && (modifiers & CONTROL) && key == KeyId::LeftBracket)
        {
            value.character = U'[';
        }
        Platform::EventType type = record.type == kCGEventKeyDown ? Platform::EventType::KeyDown
                                   : record.type == kCGEventKeyUp ? Platform::EventType::KeyUp
                                   : record.type == kCGEventFlagsChanged
                                       ? Platform::EventType::Modifiers
                                       : Platform::EventType::MouseAction;
        if (editor_event && type == Platform::EventType::KeyDown &&
            value.special == SpecialKey::None &&
            (!layout_supported ||
             (app_profile != AppProfile::Blender && !(modifiers & COMMAND) &&
              (!value.character || (record.textLength > 1 && value.character < 0x10000)))))
        {
            type = Platform::EventType::Unsupported;
        }
        return {.type = type,
                .key = key,
                .value = value,
                .modifiers = modifiers,
                .repeat = record.repeat,
                .capsLock = (record.flags & kCGEventFlagMaskAlphaShift) != 0,
                .functionHeld = function_held,
                .timestamp = record.timestamp,
                .sequence = record.sequence,
                .foregroundGeneration = record.foregroundToken >> 1,
                .lossGeneration = record.lossGeneration,
                .mouseButton = record.mouseButton,
                .mousePressed = record.mousePressed};
    }
} // namespace

namespace Platform
{
    void WakeMainLoop()
    {
        if (!main_wake_source || wake_pending.exchange(true, std::memory_order_acq_rel))
        {
            return;
        }
        CFRunLoopSourceSignal(main_wake_source);
        CFRunLoopWakeUp(CFRunLoopGetMain());
    }

    void UpdatePointerRegion(float left, float top, float width, float height)
    {
        pointer_left.store(left, std::memory_order_relaxed);
        pointer_top.store(top, std::memory_order_relaxed);
        pointer_right.store(left + width, std::memory_order_relaxed);
        pointer_bottom.store(top + height, std::memory_order_relaxed);
    }

    bool IsPreview()
    {
        return preview;
    }

    Observation ObservationState()
    {
        if (preview)
        {
            return Observation::NotNeeded;
        }
        if (!observation_allowed)
        {
            return Observation::NotGranted;
        }
        const bool observing = tap && CFMachPortIsValid(tap) && CGEventTapIsEnabled(tap) &&
                               running.load(std::memory_order_acquire);
        return observing ? Observation::Running : Observation::Failed;
    }

    bool IsPaused()
    {
        return paused;
    }

    void PrepareApplication(bool requested_preview)
    {
        preview = requested_preview;
        if (!preview)
        {
            [[NSApplication sharedApplication]
                setActivationPolicy:NSApplicationActivationPolicyAccessory];
        }
    }

    void Initialise()
    {
        CFRunLoopSourceContext context{};
        context.perform = [](void*)
        {
            glfwPostEmptyEvent();
        };
        main_wake_source = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &context);
        CFRunLoopAddSource(CFRunLoopGetMain(), main_wake_source, kCFRunLoopCommonModes);
        health_timer = [NSTimer timerWithTimeInterval:1.0
                                              repeats:YES
                                                block:^(NSTimer*) {
                                                    health_check_due = true;
                                                    WakeMainLoop();
                                                }];
        [health_timer setTolerance:0.2];
        [[NSRunLoop mainRunLoop] addTimer:health_timer forMode:NSRunLoopCommonModes];
        InitialiseMenu();
        UpdateForeground();
        UpdateLayout();
        workspace_token = [[[NSWorkspace sharedWorkspace] notificationCenter]
            addObserverForName:NSWorkspaceDidActivateApplicationNotification
                        object:nil
                         queue:[NSOperationQueue mainQueue]
                    usingBlock:^(NSNotification*) {
                        UpdateForeground();
                        WakeMainLoop();
                    }];
        if (!preview)
        {
            blender_bridge.initialise();
            RefreshObserverHealth();
        }
    }

    void Shutdown()
    {
        capture_enabled.store(false, std::memory_order_release);
        StopObserver();
        blender_bridge.shutdown();
        if (workspace_token)
        {
            [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:workspace_token];
        }
        workspace_token = nil;
        ShutdownMenu();
        ShutdownOverlayWindow();
        menu_events.clear();
        [health_timer invalidate];
        health_timer = nil;
        if (main_wake_source)
        {
            CFRunLoopRemoveSource(CFRunLoopGetMain(), main_wake_source, kCFRunLoopCommonModes);
            CFRunLoopSourceInvalidate(main_wake_source);
            CFRelease(main_wake_source);
            main_wake_source = nullptr;
        }
    }

    void RequestObservation()
    {
        if (!CGPreflightListenEventAccess())
        {
            CGRequestListenEventAccess();
        }
        next_observer_attempt = {};
        RefreshObserverHealth();
        health_check_due = true;
        WakeMainLoop();
    }

    void TogglePause()
    {
        paused = !paused;
        capture_enabled.store(false, std::memory_order_release);
        health_check_due = true;
        losses.fetch_add(1, std::memory_order_release);
        WakeMainLoop();
    }

    void TogglePreview()
    {
        preview = !preview;
        StopObserver();
        blender_bridge.shutdown();
        losses.fetch_add(1, std::memory_order_release);
        ConfigureOverlayWindow();
        if (!preview)
        {
            blender_bridge.initialise();
            UpdateForeground();
            next_observer_attempt = {};
            RefreshObserverHealth();
        }
    }

    void QueueBaseline(EventType type)
    {
        menu_events.push_back({.type = type});
    }

    void RequestQuit()
    {
        quitting = true;
    }

    InputBatchInfo ReadInputEvents(std::span<InputEvent> destination)
    {
        wake_pending.store(false, std::memory_order_release);
        const bool refresh_context = health_check_due || !queue.empty();
        const bool refresh_window_geometry = health_check_due;
        if (refresh_context)
        {
            UpdateForeground();
            UpdateLayout();
            const bool secure = IsSecureEventInputEnabled();
            if (secure != secure_input)
            {
                secure_input = secure;
                losses.fetch_add(1, std::memory_order_release);
            }
            if (health_check_due)
            {
                RefreshObserverHealth();
                reduce_motion =
                    [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldReduceMotion];
                health_check_due = false;
            }
        }
        InputBatchInfo status{};
        if (preview)
        {
            status = ReadPreviewEvents(
                destination,
                CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, kVK_Function));
        }
        else
        {
            const auto token = foreground_token.load(std::memory_order_acquire);
            status.foregroundGeneration = token >> 1;
            status.foreground = (token & 1) != 0;
            status.appProfile = app_profile;
            status.available = tap && CFMachPortIsValid(tap) && CGEventTapIsEnabled(tap) &&
                               running.load(std::memory_order_acquire) && observation_allowed &&
                               !secure_input && layout_supported;
            capture_enabled.store(!paused && status.available, std::memory_order_release);
            status.description =
                secure_input        ? "Secure Input active · observation unavailable"
                : !layout_supported ? "US / ABC layout required · observation unavailable"
                : !observation_allowed
                    ? "Input Monitoring is not granted to this build.\n"
                      "KeyGuide → Grant Input Monitoring…\n"
                      "Already enabled? Quit, remove and re-add this .app in System Settings."
                : paused ? "Observation paused"
                : observer_failure == ObserverFailure::TapCreation
                    ? "Input Monitoring allowed, but macOS could not create the passive observer.\n"
                      "Retrying every 5 seconds; KeyGuide → Retry Observation to retry now."
                : observer_failure == ObserverFailure::RunLoopSource
                    ? "Passive observer run-loop source could not be created.\n"
                      "Retrying every 5 seconds; KeyGuide → Retry Observation to retry now."
                : !status.available ? "Passive observer unavailable · retrying automatically"
                : !status.foreground
                    ? "PASSIVE · physical keys only · waiting for Zed, Ghostty or Blender"
                : app_profile == AppProfile::Blender
                    ? "PASSIVE · Blender default keymap / mesh viewport assumed · KeyGuide → Resync "
                      "Mode to resync"
                : app_profile == AppProfile::Neovim ? "PASSIVE · Ghostty / default Neovim · shell "
                                                      "vs editor is assumed · Esc twice to resync"
                                                    : "PASSIVE · Zed · Esc twice to resync";
            RawEvent record{};
            // Keep the budget bounded. Remaining records stay in FIFO order for the next frame.
            while (status.count < destination.size() && queue.pop(record))
            {
                processed_input_sequence = std::max(processed_input_sequence, record.sequence);
                destination[status.count++] = Normalize(record);
            }
            if (!queue.empty())
            {
                WakeMainLoop();
            }
            status.lossGeneration = losses.load(std::memory_order_acquire);
        }
        if (preview)
        {
            RawEvent discarded{};
            while (queue.pop(discarded))
            {
            }
        }
        status.macCommandAliases = true;
        status.paused = paused;
        status.quit = quitting;
        status.reduceMotion = reduce_motion;
        blender_bridge.update(
            blender_pid,
            !preview && !paused && status.foreground && status.available &&
                status.appProfile == AppProfile::Blender && S::readBlenderState,
            std::max(processed_input_sequence, input_sequence.load(std::memory_order_acquire)),
            refresh_context, refresh_window_geometry, status.foregroundGeneration,
            status.lossGeneration);
        status.blenderObservation = blender_bridge.read();
        status.blenderObservation.current &=
            queue.empty() &&
            status.blenderObservation.inputSequence ==
                std::max(processed_input_sequence, input_sequence.load(std::memory_order_acquire));
        if (status.foreground && status.available && !paused && !preview &&
            app_profile == AppProfile::Blender && status.blenderObservation.connected)
        {
            status.description = "PASSIVE · keyboard observation · Blender state bridge";
        }
        for (const auto& event : menu_events)
        {
            if (status.count < destination.size())
            {
                destination[status.count++] = event;
            }
        }
        menu_events.clear();
        return status;
    }
} // namespace Platform
