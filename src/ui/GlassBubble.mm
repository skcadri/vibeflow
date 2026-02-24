#include "GlassBubble.h"
#include "WaveformWidget.h"

#include <QtLiquidGlass/QtLiquidGlass.h>

#include <QHBoxLayout>
#include <QScreen>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QWindow>

#import <AppKit/AppKit.h>

GlassBubble::GlassBubble(QWidget *parent)
    : QWidget(parent)
{
    // Window flags: frameless, always on top, tool window (no dock icon), no focus stealing
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                   | Qt::Tool | Qt::WindowDoesNotAcceptFocus
                   | Qt::WindowTransparentForInput);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_MacAlwaysShowToolWindow);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);

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
    if (m_glassId >= 0) {
        QtLiquidGlass::remove(m_glassId);
    }
}

void GlassBubble::setupVibrancy()
{
    QtLiquidGlass::Options opts;
    opts.cornerRadius = 28.0;
    opts.blendingMode = QtLiquidGlass::BlendingMode::BehindWindow;
    opts.appearance = QtLiquidGlass::AdaptiveAppearance::Dark;

    m_glassId = QtLiquidGlass::addGlassEffect(this, QtLiquidGlass::Material::Hud, opts);

    if (m_glassId < 0) {
        qWarning() << "GlassBubble: failed to apply liquid glass effect, using fallback";
    }
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
        raise();
        // Set NSWindow level above everything (including other always-on-top windows)
        if (windowHandle()) {
            NSView *nsView = (__bridge NSView *)reinterpret_cast<void *>(windowHandle()->winId());
            if (nsView && nsView.window) {
                [nsView.window setLevel:NSScreenSaverWindowLevel];
                [nsView.window setIgnoresMouseEvents:YES];
                [nsView.window orderWindow:NSWindowAbove relativeTo:0];
                [nsView.window setCollectionBehavior:
                    NSWindowCollectionBehaviorCanJoinAllSpaces |
                    NSWindowCollectionBehaviorStationary |
                    NSWindowCollectionBehaviorFullScreenAuxiliary];
            }
        }
        fadeIn();
        break;

    case Processing:
        m_indicator->setStyleSheet("background-color: #FF9500; border-radius: 4px;"); // Amber
        m_statusLabel->setText("Transcribing...");
        m_waveform->freeze();
        break;
    }
}

void GlassBubble::hideImmediately()
{
    m_fadeAnim->stop();
    m_state = Hidden;
    m_waveform->reset();

    if (windowHandle()) {
        NSView *nsView = (__bridge NSView *)reinterpret_cast<void *>(windowHandle()->winId());
        if (nsView && nsView.window) {
            [nsView.window setLevel:NSNormalWindowLevel];
            [nsView.window orderOut:nil];
        }
    }

    setWindowOpacity(0.0);
    m_opacity = 0.0;
    hide();
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

    // Use availableGeometry to position above the Dock
    QRect avail = screen->availableGeometry();
    int x = avail.x() + (avail.width() - width()) / 2;
    int y = avail.y() + avail.height() - height() - 16; // 16px above Dock
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
    m_fadeAnim->disconnect(); // Clean up previous connections
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
    // Only draw fallback background if liquid glass failed
    if (m_glassId < 0) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QPainterPath path;
        path.addRoundedRect(rect(), 28, 28);
        painter.fillPath(path, QColor(30, 30, 30, 180));
    }
}
