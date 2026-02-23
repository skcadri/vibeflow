#include "WaveformWidget.h"

#include <QPainter>
#include <QPainterPath>
#include <cmath>
#include <cstdlib>

WaveformWidget::WaveformWidget(QWidget *parent)
    : QWidget(parent)
    , m_barHeights(BAR_COUNT, BAR_MIN_HEIGHT)
    , m_targetHeights(BAR_COUNT, BAR_MIN_HEIGHT)
{
    m_animTimer.setInterval(33); // ~30fps
    connect(&m_animTimer, &QTimer::timeout, this, &WaveformWidget::onAnimationTick);
    m_animTimer.start();

    setAttribute(Qt::WA_TranslucentBackground);
}

QSize WaveformWidget::sizeHint() const
{
    int w = BAR_COUNT * BAR_WIDTH + (BAR_COUNT - 1) * BAR_GAP;
    return QSize(w, static_cast<int>(BAR_MAX_HEIGHT));
}

void WaveformWidget::updateLevel(float rmsLevel)
{
    if (m_frozen) return;

    m_level = rmsLevel;

    // Compress the dynamic range so very quiet microphones still animate visibly.
    float normalizedLevel = qBound(0.0f, std::sqrt(qMax(0.0f, rmsLevel)) * 6.0f, 1.0f);

    for (int i = 0; i < BAR_COUNT; i++) {
        // Create a wave-like distribution centered in the middle
        float centerDist = std::abs(i - BAR_COUNT / 2.0f) / (BAR_COUNT / 2.0f);
        float envelope = 1.0f - centerDist * 0.5f;

        // Add some per-bar randomness
        float randomFactor = 0.7f + (std::rand() % 30) / 100.0f;

        float target = BAR_MIN_HEIGHT + (BAR_MAX_HEIGHT - BAR_MIN_HEIGHT)
                        * normalizedLevel * envelope * randomFactor;
        m_targetHeights[i] = qBound(BAR_MIN_HEIGHT, target, BAR_MAX_HEIGHT);
    }
}

void WaveformWidget::freeze()
{
    m_frozen = true;
}

void WaveformWidget::reset()
{
    m_frozen = false;
    m_level = 0.0f;
    m_barHeights.fill(BAR_MIN_HEIGHT);
    m_targetHeights.fill(BAR_MIN_HEIGHT);
    update();
}

void WaveformWidget::onAnimationTick()
{
    bool needsUpdate = false;

    for (int i = 0; i < BAR_COUNT; i++) {
        float diff = m_targetHeights[i] - m_barHeights[i];
        if (std::abs(diff) > 0.5f) {
            m_barHeights[i] += diff * LERP_FACTOR;
            needsUpdate = true;
        }
    }

    // If not frozen and level is low, slowly decay targets
    if (!m_frozen && m_level < 0.01f) {
        for (int i = 0; i < BAR_COUNT; i++) {
            m_targetHeights[i] = BAR_MIN_HEIGHT;
        }
        needsUpdate = true;
    }

    if (needsUpdate)
        update();
}

void WaveformWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor barColor(255, 255, 255, 204); // White at 80% opacity
    painter.setBrush(barColor);
    painter.setPen(Qt::NoPen);

    int totalWidth = BAR_COUNT * BAR_WIDTH + (BAR_COUNT - 1) * BAR_GAP;
    int startX = (width() - totalWidth) / 2;
    int baseY = height();

    for (int i = 0; i < BAR_COUNT; i++) {
        int x = startX + i * (BAR_WIDTH + BAR_GAP);
        float h = m_barHeights[i];
        int y = baseY - static_cast<int>(h);

        QPainterPath path;
        path.addRoundedRect(QRectF(x, y, BAR_WIDTH, h), 2.0, 2.0);
        painter.drawPath(path);
    }
}
