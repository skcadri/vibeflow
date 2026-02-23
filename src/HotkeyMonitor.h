#pragma once

#include <QObject>
#include <CoreGraphics/CoreGraphics.h>

class QThread;

class HotkeyMonitor : public QObject
{
    Q_OBJECT
public:
    explicit HotkeyMonitor(QObject *parent = nullptr);
    ~HotkeyMonitor();

    bool start();
    void stop();

signals:
    void activated();
    void deactivated();
    void cancelled();

private:
    static CGEventRef eventCallback(CGEventTapProxy proxy, CGEventType type,
                                     CGEventRef event, void *userInfo);
    void runLoop();

    QThread *m_thread = nullptr;
    CFMachPortRef m_tap = nullptr;
    CFRunLoopSourceRef m_runLoopSource = nullptr;
    CFRunLoopRef m_runLoop = nullptr;
    bool m_active = false;
    bool m_running = false;
};
