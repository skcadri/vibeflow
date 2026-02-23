#include "AudioCapture.h"

#include <QAudioSource>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QTimer>
#include <QDebug>
#include <cmath>
#include <cstdio>

// --- AudioBuffer implementation ---

AudioBuffer::AudioBuffer(QObject *parent)
    : QIODevice(parent)
{
    open(QIODevice::WriteOnly);
}

void AudioBuffer::clear()
{
    m_data.clear();
}

qint64 AudioBuffer::readData(char *, qint64)
{
    return 0; // Write-only device
}

qint64 AudioBuffer::writeData(const char *data, qint64 len)
{
    m_data.append(data, len);
    emit dataWritten(len);
    return len;
}

// --- AudioCapture implementation ---

AudioCapture::AudioCapture(QObject *parent)
    : QObject(parent)
{
    m_audioBuffer = new AudioBuffer(this);

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
    m_audioBuffer->clear();
    m_currentRms = 0.0f;

    QAudioDevice defaultDevice = QMediaDevices::defaultAudioInput();
    if (defaultDevice.isNull()) {
        fprintf(stderr, "[ERROR] AudioCapture: no audio input device found\n");
        fflush(stderr);
        return;
    }

    fprintf(stderr, "[INFO] AudioCapture: device: %s\n",
            defaultDevice.description().toUtf8().constData());

    // Log supported ranges
    fprintf(stderr, "[INFO] AudioCapture: supported sample rates: %d - %d\n",
            defaultDevice.minimumSampleRate(), defaultDevice.maximumSampleRate());
    fprintf(stderr, "[INFO] AudioCapture: supported channels: %d - %d\n",
            defaultDevice.minimumChannelCount(), defaultDevice.maximumChannelCount());
    fflush(stderr);

    // Try our preferred format first: 16kHz mono Int16 (what whisper.cpp expects)
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    if (!defaultDevice.isFormatSupported(format)) {
        fprintf(stderr, "[INFO] AudioCapture: 16kHz mono not supported, trying device preferred format\n");
        fflush(stderr);

        // Use the device's preferred format and we'll resample later
        format = defaultDevice.preferredFormat();
        fprintf(stderr, "[INFO] AudioCapture: using format: %dHz, %dch, sample=%d\n",
                format.sampleRate(), format.channelCount(), (int)format.sampleFormat());
        fflush(stderr);

        // If preferred format isn't Int16, try Int16 at the preferred rate/channels
        if (format.sampleFormat() != QAudioFormat::Int16) {
            QAudioFormat tryFormat;
            tryFormat.setSampleRate(format.sampleRate());
            tryFormat.setChannelCount(format.channelCount());
            tryFormat.setSampleFormat(QAudioFormat::Int16);
            if (defaultDevice.isFormatSupported(tryFormat)) {
                format = tryFormat;
                fprintf(stderr, "[INFO] AudioCapture: switched to Int16 at %dHz %dch\n",
                        format.sampleRate(), format.channelCount());
                fflush(stderr);
            }
        }
    } else {
        fprintf(stderr, "[INFO] AudioCapture: 16kHz mono Int16 supported natively\n");
        fflush(stderr);
    }

    m_captureFormat = format;

    m_source = new QAudioSource(defaultDevice, format, this);
    m_source->setBufferSize(format.sampleRate() * format.channelCount() * 2); // 1 second

    // Pull mode: QAudioSource writes audio data directly into our AudioBuffer
    m_source->start(m_audioBuffer);

    fprintf(stderr, "[INFO] AudioCapture: started (pull mode), state=%d error=%d\n",
            (int)m_source->state(), (int)m_source->error());
    fflush(stderr);

    // Track incoming data for RMS visualization
    connect(m_audioBuffer, &AudioBuffer::dataWritten, this, [this](qint64 len) {
        const QByteArray &buf = m_audioBuffer->buffer();
        if (buf.size() >= len) {
            computeRms(buf.constData() + buf.size() - len, len);
        }
    });

    m_levelTimer->start();
    fprintf(stderr, "[INFO] AudioCapture: recording started\n");
    fflush(stderr);
}

void AudioCapture::stop()
{
    m_levelTimer->stop();

    if (m_source) {
        m_source->stop();
        disconnect(m_audioBuffer, nullptr, this, nullptr);
        delete m_source;
        m_source = nullptr;
    }

    fprintf(stderr, "[INFO] AudioCapture: recording stopped, %lld bytes captured (%lld samples at %dHz %dch)\n",
            (long long)m_audioBuffer->size(),
            (long long)(m_audioBuffer->size() / (m_captureFormat.bytesPerSample() * m_captureFormat.channelCount())),
            m_captureFormat.sampleRate(), m_captureFormat.channelCount());
    fflush(stderr);
}

