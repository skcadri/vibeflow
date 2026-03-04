#pragma once

#include <QObject>
#include <QIODevice>
#include <QVector>
#include <QByteArray>
#include <QAudioFormat>

class QAudioSource;
class QTimer;

// Custom QIODevice that receives audio data written by QAudioSource in pull mode
class AudioBuffer : public QIODevice
{
    Q_OBJECT
public:
    explicit AudioBuffer(QObject *parent = nullptr);

    void clear();
    void append(const char *data, qint64 len);
    QByteArray buffer() const { return m_data; }
    qint64 size() const override { return m_data.size(); }

signals:
    void dataWritten(qint64 bytes);

protected:
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;

private:
    QByteArray m_data;
};

class QMediaDevices;

class AudioCapture : public QObject
{
    Q_OBJECT
public:
    explicit AudioCapture(QObject *parent = nullptr);
    ~AudioCapture();

    void initDevice();
    void start();
    void stop();
    void setKeepActive(bool keep);
    QVector<float> getRecordedAudio() const;

signals:
    void levelChanged(float rmsLevel);

private slots:
    void onTimerTick();
    void onDefaultInputChanged();

private:
    void computeRms(const char *data, qint64 len);
    void destroySource();

    QAudioSource *m_source = nullptr;
    QIODevice *m_inputDevice = nullptr;
    AudioBuffer *m_audioBuffer = nullptr;
    QTimer *m_levelTimer = nullptr;
    QMediaDevices *m_mediaDevices = nullptr;
    QAudioFormat m_captureFormat;
    float m_currentRms = 0.0f;
    bool m_deviceReady = false;
    bool m_keepActive = false;
};
