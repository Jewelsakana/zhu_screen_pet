#include "ui/CaptureUiController.h"

#include <QWidget>
#include <QStringList>

#include "app/ScreenObservationCoordinator.h"
#include "app/SettingsController.h"
#include "infrastructure/DesktopWindowPolicy.h"

namespace zhu_screen_pet {

CaptureUiController::CaptureUiController(ScreenObservationCoordinator* observation,
                                         QObject* parent)
    : QObject(parent), observation_(observation)
{
}

void CaptureUiController::setSettingsController(SettingsController* settings)
{
    settings_ = settings;
}

void CaptureUiController::registerWindow(QWidget* window)
{
    if (window == nullptr) return;
    for (const QPointer<QWidget>& existing : windows_) {
        if (existing == window) return;
    }
    windows_.append(window);
    QString detail;
    if (!DesktopWindowPolicy::setExcludedFromCapture(
            window, config_.excludeOwnWindowsFromCapture, &detail)) {
        exclusionAvailable_ = false;
        exclusionDetail_ = QStringLiteral("%1: %2").arg(window->objectName(), detail);
        if (config_.excludeOwnWindowsFromCapture) ownWindowsExcluded_ = false;
        const bool disableAutomatic = config_.excludeOwnWindowsFromCapture
            && config_.automaticScreenAnalysisEnabled;
        if (disableAutomatic) {
            config_.automaticScreenAnalysisEnabled = false;
            if (observation_ != nullptr) observation_->applyConfiguration(config_);
            if (settings_ != nullptr) {
                AppError saveError;
                if (!settings_->updateUiConfig(config_, &saveError)) emit operationFailed(saveError);
            }
        }
        emit ownWindowExclusionStatusChanged(
            false, ownWindowsExcluded_, exclusionDetail_);
        emit operationFailed({AppErrorCode::Io,
                              disableAutomatic
                                  ? QStringLiteral("无法排除桌宠窗口，已关闭定时截图")
                                  : QStringLiteral("无法应用桌宠窗口截图策略"),
                              0, ErrorDomain::Infrastructure, exclusionDetail_,
                              QStringLiteral("screen.exclude_own_windows"), {}, false});
    }
}

UiConfig CaptureUiController::applyConfiguration(const UiConfig& source)
{
    config_ = source.normalized();
    QString detail;
    const bool applied = applyOwnWindowExclusion(
        config_.excludeOwnWindowsFromCapture, &detail);
    if (!applied) {
        const bool disableAutomatic = config_.excludeOwnWindowsFromCapture
            && config_.automaticScreenAnalysisEnabled;
        if (disableAutomatic) config_.automaticScreenAnalysisEnabled = false;
        AppError error{AppErrorCode::Io,
                       disableAutomatic
                           ? QStringLiteral("无法排除桌宠窗口，已关闭定时截图")
                           : QStringLiteral("无法应用桌宠窗口截图策略"), 0,
                       ErrorDomain::Infrastructure, detail,
                       QStringLiteral("screen.exclude_own_windows"), {}, false};
        emit operationFailed(error);
    }
    if (observation_ != nullptr) observation_->applyConfiguration(config_);
    return config_;
}

bool CaptureUiController::setScreenCaptureEnabled(bool enabled, AppError* error)
{
    UiConfig updated = config_;
    updated.screenCaptureEnabled = enabled;
    if (!enabled) {
        updated.automaticScreenAnalysisEnabled = false;
        updated.captureOnChat = false;
    }
    return saveUiConfig(updated, error);
}

bool CaptureUiController::setCaptureOnChatEnabled(bool enabled, AppError* error)
{
    if (!config_.screenCaptureEnabled) return false;
    UiConfig updated = config_;
    updated.captureOnChat = enabled;
    return saveUiConfig(updated, error);
}

bool CaptureUiController::ownWindowExclusionAvailable() const
{
    return exclusionAvailable_;
}

bool CaptureUiController::ownWindowsExcluded() const
{
    return ownWindowsExcluded_;
}

QString CaptureUiController::ownWindowExclusionDetail() const
{
    return exclusionDetail_;
}

bool CaptureUiController::applyOwnWindowExclusion(bool excluded, QString* detail)
{
    bool succeeded = true;
    QStringList failures;
    for (auto it = windows_.begin(); it != windows_.end();) {
        if (it->isNull()) {
            it = windows_.erase(it);
            continue;
        }
        QString windowError;
        if (!DesktopWindowPolicy::setExcludedFromCapture(it->data(), excluded, &windowError)) {
            succeeded = false;
            failures.append(QStringLiteral("%1: %2")
                                .arg(it->data()->objectName(), windowError));
        }
        ++it;
    }
    if (succeeded) ownWindowsExcluded_ = excluded;
    else if (excluded) ownWindowsExcluded_ = false;
    exclusionAvailable_ = succeeded;
    exclusionDetail_ = failures.join(QStringLiteral(" | "));
    if (detail != nullptr) *detail = exclusionDetail_;
    emit ownWindowExclusionStatusChanged(exclusionAvailable_, excluded, exclusionDetail_);
    return succeeded;
}

bool CaptureUiController::saveUiConfig(const UiConfig& config, AppError* error)
{
    if (settings_ == nullptr) return false;
    return settings_->updateUiConfig(config, error);
}

} // namespace zhu_screen_pet
