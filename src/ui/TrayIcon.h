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

signals:
    void inputModeChanged(bool useTypeMode);
    void translateModeChanged(bool translate);
    void keepMicActiveChanged(bool keepActive);
    void serverModeChanged(bool enabled);
    void testPasteRequested();
    void recentTranscriptionsRequested();
    void vocabularyRequested();

private:
    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_typeModeAction = nullptr;
    QAction *m_translateAction = nullptr;
    QAction *m_keepMicActiveAction = nullptr;
    QAction *m_serverAction = nullptr;
};
