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

// ---------------------------------------------------------------------------
// Helpers (file-scoped)
// ---------------------------------------------------------------------------

// Poll until the target app is frontmost, or timeout.
static bool waitForAppFrontmost(qint64 targetPid, int timeoutMs)
{
    const int pollIntervalUs = 10000; // 10ms
    const int maxPolls = (timeoutMs * 1000) / pollIntervalUs;

    for (int i = 0; i < maxPolls; ++i) {
        @autoreleasepool {
            NSRunningApplication *front = [[NSWorkspace sharedWorkspace] frontmostApplication];
            if (front && front.processIdentifier == (pid_t)targetPid) {
                fprintf(stderr, "[INFO] TextPaster: app pid=%lld confirmed frontmost after ~%dms\n",
                        (long long)targetPid, i * 10);
                fflush(stderr);
                return true;
            }
        }
        usleep(pollIntervalUs);
    }
    return false;
}

// Insert text via Accessibility API — directly sets text in the focused text field.
// Tries PID-targeted lookup first, then falls back to system-wide.
static bool insertTextViaAX(const QString &text, qint64 targetPid)
{
    AXUIElementRef focusedElement = NULL;
    AXError err = kAXErrorFailure;

    // Strategy 1: Get focused element from the specific target app (avoids focus race)
    if (targetPid > 0) {
        AXUIElementRef appElement = AXUIElementCreateApplication((pid_t)targetPid);
        if (appElement) {
            err = AXUIElementCopyAttributeValue(appElement,
                kAXFocusedUIElementAttribute, (CFTypeRef *)&focusedElement);
            CFRelease(appElement);

            if (err == kAXErrorSuccess && focusedElement) {
                fprintf(stderr, "[INFO] TextPaster: got focused element from app pid=%lld\n",
                        (long long)targetPid);
                fflush(stderr);
            } else {
                fprintf(stderr, "[WARN] TextPaster: app-specific AX failed (err=%d), trying system-wide\n",
                        (int)err);
                fflush(stderr);
                focusedElement = NULL;
            }
        }
    }

    // Strategy 2: Fall back to system-wide focused element
    if (!focusedElement) {
        AXUIElementRef systemWide = AXUIElementCreateSystemWide();
        if (!systemWide) {
            fprintf(stderr, "[WARN] TextPaster: failed to create AXUIElementCreateSystemWide\n");
            fflush(stderr);
            return false;
        }

        err = AXUIElementCopyAttributeValue(systemWide,
            kAXFocusedUIElementAttribute, (CFTypeRef *)&focusedElement);
        CFRelease(systemWide);

        if (err != kAXErrorSuccess || !focusedElement) {
            fprintf(stderr, "[WARN] TextPaster: AX can't get focused element (err=%d)\n", (int)err);
            fflush(stderr);
            return false;
        }
    }

    // Check if the focused element actually accepts text input
    Boolean settable = false;
    AXError settableErr = AXUIElementIsAttributeSettable(focusedElement,
        kAXSelectedTextAttribute, &settable);
    if (settableErr != kAXErrorSuccess || !settable) {
        fprintf(stderr, "[WARN] TextPaster: focused element does not accept text "
                "(settable=%d, err=%d)\n", (int)settable, (int)settableErr);
        fflush(stderr);
        CFRelease(focusedElement);
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

        CGEventPost(kCGHIDEventTap, keyDown);
        CGEventPost(kCGHIDEventTap, keyUp);

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

// Simulate Cmd+V via CGEvent (posted to HID tap — hardware-level, trusted by apps).
static bool pasteCmdV_CGEvent()
{
    constexpr CGKeyCode kVK_V = 0x09;
    CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
    if (!source) {
        fprintf(stderr, "[WARN] TextPaster: failed to create event source for paste\n");
        fflush(stderr);
        return false;
    }

    CGEventRef vDown = CGEventCreateKeyboardEvent(source, kVK_V, true);
    CGEventSetFlags(vDown, kCGEventFlagMaskCommand);
    CGEventRef vUp = CGEventCreateKeyboardEvent(source, kVK_V, false);
    CGEventSetFlags(vUp, kCGEventFlagMaskCommand);

    CGEventPost(kCGHIDEventTap, vDown);
    CGEventPost(kCGHIDEventTap, vUp);

    CFRelease(vDown);
    CFRelease(vUp);
    CFRelease(source);

    fprintf(stderr, "[INFO] TextPaster: posted Cmd+V via CGEvent\n");
    fflush(stderr);
    return true;
}

// Set clipboard and paste via Cmd+V.
static bool pasteViaClipboard(const QString &text)
{
    @autoreleasepool {
        NSPasteboard *pb = [NSPasteboard generalPasteboard];
        [pb clearContents];
        [pb setString:text.toNSString() forType:NSPasteboardTypeString];

        fprintf(stderr, "[INFO] TextPaster: clipboard set (%lld chars)\n",
                (long long)text.size());
        fflush(stderr);

        return pasteCmdV_CGEvent();
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool TextPaster::typeText(const QString &text)
{
    if (text.isEmpty()) return true;

    // Primary: AX API (most reliable, doesn't touch clipboard or require key injection)
    if (insertTextViaAX(text, 0)) return true;

    // Fallback: CGEvent Unicode keyboard injection
    fprintf(stderr, "[INFO] TextPaster::typeText: AX failed, falling back to CGEvent injection\n");
    fflush(stderr);
    return typeTextViaCGEvent(text);
}

bool TextPaster::activateApp(qint64 pid, int timeoutMs)
{
    @autoreleasepool {
        NSRunningApplication *app = [NSRunningApplication
            runningApplicationWithProcessIdentifier:(pid_t)pid];
        if (!app) {
            fprintf(stderr, "[WARN] TextPaster: no running app for pid=%lld\n", (long long)pid);
            fflush(stderr);
            return false;
        }

        BOOL activateResult = NO;

        // macOS 14+: use modern cooperative activation API
        if (@available(macOS 14.0, *)) {
            [[NSApplication sharedApplication] yieldActivationToApplication:app];
            activateResult = [app activateFromApplication:[NSRunningApplication currentApplication]
                                                  options:NSApplicationActivateAllWindows];
            fprintf(stderr, "[INFO] TextPaster: activate (modern API) pid=%lld result=%d\n",
                    (long long)pid, (int)activateResult);
            fflush(stderr);
        } else {
            // Fallback for macOS 13 and earlier
            #pragma clang diagnostic push
            #pragma clang diagnostic ignored "-Wdeprecated-declarations"
            activateResult = [app activateWithOptions:NSApplicationActivateAllWindows];
            #pragma clang diagnostic pop
            fprintf(stderr, "[INFO] TextPaster: activate (legacy API) pid=%lld result=%d\n",
                    (long long)pid, (int)activateResult);
            fflush(stderr);
        }

        if (!activateResult) {
            fprintf(stderr, "[WARN] TextPaster: activation request failed for pid=%lld\n",
                    (long long)pid);
            fflush(stderr);
            return false;
        }

        // Poll to verify the target app actually became frontmost
        bool verified = waitForAppFrontmost(pid, timeoutMs);
        if (!verified) {
            fprintf(stderr, "[WARN] TextPaster: app pid=%lld did not become frontmost within %dms\n",
                    (long long)pid, timeoutMs);
            fflush(stderr);
        }
        return verified;
    }
}

bool TextPaster::paste(const QString &text)
{
    return pasteToPid(text, 0);
}

bool TextPaster::pasteToPid(const QString &text, qint64 targetPid)
{
    @autoreleasepool {
        if (targetPid > 0) {
            activateApp(targetPid);
            // activateApp now includes polling verification, no separate usleep needed
        }
        return pasteViaClipboard(text);
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

        // Step 1: Activate the target app and verify it's frontmost
        if (targetPid > 0) {
            fprintf(stderr, "[INFO] TextPaster::typeAtCursor: activating target pid=%lld\n",
                    (long long)targetPid);
            fflush(stderr);
            if (!activateApp(targetPid)) {
                fprintf(stderr, "[WARN] TextPaster::typeAtCursor: activation failed, "
                        "falling back to paste\n");
                fflush(stderr);
                return pasteViaClipboard(text);
            }
        }

        // Step 2: Try AX insertion (most reliable on Tahoe — pure AX, no CGEvent)
        if (insertTextViaAX(text, targetPid)) {
            return true;
        }

        // Step 3: Fallback to clipboard paste (AppleScript Cmd+V)
        fprintf(stderr, "[INFO] TextPaster::typeAtCursor: AX insertion failed, "
                "falling back to paste\n");
        fflush(stderr);
        return pasteViaClipboard(text);
    }
}

void TextPaster::logDiagnostics()
{
    @autoreleasepool {
        fprintf(stderr, "[DIAG] TextPaster diagnostics:\n");

        // Prompt for Accessibility permission if not granted yet.
        // AXIsProcessTrustedWithOptions with kAXTrustedCheckOptionPrompt opens
        // the System Settings dialog on first launch.
        NSDictionary *opts = @{(__bridge NSString *)kAXTrustedCheckOptionPrompt: @YES};
        bool trusted = AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)opts);
        fprintf(stderr, "[DIAG]   AXIsProcessTrusted = %d\n", trusted);

        CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
        fprintf(stderr, "[DIAG]   CGEventSource creation = %s\n",
                source ? "OK" : "FAILED");
        if (source) CFRelease(source);

        NSOperatingSystemVersion v = [[NSProcessInfo processInfo] operatingSystemVersion];
        fprintf(stderr, "[DIAG]   macOS version = %ld.%ld.%ld\n",
                (long)v.majorVersion, (long)v.minorVersion, (long)v.patchVersion);
        fflush(stderr);
    }
}
