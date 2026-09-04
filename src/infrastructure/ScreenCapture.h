#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QSize>

#include "infrastructure/ImageCompressor.h"

class QTimer;

namespace zhu_screen_pet {

struct CapturedImage
{
    QByteArray data;
    QString format;
    QSize size;
    QDateTime capturedAt;
    QString filePath;
};

/** 桌面截图服务：当前阶段使用 Qt 兼容后端，为后续 DXGI 后端预留独立接口。 */
class ScreenCapture final : public QObject
{
    Q_OBJECT

public:
    explicit ScreenCapture(QObject* parent = nullptr);
    void configure(bool enabled, int intervalMs, QString captureDirectory,
                   ImageCompressionOptions options);
    void start();
    void stop();
    bool isEnabled() const;
    bool captureNow(QString* errorMessage = nullptr);
    /** 清除本服务生成的截图文件，但保留截图目录本身。 */
    bool clearCaptures(QString* errorMessage = nullptr) const;

signals:
    void captured(const CapturedImage& image);
    void captureFailed(const QString& errorMessage);

private slots:
    void captureOnTimer();

private:
    bool captureInternal(QString* errorMessage);

    QTimer* timer_ = nullptr;
    bool enabled_ = false;
    int intervalMs_ = 5000;
    QString captureDirectory_;
    ImageCompressionOptions options_;
};

} // namespace zhu_screen_pet

Q_DECLARE_METATYPE(zhu_screen_pet::CapturedImage)
