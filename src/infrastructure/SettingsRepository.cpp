#include "infrastructure/SettingsRepository.h"

#include <utility>

namespace zhu_screen_pet {

SettingsRepository::SettingsRepository(QString filePath)
    : settings_(std::move(filePath), QSettings::IniFormat)
{
}

bool SettingsRepository::load(QString* errorMessage)
{
    settings_.sync();
    if (settings_.status() != QSettings::NoError) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("failed to load settings: %1").arg(settings_.fileName());
        }
        return false;
    }
    return true;
}

bool SettingsRepository::save(QString* errorMessage)
{
    settings_.sync();
    if (settings_.status() != QSettings::NoError) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("failed to save settings: %1").arg(settings_.fileName());
        }
        return false;
    }
    return true;
}

QVariant SettingsRepository::value(const QString& key, const QVariant& defaultValue) const
{
    return settings_.value(key, defaultValue);
}

void SettingsRepository::setValue(const QString& key, const QVariant& value)
{
    settings_.setValue(key, value);
}

} // namespace zhu_screen_pet
