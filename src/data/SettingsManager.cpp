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

// Whisper reads the prompt as "what was said just before" and copies its shape.
// Never format it as a "Label: name, name, ..." header — that taught the model to
// emit fake speaker tags made of names ("Artyaz, Mathias, Salman, Oswego, : ...").
// A plain spoken sentence with no colon still biases spelling without that shape.
QString SettingsManager::buildPromptString() const
{
    QStringList words = vocabulary();
    if (words.isEmpty()) return {};
    const QString last = words.takeLast();
    const QString list = words.isEmpty() ? last : words.join(", ") + " and " + last;
    return QStringLiteral("I mentioned ") + list + QStringLiteral(".");
}
