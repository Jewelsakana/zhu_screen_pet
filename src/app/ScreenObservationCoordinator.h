#pragma once

#include <QObject>

#include "app/UiConfig.h"
#include "core/AppError.h"
#include "infrastructure/ScreenCapture.h"

namespace zhu_screen_pet {

/** 管理截图授权、定时、去重和临时文件生命周期。 */
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
    void setBusy(bool busy);
    void shutdown();
    bool captureForChat(CapturedImage* image, AppError* error = nullptr);
    void finishScheduledRequest();

signals:
    void scheduledImageReady(const CapturedImage& image);
    void operationFailed(const AppError& error);

private:
    void onCaptured(const CapturedImage& image);
    void updateTimer();
    void discard(const QString& path, const QString& operation);
    void report(const QString& detail, const QString& operation);

    ScreenCapture* capture_ = nullptr;
    UiConfig config_;
    QString captureDirectory_;
    QString activePath_;
    QByteArray lastSentFingerprint_;
    bool observationReady_ = false;
    bool busy_ = false;
    bool shutDown_ = false;
};

} // namespace zhu_screen_pet
