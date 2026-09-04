#include <QApplication>
#include <QCoreApplication>
#include <QIcon>

#include "app/ApplicationBootstrapper.h"

/** 应用入口只负责 Qt 元信息和启动编排器生命周期。 */
int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("zhu_screen_pet"));
    QCoreApplication::setApplicationName(QStringLiteral("zhu_screen_pet"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("小珠看着你"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    application.setWindowIcon(QIcon(QStringLiteral(":/app-icon.svg")));

    zhu_screen_pet::ApplicationBootstrapper bootstrapper(&application);
    if (!bootstrapper.initialize()) return 1;
    return bootstrapper.run();
}
