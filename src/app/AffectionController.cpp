#include "app/AffectionController.h"

#include <limits>

#include "infrastructure/SettingsRepository.h"

namespace zhu_screen_pet {
namespace {
constexpr auto AffectionLevelKey = "affection/level";
constexpr auto AffectionExperienceKey = "affection/experience_in_level";
}

AffectionController::AffectionController(SettingsRepository* settings, QObject* parent)
    : QObject(parent), settings_(settings)
{
}

bool AffectionController::initialize(QString* errorMessage)
{
    if (initialized_) return true;
    if (settings_ == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("affection settings are unavailable");
        }
        return false;
    }
    level_ = qBound(0, settings_->value(
        QString::fromLatin1(AffectionLevelKey), 0).toInt(), MaximumLevel);
    qint64 experience = qMax<qint64>(0, settings_->value(
        QString::fromLatin1(AffectionExperienceKey), 0).toLongLong());
    if (level_ < MaximumLevel) {
        const qint64 availableLevels = MaximumLevel - level_;
        const qint64 gainedLevels = qMin(experience / ExperiencePerLevel, availableLevels);
        level_ += static_cast<int>(gainedLevels);
        experience -= gainedLevels * ExperiencePerLevel;
        experienceInLevel_ = level_ >= MaximumLevel
            ? 0 : static_cast<int>(experience);
    } else {
        experienceInLevel_ = 0;
    }
    initialized_ = true;
    return true;
}

bool AffectionController::shutdown(QString* errorMessage)
{
    return !initialized_ || save(errorMessage);
}

bool AffectionController::addExperience(int amount, QString* errorMessage)
{
    if (!initialized_) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("affection controller is not initialized");
        }
        return false;
    }
    if (amount <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("affection experience must be positive");
        }
        return false;
    }
    if (level_ >= MaximumLevel) return true;
    qint64 experience = static_cast<qint64>(experienceInLevel_) + amount;
    const int previousReward = proactivityRewardForLevel(level_);
    while (level_ < MaximumLevel && experience >= ExperiencePerLevel) {
        experience -= ExperiencePerLevel;
        ++level_;
        emit levelUp(level_);
    }
    if (level_ >= MaximumLevel) experience = 0;
    experienceInLevel_ = static_cast<int>(qMin<qint64>(
        experience, std::numeric_limits<int>::max()));
    const int reward = proactivityRewardForLevel(level_);
    if (reward > previousReward) emit proactivityRewardUnlocked(reward);
    emit progressChanged(level_, progressPercent());
    if (save(errorMessage)) return true;
    emit persistenceFailed(errorMessage == nullptr ? QString{} : *errorMessage);
    return false;
}

int AffectionController::level() const { return level_; }
int AffectionController::experienceInLevel() const { return experienceInLevel_; }

int AffectionController::progressPercent() const
{
    if (level_ >= MaximumLevel) return 100;
    return qBound(0, experienceInLevel_ * 100 / ExperiencePerLevel, 99);
}

bool AffectionController::isMaximumLevel() const
{
    return level_ >= MaximumLevel;
}

int AffectionController::proactivityRewardForLevel(int level)
{
    const int bounded = qBound(0, level, MaximumLevel);
    if (bounded >= 7) return 3;
    if (bounded >= 4) return 2;
    if (bounded >= 1) return 1;
    return 0;
}

bool AffectionController::save(QString* errorMessage)
{
    if (settings_ == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("affection settings are unavailable");
        }
        return false;
    }
    settings_->setValue(QString::fromLatin1(AffectionLevelKey), level_);
    settings_->setValue(QString::fromLatin1(AffectionExperienceKey), experienceInLevel_);
    return settings_->save(errorMessage);
}

} // namespace zhu_screen_pet
