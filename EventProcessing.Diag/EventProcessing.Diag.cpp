#include "EventProcessingDiagDlg.h"
#include "MetavisionRuntime.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("CEventProcessingDiagDlg"));
    QApplication::setOrganizationName(QStringLiteral("EventCameraProcessing"));
    eventcore::EnsureBundledHalPluginPath();
    EventProcessingDiagDialog dialog;
    dialog.show();
    return application.exec();
}
