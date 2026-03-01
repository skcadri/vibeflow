#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QVector>

struct TranscriptionEntry {
    QString text;
    QDateTime timestamp;
};

class SettingsManager : public QObject
{
    Q_OBJECT
public:
    explicit SettingsManager(QObject *parent = nullptr);

    // Recent Transcriptions
    QVector<TranscriptionEntry> recentTranscriptions() const;
    void addTranscription(const QString &text);
    void clearTranscriptions();

    // Vocabulary
    QStringList vocabulary() const;
    void setVocabulary(const QStringList &words);
    void addWord(const QString &word);
    void removeWord(const QString &word);
    QString buildPromptString() const;

    static constexpr int MaxHistory = 50;
};
