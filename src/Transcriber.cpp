#include "Transcriber.h"
#include "whisper.h"

#include <QDebug>
#include <thread>

// Shared hallucination phrases — Whisper emits these on silent/very short audio
static const QStringList s_hallucinations = {
    QStringLiteral("Subtitles by the Amara.org community"),
    QStringLiteral("Thanks for watching!"),
    QStringLiteral("Thank you for watching!"),
    QStringLiteral("Thank you for watching."),
    QStringLiteral("Transcription by ESO. Translation by —"),
    QStringLiteral("ご視聴ありがとうございました"),
};

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

whisper_full_params Transcriber::makeDefaultParams() const
{
    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_BEAM_SEARCH);
    params.beam_search.beam_size = 5;
    params.language = "auto";
    params.n_threads = std::min(16, (int)std::thread::hardware_concurrency());
    params.no_timestamps = false;
    params.translate = m_translate;
    params.print_progress = false;
    params.print_realtime = false;
    params.print_special = false;
    params.print_timestamps = false;
    params.suppress_blank = true;
    params.entropy_thold = 2.4f;
    params.logprob_thold = -1.0f;
    return params;
}

bool Transcriber::isHallucination(const QString &text)
{
    for (const auto &h : s_hallucinations) {
        if (text.contains(h, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

QString Transcriber::transcribe(const QVector<float> &audioSamples, int sampleRate)
{
    std::lock_guard<std::mutex> lock(m_mutex);

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

    // Append trailing silence so Whisper's decoder can cleanly finish the last segment.
    // Without this, abruptly-ending speech causes the final words to be dropped.
    const int padSamples = sampleRate * 3 / 10; // 300ms
    audio.resize(audio.size() + padSamples, 0.0f);

    whisper_full_params params = makeDefaultParams();

    // Apply vocabulary prompt if set — QByteArray must stay in scope during whisper_full()
    QByteArray promptUtf8;
    if (!m_initialPrompt.isEmpty()) {
        promptUtf8 = m_initialPrompt.toUtf8();
        params.initial_prompt = promptUtf8.constData();
    }

    int ret = whisper_full(m_ctx, params, audio.constData(), audio.size());

    if (ret != 0) {
        qWarning() << "Transcriber: whisper_full failed with code" << ret;
        return {};
    }

    // Check detected language
    int detectedLangId = whisper_full_lang_id(m_ctx);
    const char *detectedLang = whisper_lang_str(detectedLangId);
    qInfo() << "Transcriber: detected language:" << detectedLang
            << "segments:" << whisper_full_n_segments(m_ctx);

    // Suppress Hindi — if Whisper detected Hindi, re-run forced as Urdu
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

    if (isHallucination(trimmed)) {
        qInfo() << "Transcriber: suppressed hallucination:" << trimmed;
        return {};
    }

    qInfo() << "Transcribed:" << trimmed;
    return trimmed;
}

TranscriptionResult Transcriber::transcribeWithSegments(
    const QVector<float> &audioSamples, int sampleRate, const QString &overridePrompt)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    TranscriptionResult result;

    if (!m_ctx) {
        qWarning() << "Transcriber: model not loaded";
        return result;
    }

    if (audioSamples.isEmpty()) {
        qWarning() << "Transcriber: empty audio";
        return result;
    }

    // Pad audio shorter than 1 second
    const int minSamples = sampleRate;
    QVector<float> audio = audioSamples;
    if (audio.size() < minSamples) {
        audio.resize(minSamples, 0.0f);
    }

    // Append trailing silence
    const int padSamples = sampleRate * 3 / 10; // 300ms
    audio.resize(audio.size() + padSamples, 0.0f);

    whisper_full_params params = makeDefaultParams();
    params.tdrz_enable = true; // speaker turn detection

    // Use overridePrompt if provided (e.g. context carry-forward), else member prompt
    QByteArray promptUtf8;
    const QString &prompt = overridePrompt.isEmpty() ? m_initialPrompt : overridePrompt;
    if (!prompt.isEmpty()) {
        promptUtf8 = prompt.toUtf8();
        params.initial_prompt = promptUtf8.constData();
    }

    int ret = whisper_full(m_ctx, params, audio.constData(), audio.size());
    if (ret != 0) {
        qWarning() << "Transcriber: whisper_full failed with code" << ret;
        return result;
    }

    int nSegments = whisper_full_n_segments(m_ctx);
    std::string fullText;

    for (int i = 0; i < nSegments; i++) {
        const char *segText = whisper_full_get_segment_text(m_ctx, i);
        QString segQStr = QString::fromUtf8(segText).trimmed();

        // Per-segment hallucination filter
        if (isHallucination(segQStr))
            continue;

        int64_t t0 = whisper_full_get_segment_t0(m_ctx, i); // centiseconds
        int64_t t1 = whisper_full_get_segment_t1(m_ctx, i);
        bool speakerTurn = whisper_full_get_segment_speaker_turn_next(m_ctx, i);

        TranscriptionSegment seg;
        seg.start = t0 / 100.0f;
        seg.end = t1 / 100.0f;
        seg.text = segQStr.toStdString();
        seg.speakerTurnNext = speakerTurn;
        result.segments.push_back(std::move(seg));

        if (!fullText.empty())
            fullText += " ";
        fullText += seg.text;
    }

    result.text = fullText;
    qInfo() << "TranscribeWithSegments:" << result.segments.size() << "segments,"
            << result.text.size() << "chars";
    return result;
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

void Transcriber::setInitialPrompt(const QString &prompt)
{
    m_initialPrompt = prompt;
    qInfo() << "Transcriber: initial prompt set to" << (prompt.isEmpty() ? "(empty)" : prompt);
}
