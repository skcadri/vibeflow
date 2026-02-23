#pragma once

#include <QWidget>
#include <QLabel>
#include <QPropertyAnimation>

class WaveformWidget;

class GlassBubble : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal bubbleOpacity READ bubbleOpacity WRITE setBubbleOpacity)

public:
    enum State { Hidden, Recording, Processing };

    explicit GlassBubble(QWidget *parent = nullptr);
    ~GlassBubble();

    void setState(State state);

    qreal bubbleOpacity() const { return m_opacity; }
    void setBubbleOpacity(qreal opacity);

public slots:
    void updateLevel(float rmsLevel);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void positionBottomCenter();
    void fadeIn();
    void fadeOut();
    void setupVibrancy();

    WaveformWidget *m_waveform = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_indicator = nullptr;
    QPropertyAnimation *m_fadeAnim = nullptr;
    State m_state = Hidden;
    qreal m_opacity = 0.0;
    int m_glassId = -1;
};
