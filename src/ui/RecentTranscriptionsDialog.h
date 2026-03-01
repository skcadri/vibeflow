#pragma once

#include <QDialog>
#include <QVector>
#include "../data/SettingsManager.h"

class QListWidget;
class QLabel;

class RecentTranscriptionsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RecentTranscriptionsDialog(SettingsManager *settings, QWidget *parent = nullptr);

private:
    void populateList();
    void onItemClicked(int row);

    SettingsManager *m_settings = nullptr;
    QListWidget *m_listWidget = nullptr;
    QLabel *m_statusLabel = nullptr;
};
