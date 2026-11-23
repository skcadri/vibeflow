#include "LivePreview.h"
#include "transcribe.h"
#include "transcribe/parakeet.h"

#include <QDebug>
#include <QMetaObject>

// Nemotron lookahead: 6 = 480 ms (text trails speech by ~0.6 s, same accuracy as
// the slowest setting). Menu is {0, 1, 6, 13}; see the model doc in
// deps/transcribe.cpp/docs/models/nemotron-speech-streaming-en-0.6b.md.
static constexpr int kLookahead = 6;

LivePreview::LivePreview(QObject *parent)
    : QObject(parent)
{
}

LivePreview::~LivePreview()
{
    transcribe_session_free(m_session); // owns its model (transcribe_open)
}

void LivePreview::load(const QString &modelPath)
{
    const transcribe_status st = transcribe_open(modelPath.toUtf8().constData(), nullptr, nullptr, &m_session);
    if (st != TRANSCRIBE_OK) {
        qWarning() << "LivePreview: failed to load" << modelPath << transcribe_status_string(st);
        m_session = nullptr;
        return;
    }
    qInfo() << "LivePreview: loaded" << modelPath;
}

void LivePreview::start()
{
    m_active = true;
    QMetaObject::invokeMethod(this, &LivePreview::begin, Qt::QueuedConnection);
}

void LivePreview::stop()
{
    m_active = false;
}

void LivePreview::begin()
{
    if (!m_session)
        return;
    transcribe_stream_reset(m_session);

    transcribe_parakeet_stream_ext ext;
    transcribe_parakeet_stream_ext_init(&ext);
    ext.att_context_right = kLookahead;

    transcribe_stream_params params;
    transcribe_stream_params_init(&params);
    params.family = &ext.ext;

    const transcribe_status st = transcribe_stream_begin(m_session, nullptr, &params);
    if (st != TRANSCRIBE_OK)
        qWarning() << "LivePreview: stream_begin failed:" << transcribe_status_string(st);
}

void LivePreview::feed(const QVector<float> &samples)
{
    if (!m_active || !m_session || samples.isEmpty()
        || transcribe_stream_get_state(m_session) != TRANSCRIBE_STREAM_ACTIVE)
        return;

    transcribe_stream_update update;
    transcribe_stream_update_init(&update);
    const transcribe_status st = transcribe_stream_feed(m_session, samples.constData(), samples.size(), &update);
    if (st != TRANSCRIBE_OK) {
        qWarning() << "LivePreview: stream_feed failed:" << transcribe_status_string(st);
        return;
    }
    if (!update.committed_changed && !update.tentative_changed)
        return;

    transcribe_stream_text text;
    transcribe_stream_text_init(&text);
    if (transcribe_stream_get_text(m_session, &text) != TRANSCRIBE_OK)
        return;
    emit textChanged(QString::fromUtf8(text.committed_text, (qsizetype)text.committed_text_bytes),
                     QString::fromUtf8(text.tentative_text, (qsizetype)text.tentative_text_bytes));
}
