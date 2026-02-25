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

bool TextPaster::activateApp(qint64 pid, int /*timeoutMs*/)
{
    @autoreleasepool {
        NSRunningApplication *app = [NSRunningApplication runningApplicationWithProcessIdentifier:(pid_t)pid];
        if (!app) {
            fprintf(stderr, "[WARN] TextPaster: no running app for pid=%lld\n", (long long)pid);
            fflush(stderr);
            return false;
        }

        #pragma clang diagnostic push
        #pragma clang diagnostic ignored "-Wdeprecated-declarations"
        BOOL ok = [app activateWithOptions:NSApplicationActivateAllWindows];
        #pragma clang diagnostic pop

        fprintf(stderr, "[INFO] TextPaster: activated pid=%lld result=%d\n",
                (long long)pid, (int)ok);
        fflush(stderr);
        return ok;
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

        fprintf(stderr, "[INFO] TextPaster: clipboard set (%lld chars), targetPid=%lld\n",
                (long long)text.size(), (long long)targetPid);
        fflush(stderr);

        // 2. Re-activate the target app to ensure focus + first responder
        if (targetPid > 0) {
            activateApp(targetPid);
            usleep(80000); // 80ms settle
        }

        // 3. Simple Cmd+V: keyDown + keyUp with command flag, posted to HID tap
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

        fprintf(stderr, "[INFO] TextPaster: posted Cmd+V to kCGHIDEventTap\n");
        fflush(stderr);
        return true;
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

        // Re-activate target app to ensure it has focus + first responder
        if (targetPid > 0) {
            fprintf(stderr, "[INFO] TextPaster::typeAtCursor: activating target pid=%lld\n",
                    (long long)targetPid);
            fflush(stderr);
            activateApp(targetPid);
            usleep(80000); // 80ms settle
        }

        if (!typeText(text)) {
            fprintf(stderr, "[INFO] TextPaster::typeAtCursor: type failed, falling back to paste\n");
            fflush(stderr);
            return pasteToPid(text, targetPid);
        }
        return true;
    }
}
