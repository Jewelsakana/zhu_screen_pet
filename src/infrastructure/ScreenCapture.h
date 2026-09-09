#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QSize>

#include "infrastructure/ImageCompressor.h"
#include "infrastructure/ScreenCapturePolicy.h"

class QTimer;

namespace zhu_screen_pet {

enum class CaptureTrigger
{
    Manual,
    Scheduled,
    Chat
};

struct CapturedImage
{
    QByteArray data;
    /** 64x36 差分哈希摘要，不包含可还原的截图图像。 */
    QByteArray fingerprint;
    QString format;
    QSize size;
    QDateTime capturedAt;
    QString filePath;
    CaptureTrigger trigger = CaptureTrigger::Manual;
};

/** 桌面截图服务：当前阶段使用 Qt 兼容后端，为后续 DXGI 后端预留独立接口。 */
class ScreenCapture final : public QObject
{
    Q_OBJECT

public:
    explicit ScreenCapture(QObject* parent = nullptr);
    void configure(bool enabled, int intervalMs, QString captureDirectory,
                   ImageCompressionOptions options);
    void setCaptureDirectory(const QString& captureDirectory);
    void start();
    void stop();
    bool isEnabled() const;
    bool captureNow(QString* errorMessage = nullptr,
                    CaptureTrigger trigger = CaptureTrigger::Manual);
    /** 捕获到内存但不写磁盘、不发送 captured 信号，供单次聊天附件使用。 */
    bool captureImage(CapturedImage* image, QString* errorMessage = nullptr,
                      CaptureTrigger trigger = CaptureTrigger::Chat);
    /** 删除指定的一张由本服务生成的截图。 */
    bool removeCapture(const QString& filePath, QString* errorMessage = nullptr) const;
    /** 显式清除手动测试生成的截图文件，但保留截图目录本身。 */
    bool clearCaptures(QString* errorMessage = nullptr) const;

signals:
    void captured(const CapturedImage& image);
    void captureFailed(const QString& errorMessage);

private slots:
    void captureOnTimer();

private:
    bool captureInternal(CapturedImage* image, QString* errorMessage,
                         CaptureTrigger trigger, bool persistToDisk);

    QTimer* timer_ = nullptr;
    bool enabled_ = false;
    int intervalMs_ = 60000;
    QString captureDirectory_;
    ImageCompressionOptions options_;
};

} // namespace zhu_screen_pet

Q_DECLARE_METATYPE(zhu_screen_pet::CapturedImage)
Q_DECLARE_METATYPE(zhu_screen_pet::CaptureTrigger)
