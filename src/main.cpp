#include <QApplication>
#include "App.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("VibeFlow");
    app.setApplicationVersion("1.0.0");
    app.setQuitOnLastWindowClosed(false);

    App controller;
    controller.initialize();

    return app.exec();
}
