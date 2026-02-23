#include "TextPaster.h"

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

void TextPaster::paste(const QString &text)
{
    @autoreleasepool {
        // 1. Set clipboard
        NSPasteboard *pb = [NSPasteboard generalPasteboard];
        [pb clearContents];
        [pb setString:text.toNSString() forType:NSPasteboardTypeString];

        // 2. Small delay for clipboard to settle
        usleep(50000); // 50ms

        // 3. Simulate Cmd+V
        CGEventRef keyDown = CGEventCreateKeyboardEvent(nullptr, 9, true);  // 'v' = keycode 9
        CGEventSetFlags(keyDown, kCGEventFlagMaskCommand);
        CGEventRef keyUp = CGEventCreateKeyboardEvent(nullptr, 9, false);
        CGEventSetFlags(keyUp, kCGEventFlagMaskCommand);

        CGEventPost(kCGAnnotatedSessionEventTap, keyDown);
        CGEventPost(kCGAnnotatedSessionEventTap, keyUp);

        CFRelease(keyDown);
        CFRelease(keyUp);
    }
}
