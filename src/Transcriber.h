#pragma once

#include <QObject>
#include <QVector>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <mutex>
#include <vector>
#include <string>

struct transcribe_session;

struct TranscriptionSegment {
    float start;    // seconds
    float end;      // seconds
    std::string text;
};

struct TranscriptionResult {
    std::string text;
    std::vector<TranscriptionSegment> segments;
};

// Two models via transcribe.cpp:
//   - Qwen3-ASR 1.7B: hotkey dictation (more accurate English, real vocabulary support).
//   - Whisper large-v3-turbo: Urdu (Qwen has no Urdu) and the HTTP server, which
//     needs segment timestamps that Qwen3-ASR can't produce.
class Transcriber : public QObject
{
    Q_OBJECT
public:
    explicit Transcriber(QObject *parent = nullptr);
    ~Transcriber();

    void loadModels(const QString &qwenPath, const QString &whisperPath);
    QString transcribe(const QVector<float> &audioSamples, int sampleRate);
    TranscriptionResult transcribeWithSegments(const QVector<float> &audioSamples, int sampleRate,
                                               const QString &contextPrompt = QString());
    bool isLoaded() const;
    void unload();
    void setVocabulary(const QStringList &words);

    // Strips a hallucinated leading speaker label (e.g. "Tim Cynova: ") from text.
    // Public so TranscriptionServer can sanitize the chunk carry-forward prompt.
    static QString stripSpeakerLabel(const QString &text);

signals:
    void modelLoaded();
    void modelLoadFailed(const QString &error);

private:
    bool run(transcribe_session *session, const std::vector<float> &pcm,
             const char *language, const QString &contextPrompt);

    transcribe_session *m_qwen = nullptr;
    transcribe_session *m_whisper = nullptr;
    std::mutex m_mutex; // sessions are single-threaded; guards both + m_vocabulary
    std::vector<QByteArray> m_vocabulary;
};
