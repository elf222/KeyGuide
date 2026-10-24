// platform/mac/overlay_menu.mm
#include "platform/mac/mac_bridge.hpp"
#include "menu/menu_model.hpp"
#import <Cocoa/Cocoa.h>

// Menu-bar apps have no Edit menu, so text fields would ignore Cmd-V/C/X/A/Z.
static BOOL HandleEditingShortcut(NSEvent* event)
{
    if (([event modifierFlags] & NSEventModifierFlagDeviceIndependentFlagsMask) !=
        NSEventModifierFlagCommand)
    {
        return NO;
    }
    NSString* key = [event charactersIgnoringModifiers];
    SEL action = [key isEqualToString:@"v"]   ? @selector(paste:)
                 : [key isEqualToString:@"c"] ? @selector(copy:)
                 : [key isEqualToString:@"x"] ? @selector(cut:)
                 : [key isEqualToString:@"a"] ? @selector(selectAll:)
                 : [key isEqualToString:@"z"] ? NSSelectorFromString(@"undo:")
                                              : nil;
    return action && [NSApp sendAction:action to:nil from:nil];
}

@interface KeyGuideTextField : NSTextField
@end
@implementation KeyGuideTextField
- (BOOL)performKeyEquivalent:(NSEvent*)event
{
    return HandleEditingShortcut(event) || [super performKeyEquivalent:event];
}
@end

@interface KeyGuideSecureTextField : NSSecureTextField
@end
@implementation KeyGuideSecureTextField
- (BOOL)performKeyEquivalent:(NSEvent*)event
{
    return HandleEditingShortcut(event) || [super performKeyEquivalent:event];
}
@end

@interface KeyGuideMenu : NSObject <NSMenuDelegate>
- (void)perform:(NSMenuItem*)item;
@end

namespace
{
    NSStatusItem* status_item = nil;
    KeyGuideMenu* controller = nil;

    NSString* ToNSString(std::string_view text)
    {
        return [[NSString alloc] initWithBytes:text.data()
                                        length:text.size()
                                      encoding:NSUTF8StringEncoding]
                   ?: @"";
    }

    NSAttributedString* StyledTitle(NSString* title, MenuStyle style)
    {
        NSFont* font = style == MenuStyle::Title
                           ? [NSFont menuFontOfSize:0]
                           : [NSFont menuFontOfSize:[NSFont smallSystemFontSize]];
        if (style == MenuStyle::Title)
        {
            font = [[NSFontManager sharedFontManager] convertFont:font toHaveTrait:NSBoldFontMask];
        }
        NSColor* color = style == MenuStyle::Title     ? [NSColor labelColor]
                         : style == MenuStyle::Warning ? [NSColor systemOrangeColor]
                                                       : [NSColor secondaryLabelColor];
        return [[NSAttributedString alloc]
            initWithString:title
                attributes:@{NSFontAttributeName : font, NSForegroundColorAttributeName : color}];
    }

    void Populate(NSMenu* menu, const std::vector<MenuItem>& items)
    {
        [menu setAutoenablesItems:NO];
        for (const auto& model : items)
        {
            if (model.separator)
            {
                [menu addItem:[NSMenuItem separatorItem]];
                continue;
            }
            NSString* title = ToNSString(model.title);
            NSString* key =
                model.keyEquivalent ? [NSString stringWithFormat:@"%c", model.keyEquivalent] : @"";
            NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title action:nil keyEquivalent:key];
            if (model.style != MenuStyle::Normal)
            {
                [item setAttributedTitle:StyledTitle(title, model.style)];
            }
            if (!model.children.empty())
            {
                NSMenu* child = [[NSMenu alloc] initWithTitle:title];
                Populate(child, model.children);
                [item setSubmenu:child];
            }
            else if (model.action != MenuAction::None)
            {
                [item setTarget:controller];
                [item setAction:@selector(perform:)];
                [item setTag:static_cast<NSInteger>(model.action)];
                [item setRepresentedObject:@[ @(model.argument), ToNSString(model.payload) ]];
            }
            [item setEnabled:model.enabled && (!model.children.empty() ||
                                               model.action != MenuAction::None)];
            [item setState:model.checked ? NSControlStateValueOn : NSControlStateValueOff];
            [menu addItem:item];
        }
    }

    NSAlert* MakeAlert(std::string_view title, std::string_view message)
    {
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:ToNSString(title)];
        [alert setInformativeText:ToNSString(message)];
        // Accessory (menu-bar) apps must activate before showing a modal window.
        [NSApp activateIgnoringOtherApps:YES];
        return alert;
    }
} // namespace

