#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>

class TrayIcon : public QObject
{
    Q_OBJECT
public:
    explicit TrayIcon(QObject *parent = nullptr);
    ~TrayIcon();

    void show();
    void showMessage(const QString &title, const QString &message);
    void setServerPort(int port); // 0 = off, >0 = running on port
    void setLivePreviewChecked(bool checked); // initial state; doesn't emit

signals:
    void inputModeChanged(bool useTypeMode);
    void keepMicActiveChanged(bool keepActive);
    void serverModeChanged(bool enabled);
    void livePreviewChanged(bool enabled);
    void testPasteRequested();
    void recentTranscriptionsRequested();
    void vocabularyRequested();

private:
    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_typeModeAction = nullptr;
    QAction *m_keepMicActiveAction = nullptr;
    QAction *m_serverAction = nullptr;
    QAction *m_livePreviewAction = nullptr;
};
