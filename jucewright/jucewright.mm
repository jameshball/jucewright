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
            }

            return reinterpret_cast<NSInteger (*) (id, SEL, NSPoint, NSInteger)> (originalWindowNumberAtPoint) (self, selector, point, below);
        }
    }

    // macOS may refuse to bring a launched app to the front while the user
    // works in another app. JUCE then dismisses popup menus, and hit-tests
    // mouse input against the real window stack, so input to a window behind
    // another app's finds no component. Automated sessions behave as if the
    // app were frontmost, without moving any window.
    void treatApplicationAsFrontmost()
    {
        if (originalWindowNumberAtPoint != nullptr)
            return;

        method_setImplementation (class_getInstanceMethod ([NSApplication class], @selector (isActive)),
                                  imp_implementationWithBlock (^BOOL (id) { return YES; }));

        auto* windowAtPoint = class_getClassMethod ([NSWindow class], @selector (windowNumberAtPoint:belowWindowWithWindowNumber:));
        originalWindowNumberAtPoint = method_setImplementation (windowAtPoint, reinterpret_cast<IMP> (frontmostOwnWindowNumberAtPoint));
    }
}
#endif
