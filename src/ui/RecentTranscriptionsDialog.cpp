#include "RecentTranscriptionsDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QClipboard>
#include <QApplication>
#include <QTimer>

RecentTranscriptionsDialog::RecentTranscriptionsDialog(SettingsManager *settings, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle("Recent Transcriptions");
    resize(500, 400);

    auto *layout = new QVBoxLayout(this);

    m_listWidget = new QListWidget(this);
    m_listWidget->setWordWrap(true);
    m_listWidget->setAlternatingRowColors(true);
    layout->addWidget(m_listWidget);

    connect(m_listWidget, &QListWidget::currentRowChanged, this, &RecentTranscriptionsDialog::onItemClicked);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: green; font-weight: bold;");
    layout->addWidget(m_statusLabel);

    auto *buttonLayout = new QHBoxLayout();
    auto *refreshBtn = new QPushButton("Refresh", this);
    auto *clearBtn = new QPushButton("Clear All", this);
    auto *closeBtn = new QPushButton("Close", this);
    buttonLayout->addWidget(refreshBtn);
    buttonLayout->addWidget(clearBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeBtn);
    layout->addLayout(buttonLayout);

    connect(refreshBtn, &QPushButton::clicked, this, [this]() {
        populateList();
        m_statusLabel->setText("Refreshed.");
        QTimer::singleShot(2000, this, [this]() { m_statusLabel->clear(); });
    });
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        m_settings->clearTranscriptions();
        m_listWidget->clear();
        m_statusLabel->setText("History cleared.");
    });
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    populateList();
}

void RecentTranscriptionsDialog::populateList()
{
    m_listWidget->clear();
    auto entries = m_settings->recentTranscriptions();

    if (entries.isEmpty()) {
        m_listWidget->addItem("No transcriptions yet.");
        m_listWidget->item(0)->setFlags(Qt::NoItemFlags);
        return;
    }

    for (const auto &entry : entries) {
        QString label = entry.timestamp.toString("yyyy-MM-dd hh:mm") + " — " + entry.text;
        m_listWidget->addItem(label);
    }
}

void RecentTranscriptionsDialog::onItemClicked(int row)
{
    if (row < 0) return;

    auto entries = m_settings->recentTranscriptions();
    if (row >= entries.size()) return;

    QApplication::clipboard()->setText(entries[row].text);
    m_statusLabel->setText("Copied to clipboard!");

    QTimer::singleShot(2000, this, [this]() {
        m_statusLabel->clear();
    });
}
