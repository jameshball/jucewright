#include "jucewright.cpp"

#if JUCEWRIGHT_ENABLE_AUTOMATION
    #import <Cocoa/Cocoa.h>
    #include <objc/runtime.h>

namespace jucewright
{
    namespace
    {
        IMP originalWindowNumberAtPoint = nullptr;

        NSInteger frontmostOwnWindowNumberAtPoint (id self, SEL selector, NSPoint point, NSInteger below)
        {
            // As the real lookup does, take the frontmost window a click at the
            // point would reach, but among this app's windows only (panels too),
            // passing through click-through ones such as tooltips.
            if (below == 0)
            {
                for (NSNumber* number in [NSWindow windowNumbersWithOptions: 0])
                {
                    NSWindow* window = [NSApp windowWithWindowNumber: [number integerValue]];

                    if (window != nil && [window isVisible] && ! [window ignoresMouseEvents] && [window alphaValue] > 0
                        && NSPointInRect (point, [window frame]))
                        return [window windowNumber];
                }

                // An app opened in the background (open -g) never has its
                // windows put on screen, so take JUCE's own front-to-back
                // order of the windows it is showing.
                auto& desktop = juce::Desktop::getInstance();

                for (int index = desktop.getNumComponents(); --index >= 0;)
                {
                    auto* component = desktop.getComponent (index);
                    auto* peer = component != nullptr && component->isVisible() ? component->getPeer() : nullptr;
                    auto* view = peer != nullptr ? static_cast<NSView*> (peer->getNativeHandle()) : nil;
                    NSWindow* window = view != nil ? [view window] : nil;

                    if (window != nil && ! [window ignoresMouseEvents] && [window alphaValue] > 0 && NSPointInRect (point, [window frame]))
                        return [window windowNumber];
                }
            }

            return reinterpret_cast<NSInteger (*) (id, SEL, NSPoint, NSInteger)> (originalWindowNumberAtPoint) (self, selector, point, below);
        }

        // A background session's key window, by number so a closed window
        // leaves nothing dangling. Making a window key for real would bring
        // the app to the front, because it reports itself active.
        NSInteger virtualKeyWindow = 0;

        void keepKeyWindowVirtual()
        {
            method_setImplementation (class_getInstanceMethod ([NSWindow class], @selector (makeKeyWindow)),
                                      imp_implementationWithBlock (^(NSWindow* window) { virtualKeyWindow = [window windowNumber]; }));
            method_setImplementation (class_getInstanceMethod ([NSWindow class], @selector (makeKeyAndOrderFront:)),
                                      imp_implementationWithBlock (^(NSWindow* window, id) {
                                          virtualKeyWindow = [window windowNumber];
                                          [window orderFront: nil];
                                      }));
            method_setImplementation (class_getInstanceMethod ([NSWindow class], @selector (isKeyWindow)),
                                      imp_implementationWithBlock (^BOOL (NSWindow* window) { return virtualKeyWindow != 0 && [window windowNumber] == virtualKeyWindow; }));
            method_setImplementation (class_getInstanceMethod ([NSApplication class], @selector (keyWindow)),
                                      imp_implementationWithBlock (^NSWindow* (id) { return virtualKeyWindow != 0 ? [NSApp windowWithWindowNumber: virtualKeyWindow] : nil; }));
        }
    }

    // macOS may refuse to bring a launched app to the front while the user
    // works in another app. JUCE then dismisses popup menus, and hit-tests
    // mouse input against the real window stack, so input to a window behind
    // another app's finds no component. Automated sessions behave as if the
    // app were frontmost, without moving any window, and launch --background
    // keeps the user's app in front.
    void treatApplicationAsFrontmost()
    {
        if (originalWindowNumberAtPoint != nullptr)
            return;

        method_setImplementation (class_getInstanceMethod ([NSApplication class], @selector (isActive)),
                                  imp_implementationWithBlock (^BOOL (id) { return YES; }));

        auto* windowAtPoint = class_getClassMethod ([NSWindow class], @selector (windowNumberAtPoint:belowWindowWithWindowNumber:));
        originalWindowNumberAtPoint = method_setImplementation (windowAtPoint, reinterpret_cast<IMP> (frontmostOwnWindowNumberAtPoint));

        // A window keeps the size a test asks for, even taller than the
        // screen; macOS would otherwise shrink the native window under the
        // component, and input to the part left outside it would be lost.
        method_setImplementation (class_getInstanceMethod ([NSWindow class], @selector (constrainFrameRect:toScreen:)),
                                  imp_implementationWithBlock (^NSRect (NSWindow*, NSRect frame, NSScreen*) { return frame; }));

        if (juce::SystemStats::getEnvironmentVariable ("JUCEWRIGHT_BACKGROUND", {}).isNotEmpty())
            keepKeyWindowVirtual();
    }
}
#endif
