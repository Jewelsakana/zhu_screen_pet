#include "infrastructure/ScreenFingerprint.h"

#include <QImage>

namespace zhu_screen_pet {

namespace {
int countSetBits(unsigned char value)
{
    int count = 0;
    while (value != 0) {
        value &= static_cast<unsigned char>(value - 1);
        ++count;
    }
    return count;
}
}

QByteArray ScreenFingerprint::create(const QImage& image)
{
    if (image.isNull()) return {};
    const QImage normalized = image
        .scaled(Width + 1, Height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
        .convertToFormat(QImage::Format_Grayscale8);
    if (normalized.isNull()) return {};

    constexpr int bitCount = Width * Height;
    QByteArray fingerprint((bitCount + 7) / 8, '\0');
    for (int y = 0; y < Height; ++y) {
        const uchar* row = normalized.constScanLine(y);
        for (int x = 0; x < Width; ++x) {
            if (row[x] <= row[x + 1]) continue;
            const int bitIndex = y * Width + x;
            fingerprint[bitIndex / 8] = static_cast<char>(
                static_cast<unsigned char>(fingerprint.at(bitIndex / 8))
                | static_cast<unsigned char>(1U << (bitIndex % 8)));
        }
    }
    return fingerprint;
}

double ScreenFingerprint::differenceRatio(const QByteArray& previous,
                                          const QByteArray& current)
{
    constexpr int byteCount = (Width * Height + 7) / 8;
    if (previous.size() != byteCount || current.size() != byteCount) return 1.0;

    int differentBits = 0;
    for (int i = 0; i < byteCount; ++i) {
        const auto difference = static_cast<unsigned char>(previous.at(i))
            ^ static_cast<unsigned char>(current.at(i));
        differentBits += countSetBits(difference);
    }
    return static_cast<double>(differentBits) / static_cast<double>(Width * Height);
}

bool ScreenFingerprint::hasSignificantChange(const QByteArray& previous,
                                             const QByteArray& current,
                                             double threshold)
{
    return differenceRatio(previous, current) >= threshold;
}

} // namespace zhu_screen_pet
