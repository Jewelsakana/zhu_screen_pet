#pragma once

#include <QPointer>
#include <QVector>
#include <QObject>

#include "app/UiConfig.h"
#include "core/AppError.h"

class QWidget;
class QEvent;

namespace zhu_screen_pet {

class ScreenObservationCoordinator;
class SettingsController;

/** 统一协调截图 UI 配置、自身窗口捕获策略及运行时降级。 */
class CaptureUiController final : public QObject
{
    Q_OBJECT

public:
    explicit CaptureUiController(ScreenObservationCoordinator* observation,
                                 QObject* parent = nullptr);

    void setSettingsController(SettingsController* settings);
    void registerWindow(QWidget* window);
    UiConfig applyConfiguration(const UiConfig& source);
    bool setScreenCaptureEnabled(bool enabled, AppError* error = nullptr);
    bool setCaptureOnChatEnabled(bool enabled, AppError* error = nullptr);

    bool ownWindowExclusionAvailable() const;
    bool ownWindowsExcluded() const;
    QString ownWindowExclusionDetail() const;

signals:
    void operationFailed(const AppError& error);
    void ownWindowExclusionStatusChanged(bool available, bool excluded,
                                         const QString& detail);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void applyRegisteredWindowExclusion(QWidget* window);
    void reportWindowExclusionFailure(QWidget* window, const QString& detail);
    bool applyOwnWindowExclusion(bool excluded, QString* detail);
    bool saveUiConfig(const UiConfig& config, AppError* error);

    ScreenObservationCoordinator* observation_ = nullptr;
    SettingsController* settings_ = nullptr;
    QVector<QPointer<QWidget>> windows_;
    UiConfig config_;
    bool exclusionAvailable_ = true;
    bool ownWindowsExcluded_ = true;
    QString exclusionDetail_;
};

} // namespace zhu_screen_pet
