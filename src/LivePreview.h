#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <atomic>

struct transcribe_session;

// Live "what you're saying" text from a small streaming model (Nemotron Speech
// Streaming 0.6B, English-only). Preview only: the pasted text still comes from
// Transcriber's full Qwen pass on release.
//
// Lives on its own QThread. start()/stop() are called from the UI thread; every
// slot runs on the worker thread, which owns the session.
class LivePreview : public QObject
{
    Q_OBJECT
public:
    explicit LivePreview(QObject *parent = nullptr);
    ~LivePreview();

    void start(); // begin a new stream for a new recording
    void stop();  // drop any audio still queued; the next start() resets the stream

public slots:
    void load(const QString &modelPath);
    void feed(const QVector<float> &samples);

signals:
    // committed = settled words; tentative = newest words that may still change
    void textChanged(const QString &committed, const QString &tentative);

private slots:
    void begin();

private:
    transcribe_session *m_session = nullptr;
    std::atomic<bool> m_active{false};
};
