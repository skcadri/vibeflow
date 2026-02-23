#pragma once

#include <QObject>
#include <QTimer>
#include <CoreGraphics/CoreGraphics.h>

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

private slots:
    void pollModifiers();

private:
    QTimer *m_pollTimer = nullptr;
    bool m_active = false;
    bool m_running = false;
    bool m_escapeWasDown = false;
};
