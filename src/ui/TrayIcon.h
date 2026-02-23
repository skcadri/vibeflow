#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>

class TrayIcon : public QObject
{
    Q_OBJECT
public:
    explicit TrayIcon(QObject *parent = nullptr);
    ~TrayIcon();

    void show();
    void showMessage(const QString &title, const QString &message);

private:
    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_menu = nullptr;
};
