#include "infrastructure/ImageCompressor.h"

#include <QBuffer>
#include <QImageWriter>

namespace zhu_screen_pet {

bool ImageCompressor::compress(const QImage& source, const ImageCompressionOptions& options,
                               QByteArray* data, QString* actualFormat,
                               QSize* outputSize, QString* errorMessage)
{
    if (data == nullptr || source.isNull()) {
        if (errorMessage) *errorMessage = QStringLiteral("图像数据或输出指针无效");
        return false;
    }
    const int maxWidth = qBound(1, options.maxWidth, 8192);
    const int quality = qBound(1, options.quality, 100);
    QImage image = source;
    if (image.width() > maxWidth) {
        image = image.scaledToWidth(maxWidth, Qt::SmoothTransformation);
    }

    QString format = options.format.trimmed().toLower();
    if (format == QStringLiteral("jpg")) format = QStringLiteral("jpeg");
    if (format != QStringLiteral("jpeg") && format != QStringLiteral("webp")) {
        format = QStringLiteral("jpeg");
    }
    const QByteArray formatBytes = format.toLatin1();
    QBuffer buffer(data);
    if (!buffer.open(QIODevice::WriteOnly)) {
        if (errorMessage) *errorMessage = QStringLiteral("无法创建图像缓冲区");
        return false;
    }
    QImageWriter writer(&buffer, formatBytes);
    writer.setQuality(quality);
    if (!writer.write(image)) {
        if (format == QStringLiteral("webp")) {
            data->clear();
            buffer.close();
            if (!buffer.open(QIODevice::WriteOnly)) return false;
            QImageWriter fallback(&buffer, QByteArrayLiteral("jpeg"));
            fallback.setQuality(quality);
            if (!fallback.write(image)) {
                if (errorMessage) *errorMessage = fallback.errorString();
                return false;
            }
            format = QStringLiteral("jpeg");
        } else {
            if (errorMessage) *errorMessage = writer.errorString();
            return false;
        }
    }
    if (actualFormat) *actualFormat = format;
    if (outputSize) *outputSize = image.size();
    return true;
}

} // namespace zhu_screen_pet
