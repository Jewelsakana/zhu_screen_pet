#include "infrastructure/ScreenCapture.h"

#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QPixmap>
#include <QScreen>
#include <QTimer>
#include <QUrl>
#include <QFile>
#include <QFileInfoList>
#include <QElapsedTimer>
#include <QUuid>
#include <QPointer>

#include <utility>
#include <iterator>

#include "infrastructure/ImageCompressor.h"
#include "infrastructure/ScreenFingerprint.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace zhu_screen_pet {
namespace {
QString foregroundWindowHint()
{
#ifdef Q_OS_WIN
    const HWND window = GetForegroundWindow();
    if (window == nullptr) return {};
    wchar_t title[512] = {};
    const int length = GetWindowTextW(window, title, static_cast<int>(std::size(title)));
    return length > 0 ? QString::fromWCharArray(title, length) : QString{};
#else
    return {};
#endif
}
}

ScreenCapture::ScreenCapture(QObject* parent)
    : QObject(parent), timer_(new QTimer(this))
{
    timer_->setSingleShot(false);
    connect(timer_, &QTimer::timeout, this, &ScreenCapture::captureOnTimer);
}

ScreenCapture::~ScreenCapture()
{
    timer_->stop();
    preprocessing_.shutdownAndWait(3000);
}

void ScreenCapture::configure(bool enabled, int intervalMs, QString captureDirectory,
                              ImageCompressionOptions options)
{
    enabled_ = enabled;
    intervalMs_ = qBound(ScreenCapturePolicy::MinimumAutomaticIntervalMs, intervalMs,
                          ScreenCapturePolicy::MaximumAutomaticIntervalMs);
    captureDirectory_ = std::move(captureDirectory);
    options_ = std::move(options);
    if (timer_->isActive()) {
        timer_->setInterval(intervalMs_);
    }
}

void ScreenCapture::setCaptureDirectory(const QString& captureDirectory)
{
    captureDirectory_ = captureDirectory;
}

void ScreenCapture::start()
{
    if (!enabled_) return;
    timer_->start(intervalMs_);
}

void ScreenCapture::stop()
{
    timer_->stop();
}

bool ScreenCapture::isEnabled() const
{
    return enabled_;
}

bool ScreenCapture::captureNow(QString* errorMessage, CaptureTrigger trigger)
{
    CapturedImage image;
    if (!captureInternal(&image, errorMessage, trigger,
                         trigger == CaptureTrigger::Manual)) {
        return false;
    }
    emit captured(image);
    return true;
}

bool ScreenCapture::captureImage(CapturedImage* image, QString* errorMessage,
                                 CaptureTrigger trigger)
{
    if (image == nullptr) {
        if (errorMessage) *errorMessage = QStringLiteral("截图输出不能为空");
        return false;
    }
    return captureInternal(image, errorMessage, trigger, false);
}

bool ScreenCapture::removeCapture(const QString& filePath, QString* errorMessage) const
{
    if (filePath.isEmpty()) return true;
    const QFileInfo fileInfo(filePath);
    const QDir captureDirectory(captureDirectory_);
    const QString expectedDirectory = QDir::cleanPath(captureDirectory.absolutePath());
    if (QDir::cleanPath(fileInfo.absolutePath()) != expectedDirectory
        || !fileInfo.fileName().startsWith(QStringLiteral("capture_"))) {
        if (errorMessage) *errorMessage = QStringLiteral("拒绝删除截图目录之外的文件");
        return false;
    }
    if (!fileInfo.exists() || QFile::remove(fileInfo.absoluteFilePath())) return true;
    if (errorMessage) {
        *errorMessage = QStringLiteral("无法删除截图文件: %1").arg(fileInfo.absoluteFilePath());
    }
    return false;
}

bool ScreenCapture::clearCaptures(QString* errorMessage) const
{
    if (captureDirectory_.isEmpty()) return true;
    const QDir directory(captureDirectory_);
    if (!directory.exists()) return true;
    const QFileInfoList files = directory.entryInfoList(
        {QStringLiteral("capture_*.jpeg"), QStringLiteral("capture_*.jpg"),
         QStringLiteral("capture_*.webp")},
        QDir::Files | QDir::NoSymLinks);
    for (const QFileInfo& fileInfo : files) {
        if (!QFile::remove(fileInfo.absoluteFilePath())) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("无法删除截图文件: %1")
                    .arg(fileInfo.absoluteFilePath());
            }
            return false;
        }
    }
    return true;
}

void ScreenCapture::captureOnTimer()
{
    captureScheduledAsync();
}

