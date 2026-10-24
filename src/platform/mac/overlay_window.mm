// platform/mac/overlay_window.mm
#include "platform/mac/mac_bridge.hpp"
#include <algorithm>
#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>
#include "raylib.h"
#include "platform/preview_input.hpp"
#include "settings/settings.hpp"

namespace
{
    bool window_redraw_requested = true;
    NSMutableArray* redraw_tokens = nil;

    NSWindow* OverlayWindow()
    {
        return (__bridge NSWindow*)GetWindowHandle();
    }

    using FocusMethod = BOOL (*)(id, SEL);
    __weak NSWindow* focus_window = nil;
    FocusMethod original_key_method = nullptr, original_main_method = nullptr;

    BOOL CanBecomeKey(id window, SEL selector)
    {
        if (window == focus_window)
        {
            return Platform::IsPreview() ? YES : NO;
        }
        const auto original =
            selector == @selector(canBecomeKeyWindow) ? original_key_method : original_main_method;
        return original ? original(window, selector) : NO;
    }

    void InstallFocusPolicy(NSWindow* window)
    {
        focus_window = window;
        static bool installed = false;
        if (installed)
        {
            return;
        }
        // Preserve AppKit/KVO's window class identity; replacing the object's class breaks observer
        // cleanup.
        Class window_class = [window class];
        const auto key_method =
            class_getInstanceMethod(window_class, @selector(canBecomeKeyWindow));
        const auto main_method =
            class_getInstanceMethod(window_class, @selector(canBecomeMainWindow));
        original_key_method = reinterpret_cast<FocusMethod>(method_getImplementation(key_method));
        original_main_method = reinterpret_cast<FocusMethod>(method_getImplementation(main_method));
        class_replaceMethod(window_class, @selector(canBecomeKeyWindow), (IMP)CanBecomeKey,
                            method_getTypeEncoding(key_method));
        class_replaceMethod(window_class, @selector(canBecomeMainWindow), (IMP)CanBecomeKey,
                            method_getTypeEncoding(main_method));
        installed = true;
    }

    void ApplyVisibility(NSWindow* window)
    {
        if (!S::windowVisible)
        {
            [window orderOut:nil];
        }
        else if (Platform::IsPreview())
        {
            [window makeKeyAndOrderFront:nil];
        }
        else
        {
            [window orderFrontRegardless];
        }
    }

    void PlaceWindow(NSWindow* window, NSPoint origin, NSScreen* screen)
    {
        if (!window || !screen)
        {
            return;
        }
        const auto visible = [screen visibleFrame];
        const auto size = [window frame].size;
        origin.x = std::clamp(origin.x, NSMinX(visible),
                              std::max(NSMinX(visible), NSMaxX(visible) - size.width));
        origin.y = std::clamp(origin.y, NSMinY(visible),
                              std::max(NSMinY(visible), NSMaxY(visible) - size.height));
        [window setFrameOrigin:origin];
    }

    void PlaceAtPosition(NSWindow* window, NSScreen* screen, Default::WindowPosition position)
    {
        if (!window || !screen)
        {
            return;
        }
        using enum Default::WindowPosition;
        const auto visible = [screen visibleFrame];
        const auto size = [window frame].size;
        CGFloat x = NSMidX(visible) - size.width / 2, y = NSMidY(visible) - size.height / 2;
        if (position == TopLeft || position == BottomLeft)
        {
            x = NSMinX(visible) + Default::WINDOW_MARGIN.x;
        }
        if (position == TopRight || position == BottomRight)
        {
            x = NSMaxX(visible) - size.width - Default::WINDOW_MARGIN.x;
        }
        if (position == TopLeft || position == TopRight || position == TopCenter)
        {
            y = NSMaxY(visible) - size.height - Default::WINDOW_MARGIN.y;
        }
        if (position == BottomLeft || position == BottomRight || position == BottomCenter)
        {
            y = NSMinY(visible) + Default::WINDOW_MARGIN.y;
        }
        PlaceWindow(window, NSMakePoint(x, y), screen);
    }
} // namespace

namespace Platform
{
    void PumpEvents(bool wait)
    {
        if (wait)
        {
            EnableEventWaiting();
        }
        PollInputEvents();
        DisableEventWaiting();
    }

    bool TakeWindowRedrawRequest()
    {
        const bool requested = window_redraw_requested;
        window_redraw_requested = false;
        return requested;
    }

    void ShutdownOverlayWindow()
    {
        for (id token in redraw_tokens)
        {
            [[NSNotificationCenter defaultCenter] removeObserver:token];
        }
        redraw_tokens = nil;
        UpdatePointerRegion(0, 0, 0, 0);
    }

