#pragma once

#include <QString>

class TextPaster
{
public:
    static bool canSimulatePaste();
    static qint64 frontmostAppPid();
    static bool paste(const QString &text);
    static bool pasteToPid(const QString &text, qint64 targetPid);
    static bool typeText(const QString &text);
    static bool typeAtCursor(const QString &text, qint64 targetPid);
    static bool activateApp(qint64 pid, int timeoutMs = 500);
};
