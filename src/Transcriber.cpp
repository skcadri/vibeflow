#include "Transcriber.h"
#include "whisper.h"

#include <QDebug>
#include <thread>

Transcriber::Transcriber(QObject *parent)
    : QObject(parent)
{
}

Transcriber::~Transcriber()
{
    unload();
}

void Transcriber::loadModel(const QString &modelPath)
{
    struct whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = true;
    cparams.flash_attn = true;

    m_ctx = whisper_init_from_file_with_params(modelPath.toUtf8().constData(), cparams);

    if (!m_ctx) {
        emit modelLoadFailed("Failed to load whisper model from " + modelPath);
        return;
    }

    qInfo() << "Whisper model loaded:" << modelPath;
    emit modelLoaded();
}

QString Transcriber::transcribe(const QVector<float> &audioSamples, int sampleRate)
{
    if (!m_ctx) {
        qWarning() << "Transcriber: model not loaded";
        return {};
    }

    if (audioSamples.isEmpty()) {
        qWarning() << "Transcriber: empty audio";
        return {};
    }

    // Pad audio shorter than 1 second — Whisper rejects very short input
    const int minSamples = sampleRate; // 1 second
    QVector<float> audio = audioSamples;
    if (audio.size() < minSamples) {
        qInfo() << "Transcriber: padding audio from" << audio.size() << "to" << minSamples << "samples";
        audio.resize(minSamples, 0.0f);
    }

    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.language = "auto";
    params.n_threads = std::min(16, (int)std::thread::hardware_concurrency());
    params.no_timestamps = true;
    params.translate = m_translate;
    params.print_progress = false;
    params.print_realtime = false;
    params.print_special = false;
    params.print_timestamps = false;

    int ret = whisper_full(m_ctx, params, audio.constData(), audio.size());

    if (ret != 0) {
        qWarning() << "Transcriber: whisper_full failed with code" << ret;
        return {};
    }

    // Suppress Hindi — if Whisper detected Hindi, re-run forced as Urdu
    int detectedLangId = whisper_full_lang_id(m_ctx);
    if (detectedLangId == whisper_lang_id("hi")) {
        qInfo() << "Transcriber: Hindi detected, re-running as Urdu";
        params.language = "ur";
        ret = whisper_full(m_ctx, params, audio.constData(), audio.size());
        if (ret != 0) {
            qWarning() << "Transcriber: whisper_full (Urdu re-run) failed with code" << ret;
            return {};
        }
    }

    QString result;
    int nSegments = whisper_full_n_segments(m_ctx);
    for (int i = 0; i < nSegments; i++) {
        result += QString::fromUtf8(whisper_full_get_segment_text(m_ctx, i));
    }

    QString trimmed = result.trimmed();
    qInfo() << "Transcribed:" << trimmed;
    return trimmed;
}

bool Transcriber::isLoaded() const
{
    return m_ctx != nullptr;
}

void Transcriber::unload()
{
    if (m_ctx) {
        whisper_free(m_ctx);
        m_ctx = nullptr;
    }
}

void Transcriber::setTranslate(bool translate)
{
    m_translate = translate;
    qInfo() << "Transcriber: translate mode" << (translate ? "ON" : "OFF");
}
