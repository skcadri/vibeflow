#include "VocabularyDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>

VocabularyDialog::VocabularyDialog(SettingsManager *settings, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle("Vocabulary");
    resize(400, 350);

    auto *layout = new QVBoxLayout(this);

    auto *header = new QLabel(
        "Add words or phrases that you commonly use. These help improve "
        "transcription accuracy for domain-specific terminology.", this);
    header->setWordWrap(true);
    layout->addWidget(header);

    // Add word row
    auto *addLayout = new QHBoxLayout();
    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setPlaceholderText("Type a word or phrase...");
    auto *addBtn = new QPushButton("Add", this);
    addLayout->addWidget(m_lineEdit);
    addLayout->addWidget(addBtn);
    layout->addLayout(addLayout);

    auto addWord = [this]() {
        QString word = m_lineEdit->text().trimmed();
        if (word.isEmpty()) return;
        m_settings->addWord(word);
        m_lineEdit->clear();
        populateList();
    };

    connect(addBtn, &QPushButton::clicked, this, addWord);
    connect(m_lineEdit, &QLineEdit::returnPressed, this, addWord);

    // Word list
    m_listWidget = new QListWidget(this);
    m_listWidget->setSelectionMode(QListWidget::ExtendedSelection);
    layout->addWidget(m_listWidget);

    // Bottom buttons
    auto *buttonLayout = new QHBoxLayout();
    auto *removeBtn = new QPushButton("Remove Selected", this);
    auto *closeBtn = new QPushButton("Close", this);
    buttonLayout->addWidget(removeBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeBtn);
    layout->addLayout(buttonLayout);

    connect(removeBtn, &QPushButton::clicked, this, [this]() {
        auto selected = m_listWidget->selectedItems();
        for (auto *item : selected) {
            m_settings->removeWord(item->text());
        }
        populateList();
    });
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    populateList();
}

void VocabularyDialog::populateList()
{
    m_listWidget->clear();
    QStringList words = m_settings->vocabulary();
    for (const auto &word : words) {
        m_listWidget->addItem(word);
    }
}