    void ConfigureOverlayWindow()
    {
        NSWindow* window = OverlayWindow();
        if (!window)
        {
            return;
        }
        if (!redraw_tokens)
        {
            redraw_tokens = [NSMutableArray array];
            for (NSString* name in @[
                     NSWindowDidResizeNotification, NSWindowDidExposeNotification,
                     NSWindowDidChangeOcclusionStateNotification,
                     NSWindowDidChangeBackingPropertiesNotification
                 ])
            {
                id token = [[NSNotificationCenter defaultCenter]
                    addObserverForName:name
                                object:window
                                 queue:nil
                            usingBlock:^(NSNotification*) {
                                window_redraw_requested = true;
                                WakeMainLoop();
                            }];
                [redraw_tokens addObject:token];
            }
        }
        // raylib 6's desktop GetWindowHandle returns NSWindow directly, not GLFWwindow.
        InstallFocusPolicy(window);
        [NSApp setActivationPolicy:IsPreview() ? NSApplicationActivationPolicyRegular
                                               : NSApplicationActivationPolicyAccessory];
        [window setIgnoresMouseEvents:!IsPreview()];
        [window setLevel:IsPreview() ? NSNormalWindowLevel : NSFloatingWindowLevel];
        [window setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces |
                                      NSWindowCollectionBehaviorFullScreenAuxiliary];
        [window setHasShadow:YES];
        [window setHidesOnDeactivate:NO];
        [window setOpaque:NO];
        [window setBackgroundColor:[NSColor clearColor]];
        if (IsPreview())
        {
            [window setStyleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                 NSWindowStyleMaskResizable];
        }
        else
        {
            [window setStyleMask:NSWindowStyleMaskBorderless];
            MoveOverlay(S::windowPosition);
        }
        ApplyVisibility(window);
        window_redraw_requested = true;
    }

    void ToggleWindowVisibility()
    {
        S::windowVisible = !S::windowVisible;
        ApplyVisibility(OverlayWindow());
    }

    void ResizeOverlayWindow(int width, int height)
    {
        NSWindow* window = OverlayWindow();
        const auto old_frame = [window frame];
        SetWindowSize(width, height);
        if (!window)
        {
            return;
        }
        const auto size = [window frame].size;
        auto origin = old_frame.origin;
        using enum Default::WindowPosition;
        if (!IsPreview())
        {
            if (S::windowPosition == TopRight || S::windowPosition == BottomRight)
            {
                origin.x += old_frame.size.width - size.width;
            }
            else if (S::windowPosition != TopLeft && S::windowPosition != BottomLeft)
            {
                origin.x += (old_frame.size.width - size.width) / 2;
            }
        }
        if (IsPreview() || S::windowPosition == TopLeft || S::windowPosition == TopRight ||
            S::windowPosition == TopCenter)
        {
            origin.y += old_frame.size.height - size.height;
        }
        else if (S::windowPosition == Center)
        {
            origin.y += (old_frame.size.height - size.height) / 2;
        }
        PlaceWindow(window, origin, [window screen] ?: [NSScreen mainScreen]);
    }

    void MoveOverlay(Default::WindowPosition position)
    {
        NSWindow* window = OverlayWindow();
        S::windowPosition = position;
        PlaceAtPosition(window, [window screen] ?: [NSScreen mainScreen], position);
    }

    void NudgeOverlay(float x, float y)
    {
        NSWindow* window = OverlayWindow();
        if (!window)
        {
            return;
        }
        auto origin = [window frame].origin;
        origin.x += x;
        origin.y -=
            y; // Public directions use right/down, independent of Cocoa's bottom-left origin.
        PlaceWindow(window, origin, [window screen] ?: [NSScreen mainScreen]);
    }

    void MoveOverlayToNextDisplay()
    {
        NSWindow* window = OverlayWindow();
        NSArray<NSScreen*>* screens = [NSScreen screens];
        if (!window || [screens count] < 2)
        {
            return;
        }
        const NSUInteger index = [screens indexOfObject:[window screen]];
        const NSUInteger next = index == NSNotFound ? 0 : (index + 1) % [screens count];
        PlaceAtPosition(window, [screens objectAtIndex:next], S::windowPosition);
    }

    PointerPosition ReadPointerPosition()
    {
        if (!S::windowVisible)
        {
            UpdatePointerRegion(0, 0, 0, 0);
            return {};
        }
        if (IsPreview())
        {
            UpdatePointerRegion(0, 0, 0, 0);
            return ReadPreviewPointer();
        }
        NSWindow* window = OverlayWindow();
        if (!window || ![window isVisible])
        {
            UpdatePointerRegion(0, 0, 0, 0);
            return {};
        }
        const NSPoint point = [window convertPointFromScreen:[NSEvent mouseLocation]];
        NSView* view = [window contentView];
        const auto local = [view convertPoint:point fromView:nil];
        const auto bounds = [view bounds];
        const auto screen_bounds = [window convertRectToScreen:[view convertRect:bounds
                                                                          toView:nil]];
        const auto primary_screen = [[[NSScreen screens] firstObject] frame];
        UpdatePointerRegion(static_cast<float>(screen_bounds.origin.x),
                            static_cast<float>(NSMaxY(primary_screen) - NSMaxY(screen_bounds)),
                            static_cast<float>(screen_bounds.size.width),
                            static_cast<float>(screen_bounds.size.height));
        return {static_cast<float>(local.x), static_cast<float>(bounds.size.height - local.y),
                NSPointInRect(local, bounds) == YES};
    }
} // namespace Platform
