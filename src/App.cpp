#include "App.h"
#include "Transcriber.h"
#include "AudioCapture.h"
#include "HotkeyMonitor.h"
#include "TextPaster.h"
#include "ui/GlassBubble.h"
#include "ui/TrayIcon.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QTimer>
#include <cstdio>
#include <cmath>

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
    qInfo() << "Creating components...";

    m_transcriber = new Transcriber(this);
    m_audioCapture = new AudioCapture(this);
    m_hotkeyMonitor = new HotkeyMonitor(this);
    m_bubble = new GlassBubble();
    m_trayIcon = new TrayIcon(this);

    qInfo() << "Connecting signals...";

    connect(m_hotkeyMonitor, &HotkeyMonitor::activated, this, &App::onHotkeyActivated);
    connect(m_hotkeyMonitor, &HotkeyMonitor::deactivated, this, &App::onHotkeyDeactivated);
    connect(m_hotkeyMonitor, &HotkeyMonitor::cancelled, this, &App::onHotkeyCancelled);
    connect(m_audioCapture, &AudioCapture::levelChanged, m_bubble, &GlassBubble::updateLevel);
    connect(m_transcriber, &Transcriber::modelLoaded, this, &App::onModelLoaded);
    connect(m_transcriber, &Transcriber::modelLoadFailed, this, &App::onModelLoadFailed);
    connect(m_trayIcon, &TrayIcon::inputModeChanged, this, [this](bool useTypeMode) {
        m_useTypeMode = useTypeMode;
        fprintf(stderr, "[INFO] App: input mode changed to %s\n", useTypeMode ? "type" : "paste");
        fflush(stderr);
    });

    qInfo() << "Starting hotkey monitor...";
    if (!m_hotkeyMonitor->start()) {
        qWarning() << "Failed to start hotkey monitor — grant Accessibility permission";
        m_trayIcon->showMessage("VibeFlow", "Please grant Accessibility permission in System Settings");
    }

    m_trayIcon->show();
    qInfo() << "Tray icon shown, loading model...";

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
    fprintf(stderr, "[INFO] App: onHotkeyActivated (state=%d, modelReady=%d)\n", (int)m_state, m_modelReady);
    fflush(stderr);

    if (!m_modelReady) {
        fprintf(stderr, "[WARN] App: model not loaded yet, ignoring\n");
        fflush(stderr);
        return;
    }
    if (m_state != Idle) {
        fprintf(stderr, "[WARN] App: not idle (state=%d), ignoring\n", (int)m_state);
        fflush(stderr);
        return;
    }

    m_pasteTargetPid = TextPaster::frontmostAppPid();
    if (m_pasteTargetPid == static_cast<qint64>(QCoreApplication::applicationPid())) {
        m_pasteTargetPid = 0;
    }
    fprintf(stderr, "[INFO] App: captured frontmost app pid for paste target: %lld\n",
            (long long)m_pasteTargetPid);
    fflush(stderr);

    setState(Recording);
    m_audioCapture->start();
    m_bubble->setState(GlassBubble::Recording);
}

void App::onHotkeyDeactivated()
{
    fprintf(stderr, "[INFO] App: onHotkeyDeactivated (state=%d)\n", (int)m_state);
    fflush(stderr);

    if (m_state != Recording) {
        fprintf(stderr, "[WARN] App: not recording (state=%d), ignoring\n", (int)m_state);
        fflush(stderr);
        return;
    }

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
    fprintf(stderr, "[INFO] App: transcription finished, text=%lld chars: \"%s\"\n",
            (long long)text.length(), text.left(100).toUtf8().constData());
    fflush(stderr);

    // Hide bubble synchronously (no fade animation) so it releases focus immediately
    m_bubble->hideImmediately();
    setState(Idle);

    if (!text.isEmpty()) {
        QString textToPaste = text;
        if (!textToPaste.at(textToPaste.size() - 1).isSpace()) {
            textToPaste.append(' ');
        }
        // Defer injection by 50ms to let macOS process the window ordering change
        const qint64 targetPid = m_pasteTargetPid;
        const bool useTypeMode = m_useTypeMode;
        QTimer::singleShot(50, this, [this, textToPaste, targetPid, useTypeMode]() {
            if (useTypeMode) {
                fprintf(stderr, "[INFO] App: typing text at cursor (deferred)\n");
                fflush(stderr);
                if (!TextPaster::typeAtCursor(textToPaste, targetPid) && m_trayIcon) {
                    m_trayIcon->showMessage("VibeFlow",
                        "Failed to type text. Enable Accessibility for VibeFlow in System Settings.");
                }
            } else {
                fprintf(stderr, "[INFO] App: pasting text at cursor (deferred)\n");
                fflush(stderr);
                if (!TextPaster::pasteToPid(textToPaste, targetPid) && m_trayIcon) {
                    m_trayIcon->showMessage("VibeFlow",
                        "Transcribed text copied to clipboard. Enable Accessibility for VibeFlow to auto-paste.");
                }
            }
        });
    }

    m_pasteTargetPid = 0;
}

void App::setState(State state)
{
    fprintf(stderr, "[DEBUG] App: state %d -> %d\n", (int)m_state, (int)state);
    fflush(stderr);
    m_state = state;
}

void App::transcribeAsync()
{
    QVector<float> audio = m_audioCapture->getRecordedAudio();
    fprintf(stderr, "[INFO] App: transcribing %lld samples (%.1f sec)\n",
            (long long)audio.size(), audio.size() / 16000.0f);
    fflush(stderr);

    if (audio.isEmpty()) {
        fprintf(stderr, "[WARN] App: no audio captured, skipping transcription\n");
        fflush(stderr);
        if (m_trayIcon) {
            m_trayIcon->showMessage("VibeFlow",
                "No microphone data captured. Check Privacy & Security > Microphone for VibeFlow.");
        }
        onTranscriptionFinished(QString());
        return;
    }

    float peak = 0.0f;
    double sumSquares = 0.0;
    for (float sample : audio) {
        float absSample = std::fabs(sample);
        if (absSample > peak) peak = absSample;
        sumSquares += static_cast<double>(sample) * static_cast<double>(sample);
    }
    const float rms = static_cast<float>(std::sqrt(sumSquares / audio.size()));
    fprintf(stderr, "[INFO] App: audio stats peak=%.6f rms=%.6f\n", peak, rms);
    fflush(stderr);

    // On macOS permission/signing failures, capture can look "active" but produce near-silent data.
    if (audio.size() >= 8000 && peak < 0.003f && rms < 0.0008f) {
        fprintf(stderr, "[WARN] App: near-silent capture detected, skipping transcription\n");
        fflush(stderr);
        if (m_trayIcon) {
            m_trayIcon->showMessage("VibeFlow",
                "Microphone signal is near-silent. Re-enable microphone permission and use stable app signing.");
        }
        onTranscriptionFinished(QString());
        return;
    }

    QThread *thread = QThread::create([this, audio]() {
        QString text = m_transcriber->transcribe(audio, 16000);
        QMetaObject::invokeMethod(this, "onTranscriptionFinished",
                                  Qt::QueuedConnection, Q_ARG(QString, text));
    });
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}
