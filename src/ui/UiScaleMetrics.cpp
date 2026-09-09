#include "ui/UiScaleMetrics.h"

#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QWidget>
#include <QtMath>

#include "infrastructure/WindowPlacement.h"

namespace zhu_screen_pet {

UiScaleMetrics::UiScaleMetrics(int percent)
    : percent_(qMax(1, percent)), factor_(qreal(percent_) / 100.0)
{
}

int UiScaleMetrics::percent() const { return percent_; }
qreal UiScaleMetrics::factor() const { return factor_; }

int UiScaleMetrics::scaled(int designValue, int minimum) const
{
    return qMax(minimum, qRound(designValue * factor_));
}

QSize UiScaleMetrics::scaledForScreen(const QSize& designSize,
                                      const QRect& available) const
{
    const QSize screenScaled = WindowPlacement::scaleForScreen(designSize, available);
    return QSize(qMin(available.width(), scaled(screenScaled.width())),
                 qMin(available.height(), scaled(screenScaled.height())));
}

QSize UiScaleMetrics::scaledForWindow(const QWidget* window,
                                      const QSize& designSize) const
{
    QScreen* targetScreen = window == nullptr ? nullptr : window->screen();
    if (targetScreen == nullptr) targetScreen = QGuiApplication::primaryScreen();
    const QRect available = targetScreen == nullptr
        ? QRect(0, 0, 1920, 1080) : targetScreen->availableGeometry();
    return scaledForScreen(designSize, available);
}

QFont UiScaleMetrics::readableFont(const QFont& source, qreal minimumPoints,
                                   int minimumPixels) const
{
    QFont font = source;
    if (font.family().isEmpty()) font = QApplication::font();
    if (font.pointSizeF() > 0) {
        font.setPointSizeF(qMax(minimumPoints, font.pointSizeF() * factor_));
    } else if (font.pixelSize() > 0) {
        font.setPixelSize(qMax(minimumPixels, qRound(font.pixelSize() * factor_)));
    }
    return font;
}

} // namespace zhu_screen_pet
