#include "HotkeyMonitor.h"

#include <QDebug>
#include <ApplicationServices/ApplicationServices.h>
#include <cstdio>

HotkeyMonitor::HotkeyMonitor(QObject *parent)
    : QObject(parent)
{
}

HotkeyMonitor::~HotkeyMonitor()
{
    stop();
}

bool HotkeyMonitor::start()
{
    if (m_running) return true;

    // Check accessibility permission
    if (!AXIsProcessTrusted()) {
        qWarning() << "HotkeyMonitor: Accessibility permission not granted";

        NSDictionary *options = @{(__bridge NSString *)kAXTrustedCheckOptionPrompt: @YES};
        AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)options);
        return false;
    }

    CGEventMask mask = CGEventMaskBit(kCGEventFlagsChanged) | CGEventMaskBit(kCGEventKeyDown);

    m_tap = CGEventTapCreate(
        kCGSessionEventTap,
        kCGHeadInsertEventTap,
        kCGEventTapOptionListenOnly,  // Passive listener — doesn't need to intercept
        mask,
        eventCallback,
        this
    );

    if (!m_tap) {
        fprintf(stderr, "[ERROR] HotkeyMonitor: failed to create event tap! Check Accessibility permissions.\n");
        fflush(stderr);
        return false;
    }

    // Add to the MAIN run loop — this is critical.
    // CGEventTap callbacks must run on a run loop that receives system events.
    m_runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, m_tap, 0);
    CFRunLoopAddSource(CFRunLoopGetMain(), m_runLoopSource, kCFRunLoopCommonModes);
    CGEventTapEnable(m_tap, true);

    m_running = true;

    fprintf(stderr, "[INFO] HotkeyMonitor: event tap installed on main run loop\n");
    fflush(stderr);

    qInfo() << "HotkeyMonitor: started";
    return true;
}

void HotkeyMonitor::stop()
{
    if (!m_running) return;
    m_running = false;

    if (m_runLoopSource) {
        CFRunLoopRemoveSource(CFRunLoopGetMain(), m_runLoopSource, kCFRunLoopCommonModes);
        CFRelease(m_runLoopSource);
        m_runLoopSource = nullptr;
    }

    if (m_tap) {
        CGEventTapEnable(m_tap, false);
        CFMachPortInvalidate(m_tap);
        CFRelease(m_tap);
        m_tap = nullptr;
    }
}

CGEventRef HotkeyMonitor::eventCallback(CGEventTapProxy proxy, CGEventType type,
                                          CGEventRef event, void *userInfo)
{
    (void)proxy;

    auto *self = static_cast<HotkeyMonitor *>(userInfo);

    // Re-enable tap if it gets disabled by the system
    if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
        fprintf(stderr, "[WARN] HotkeyMonitor: event tap disabled by system, re-enabling\n");
        fflush(stderr);
        if (self->m_tap) {
            CGEventTapEnable(self->m_tap, true);
        }
        return event;
    }

    CGEventFlags flags = CGEventGetFlags(event);
    bool cmdHeld = (flags & kCGEventFlagMaskCommand) != 0;
    bool ctrlHeld = (flags & kCGEventFlagMaskControl) != 0;

    if (type == kCGEventFlagsChanged) {
        bool bothHeld = cmdHeld && ctrlHeld;

        if (bothHeld && !self->m_active) {
            self->m_active = true;
            fprintf(stderr, "[INFO] HotkeyMonitor: ACTIVATED (flags=0x%llx)\n",
                    (unsigned long long)flags);
            fflush(stderr);
            emit self->activated();
        } else if (!bothHeld && self->m_active) {
            self->m_active = false;
            fprintf(stderr, "[INFO] HotkeyMonitor: DEACTIVATED (flags=0x%llx)\n",
                    (unsigned long long)flags);
            fflush(stderr);
            emit self->deactivated();
        }
    }

    if (type == kCGEventKeyDown && self->m_active) {
        CGKeyCode keycode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
        if (keycode == 53) { // Escape
            self->m_active = false;
            fprintf(stderr, "[INFO] HotkeyMonitor: CANCELLED (Escape)\n");
            fflush(stderr);
            emit self->cancelled();
        }
    }

    return event;
}
