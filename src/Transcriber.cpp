#include "Transcriber.h"
#include "transcribe.h"

#include <QDebug>
#include <QRegularExpression>

// Canned phrases Whisper emits on silent/very short audio
static const QStringList s_hallucinations = {
    QStringLiteral("Subtitles by the Amara.org community"),
    QStringLiteral("Thanks for watching!"),
    QStringLiteral("Thank you for watching!"),
    QStringLiteral("Thank you for watching."),
    QStringLiteral("Transcription by ESO. Translation by —"),
    QStringLiteral("ご視聴ありがとうございました"),
};

static bool isHallucination(const QString &text)
{
    for (const auto &h : s_hallucinations) {
        if (text.contains(h, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

// Qwen3-ASR has no Urdu: Urdu speech comes back as Hindi in Devanagari script.
// Any Devanagari in the output means "redo this with Whisper in Urdu".
static bool containsDevanagari(const QString &text)
{
    for (QChar c : text) {
        if (c.unicode() >= 0x0900 && c.unicode() <= 0x097F)
            return true;
    }
    return false;
}

// Pads very short clips to 1s and appends 300ms of silence so the decoder can
// finish the last word cleanly (abruptly-ending speech drops final words).
static std::vector<float> preparePcm(const QVector<float> &audio, int sampleRate)
{
    std::vector<float> pcm(audio.begin(), audio.end());
    if ((int)pcm.size() < sampleRate)
        pcm.resize(sampleRate, 0.0f);
    pcm.resize(pcm.size() + sampleRate * 3 / 10, 0.0f);
    return pcm;
}

// Cleans each segment of the last run (fake speaker labels, canned phrases) and
// drops the ones left empty. Families without timestamps report the whole
// transcript as one segment or none, so fall back to the full text.
static std::vector<TranscriptionSegment> cleanSegments(const transcribe_session *session)
{
    std::vector<TranscriptionSegment> out;
    auto add = [&out](const char *raw, int64_t t0Ms, int64_t t1Ms) {
        const QString text = Transcriber::stripSpeakerLabel(QString::fromUtf8(raw).trimmed());
        if (text.isEmpty() || isHallucination(text))
            return;
        out.push_back({t0Ms / 1000.0f, t1Ms / 1000.0f, text.toStdString()});
    };

    const int n = transcribe_n_segments(session);
    for (int i = 0; i < n; i++) {
        transcribe_segment seg;
        transcribe_segment_init(&seg);
        transcribe_get_segment(session, i, &seg);
        if (seg.text)
            add(seg.text, seg.t0_ms, seg.t1_ms);
    }
    if (n == 0)
        add(transcribe_full_text(session), 0, 0);
    return out;
}

static std::string joinText(const std::vector<TranscriptionSegment> &segments)
{
    std::string text;
    for (const auto &seg : segments) {
        if (!text.empty())
            text += " ";
        text += seg.text;
    }
    return text;
}

Transcriber::Transcriber(QObject *parent)
    : QObject(parent)
{
}

Transcriber::~Transcriber()
{
    unload();
}

void Transcriber::loadModels(const QString &qwenPath, const QString &whisperPath)
{
    auto open = [](const QString &path, transcribe_session **out) -> QString {
        const transcribe_status st = transcribe_open(path.toUtf8().constData(), nullptr, nullptr, out);
        if (st == TRANSCRIBE_OK)
            return {};
        return QString("%1 (%2)").arg(path, QString::fromUtf8(transcribe_status_string(st)));
    };

    std::lock_guard<std::mutex> lock(m_mutex);
    QString error = open(qwenPath, &m_qwen);
    if (error.isEmpty())
        error = open(whisperPath, &m_whisper);
    if (!error.isEmpty()) {
        emit modelLoadFailed("Failed to load " + error);
        return;
    }

    qInfo() << "Models loaded:" << qwenPath << whisperPath
            << "backend:" << transcribe_model_backend(transcribe_get_model(m_qwen));
    emit modelLoaded();
}

// Runs one model with the current vocabulary. language/contextPrompt may be empty.
bool Transcriber::run(transcribe_session *session, const std::vector<float> &pcm,
                      const char *language, const QString &contextPrompt)
{
    std::vector<const char *> vocab;
    for (const auto &w : m_vocabulary)
        vocab.push_back(w.constData());
    const QByteArray prompt = contextPrompt.toUtf8();

    transcribe_run_params params;
    transcribe_run_params_init(&params);
    params.language = language;
    if (!vocab.empty()) {
        params.vocabulary = vocab.data();
        params.n_vocabulary = (int32_t)vocab.size();
    }
    if (!prompt.isEmpty())
        params.prompt = prompt.constData();

    const transcribe_status st = transcribe_run(session, pcm.data(), (int)pcm.size(), &params);
    if (st != TRANSCRIBE_OK) {
        qWarning() << "Transcriber: transcribe_run failed:" << transcribe_status_string(st);
        return false;
    }
    return true;
}

QString Transcriber::transcribe(const QVector<float> &audioSamples, int sampleRate)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_qwen || !m_whisper) {
        qWarning() << "Transcriber: models not loaded";
        return {};
    }
    if (audioSamples.isEmpty()) {
        qWarning() << "Transcriber: empty audio";
        return {};
    }

    const std::vector<float> pcm = preparePcm(audioSamples, sampleRate);

    if (!run(m_qwen, pcm, nullptr, {}))
        return {};
    QString text = QString::fromStdString(joinText(cleanSegments(m_qwen)));
    qInfo() << "Transcriber: Qwen3-ASR detected language:" << transcribe_detected_language(m_qwen);

    if (containsDevanagari(text)) {
        qInfo() << "Transcriber: Hindi script from Qwen, re-running with Whisper as Urdu";
        if (!run(m_whisper, pcm, "ur", {}))
            return {};
        text = QString::fromStdString(joinText(cleanSegments(m_whisper)));
    }

    qInfo() << "Transcribed:" << text;
    return text;
}

TranscriptionResult Transcriber::transcribeWithSegments(
    const QVector<float> &audioSamples, int sampleRate, const QString &contextPrompt)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    TranscriptionResult result;
    if (!m_whisper) {
        qWarning() << "Transcriber: models not loaded";
        return result;
    }
    if (audioSamples.isEmpty()) {
        qWarning() << "Transcriber: empty audio";
        return result;
    }

    if (!run(m_whisper, preparePcm(audioSamples, sampleRate), nullptr, contextPrompt))
        return result;

    result.segments = cleanSegments(m_whisper);
    result.text = joinText(result.segments);
    qInfo() << "TranscribeWithSegments:" << result.segments.size() << "segments,"
            << result.text.size() << "chars";
    return result;
}

bool Transcriber::isLoaded() const
{
    return m_qwen && m_whisper;
}

void Transcriber::unload()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    transcribe_session_free(m_qwen); // sessions from transcribe_open own their model
    transcribe_session_free(m_whisper);
    m_qwen = nullptr;
    m_whisper = nullptr;
}

void Transcriber::setVocabulary(const QStringList &words)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_vocabulary.clear();
    for (const auto &w : words)
        m_vocabulary.push_back(w.toUtf8());
    qInfo() << "Transcriber: vocabulary set to" << words;
}

// Removes a leading speaker-attribution label that Whisper hallucinates
// (e.g. "Tim Cynova: ", "Artyaz, Mathias, Salman, Oswego, : ").
// Spoken dictation never contains a literal colon, so a leading run of 1–6
// capitalized words (optionally comma-separated) followed by a colon is a model
// artifact, not user speech. Callers must apply this per segment — the label
// can start any segment, not just the first.
// Applied repeatedly to peel off stacked labels ("Tim Cynova: Tim Cynova: ...").
QString Transcriber::stripSpeakerLabel(const QString &text)
{
    static const QRegularExpression re(
        QStringLiteral(u"^\\s*[A-Z][\\w'’.\\-]*(?:,?\\s+[A-Z][\\w'’.\\-]*){0,5},?\\s*:\\s*"));
    QString out = text;
    QString prev;
    do {
        prev = out;
        out.remove(re); // `^` anchor means only the leading label is matched
    } while (out != prev && !out.isEmpty());
    return out;
}
