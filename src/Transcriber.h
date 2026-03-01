#pragma once

#include <QObject>
#include <QVector>
#include <QString>

struct whisper_context;

class Transcriber : public QObject
{
    Q_OBJECT
public:
    explicit Transcriber(QObject *parent = nullptr);
    ~Transcriber();

    void loadModel(const QString &modelPath);
    QString transcribe(const QVector<float> &audioSamples, int sampleRate);
    bool isLoaded() const;
    void unload();
    void setTranslate(bool translate);
    void setInitialPrompt(const QString &prompt);

signals:
    void modelLoaded();
    void modelLoadFailed(const QString &error);

private:
    whisper_context *m_ctx = nullptr;
    bool m_translate = false;
    QString m_initialPrompt;
};
