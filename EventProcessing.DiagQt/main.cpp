#include "MainWindow.h"

#include <QApplication>

#include "MetavisionRuntime.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    eventcore::EnsureBundledHalPluginPath();

    MainWindow window;
    window.show();

    return app.exec();
}
