#include "HotkeyMonitor.h"

#include <QThread>
#include <QDebug>
#include <ApplicationServices/ApplicationServices.h>

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

        // Prompt the user
        NSDictionary *options = @{(__bridge NSString *)kAXTrustedCheckOptionPrompt: @YES};
        AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)options);
        return false;
    }

    m_running = true;

    m_thread = QThread::create([this]() {
        runLoop();
    });
    m_thread->start();

    qInfo() << "HotkeyMonitor: started";
    return true;
}

void HotkeyMonitor::stop()
{
    m_running = false;

    if (m_runLoop) {
        CFRunLoopStop(m_runLoop);
    }

    if (m_thread) {
        m_thread->quit();
        m_thread->wait(2000);
        delete m_thread;
        m_thread = nullptr;
    }

    if (m_tap) {
        CFMachPortInvalidate(m_tap);
        CFRelease(m_tap);
        m_tap = nullptr;
    }

    if (m_runLoopSource) {
        CFRelease(m_runLoopSource);
        m_runLoopSource = nullptr;
    }

    m_runLoop = nullptr;
}

CGEventRef HotkeyMonitor::eventCallback(CGEventTapProxy proxy, CGEventType type,
                                          CGEventRef event, void *userInfo)
{
    (void)proxy;

    auto *self = static_cast<HotkeyMonitor *>(userInfo);

    // Re-enable tap if it gets disabled
    if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
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
            QMetaObject::invokeMethod(self, "activated", Qt::QueuedConnection);
        } else if (!bothHeld && self->m_active) {
            self->m_active = false;
            QMetaObject::invokeMethod(self, "deactivated", Qt::QueuedConnection);
        }
    }

    if (type == kCGEventKeyDown && self->m_active) {
        CGKeyCode keycode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
        if (keycode == 53) { // Escape
            self->m_active = false;
            QMetaObject::invokeMethod(self, "cancelled", Qt::QueuedConnection);
            return nullptr; // Consume the escape key
        }
    }

    return event;
}

void HotkeyMonitor::runLoop()
{
    CGEventMask mask = CGEventMaskBit(kCGEventFlagsChanged) | CGEventMaskBit(kCGEventKeyDown);

    m_tap = CGEventTapCreate(
        kCGSessionEventTap,
        kCGHeadInsertEventTap,
        kCGEventTapOptionDefault,
        mask,
        eventCallback,
        this
    );

    if (!m_tap) {
        qWarning() << "HotkeyMonitor: failed to create event tap";
        return;
    }

    m_runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, m_tap, 0);
    m_runLoop = CFRunLoopGetCurrent();

    CFRunLoopAddSource(m_runLoop, m_runLoopSource, kCFRunLoopCommonModes);
    CGEventTapEnable(m_tap, true);

    CFRunLoopRun();
}
