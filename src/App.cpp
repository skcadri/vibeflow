#include "App.h"
#include "Transcriber.h"
#include "AudioCapture.h"
#include "HotkeyMonitor.h"
#include "TextPaster.h"
#include "ui/GlassBubble.h"
#include "ui/TrayIcon.h"

#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QTimer>

App::App(QObject *parent)
    : QObject(parent)
{
}

App::~App()
{
    if (m_hotkeyMonitor)
        m_hotkeyMonitor->stop();
}

void App::initialize()
{
    // Create components
    m_transcriber = new Transcriber(this);
    m_audioCapture = new AudioCapture(this);
    m_hotkeyMonitor = new HotkeyMonitor(this);
    m_bubble = new GlassBubble();
    m_trayIcon = new TrayIcon(this);

    // Connect hotkey signals
    connect(m_hotkeyMonitor, &HotkeyMonitor::activated, this, &App::onHotkeyActivated);
    connect(m_hotkeyMonitor, &HotkeyMonitor::deactivated, this, &App::onHotkeyDeactivated);
    connect(m_hotkeyMonitor, &HotkeyMonitor::cancelled, this, &App::onHotkeyCancelled);

    // Connect audio level to bubble waveform
    connect(m_audioCapture, &AudioCapture::levelChanged, m_bubble, &GlassBubble::updateLevel);

    // Connect transcriber
    connect(m_transcriber, &Transcriber::modelLoaded, this, &App::onModelLoaded);
    connect(m_transcriber, &Transcriber::modelLoadFailed, this, &App::onModelLoadFailed);

    // Start hotkey monitor
    if (!m_hotkeyMonitor->start()) {
        qWarning() << "Failed to start hotkey monitor — grant Accessibility permission";
        m_trayIcon->showMessage("VibeFlow", "Please grant Accessibility permission in System Settings");
    }

    // Show tray icon
    m_trayIcon->show();

    // Load model in background
    loadModelAsync();
}

void App::loadModelAsync()
{
    // Search for model in multiple locations
    QStringList searchPaths = {
        QApplication::applicationDirPath() + "/../Resources/ggml-large-v3.bin",  // Inside .app bundle
        QDir::homePath() + "/vibeflow/models/ggml-large-v3.bin",                 // Dev location
        QDir::homePath() + "/.vibeflow/models/ggml-large-v3.bin",               // User config dir
    };

    QString modelPath;
    for (const auto &path : searchPaths) {
        if (QFile::exists(path)) {
            modelPath = path;
            break;
        }
    }

    if (modelPath.isEmpty()) {
        qWarning() << "Model not found in any search path";
        m_trayIcon->showMessage("VibeFlow",
            "Model not found. Place ggml-large-v3.bin in ~/vibeflow/models/");
        return;
    }

    qInfo() << "Using model:" << modelPath;

    m_trayIcon->showMessage("VibeFlow", "Loading model...");

    QThread *thread = QThread::create([this, modelPath]() {
        m_transcriber->loadModel(modelPath);
    });
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}

void App::onModelLoaded()
{
    m_modelReady = true;
    m_trayIcon->showMessage("VibeFlow", "Ready — hold ⌘+Ctrl to dictate");
    qInfo() << "VibeFlow ready";
}

void App::onModelLoadFailed(const QString &error)
{
    qWarning() << "Model load failed:" << error;
    m_trayIcon->showMessage("VibeFlow", "Model load failed: " + error);
}

void App::onHotkeyActivated()
{
    if (!m_modelReady) {
        qWarning() << "Model not loaded yet";
        return;
    }
    if (m_state != Idle)
        return;

    setState(Recording);
    m_audioCapture->start();
    m_bubble->setState(GlassBubble::Recording);
}

void App::onHotkeyDeactivated()
{
    if (m_state != Recording)
        return;

    m_audioCapture->stop();
    setState(Processing);
    m_bubble->setState(GlassBubble::Processing);

    transcribeAsync();
}

void App::onHotkeyCancelled()
{
    if (m_state == Recording) {
        m_audioCapture->stop();
    }
    setState(Idle);
    m_bubble->setState(GlassBubble::Hidden);
}

void App::onTranscriptionFinished(const QString &text)
{
    m_bubble->setState(GlassBubble::Hidden);
    setState(Idle);

    if (!text.isEmpty()) {
        TextPaster::paste(text);
    }
}

void App::setState(State state)
{
    m_state = state;
}

void App::transcribeAsync()
{
    QVector<float> audio = m_audioCapture->getRecordedAudio();

    QThread *thread = QThread::create([this, audio]() {
        QString text = m_transcriber->transcribe(audio, 16000);
        QMetaObject::invokeMethod(this, "onTranscriptionFinished",
                                  Qt::QueuedConnection, Q_ARG(QString, text));
    });
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}
