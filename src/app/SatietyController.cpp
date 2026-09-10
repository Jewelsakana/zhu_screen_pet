#include "app/SatietyController.h"

#include <QTimer>

#include "infrastructure/SettingsRepository.h"

namespace zhu_screen_pet {
namespace {
constexpr auto SatietyValueKey = "satiety/value";
constexpr auto SatietyElapsedKey = "satiety/elapsed_seconds";
}

SatietyController::SatietyController(SettingsRepository* settings, QObject* parent)
    : QObject(parent), settings_(settings), refreshTimer_(new QTimer(this)),
      saveTimer_(new QTimer(this))
{
    refreshTimer_->setInterval(RefreshIntervalMs);
    saveTimer_->setInterval(SaveIntervalMs);
    connect(refreshTimer_, &QTimer::timeout, this, &SatietyController::reconcileElapsed);
    connect(saveTimer_, &QTimer::timeout, this, [this]() {
        reconcileElapsed();
        QString error;
        if (!save(&error)) emit persistenceFailed(error);
    });
}

bool SatietyController::initialize(QString* errorMessage)
{
    if (initialized_) return true;
    if (settings_ == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("satiety settings are unavailable");
        }
        return false;
    }
    value_ = qBound(MinimumValue, settings_->value(
        QString::fromLatin1(SatietyValueKey), MaximumValue).toInt(), MaximumValue);
    elapsedSeconds_ = qMax<qint64>(0, settings_->value(
        QString::fromLatin1(SatietyElapsedKey), 0).toLongLong());
    applyElapsedSeconds(0);
    initialized_ = true;
    return true;
}

void SatietyController::start()
{
    if (!initialized_ || running_) return;
    running_ = true;
    activeElapsed_.start();
    refreshTimer_->start();
    saveTimer_->start();
    emit valueChanged(value_);
}

bool SatietyController::shutdown(QString* errorMessage)
{
    if (!initialized_) return true;
    if (running_) reconcileElapsed();
    running_ = false;
    refreshTimer_->stop();
    saveTimer_->stop();
    return save(errorMessage);
}

bool SatietyController::increase(int amount, QString* errorMessage)
{
    if (!initialized_ || amount <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("satiety controller is unavailable or amount is invalid");
        }
        return false;
    }
    const int previous = value_;
    value_ = qMin(MaximumValue, value_ + amount);
    if (!save(errorMessage)) {
        value_ = previous;
        return false;
    }
    emit valueChanged(value_);
    return true;
}

int SatietyController::value() const { return value_; }

void SatietyController::reconcileElapsed()
{
    if (!running_ || !activeElapsed_.isValid()) return;
    applyElapsedSeconds(activeElapsed_.restart() / 1000);
}

void SatietyController::applyElapsedSeconds(qint64 seconds)
{
    if (value_ <= MinimumValue) {
        elapsedSeconds_ = 0;
        return;
    }
    elapsedSeconds_ += qMax<qint64>(0, seconds);
    const int previous = value_;
    while (elapsedSeconds_ >= DecayIntervalSeconds && value_ > MinimumValue) {
        elapsedSeconds_ -= DecayIntervalSeconds;
        value_ = qMax(MinimumValue, value_ - DecayAmount);
    }
    if (value_ <= MinimumValue) elapsedSeconds_ = 0;
    if (value_ != previous) emit valueChanged(value_);
}

bool SatietyController::save(QString* errorMessage)
{
    if (settings_ == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("satiety settings are unavailable");
        }
        return false;
    }
    settings_->setValue(QString::fromLatin1(SatietyValueKey), value_);
    settings_->setValue(QString::fromLatin1(SatietyElapsedKey), elapsedSeconds_);
    return settings_->save(errorMessage);
}

} // namespace zhu_screen_pet
