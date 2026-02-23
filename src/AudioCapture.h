#pragma once

#include <QObject>
#include <QVector>
#include <QByteArray>

class QAudioSource;
class QIODevice;
class QTimer;

class AudioCapture : public QObject
{
    Q_OBJECT
public:
    explicit AudioCapture(QObject *parent = nullptr);
    ~AudioCapture();

    void start();
    void stop();
    QVector<float> getRecordedAudio() const;

signals:
    void levelChanged(float rmsLevel);

private slots:
    void onReadyRead();
    void onTimerTick();

private:
    QAudioSource *m_source = nullptr;
    QIODevice *m_device = nullptr;
    QByteArray m_rawBuffer;
    QTimer *m_levelTimer = nullptr;
    float m_currentRms = 0.0f;
};