void AudioCapture::computeRms(const char *data, qint64 len)
{
    // Handle different sample formats
    int sampleCount = 0;
    double sum = 0.0;

    if (m_captureFormat.sampleFormat() == QAudioFormat::Int16) {
        const int16_t *samples = reinterpret_cast<const int16_t *>(data);
        sampleCount = static_cast<int>(len / sizeof(int16_t));
        for (int i = 0; i < sampleCount; i++) {
            double s = samples[i] / 32768.0;
            sum += s * s;
        }
    } else if (m_captureFormat.sampleFormat() == QAudioFormat::Int32) {
        const int32_t *samples = reinterpret_cast<const int32_t *>(data);
        sampleCount = static_cast<int>(len / sizeof(int32_t));
        for (int i = 0; i < sampleCount; i++) {
            double s = samples[i] / 2147483648.0;
            sum += s * s;
        }
    } else if (m_captureFormat.sampleFormat() == QAudioFormat::Float) {
        const float *samples = reinterpret_cast<const float *>(data);
        sampleCount = static_cast<int>(len / sizeof(float));
        for (int i = 0; i < sampleCount; i++) {
            double s = samples[i];
            sum += s * s;
        }
    }

    if (sampleCount > 0) {
        m_currentRms = static_cast<float>(std::sqrt(sum / sampleCount));
    }
}

void AudioCapture::onTimerTick()
{
    static int tickCount = 0;
    tickCount++;
    if (tickCount <= 10) {
        fprintf(stderr, "[DEBUG] AudioCapture tick #%d: buffer=%lld bytes, rms=%.4f, state=%d err=%d\n",
                tickCount, (long long)m_audioBuffer->size(), m_currentRms,
                m_source ? (int)m_source->state() : -1,
                m_source ? (int)m_source->error() : -1);
        fflush(stderr);

        // On first tick with data, log some raw sample values for diagnosis
        if (tickCount == 3 && m_audioBuffer->size() > 100) {
            const QByteArray &buf = m_audioBuffer->buffer();
            if (m_captureFormat.sampleFormat() == QAudioFormat::Int16) {
                const int16_t *s = reinterpret_cast<const int16_t *>(buf.constData());
                int n = qMin(10, (int)(buf.size() / 2));
                fprintf(stderr, "[DEBUG] First %d raw samples:", n);
                for (int i = 0; i < n; i++) fprintf(stderr, " %d", s[i]);
                fprintf(stderr, "\n");
                fflush(stderr);
            } else if (m_captureFormat.sampleFormat() == QAudioFormat::Float) {
                const float *s = reinterpret_cast<const float *>(buf.constData());
                int n = qMin(10, (int)(buf.size() / 4));
                fprintf(stderr, "[DEBUG] First %d raw float samples:", n);
                for (int i = 0; i < n; i++) fprintf(stderr, " %.6f", s[i]);
                fprintf(stderr, "\n");
                fflush(stderr);
            }
        }
    }

    emit levelChanged(m_currentRms);
    m_currentRms *= 0.8f; // Decay for smooth animation
}

QVector<float> AudioCapture::getRecordedAudio() const
{
    const QByteArray &raw = m_audioBuffer->buffer();

    int bytesPerSample = m_captureFormat.bytesPerSample();
    int channels = m_captureFormat.channelCount();
    int captureRate = m_captureFormat.sampleRate();
    int totalFrames = raw.size() / (bytesPerSample * channels);

    fprintf(stderr, "[INFO] AudioCapture: converting %d frames from %dHz %dch to 16kHz mono\n",
            totalFrames, captureRate, channels);
    fflush(stderr);

    // Step 1: Convert to mono float32
    QVector<float> monoFloat(totalFrames);

    if (m_captureFormat.sampleFormat() == QAudioFormat::Int16) {
        const int16_t *samples = reinterpret_cast<const int16_t *>(raw.constData());
        for (int i = 0; i < totalFrames; i++) {
            float sum = 0.0f;
            for (int ch = 0; ch < channels; ch++) {
                sum += samples[i * channels + ch] / 32768.0f;
            }
            monoFloat[i] = sum / channels;
        }
    } else if (m_captureFormat.sampleFormat() == QAudioFormat::Int32) {
        const int32_t *samples = reinterpret_cast<const int32_t *>(raw.constData());
        for (int i = 0; i < totalFrames; i++) {
            float sum = 0.0f;
            for (int ch = 0; ch < channels; ch++) {
                sum += samples[i * channels + ch] / 2147483648.0f;
            }
            monoFloat[i] = sum / channels;
        }
    } else if (m_captureFormat.sampleFormat() == QAudioFormat::Float) {
        const float *samples = reinterpret_cast<const float *>(raw.constData());
        for (int i = 0; i < totalFrames; i++) {
            float sum = 0.0f;
            for (int ch = 0; ch < channels; ch++) {
                sum += samples[i * channels + ch];
            }
            monoFloat[i] = sum / channels;
        }
    }

    // Step 2: Resample to 16kHz if needed (simple linear interpolation)
    if (captureRate == 16000) {
        return monoFloat;
    }

    double ratio = (double)captureRate / 16000.0;
    int outputFrames = (int)(totalFrames / ratio);
    QVector<float> resampled(outputFrames);

    for (int i = 0; i < outputFrames; i++) {
        double srcPos = i * ratio;
        int srcIdx = (int)srcPos;
        float frac = (float)(srcPos - srcIdx);

        if (srcIdx + 1 < totalFrames) {
            resampled[i] = monoFloat[srcIdx] * (1.0f - frac) + monoFloat[srcIdx + 1] * frac;
        } else if (srcIdx < totalFrames) {
            resampled[i] = monoFloat[srcIdx];
        }
    }

    fprintf(stderr, "[INFO] AudioCapture: resampled %d -> %d frames (16kHz)\n",
            totalFrames, outputFrames);
    fflush(stderr);

    return resampled;
}
