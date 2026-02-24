#include "TrayIcon.h"

#include <QApplication>
#include <QPixmap>
#include <QPainter>

TrayIcon::TrayIcon(QObject *parent)
    : QObject(parent)
{
    m_trayIcon = new QSystemTrayIcon(this);

    // Create a simple mic icon programmatically (template image for macOS)
    QPixmap pixmap(22, 22);
    pixmap.fill(Qt::transparent);
    {
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::black);

        // Mic body (rounded rect)
        p.drawRoundedRect(8, 3, 6, 10, 3, 3);

        // Mic arc
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(Qt::black, 1.5));
        p.drawArc(5, 6, 12, 10, 0, 180 * 16);

        // Mic stand
        p.drawLine(11, 16, 11, 19);
        p.drawLine(7, 19, 15, 19);
    }

    QIcon icon(pixmap);
    icon.setIsMask(true); // macOS template image — adapts to dark/light menu bar

    m_trayIcon->setIcon(icon);
    m_trayIcon->setToolTip("VibeFlow — Hold ⌘+Ctrl to dictate");

    // Context menu
    m_menu = new QMenu();
    m_typeModeAction = m_menu->addAction("Type at Cursor");
    m_typeModeAction->setCheckable(true);
    m_typeModeAction->setChecked(true);
    connect(m_typeModeAction, &QAction::toggled, this, &TrayIcon::inputModeChanged);
    m_menu->addSeparator();
    m_menu->addAction("About VibeFlow", []() {
        // Minimal — just a placeholder
    });
    m_menu->addSeparator();
    m_menu->addAction("Quit", qApp, &QApplication::quit);

    m_trayIcon->setContextMenu(m_menu);
}

TrayIcon::~TrayIcon()
{
    delete m_menu;
}

void TrayIcon::show()
{
    m_trayIcon->show();
}

void TrayIcon::showMessage(const QString &title, const QString &message)
{
    m_trayIcon->showMessage(title, message, QSystemTrayIcon::Information, 3000);
}
