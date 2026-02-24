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

// Insert text via Accessibility API — directly sets text in the focused text field.
// This is the most reliable method on modern macOS (Sequoia+/Tahoe).
static bool insertTextViaAX(const QString &text)
{
    AXUIElementRef systemWide = AXUIElementCreateSystemWide();
    if (!systemWide) {
        fprintf(stderr, "[WARN] TextPaster: failed to create AXUIElementCreateSystemWide\n");
        fflush(stderr);
        return false;
    }

    AXUIElementRef focusedElement = NULL;
    AXError err = AXUIElementCopyAttributeValue(systemWide,
        kAXFocusedUIElementAttribute, (CFTypeRef *)&focusedElement);
    CFRelease(systemWide);

    if (err != kAXErrorSuccess || !focusedElement) {
        fprintf(stderr, "[WARN] TextPaster: AX can't get focused element (err=%d)\n", (int)err);
        fflush(stderr);
        return false;
    }

    // Set kAXSelectedTextAttribute — replaces current selection (or inserts at cursor if no selection)
    NSString *nsText = text.toNSString();
    err = AXUIElementSetAttributeValue(focusedElement,
        kAXSelectedTextAttribute, (__bridge CFStringRef)nsText);
    CFRelease(focusedElement);

    if (err != kAXErrorSuccess) {
        fprintf(stderr, "[WARN] TextPaster: AX set selected text failed (err=%d)\n", (int)err);
        fflush(stderr);
        return false;
    }

    fprintf(stderr, "[INFO] TextPaster: AX inserted %lld chars at cursor\n", (long long)text.size());
    fflush(stderr);
    return true;
}

// Fallback: inject text as Unicode keyboard events via CGEvent.
static bool typeTextViaCGEvent(const QString &text)
{
    if (text.isEmpty()) return true;

    CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
    if (!source) {
        fprintf(stderr, "[WARN] TextPaster::typeTextViaCGEvent: failed to create event source\n");
        fflush(stderr);
        return false;
    }

    const auto *rawUtf16 = reinterpret_cast<const UniChar *>(text.utf16());
    const size_t totalLen = static_cast<size_t>(text.size());
    std::vector<UniChar> utf16(rawUtf16, rawUtf16 + totalLen);

    const size_t kMaxChunk = 20;
    for (size_t i = 0; i < utf16.size(); i += kMaxChunk) {
        size_t chunkLen = std::min(kMaxChunk, utf16.size() - i);

        CGEventRef keyDown = CGEventCreateKeyboardEvent(source, 0, true);
        CGEventRef keyUp   = CGEventCreateKeyboardEvent(source, 0, false);
        if (!keyDown || !keyUp) {
            if (keyDown) CFRelease(keyDown);
            if (keyUp) CFRelease(keyUp);
            continue;
        }

        CGEventKeyboardSetUnicodeString(keyDown, chunkLen, &utf16[i]);
        CGEventKeyboardSetUnicodeString(keyUp,   chunkLen, &utf16[i]);

        CGEventPost(kCGSessionEventTap, keyDown);
        CGEventPost(kCGSessionEventTap, keyUp);

        CFRelease(keyDown);
        CFRelease(keyUp);

        usleep(5000);
    }

    CFRelease(source);
    fprintf(stderr, "[INFO] TextPaster::typeTextViaCGEvent: typed %lld chars in %lld chunks\n",
            (long long)utf16.size(), (long long)((utf16.size() + kMaxChunk - 1) / kMaxChunk));
    fflush(stderr);
    return true;
}

bool TextPaster::typeText(const QString &text)
{
    if (text.isEmpty()) return true;

    // Primary: AX API (most reliable, doesn't touch clipboard or require key injection)
    if (insertTextViaAX(text)) return true;

    // Fallback: CGEvent Unicode keyboard injection
    fprintf(stderr, "[INFO] TextPaster::typeText: AX failed, falling back to CGEvent injection\n");
    fflush(stderr);
    return typeTextViaCGEvent(text);
}

bool TextPaster::activateApp(qint64 pid, int timeoutMs)
{
    @autoreleasepool {
        NSRunningApplication *app = [NSRunningApplication runningApplicationWithProcessIdentifier:(pid_t)pid];
        if (!app) {
            fprintf(stderr, "[WARN] TextPaster: no running app for pid=%lld\n", (long long)pid);
            fflush(stderr);
            return false;
        }

        // Use modern activate API (macOS 14+); fall back to deprecated one on older systems.
        if ([app respondsToSelector:@selector(activateFromApplication:options:)]) {
            [app activateFromApplication:[NSRunningApplication currentApplication] options:0];
        } else {
            #pragma clang diagnostic push
            #pragma clang diagnostic ignored "-Wdeprecated-declarations"
            [app activateWithOptions:NSApplicationActivateIgnoringOtherApps];
            #pragma clang diagnostic pop
        }

        // Poll until the target app is frontmost or timeout
        const int pollIntervalUs = 10000; // 10ms
        int elapsed = 0;
        while (elapsed < timeoutMs * 1000) {
            usleep(pollIntervalUs);
            elapsed += pollIntervalUs;
            if (frontmostAppPid() == pid) {
                fprintf(stderr, "[INFO] TextPaster: target app pid=%lld is now frontmost (after %dms)\n",
                        (long long)pid, elapsed / 1000);
                fflush(stderr);
                return true;
            }
        }

        fprintf(stderr, "[WARN] TextPaster: timed out waiting for pid=%lld to become frontmost\n", (long long)pid);
        fflush(stderr);
        return false;
    }
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

        // 3. Always re-activate the target app to ensure focus + first responder
        if (targetPid > 0) {
            fprintf(stderr, "[INFO] TextPaster: re-activating target pid=%lld\n",
                    (long long)targetPid);
            fflush(stderr);
            activateApp(targetPid);
            usleep(100000); // 100ms settle for full first responder restoration
        }

        // 4. Always post Cmd+V to the session (frontmost app)
        bool posted = postCmdV(kCGSessionEventTap, 0);
        if (!posted) {
            fprintf(stderr, "[WARN] TextPaster: failed to create CG events for paste\n");
            fflush(stderr);
            return false;
        }

        fprintf(stderr, "[INFO] TextPaster: posted Cmd+V to frontmost app (pid=%lld)\n",
                (long long)frontmostAppPid());
        fflush(stderr);
        return trusted;
    }
}

bool TextPaster::typeAtCursor(const QString &text, qint64 targetPid)
{
    @autoreleasepool {
        const bool trusted = canSimulatePaste();
        fprintf(stderr, "[INFO] TextPaster::typeAtCursor: (trusted=%d, targetPid=%lld)\n",
                trusted ? 1 : 0, (long long)targetPid);
        fflush(stderr);

        if (!trusted) {
            fprintf(stderr, "[WARN] TextPaster::typeAtCursor: Accessibility not trusted\n");
            fflush(stderr);
            return false;
        }

        // Always re-activate target app to ensure it has focus + first responder
        if (targetPid > 0) {
            fprintf(stderr, "[INFO] TextPaster::typeAtCursor: activating target pid=%lld\n",
                    (long long)targetPid);
            fflush(stderr);
            activateApp(targetPid);
            usleep(100000); // 100ms settle for the app to fully regain first responder
        }

        return typeText(text);
    }
}
