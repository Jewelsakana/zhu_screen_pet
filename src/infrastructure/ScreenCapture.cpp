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

#include <utility>

#include "infrastructure/ImageCompressor.h"

namespace zhu_screen_pet {

ScreenCapture::ScreenCapture(QObject* parent)
    : QObject(parent), timer_(new QTimer(this))
{
    timer_->setSingleShot(false);
    connect(timer_, &QTimer::timeout, this, &ScreenCapture::captureOnTimer);
}

void ScreenCapture::configure(bool enabled, int intervalMs, QString captureDirectory,
                              ImageCompressionOptions options)
{
    enabled_ = enabled;
    intervalMs_ = qBound(1000, intervalMs, 600000);
    captureDirectory_ = std::move(captureDirectory);
    options_ = std::move(options);
    if (timer_->isActive()) {
        timer_->setInterval(intervalMs_);
    }
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

bool ScreenCapture::captureNow(QString* errorMessage)
{
    return captureInternal(errorMessage);
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
    QString error;
    if (!captureInternal(&error)) emit captureFailed(error);
}

bool ScreenCapture::captureInternal(QString* errorMessage)
{
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
    CapturedImage result;
    result.capturedAt = QDateTime::currentDateTimeUtc();
    QString actualFormat;
    QString compressionError;
    if (!ImageCompressor::compress(pixmap.toImage(), options_, &result.data, &actualFormat,
                                   &result.size, &compressionError)) {
        if (errorMessage) *errorMessage = compressionError;
        return false;
    }
    result.format = actualFormat;
    if (!captureDirectory_.isEmpty()) {
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
    emit captured(result);
    return true;
}

} // namespace zhu_screen_pet
