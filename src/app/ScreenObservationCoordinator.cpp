#include "app/ScreenObservationCoordinator.h"

#include "infrastructure/ImageCompressor.h"
#include "infrastructure/ScreenFingerprint.h"

namespace zhu_screen_pet {

ScreenObservationCoordinator::ScreenObservationCoordinator(QObject* parent)
    : QObject(parent), capture_(new ScreenCapture(this))
{
    connect(capture_, &ScreenCapture::captureFailed, this, [this](const QString& detail) {
        report(detail, QStringLiteral("screen.capture_scheduled"));
    });
    connect(capture_, &ScreenCapture::captured,
            this, &ScreenObservationCoordinator::onCaptured);
}

ScreenObservationCoordinator::~ScreenObservationCoordinator()
{
    capture_->stop();
    capture_->clearCaptures(nullptr);
}

void ScreenObservationCoordinator::shutdown()
{
    if (shutDown_) return;
    shutDown_ = true;
    capture_->stop();
    QString detail;
    if (!capture_->clearCaptures(&detail)) report(detail, QStringLiteral("screen.cleanup_shutdown"));
}

ScreenCapture* ScreenObservationCoordinator::screenCapture() const { return capture_; }

void ScreenObservationCoordinator::setCaptureDirectory(const QString& directory)
{
    captureDirectory_ = directory;
    capture_->setCaptureDirectory(directory);
    QString detail;
    if (!capture_->clearCaptures(&detail)) report(detail, QStringLiteral("screen.cleanup_startup"));
}

void ScreenObservationCoordinator::applyConfiguration(const UiConfig& config)
{
    config_ = config.normalized();
    ImageCompressionOptions compression;
    compression.format = config_.captureImageFormat;
    compression.maxWidth = config_.captureMaxWidth;
    compression.quality = config_.captureQuality;
    capture_->configure(config_.screenCaptureEnabled, config_.screenCaptureIntervalMs,
                        captureDirectory_, compression);
    updateTimer();
}

void ScreenObservationCoordinator::setObservationReady(bool ready)
{
    observationReady_ = ready;
    updateTimer();
}

void ScreenObservationCoordinator::setBusy(bool busy)
{
    busy_ = busy;
    updateTimer();
}

bool ScreenObservationCoordinator::captureForChat(CapturedImage* image, AppError* error)
{
    capture_->stop();
    QString detail;
    if (config_.screenCaptureEnabled && config_.captureOnChat
        && capture_->captureImage(image, &detail, CaptureTrigger::Chat)) return true;
    if (detail.isEmpty()) detail = QStringLiteral("screen capture on chat is disabled");
    AppError value{AppErrorCode::Io, QStringLiteral("无法附加当前屏幕"), 0,
                   ErrorDomain::Infrastructure, detail,
                   QStringLiteral("screen.capture_chat"), {}, false};
    if (error) *error = value;
    emit operationFailed(value);
    updateTimer();
    return false;
}

void ScreenObservationCoordinator::finishScheduledRequest()
{
    discard(activePath_, QStringLiteral("screen.cleanup_request"));
    activePath_.clear();
    setBusy(false);
}

void ScreenObservationCoordinator::onCaptured(const CapturedImage& image)
{
    if (image.trigger == CaptureTrigger::Manual) return;
    if (image.trigger != CaptureTrigger::Scheduled || !config_.screenCaptureEnabled
        || !config_.automaticScreenAnalysisEnabled || !observationReady_ || busy_) {
        discard(image.filePath, QStringLiteral("screen.cleanup_ignored"));
        updateTimer();
        return;
    }
    if (!lastSentFingerprint_.isEmpty() && !image.fingerprint.isEmpty()
        && !ScreenFingerprint::hasSignificantChange(lastSentFingerprint_, image.fingerprint)) {
        discard(image.filePath, QStringLiteral("screen.cleanup_duplicate"));
        updateTimer();
        return;
    }
    if (!image.fingerprint.isEmpty()) lastSentFingerprint_ = image.fingerprint;
    activePath_ = image.filePath;
    setBusy(true);
    emit scheduledImageReady(image);
}

void ScreenObservationCoordinator::updateTimer()
{
    if (!shutDown_ && config_.screenCaptureEnabled && config_.automaticScreenAnalysisEnabled
        && observationReady_ && !busy_) capture_->start();
    else capture_->stop();
}

void ScreenObservationCoordinator::discard(const QString& path, const QString& operation)
{
    QString detail;
    if (!capture_->removeCapture(path, &detail)) report(detail, operation);
}

void ScreenObservationCoordinator::report(const QString& detail, const QString& operation)
{
    if (detail.isEmpty()) return;
    emit operationFailed({AppErrorCode::Io, QStringLiteral("屏幕截图操作失败"), 0,
                          ErrorDomain::Infrastructure, detail, operation, {}, false});
}

} // namespace zhu_screen_pet