@implementation KeyGuideMenu

- (void)menuNeedsUpdate:(NSMenu*)menu
{
    [menu removeAllItems];
    Populate(menu, BuildMenu());
}

- (void)perform:(NSMenuItem*)item
{
    NSArray* parameters = [item representedObject];
    const int argument = [parameters count] > 0 ? [parameters[0] intValue] : 0;
    NSString* payload = [parameters count] > 1 ? parameters[1] : @"";
    PerformMenuAction(static_cast<MenuAction>([item tag]), argument,
                      std::string([payload UTF8String] ?: ""));
}

@end

namespace Platform
{
    void InitialiseMenu()
    {
        controller = [[KeyGuideMenu alloc] init];
        status_item =
            [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
        NSImage* icon = [NSImage imageWithSystemSymbolName:@"keyboard.badge.eye"
                                  accessibilityDescription:@"KeyGuide"]
                            ?: [NSImage imageWithSystemSymbolName:@"keyboard"
                                         accessibilityDescription:@"KeyGuide"];
        if (icon)
        {
            [icon setTemplate:YES];
            [[status_item button] setImage:icon];
        }
        else
        {
            [[status_item button] setTitle:@"KG"];
        }
        [[status_item button] setToolTip:@"KeyGuide · shortcut hints and AI tips"];
        NSMenu* menu = [[NSMenu alloc] initWithTitle:@"KeyGuide"];
        [menu setDelegate:controller];
        [status_item setMenu:menu];
    }

    void ShutdownMenu()
    {
        if (status_item)
        {
            [[NSStatusBar systemStatusBar] removeStatusItem:status_item];
        }
        status_item = nil;
        controller = nil;
    }

    std::optional<std::string> PromptText(std::string_view title, std::string_view message,
                                          std::string_view initial, bool secure)
    {
        NSAlert* alert = MakeAlert(title, message);
        [alert addButtonWithTitle:@"Save"];
        [alert addButtonWithTitle:@"Cancel"];
        const NSRect frame = NSMakeRect(0, 0, 360, 24);
        NSTextField* field = secure ? [[KeyGuideSecureTextField alloc] initWithFrame:frame]
                                    : [[KeyGuideTextField alloc] initWithFrame:frame];
        [field setStringValue:ToNSString(initial)];
        [alert setAccessoryView:field];
        [[alert window] setInitialFirstResponder:field];
        if ([alert runModal] != NSAlertFirstButtonReturn)
        {
            return std::nullopt;
        }
        return std::string([[field stringValue] UTF8String] ?: "");
    }

    void ShowMessage(std::string_view title, std::string_view message)
    {
        NSAlert* alert = MakeAlert(title, message);
        [alert addButtonWithTitle:@"OK"];
        [alert runModal];
    }

    bool OpenPath(const std::string& path)
    {
        if (path.empty())
        {
            return false;
        }
        return [[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:ToNSString(path)]];
    }

    bool RevealPath(const std::string& path)
    {
        NSString* standard = [ToNSString(path) stringByStandardizingPath];
        [[NSWorkspace sharedWorkspace]
            activateFileViewerSelectingURLs:@[ [NSURL fileURLWithPath:standard] ]];
        return true;
    }
} // namespace Platform