void ScreenCapture::captureScheduledAsync()
{
    if (preprocessingActive_) return;
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen == nullptr) { emit captureFailed(QStringLiteral("当前没有可用的显示器")); return; }
    QElapsedTimer elapsed; elapsed.start();
    const QPixmap pixmap = screen->grabWindow(0);
    if (pixmap.isNull()) { emit captureFailed(QStringLiteral("无法获取屏幕图像")); return; }
    const QImage sourceImage = pixmap.toImage();
    const ImageCompressionOptions options = options_;
    const QDateTime capturedAt = QDateTime::currentDateTimeUtc();
    const QString captureId = QUuid::createUuid().toString(QUuid::Id128);
    const int captureDuration = static_cast<int>(elapsed.elapsed());
    const QString appHint = foregroundWindowHint();
    preprocessingActive_ = true;
    const QPointer<ScreenCapture> guard(this);
    preprocessing_.submit([guard, sourceImage, options, capturedAt, captureId, captureDuration, appHint](const std::shared_ptr<CancellationToken>& token) {
        CapturedImage result; QString error; QString format;
        result.captureId=captureId; result.capturedAt=capturedAt; result.trigger=CaptureTrigger::Scheduled;
        result.source=QStringLiteral("primary_screen"); result.durationMs=captureDuration;
        result.appHint=appHint;
        if (!token->isCancellationRequested()) result.fingerprint=ScreenFingerprint::create(sourceImage);
        const bool ok=!token->isCancellationRequested() && ImageCompressor::compress(
            sourceImage,options,&result.data,&format,&result.size,&error);
        result.format=format;
        if (guard.isNull()) return;
        QMetaObject::invokeMethod(guard, [guard, result, error, ok]() {
            if (guard.isNull()) return; guard->preprocessingActive_=false;
            if (ok) emit guard->captured(result); else if (!error.isEmpty()) emit guard->captureFailed(error);
        }, Qt::QueuedConnection);
    }, [guard](std::exception_ptr) {
        if (guard.isNull()) return;
        QMetaObject::invokeMethod(guard, [guard]() {
            if (guard.isNull()) return; guard->preprocessingActive_=false;
            emit guard->captureFailed(QStringLiteral("截图后台处理失败"));
        }, Qt::QueuedConnection);
    });
}

bool ScreenCapture::captureInternal(CapturedImage* output, QString* errorMessage,
                                    CaptureTrigger trigger, bool persistToDisk)
{
    if (output == nullptr) {
        if (errorMessage) *errorMessage = QStringLiteral("截图输出不能为空");
        return false;
    }
    QElapsedTimer elapsed; elapsed.start();
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen == nullptr) {
        if (errorMessage) *errorMessage = QStringLiteral("当前没有可用的显示器");
        return false;
    }
    const QPixmap pixmap = screen->grabWindow(0);
    if (pixmap.isNull()) {
        if (errorMessage) *errorMessage = QStringLiteral("无法获取屏幕图像");
        return false;
    }
    const QImage sourceImage = pixmap.toImage();
    CapturedImage result;
    result.captureId = QUuid::createUuid().toString(QUuid::Id128);
    result.capturedAt = QDateTime::currentDateTimeUtc();
    result.trigger = trigger;
    result.appHint = foregroundWindowHint();
    result.fingerprint = ScreenFingerprint::create(sourceImage);
    QString actualFormat;
    QString compressionError;
    if (!ImageCompressor::compress(sourceImage, options_, &result.data, &actualFormat,
                                   &result.size, &compressionError)) {
        if (errorMessage) *errorMessage = compressionError;
        return false;
    }
    result.format = actualFormat;
    result.durationMs = static_cast<int>(elapsed.elapsed());
    if (persistToDisk && !captureDirectory_.isEmpty()) {
        if (!QDir().mkpath(captureDirectory_)) {
            if (errorMessage) *errorMessage = QStringLiteral("无法创建截图目录: %1")
                .arg(captureDirectory_);
            return false;
        }
        const QString stamp = result.capturedAt.toString(QStringLiteral("yyyyMMdd_hhmmss_zzz"));
        const QString path = QDir(captureDirectory_).filePath(
            QStringLiteral("capture_%1.%2").arg(stamp, result.format));
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(result.data) != result.data.size()) {
            if (errorMessage) *errorMessage = QStringLiteral("无法保存截图: %1").arg(path);
            return false;
        }
        result.filePath = path;
    }
    *output = std::move(result);
    return true;
}

} // namespace zhu_screen_pet
