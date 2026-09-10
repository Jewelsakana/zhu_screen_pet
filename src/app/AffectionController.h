#pragma once

#include <QObject>

namespace zhu_screen_pet {

class SettingsRepository;

/** 管理好感度等级；经验来源由后续商店、互动等功能调用 addExperience 接入。 */
class AffectionController final : public QObject
{
    Q_OBJECT

public:
    static constexpr int MaximumLevel = 10;
    static constexpr int ExperiencePerLevel = 1000;

    explicit AffectionController(SettingsRepository* settings,
                                 QObject* parent = nullptr);

    bool initialize(QString* errorMessage = nullptr);
    bool shutdown(QString* errorMessage = nullptr);
    bool addExperience(int amount, QString* errorMessage = nullptr);

    int level() const;
    int experienceInLevel() const;
    int progressPercent() const;
    bool isMaximumLevel() const;

    /** 好感 1/4/7 级分别解锁主动程度 1/2/3。 */
    static int proactivityRewardForLevel(int level);

signals:
    void progressChanged(int level, int percent);
    void levelUp(int level);
    void proactivityRewardUnlocked(int proactiveLevel);
    void persistenceFailed(const QString& detail);

private:
    bool save(QString* errorMessage = nullptr);

    SettingsRepository* settings_ = nullptr;
    int level_ = 0;
    int experienceInLevel_ = 0;
    bool initialized_ = false;
};

} // namespace zhu_screen_pet
