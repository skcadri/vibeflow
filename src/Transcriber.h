#pragma once

#include <QObject>
#include <QVector>
#include <QString>
#include <mutex>
#include <vector>
#include <string>

struct whisper_context;
struct whisper_full_params;

struct TranscriptionSegment {
    float start;    // seconds
    float end;      // seconds
    std::string text;
    bool speakerTurnNext;
};

struct TranscriptionResult {
    std::string text;
    std::vector<TranscriptionSegment> segments;
};

class Transcriber : public QObject
{
    Q_OBJECT
public:
    explicit Transcriber(QObject *parent = nullptr);
    ~Transcriber();

    void loadModel(const QString &modelPath);
    QString transcribe(const QVector<float> &audioSamples, int sampleRate);
    TranscriptionResult transcribeWithSegments(const QVector<float> &audioSamples, int sampleRate,
                                               const QString &overridePrompt = QString());
    bool isLoaded() const;
    void unload();
    void setTranslate(bool translate);
    void setInitialPrompt(const QString &prompt);

    // For external callers (TranscriptionServer) that need chunk-level mutex control
    std::mutex &mutex() { return m_mutex; }

signals:
    void modelLoaded();
    void modelLoadFailed(const QString &error);

private:
    whisper_full_params makeDefaultParams() const;
    static bool isHallucination(const QString &text);

    whisper_context *m_ctx = nullptr;
    std::mutex m_mutex;
    bool m_translate = false;
    QString m_initialPrompt;
};
