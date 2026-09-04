#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>

namespace zhu_screen_pet {

/** 图像压缩参数。格式支持 jpeg 和 webp；webp 不可用时回退为 jpeg。 */
struct ImageCompressionOptions
{
    QString format = QStringLiteral("jpeg");
    int maxWidth = 1280;
    int quality = 75;
};

class ImageCompressor final
{
public:
    static bool compress(const QImage& source, const ImageCompressionOptions& options,
                         QByteArray* data, QString* actualFormat = nullptr,
                         QSize* outputSize = nullptr, QString* errorMessage = nullptr);
};

} // namespace zhu_screen_pet
