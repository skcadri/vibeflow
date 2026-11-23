#pragma once

#include <QWidget>
#include <QTextDocument>

class QPropertyAnimation;

// Glass panel above the waveform pill showing what you're saying while you
// dictate. Width is a fixed share of the screen; height grows upward with the
// text (bottom stays anchored above the pill) up to a cap, after which the
// oldest lines scroll off the top.
class LiveTextBubble : public QWidget
{
    Q_OBJECT
public:
    explicit LiveTextBubble(QWidget *anchor);
    ~LiveTextBubble();

public slots:
    void setText(const QString &committed, const QString &tentative);
    void hideNow();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void setDocument(const QString &bright, const QString &dim);

    QWidget *m_anchor;
    QTextDocument m_doc;
    QPropertyAnimation *m_fade = nullptr; // fade-in only; hiding is instant
    int m_glassId = -1;
};
