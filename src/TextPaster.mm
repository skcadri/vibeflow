#include "TextPaster.h"

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ApplicationServices/ApplicationServices.h>

bool TextPaster::canSimulatePaste()
{
    return AXIsProcessTrusted();
}

qint64 TextPaster::frontmostAppPid()
{
    @autoreleasepool {
        NSRunningApplication *app = [[NSWorkspace sharedWorkspace] frontmostApplication];
        if (!app) return 0;
        return static_cast<qint64>(app.processIdentifier);
    }
}

static bool postCmdV(CGEventTapLocation tap, pid_t pid)
{
    constexpr CGKeyCode kVK_Command = 0x37;
    constexpr CGKeyCode kVK_V = 0x09;

    CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
    if (!source) {
        return false;
    }

    CGEventRef cmdDown = CGEventCreateKeyboardEvent(source, kVK_Command, true);
    CGEventRef vDown = CGEventCreateKeyboardEvent(source, kVK_V, true);
    CGEventSetFlags(vDown, kCGEventFlagMaskCommand);
    CGEventRef vUp = CGEventCreateKeyboardEvent(source, kVK_V, false);
    CGEventSetFlags(vUp, kCGEventFlagMaskCommand);
    CGEventRef cmdUp = CGEventCreateKeyboardEvent(source, kVK_Command, false);

    if (pid > 0) {
        CGEventPostToPid(pid, cmdDown);
        CGEventPostToPid(pid, vDown);
        CGEventPostToPid(pid, vUp);
        CGEventPostToPid(pid, cmdUp);
    } else {
        CGEventPost(tap, cmdDown);
        CGEventPost(tap, vDown);
        CGEventPost(tap, vUp);
        CGEventPost(tap, cmdUp);
    }

    CFRelease(cmdDown);
    CFRelease(vDown);
    CFRelease(vUp);
    CFRelease(cmdUp);
    CFRelease(source);
    return true;
}

bool TextPaster::paste(const QString &text)
{
    return pasteToPid(text, 0);
}

bool TextPaster::pasteToPid(const QString &text, qint64 targetPid)
{
    @autoreleasepool {
        // 1. Set clipboard
        NSPasteboard *pb = [NSPasteboard generalPasteboard];
        [pb clearContents];
        [pb setString:text.toNSString() forType:NSPasteboardTypeString];

        const bool trusted = canSimulatePaste();
        fprintf(stderr, "[INFO] TextPaster: attempting paste (trusted=%d, targetPid=%lld)\n",
                trusted ? 1 : 0, (long long)targetPid);
        if (!trusted) {
            fprintf(stderr, "[WARN] TextPaster: Accessibility not trusted; attempting best-effort key injection\n");
        }
        fflush(stderr);

        // 2. Small delay for clipboard to settle
        usleep(50000); // 50ms

        const qint64 currentFrontPid = frontmostAppPid();
        fprintf(stderr, "[INFO] TextPaster: current frontmost pid=%lld\n", (long long)currentFrontPid);
        fflush(stderr);

        // 3. If focus drifted away from the original target, post directly to target PID.
        bool posted = false;
        if (targetPid > 0 && currentFrontPid != targetPid) {
            posted = postCmdV(kCGSessionEventTap, static_cast<pid_t>(targetPid));
            if (!posted) {
                fprintf(stderr, "[WARN] TextPaster: direct pid paste failed for pid=%lld\n", (long long)targetPid);
                fflush(stderr);
                return false;
            }
            fprintf(stderr, "[INFO] TextPaster: posted Cmd+V directly to pid=%lld (focus drift detected)\n",
                    (long long)targetPid);
            fflush(stderr);
            return trusted;
        }

        // 4. Normal path: post to currently focused app.
        posted = postCmdV(kCGSessionEventTap, 0);
        if (!posted) {
            fprintf(stderr, "[WARN] TextPaster: failed to create CG events for paste\n");
            fflush(stderr);
            return false;
        }

        return trusted;
    }
}
