#include <QApplication>
#include <QLoggingCategory>
#include <cstdio>
#include "App.h"

static void messageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    (void)ctx;
    const char *prefix = "";
    switch (type) {
    case QtDebugMsg:    prefix = "[DEBUG]"; break;
    case QtInfoMsg:     prefix = "[INFO]"; break;
    case QtWarningMsg:  prefix = "[WARN]"; break;
    case QtCriticalMsg: prefix = "[ERROR]"; break;
    case QtFatalMsg:    prefix = "[FATAL]"; break;
    }
    fprintf(stderr, "%s %s\n", prefix, msg.toUtf8().constData());
    fflush(stderr);
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(messageHandler);

    fprintf(stderr, "[INFO] VibeFlow starting...\n");
    fflush(stderr);

    QApplication app(argc, argv);
    fprintf(stderr, "[INFO] QApplication created\n");
    fflush(stderr);

    app.setOrganizationName("sohaib");
    app.setOrganizationDomain("com.sohaib");
    app.setApplicationName("VibeFlow");
    app.setApplicationVersion("1.0.0");
    app.setQuitOnLastWindowClosed(false);

    fprintf(stderr, "[INFO] Creating App controller...\n");
    fflush(stderr);

    App controller;

    fprintf(stderr, "[INFO] Calling initialize()...\n");
    fflush(stderr);

    controller.initialize();

    fprintf(stderr, "[INFO] Entering event loop\n");
    fflush(stderr);

    return app.exec();
}
