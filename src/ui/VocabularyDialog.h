#pragma once

#include <QDialog>
#include "../data/SettingsManager.h"

class QListWidget;
class QLineEdit;

class VocabularyDialog : public QDialog
{
    Q_OBJECT
public:
    explicit VocabularyDialog(SettingsManager *settings, QWidget *parent = nullptr);

private:
    void populateList();

    SettingsManager *m_settings = nullptr;
    QListWidget *m_listWidget = nullptr;
    QLineEdit *m_lineEdit = nullptr;
};
