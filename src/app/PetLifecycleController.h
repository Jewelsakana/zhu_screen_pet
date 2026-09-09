#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QStringList>

class QTimer;

namespace zhu_screen_pet {

class SettingsRepository;

/** 管理启动问候、空闲文案和仅按本次应用在线时间增长的宠物等级。 */
class PetLifecycleController final : public QObject
{
    Q_OBJECT

public:
    static constexpr int MaximumLevel = 100;
    static constexpr int IdleDescriptionIntervalMs = 60 * 1000;
    static constexpr int ProgressRefreshIntervalMs = 60 * 1000;
    static constexpr int ProgressSaveIntervalMs = 5 * 60 * 1000;

    explicit PetLifecycleController(SettingsRepository* settings,
                                    QObject* parent = nullptr);

    bool initialize(QString* errorMessage = nullptr);
    /** 更新人格配置中的用户称呼；后续启动问候使用该称呼。 */
    void setUserAddress(const QString& userAddress);
    void start();
    bool shutdown(QString* errorMessage = nullptr);

    int level() const;
    int progressPercent() const;
    QString currentIdleDescription() const;
    QString startupGreeting() const;

    static QString greetingForHour(int hour, const QString& userAddress);
    /** 返回从 level 升到下一级所需的在线秒数。 */
    static qint64 secondsRequiredForLevel(int level);

signals:
    void startupGreetingRequested(const QString& text);
    void idleDescriptionChanged(const QString& text);
    void progressChanged(int level, int percent);
    void levelUp(int level);
    void persistenceFailed(const QString& detail);

private:
    void rotateIdleDescription();
    void reconcileElapsed();
    void applyElapsedSeconds(qint64 seconds);
    bool save(QString* errorMessage = nullptr);

    SettingsRepository* settings_ = nullptr;
    QTimer* idleTimer_ = nullptr;
    QTimer* progressTimer_ = nullptr;
    QTimer* saveTimer_ = nullptr;
    QElapsedTimer activeElapsed_;
    QStringList idleDescriptions_;
    QString idleDescription_;
    QString userAddress_ = QStringLiteral("主人大人");
    int level_ = 0;
    qint64 elapsedInLevelSeconds_ = 0;
    bool initialized_ = false;
    bool running_ = false;
};

} // namespace zhu_screen_pet
