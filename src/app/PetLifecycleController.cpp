#include "app/PetLifecycleController.h"

#include <QRandomGenerator>
#include <QTime>
#include <QTimer>

#include "infrastructure/SettingsRepository.h"

namespace zhu_screen_pet {
namespace {
constexpr auto LevelKey = "pet/level";
constexpr auto LevelElapsedKey = "pet/level_elapsed_seconds";
}

PetLifecycleController::PetLifecycleController(SettingsRepository* settings, QObject* parent)
    : QObject(parent), settings_(settings), idleTimer_(new QTimer(this)),
      progressTimer_(new QTimer(this)), saveTimer_(new QTimer(this)),
      idleDescriptions_({QStringLiteral("摸鱼中~"), QStringLiteral("视奸中~"),
                         QStringLiteral("吃饭中~"), QStringLiteral("睡觉中~"),
                         QStringLiteral("无聊中~")})
{
    idleTimer_->setInterval(IdleDescriptionIntervalMs);
    progressTimer_->setInterval(ProgressRefreshIntervalMs);
    saveTimer_->setInterval(ProgressSaveIntervalMs);
    connect(idleTimer_, &QTimer::timeout, this, &PetLifecycleController::rotateIdleDescription);
    connect(progressTimer_, &QTimer::timeout, this, [this]() {
        reconcileElapsed();
        emit progressChanged(level_, progressPercent());
    });
    connect(saveTimer_, &QTimer::timeout, this, [this]() {
        reconcileElapsed();
        QString error;
        if (!save(&error)) emit persistenceFailed(error);
    });
}

bool PetLifecycleController::initialize(QString* errorMessage)
{
    if (initialized_) return true;
    if (settings_ == nullptr) {
        if (errorMessage) *errorMessage = QStringLiteral("pet lifecycle settings are unavailable");
        return false;
    }
    level_ = qBound(0, settings_->value(QString::fromLatin1(LevelKey), 0).toInt(), MaximumLevel);
    elapsedInLevelSeconds_ = qMax<qint64>(0, settings_->value(
        QString::fromLatin1(LevelElapsedKey), 0).toLongLong());
    if (level_ >= MaximumLevel) {
        level_ = MaximumLevel;
        elapsedInLevelSeconds_ = 0;
    } else {
        applyElapsedSeconds(0);
    }
    rotateIdleDescription();
    initialized_ = true;
    return true;
}

void PetLifecycleController::start()
{
    if (!initialized_ || running_) return;
    running_ = true;
    activeElapsed_.start();
    idleTimer_->start();
    if (level_ < MaximumLevel) {
        progressTimer_->start();
        saveTimer_->start();
    }
    emit idleDescriptionChanged(idleDescription_);
    emit progressChanged(level_, progressPercent());
    emit startupGreetingRequested(startupGreeting());
}

void PetLifecycleController::setUserAddress(const QString& userAddress)
{
    const QString normalized = userAddress.trimmed();
    userAddress_ = normalized.isEmpty() ? QStringLiteral("主人大人") : normalized;
}

bool PetLifecycleController::shutdown(QString* errorMessage)
{
    if (!initialized_) return true;
    if (running_) reconcileElapsed();
    running_ = false;
    idleTimer_->stop();
    progressTimer_->stop();
    saveTimer_->stop();
    return save(errorMessage);
}

int PetLifecycleController::level() const { return level_; }

int PetLifecycleController::progressPercent() const
{
    if (level_ >= MaximumLevel) return 100;
    const qint64 required = secondsRequiredForLevel(level_);
    return required <= 0 ? 0 : qBound(0, static_cast<int>(
        elapsedInLevelSeconds_ * 100 / required), 99);
}

QString PetLifecycleController::currentIdleDescription() const
{
    return idleDescription_;
}

QString PetLifecycleController::startupGreeting() const
{
    return greetingForHour(QTime::currentTime().hour(), userAddress_);
}

QString PetLifecycleController::greetingForHour(int hour, const QString& userAddress)
{
    const int normalizedHour = qBound(0, hour, 23);
    QString prefix;
    if (normalizedHour < 6) prefix = QStringLiteral("凌晨好呀");
    else if (normalizedHour < 11) prefix = QStringLiteral("早上好呀");
    else if (normalizedHour < 13) prefix = QStringLiteral("中午好呀");
    else if (normalizedHour < 19) prefix = QStringLiteral("下午好呀");
    else prefix = QStringLiteral("晚上好呀");
    const QString name = userAddress.trimmed();
    return name.isEmpty() ? prefix + QStringLiteral("！")
                          : QStringLiteral("%1，%2！").arg(prefix, name);
}

qint64 PetLifecycleController::secondsRequiredForLevel(int level)
{
    const int value = qBound(0, level, MaximumLevel);
    if (value >= MaximumLevel) return 0;
    if (value < 10) {
        // 3..9 分钟，均高于 2 分钟的 UI 更新间隔。
        return (3 + value * 6 / 9) * 60LL;
    }
    if (value < 50) {
        // 10..59 分钟。
        return (10 + (value - 10) * 49 / 39) * 60LL;
    }
    if (value < 90) {
        // 1..9 小时。
        return (1 + (value - 50) * 8 / 39) * 60LL * 60LL;
    }
    // 10..19 小时。
    return (10 + (value - 90)) * 60LL * 60LL;
}

void PetLifecycleController::rotateIdleDescription()
{
    if (idleDescriptions_.isEmpty()) return;
    int index = QRandomGenerator::global()->bounded(idleDescriptions_.size());
    if (idleDescriptions_.size() > 1 && idleDescriptions_.at(index) == idleDescription_) {
        index = (index + 1) % idleDescriptions_.size();
    }
    idleDescription_ = idleDescriptions_.at(index);
    if (initialized_) emit idleDescriptionChanged(idleDescription_);
}

void PetLifecycleController::reconcileElapsed()
{
    if (!running_ || !activeElapsed_.isValid()) return;
    const qint64 milliseconds = activeElapsed_.restart();
    applyElapsedSeconds(milliseconds / 1000);
}

void PetLifecycleController::applyElapsedSeconds(qint64 seconds)
{
    elapsedInLevelSeconds_ += qMax<qint64>(0, seconds);
    while (level_ < MaximumLevel) {
        const qint64 required = secondsRequiredForLevel(level_);
        if (required <= 0 || elapsedInLevelSeconds_ < required) break;
        elapsedInLevelSeconds_ -= required;
        ++level_;
        emit levelUp(level_);
    }
    if (level_ >= MaximumLevel) {
        level_ = MaximumLevel;
        elapsedInLevelSeconds_ = 0;
        progressTimer_->stop();
        saveTimer_->stop();
    }
}

bool PetLifecycleController::save(QString* errorMessage)
{
    if (settings_ == nullptr) {
        if (errorMessage) *errorMessage = QStringLiteral("pet lifecycle settings are unavailable");
        return false;
    }
    settings_->setValue(QString::fromLatin1(LevelKey), level_);
    settings_->setValue(QString::fromLatin1(LevelElapsedKey), elapsedInLevelSeconds_);
    return settings_->save(errorMessage);
}

} // namespace zhu_screen_pet
