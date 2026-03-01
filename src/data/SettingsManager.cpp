#include "SettingsManager.h"
#include <QSettings>
#include <QVariantList>
#include <QVariantMap>

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent)
{
}

QVector<TranscriptionEntry> SettingsManager::recentTranscriptions() const
{
    QSettings settings;
    QVariantList list = settings.value("history/entries").toList();
    QVector<TranscriptionEntry> entries;
    entries.reserve(list.size());
    for (const auto &v : list) {
        QVariantMap m = v.toMap();
        TranscriptionEntry e;
        e.text = m.value("text").toString();
        e.timestamp = QDateTime::fromMSecsSinceEpoch(m.value("timestamp").toLongLong());
        entries.append(e);
    }
    return entries;
}

void SettingsManager::addTranscription(const QString &text)
{
    if (text.trimmed().isEmpty()) return;

    QSettings settings;
    QVariantList list = settings.value("history/entries").toList();

    QVariantMap entry;
    entry["text"] = text.trimmed();
    entry["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    list.prepend(entry);

    while (list.size() > MaxHistory)
        list.removeLast();

    settings.setValue("history/entries", list);
    settings.sync();
}

void SettingsManager::clearTranscriptions()
{
    QSettings settings;
    settings.remove("history/entries");
    settings.sync();
}

QStringList SettingsManager::vocabulary() const
{
    QSettings settings;
    return settings.value("vocabulary/words").toStringList();
}

void SettingsManager::setVocabulary(const QStringList &words)
{
    QSettings settings;
    settings.setValue("vocabulary/words", words);
    settings.sync();
}

void SettingsManager::addWord(const QString &word)
{
    if (word.trimmed().isEmpty()) return;
    QStringList words = vocabulary();
    QString trimmed = word.trimmed();
    if (!words.contains(trimmed, Qt::CaseInsensitive)) {
        words.append(trimmed);
        setVocabulary(words);
    }
}

void SettingsManager::removeWord(const QString &word)
{
    QStringList words = vocabulary();
    words.removeAll(word);
    setVocabulary(words);
}

QString SettingsManager::buildPromptString() const
{
    QStringList words = vocabulary();
    if (words.isEmpty()) return {};
    return QStringLiteral("Vocabulary: ") + words.join(", ");
}
