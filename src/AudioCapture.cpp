#include "AudioCapture.h"

#include <QAudioSource>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QTimer>
#include <QDebug>
#include <cmath>

AudioCapture::AudioCapture(QObject *parent)
    : QObject(parent)
{
    m_levelTimer = new QTimer(this);
    m_levelTimer->setInterval(33); // ~30fps
    connect(m_levelTimer, &QTimer::timeout, this, &AudioCapture::onTimerTick);
}

AudioCapture::~AudioCapture()
{
    stop();
}

void AudioCapture::start()
{
    m_rawBuffer.clear();
    m_currentRms = 0.0f;

    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    QAudioDevice defaultDevice = QMediaDevices::defaultAudioInput();
    if (defaultDevice.isNull()) {
        qWarning() << "AudioCapture: no audio input device found";
        return;
    }

    if (!defaultDevice.isFormatSupported(format)) {
        qWarning() << "AudioCapture: format not supported, using nearest";
    }

    m_source = new QAudioSource(defaultDevice, format, this);
    m_device = m_source->start();

    if (!m_device) {
        qWarning() << "AudioCapture: failed to start audio source";
        delete m_source;
        m_source = nullptr;
        return;
    }

    connect(m_device, &QIODevice::readyRead, this, &AudioCapture::onReadyRead);
    m_levelTimer->start();

    qInfo() << "AudioCapture: recording started";
}

void AudioCapture::stop()
{
    m_levelTimer->stop();

    if (m_source) {
        m_source->stop();
        delete m_source;
        m_source = nullptr;
        m_device = nullptr;
    }

    qInfo() << "AudioCapture: recording stopped," << m_rawBuffer.size() << "bytes captured";
}

void AudioCapture::onReadyRead()
{
    if (!m_device) return;

    QByteArray data = m_device->readAll();
    m_rawBuffer.append(data);

    // Calculate RMS for the latest chunk
    const int16_t *samples = reinterpret_cast<const int16_t *>(data.constData());
    int sampleCount = data.size() / sizeof(int16_t);

    if (sampleCount > 0) {
        double sum = 0.0;
        for (int i = 0; i < sampleCount; i++) {
            double s = samples[i] / 32768.0;
            sum += s * s;
        }
        m_currentRms = static_cast<float>(std::sqrt(sum / sampleCount));
    }
}

void AudioCapture::onTimerTick()
{
    emit levelChanged(m_currentRms);
}

QVector<float> AudioCapture::getRecordedAudio() const
{
    // Convert int16 raw buffer to float32 samples
    const int16_t *samples = reinterpret_cast<const int16_t *>(m_rawBuffer.constData());
    int sampleCount = m_rawBuffer.size() / sizeof(int16_t);

    QVector<float> floatSamples(sampleCount);
    for (int i = 0; i < sampleCount; i++) {
        floatSamples[i] = samples[i] / 32768.0f;
    }

    return floatSamples;
}
