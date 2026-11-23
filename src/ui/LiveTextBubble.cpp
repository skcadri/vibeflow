#include "LiveTextBubble.h"
#include "GlassBubble.h"

#include <QtLiquidGlass/QtLiquidGlass.h>

#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QScreen>
#include <QTextCursor>
#include <QtMath>

// Look & feel knobs.
static constexpr double kWidthShare = 0.45;          // of the screen width
static constexpr double kMaxHeightShare = 1.0 / 3.0; // of the screen height
static constexpr int kFontPx = 16;
static constexpr int kPadX = 20;
static constexpr int kPadY = 14;
static constexpr int kGapAbovePill = 10;
static const QColor kSettledColor(255, 255, 255, 242);
static const QColor kTentativeColor(255, 255, 255, 120);
static const char *kGlassTint = "#B0101010";         // dark tint keeps white text readable over light windows

LiveTextBubble::LiveTextBubble(QWidget *anchor)
    : QWidget(nullptr)
    , m_anchor(anchor)
{
    // Same window behaviour as the pill: frameless, on top, never takes focus
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                   | Qt::Tool | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);

    QFont font = QApplication::font();
    font.setPixelSize(kFontPx);
    font.setWeight(QFont::Medium);
    m_doc.setDefaultFont(font);
    m_doc.setDocumentMargin(0);

    m_fade = new QPropertyAnimation(this, "windowOpacity", this);
    m_fade->setDuration(150);
    m_fade->setEndValue(1.0);

    QtLiquidGlass::Options opts;
    opts.cornerRadius = 18.0;
    opts.blendingMode = QtLiquidGlass::BlendingMode::BehindWindow;
    opts.appearance = QtLiquidGlass::AdaptiveAppearance::Dark;
    opts.tintColor = kGlassTint;
    m_glassId = QtLiquidGlass::addGlassEffect(this, QtLiquidGlass::Material::Hud, opts);

    setWindowOpacity(0.0);
}

LiveTextBubble::~LiveTextBubble()
{
    if (m_glassId >= 0)
        QtLiquidGlass::remove(m_glassId);
}

void LiveTextBubble::setText(const QString &committed, const QString &tentative)
{
    QString bright = committed;
    QString dim = tentative;
    while (!bright.isEmpty() && bright.front().isSpace())
        bright.remove(0, 1);
    if (bright.isEmpty()) {
        while (!dim.isEmpty() && dim.front().isSpace())
            dim.remove(0, 1);
    }
    if (bright.isEmpty() && dim.isEmpty())
        return;

    setDocument(bright, dim);
    if (!isVisible()) {
        GlassBubble::showAboveEverything(this);
        m_fade->setStartValue(0.0);
        m_fade->start();
    }
}

void LiveTextBubble::hideNow()
{
    m_fade->stop();
    hide();
    setWindowOpacity(0.0);
}

void LiveTextBubble::setDocument(const QString &bright, const QString &dim)
{
    m_doc.clear();
    QTextCursor cursor(&m_doc);
    QTextCharFormat format;
    format.setForeground(kSettledColor);
    cursor.insertText(bright, format);
    format.setForeground(kTentativeColor);
    cursor.insertText(dim, format);

    QScreen *screen = QApplication::primaryScreen();
    if (!screen)
        return;
    const QRect avail = screen->availableGeometry();
    const int w = int(avail.width() * kWidthShare);
    m_doc.setTextWidth(w - 2 * kPadX);
    const int h = qMin(qCeil(m_doc.size().height()) + 2 * kPadY, int(avail.height() * kMaxHeightShare));
    setFixedSize(w, h);
    move(avail.x() + (avail.width() - w) / 2, m_anchor->y() - kGapAbovePill - h);
    update();
}

void LiveTextBubble::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_glassId < 0) { // liquid glass unavailable: plain dark fallback, like the pill
        QPainterPath path;
        path.addRoundedRect(rect(), 18, 18);
        painter.fillPath(path, QColor(30, 30, 30, 180));
    }

    // Bottom-align the text so the newest words sit just above the pill and,
    // once the panel hits its height cap, the oldest lines clip off the top.
    painter.setClipRect(rect().adjusted(kPadX, kPadY, -kPadX, -kPadY));
    painter.translate(kPadX, height() - kPadY - qCeil(m_doc.size().height()));
    m_doc.drawContents(&painter);
}
