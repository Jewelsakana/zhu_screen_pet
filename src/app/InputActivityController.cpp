#include "app/InputActivityController.h"

#include <QTimer>

#include "infrastructure/InputActivityMonitor.h"
#include "infrastructure/SettingsRepository.h"

namespace zhu_screen_pet {
namespace {
constexpr auto InputCountKey = "activity/input_count";
constexpr auto LegacyKeyboardCountKey = "activity/keyboard_count";
constexpr auto LegacyMouseCountKey = "activity/mouse_count";
}

InputActivityController::InputActivityController(InputActivityMonitor* monitor,
                                                 SettingsRepository* settings,
                                                 QObject* parent)
    : QObject(parent), monitor_(monitor), settings_(settings), saveTimer_(new QTimer(this)),
      publishTimer_(new QTimer(this))
{
    saveTimer_->setInterval(SaveIntervalMs);
    publishTimer_->setSingleShot(true);
    publishTimer_->setInterval(100);
    connect(saveTimer_, &QTimer::timeout, this, [this]() {
        QString error;
        if (!save(&error)) emit persistenceFailed(error);
    });
    connect(publishTimer_, &QTimer::timeout, this, [this]() {
        emit countChanged(inputCount_);
    });
    if (monitor_ != nullptr) {
        connect(monitor_, &InputActivityMonitor::keyboardInputDetected,
                this, &InputActivityController::recordInput);
        connect(monitor_, &InputActivityMonitor::mouseInputDetected,
                this, &InputActivityController::recordInput);
    }
}

bool InputActivityController::initialize(QString* errorMessage)
{
    if (initialized_) return true;
    if (settings_ == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("input activity settings are unavailable");
        }
        return false;
    }
    const QVariant storedCount = settings_->value(QString::fromLatin1(InputCountKey));
    const qint64 legacyKeyboard = qBound<qint64>(0, settings_->value(
        QString::fromLatin1(LegacyKeyboardCountKey), 0).toLongLong(), MaximumCount);
    const qint64 legacyMouse = qBound<qint64>(0, settings_->value(
        QString::fromLatin1(LegacyMouseCountKey), 0).toLongLong(), MaximumCount);
    const qint64 loadedCount = storedCount.isValid() ? storedCount.toLongLong()
                                                     : legacyKeyboard + legacyMouse;
    inputCount_ = qBound<qint64>(0, loadedCount, MaximumCount);
    initialized_ = true;
    return true;
}

bool InputActivityController::start(QString* errorMessage)
{
    if (running_) return true;
    if (!initialized_ || monitor_ == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("input activity controller is not ready");
        }
        return false;
    }
    if (!monitor_->start(errorMessage)) return false;
    running_ = true;
    saveTimer_->start();
    emit countChanged(inputCount_);
    return true;
}

bool InputActivityController::shutdown(QString* errorMessage)
{
    if (!initialized_) return true;
    running_ = false;
    saveTimer_->stop();
    publishTimer_->stop();
    if (monitor_ != nullptr) monitor_->stop();
    return save(errorMessage);
}

qint64 InputActivityController::inputCount() const { return inputCount_; }

bool InputActivityController::trySpend(qint64 amount, QString* errorMessage)
{
    if (!initialized_ || amount <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("input balance is unavailable or amount is invalid");
        }
        return false;
    }
    if (inputCount_ < amount) {
        if (errorMessage != nullptr) *errorMessage = QStringLiteral("余额不足");
        return false;
    }
    const qint64 previous = inputCount_;
    inputCount_ -= amount;
    if (!save(errorMessage)) {
        inputCount_ = previous;
        return false;
    }
    publishTimer_->stop();
    emit countChanged(inputCount_);
    return true;
}

bool InputActivityController::refund(qint64 amount, QString* errorMessage)
{
    if (!initialized_ || amount <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("input balance is unavailable or amount is invalid");
        }
        return false;
    }
    const qint64 previous = inputCount_;
    inputCount_ = qMin(MaximumCount, inputCount_ + amount);
    if (!save(errorMessage)) {
        inputCount_ = previous;
        return false;
    }
    publishTimer_->stop();
    emit countChanged(inputCount_);
    return true;
}

void InputActivityController::recordInput()
{
    if (!running_) return;
    if (inputCount_ < MaximumCount) ++inputCount_;
    if (!publishTimer_->isActive()) publishTimer_->start();
}

bool InputActivityController::save(QString* errorMessage)
{
    if (settings_ == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("input activity settings are unavailable");
        }
        return false;
    }
    settings_->setValue(QString::fromLatin1(InputCountKey), inputCount_);
    return settings_->save(errorMessage);
}

} // namespace zhu_screen_pet
