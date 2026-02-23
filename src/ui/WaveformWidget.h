#pragma once

#include <QWidget>
#include <QVector>
#include <QTimer>

class WaveformWidget : public QWidget
{
    Q_OBJECT
public:
    explicit WaveformWidget(QWidget *parent = nullptr);

    QSize sizeHint() const override;

public slots:
    void updateLevel(float rmsLevel);
    void freeze();
    void reset();

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onAnimationTick();

private:
    static constexpr int BAR_COUNT = 24;
    static constexpr int BAR_WIDTH = 4;
    static constexpr int BAR_GAP = 2;
    static constexpr float BAR_MIN_HEIGHT = 4.0f;
    static constexpr float BAR_MAX_HEIGHT = 40.0f;
    static constexpr float LERP_FACTOR = 0.3f;

    QVector<float> m_barHeights;
    QVector<float> m_targetHeights;
    QTimer m_animTimer;
    float m_level = 0.0f;
    bool m_frozen = false;
};
