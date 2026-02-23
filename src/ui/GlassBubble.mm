#include "GlassBubble.h"
#include "WaveformWidget.h"

#import <AppKit/AppKit.h>

#include <QHBoxLayout>
#include <QScreen>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>

GlassBubble::GlassBubble(QWidget *parent)
    : QWidget(parent)
{
    // Window flags: frameless, always on top, tool window (no dock icon), no focus stealing
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                   | Qt::Tool | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);

    setFixedSize(300, 56);

    // Layout
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 8, 16, 8);
    layout->setSpacing(8);

    // Status indicator dot
    m_indicator = new QLabel(this);
    m_indicator->setFixedSize(8, 8);
    m_indicator->setStyleSheet("background-color: #FF3B30; border-radius: 4px;");
    layout->addWidget(m_indicator);

    // Waveform
    m_waveform = new WaveformWidget(this);
    layout->addWidget(m_waveform, 1);

    // Status label
    m_statusLabel = new QLabel("Listening...", this);
    m_statusLabel->setStyleSheet("color: rgba(255, 255, 255, 0.9); font-size: 13px; font-weight: 500;");
    layout->addWidget(m_statusLabel);

    // Fade animation
    m_fadeAnim = new QPropertyAnimation(this, "bubbleOpacity", this);

    positionBottomCenter();
    setWindowOpacity(0.0);

    setupVibrancy();
}

GlassBubble::~GlassBubble()
{
}

void GlassBubble::setupVibrancy()
{
    // Apply NSVisualEffectView for frosted glass behind the Qt widget
    NSView *nsview = reinterpret_cast<NSView *>(winId());
    if (!nsview) return;

    NSWindow *nswindow = [nsview window];
    if (!nswindow) return;

    // Make the window non-opaque for vibrancy to work
    [nswindow setOpaque:NO];
    [nswindow setBackgroundColor:[NSColor clearColor]];

    // Create visual effect view
    NSVisualEffectView *vibrant = [[NSVisualEffectView alloc] initWithFrame:[nsview bounds]];
    [vibrant setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [vibrant setBlendingMode:NSVisualEffectBlendingModeBehindWindow];
    [vibrant setMaterial:NSVisualEffectMaterialHUDWindow];
    [vibrant setState:NSVisualEffectStateActive];
    [vibrant setWantsLayer:YES];
    vibrant.layer.cornerRadius = 28.0;
    vibrant.layer.masksToBounds = YES;

    // Insert the vibrancy view behind the Qt content
    [nsview addSubview:vibrant positioned:NSWindowBelow relativeTo:nil];
}

void GlassBubble::setState(State state)
{
    if (m_state == state) return;
    m_state = state;

    switch (state) {
    case Hidden:
        m_waveform->reset();
        fadeOut();
        break;

    case Recording:
        m_indicator->setStyleSheet("background-color: #FF3B30; border-radius: 4px;"); // Red
        m_statusLabel->setText("Listening...");
        m_waveform->reset();
        positionBottomCenter();
        show();
        fadeIn();
        break;

    case Processing:
        m_indicator->setStyleSheet("background-color: #FF9500; border-radius: 4px;"); // Amber
        m_statusLabel->setText("Transcribing...");
        m_waveform->freeze();
        break;
    }
}

void GlassBubble::updateLevel(float rmsLevel)
{
    if (m_state == Recording) {
        m_waveform->updateLevel(rmsLevel);
    }
}

void GlassBubble::setBubbleOpacity(qreal opacity)
{
    m_opacity = opacity;
    setWindowOpacity(opacity);
}

void GlassBubble::positionBottomCenter()
{
    QScreen *screen = QApplication::primaryScreen();
    if (!screen) return;

    QRect geo = screen->geometry();
    int x = geo.x() + (geo.width() - width()) / 2;
    int y = geo.y() + geo.height() - height() - 48;
    move(x, y);
}

void GlassBubble::fadeIn()
{
    m_fadeAnim->stop();
    m_fadeAnim->setDuration(200);
    m_fadeAnim->setStartValue(0.0);
    m_fadeAnim->setEndValue(1.0);
    m_fadeAnim->setEasingCurve(QEasingCurve::OutCubic);
    m_fadeAnim->start();
}

void GlassBubble::fadeOut()
{
    m_fadeAnim->stop();
    m_fadeAnim->setDuration(150);
    m_fadeAnim->setStartValue(m_opacity);
    m_fadeAnim->setEndValue(0.0);
    m_fadeAnim->setEasingCurve(QEasingCurve::InCubic);
    connect(m_fadeAnim, &QPropertyAnimation::finished, this, [this]() {
        if (m_opacity <= 0.01)
            hide();
    });
    m_fadeAnim->start();
}

void GlassBubble::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Semi-transparent dark background as fallback (vibrancy view is behind)
    QPainterPath path;
    path.addRoundedRect(rect(), 28, 28);

    painter.fillPath(path, QColor(30, 30, 30, 180));
}
