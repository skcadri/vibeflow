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

    // Polling approach: CGEventSourceFlagsState requires no special permissions.
    // It reads the combined modifier key state from the window server directly.
    // This avoids the CGEventTap permission issues on macOS Tahoe where
    // Input Monitoring permission is required separately from Accessibility.

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(16); // ~60Hz polling
    connect(m_pollTimer, &QTimer::timeout, this, &HotkeyMonitor::pollModifiers);
    m_pollTimer->start();

    m_running = true;

    fprintf(stderr, "[INFO] HotkeyMonitor: polling started (60Hz, no event tap needed)\n");
    fflush(stderr);

    qInfo() << "HotkeyMonitor: started";
    return true;
}

void HotkeyMonitor::stop()
{
    if (!m_running) return;
    m_running = false;

    if (m_pollTimer) {
        m_pollTimer->stop();
        delete m_pollTimer;
        m_pollTimer = nullptr;
    }
}

void HotkeyMonitor::pollModifiers()
{
    // Read current modifier flags from the combined session state.
    // This works without any event tap or Input Monitoring permission.
    CGEventFlags flags = CGEventSourceFlagsState(kCGEventSourceStateCombinedSessionState);

    // One-time diagnostic: confirm timer is firing
    static int pollCount = 0;
    pollCount++;
    if (pollCount == 1 || pollCount == 60 || pollCount == 300) {
        fprintf(stderr, "[DEBUG] pollModifiers tick #%d flags=0x%llx\n",
                pollCount, (unsigned long long)flags);
        fflush(stderr);
    }

    bool cmdHeld = (flags & kCGEventFlagMaskCommand) != 0;
    bool ctrlHeld = (flags & kCGEventFlagMaskControl) != 0;
    bool bothHeld = cmdHeld && ctrlHeld;

    if (bothHeld && !m_active) {
        m_active = true;
        fprintf(stderr, "[INFO] HotkeyMonitor: ACTIVATED (flags=0x%llx)\n",
                (unsigned long long)flags);
        fflush(stderr);
        emit activated();
    } else if (!bothHeld && m_active) {
        m_active = false;
        fprintf(stderr, "[INFO] HotkeyMonitor: DEACTIVATED (flags=0x%llx)\n",
                (unsigned long long)flags);
        fflush(stderr);
        emit deactivated();
    }

    // Check Escape key while active (keycode 53)
    if (m_active) {
        bool escapeDown = CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, 53);
        if (escapeDown && !m_escapeWasDown) {
            m_active = false;
            m_escapeWasDown = true;
            fprintf(stderr, "[INFO] HotkeyMonitor: CANCELLED (Escape)\n");
            fflush(stderr);
            emit cancelled();
        }
        m_escapeWasDown = escapeDown;
    } else {
        m_escapeWasDown = false;
    }
}
