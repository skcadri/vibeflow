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

signals:
    void inputModeChanged(bool useTypeMode);
    void translateModeChanged(bool translate);
    void testPasteRequested();

private:
    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_typeModeAction = nullptr;
    QAction *m_translateAction = nullptr;
};
