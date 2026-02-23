#pragma once

#include <QObject>
#include <QThread>

class Transcriber;
class AudioCapture;
class HotkeyMonitor;
class GlassBubble;
class TrayIcon;

class App : public QObject
{
    Q_OBJECT
public:
    explicit App(QObject *parent = nullptr);
    ~App();

    void initialize();

    enum State { Idle, Recording, Processing };
    Q_ENUM(State)

private slots:
    void onHotkeyActivated();
    void onHotkeyDeactivated();
    void onHotkeyCancelled();
    void onModelLoaded();
    void onModelLoadFailed(const QString &error);
    void onTranscriptionFinished(const QString &text);

private:
    void setState(State state);
    void loadModelAsync();
    void transcribeAsync();

    State m_state = Idle;
    Transcriber *m_transcriber = nullptr;
    AudioCapture *m_audioCapture = nullptr;
    HotkeyMonitor *m_hotkeyMonitor = nullptr;
    GlassBubble *m_bubble = nullptr;
    TrayIcon *m_trayIcon = nullptr;
    bool m_modelReady = false;
};
