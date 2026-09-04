#include "infrastructure/AppPaths.h"

#include <QDir>
#include <QStandardPaths>

#include <utility>

namespace zhu_screen_pet {

AppPaths::AppPaths(QString rootDirectory)
    : rootDirectory_(std::move(rootDirectory))
{
    if (rootDirectory_.isEmpty()) {
        rootDirectory_ = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
}

bool AppPaths::initialize(QString* errorMessage)
{
    if (rootDirectory_.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("application data directory is empty");
        }
        return false;
    }

    const QStringList directories = {
        rootDirectory_, configDirectory(), databaseDirectory(), logDirectory(), captureDirectory()
    };
    for (const QString& directory : directories) {
        if (!QDir().mkpath(directory)) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("failed to create directory: %1").arg(directory);
            }
            return false;
        }
    }
    return true;
}

QString AppPaths::rootDirectory() const { return rootDirectory_; }
QString AppPaths::configDirectory() const { return rootDirectory_ + QStringLiteral("/config"); }
QString AppPaths::databaseDirectory() const { return rootDirectory_ + QStringLiteral("/database"); }
QString AppPaths::databasePath() const { return databaseDirectory() + QStringLiteral("/zhu_screen_pet.sqlite"); }
QString AppPaths::settingsPath() const { return configDirectory() + QStringLiteral("/settings.ini"); }
QString AppPaths::modelConfigPath() const
{
    return configDirectory() + QStringLiteral("/model-providers.json");
}
QString AppPaths::appConfigPath() const
{
    return configDirectory() + QStringLiteral("/app-settings.json");
}
QString AppPaths::logDirectory() const { return rootDirectory_ + QStringLiteral("/logs"); }
QString AppPaths::captureDirectory() const { return rootDirectory_ + QStringLiteral("/captures"); }

} // namespace zhu_screen_pet
