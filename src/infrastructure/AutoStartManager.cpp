#include "infrastructure/AutoStartManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

#include <utility>

namespace zhu_screen_pet {

namespace {
const QString DefaultRegistryPath = QStringLiteral(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
const QString DefaultValueName = QStringLiteral("ZhuScreenPet");
}

AutoStartManager::AutoStartManager(QString executablePath, QString registryPath,
                                   QString valueName)
    : executablePath_(executablePath.trimmed().isEmpty()
          ? QCoreApplication::applicationFilePath() : std::move(executablePath)),
      registryPath_(registryPath.trimmed().isEmpty()
          ? DefaultRegistryPath : std::move(registryPath)),
      valueName_(valueName.trimmed().isEmpty()
          ? DefaultValueName : std::move(valueName))
{
}

bool AutoStartManager::isSupported() const
{
#ifdef Q_OS_WIN
    return !executablePath_.trimmed().isEmpty();
#else
    return false;
#endif
}

QString AutoStartManager::startupCommand(const QString& executablePath)
{
    QString path = QDir::toNativeSeparators(executablePath.trimmed());
    path.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(path);
}

bool AutoStartManager::isEnabled() const
{
#ifdef Q_OS_WIN
    if (!isSupported()) return false;
    QSettings runKey(registryPath_, QSettings::NativeFormat);
    return runKey.value(valueName_).toString().trimmed()
        == startupCommand(executablePath_);
#else
    return false;
#endif
}

bool AutoStartManager::setEnabled(bool enabled, QString* errorMessage)
{
#ifdef Q_OS_WIN
    if (!isSupported()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("application executable path is unavailable");
        }
        return false;
    }
    QSettings runKey(registryPath_, QSettings::NativeFormat);
    if (enabled) runKey.setValue(valueName_, startupCommand(executablePath_));
    else runKey.remove(valueName_);
    runKey.sync();
    if (runKey.status() == QSettings::NoError) return true;
    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("failed to update current-user startup registry value: %1")
            .arg(valueName_);
    }
    return false;
#else
    Q_UNUSED(enabled);
    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("automatic startup is only supported on Windows");
    }
    return false;
#endif
}

} // namespace zhu_screen_pet
