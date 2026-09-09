#pragma once

#include <QObject>
#include <QDateTime>

#include "app/UiConfig.h"
#include "core/AppError.h"
#include "infrastructure/ScreenCapture.h"

namespace zhu_screen_pet {

/** 管理截图授权、定时和去重；自动截图始终只驻留内存。 */
class ScreenObservationCoordinator final : public QObject
{
    Q_OBJECT

public:
    explicit ScreenObservationCoordinator(QObject* parent = nullptr);
    ~ScreenObservationCoordinator() override;

    ScreenCapture* screenCapture() const;
    void setCaptureDirectory(const QString& directory);
    void applyConfiguration(const UiConfig& config);
    void setObservationReady(bool ready);
    /** 设置当前观察所属会话；作用域变化时清空上一会话的屏幕指纹。 */
    void setObservationScope(const QString& scopeId);
    /** 模型切换等语义边界发生变化时清空屏幕指纹。 */
    void resetFingerprint();
    void setBusy(bool busy);
    void shutdown();
    bool captureForChat(CapturedImage* image, AppError* error = nullptr);
    /** 完成一次自动视觉请求；失败会累计并在达到阈值后触发熔断。 */
    void finishScheduledRequest(bool succeeded = true);
    bool automaticCircuitOpen() const;
    int scheduledQueueDepth() const;

signals:
    void scheduledImageReady(const CapturedImage& image);
    void operationFailed(const AppError& error);

private:
    void onCaptured(const CapturedImage& image);
    void updateTimer();
    void report(const QString& detail, const QString& operation);
    void recordAutomaticFailure(const QString& detail);

    ScreenCapture* capture_ = nullptr;
    UiConfig config_;
    QString captureDirectory_;
    QString observationScopeId_;
    QByteArray lastSentFingerprint_;
    bool observationReady_ = false;
    bool busy_ = false;
    bool shutDown_ = false;
    int scheduledQueueDepth_ = 0;
    int consecutiveFailures_ = 0;
    QDateTime circuitOpenUntil_;
};

} // namespace zhu_screen_pet
