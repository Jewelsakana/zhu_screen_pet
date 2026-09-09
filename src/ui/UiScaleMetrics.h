#pragma once

#include <QFont>
#include <QRect>
#include <QSize>

class QWidget;

namespace zhu_screen_pet {

/** 集中定义 UI 比例、可读字体下限及按屏幕约束的尺寸换算。 */
class UiScaleMetrics final
{
public:
    explicit UiScaleMetrics(int percent);

    int percent() const;
    qreal factor() const;
    int scaled(int designValue, int minimum = 1) const;
    QSize scaledForScreen(const QSize& designSize, const QRect& available) const;
    QSize scaledForWindow(const QWidget* window, const QSize& designSize) const;
    QFont readableFont(const QFont& base = QFont(), qreal minimumPoints = 8.5,
                       int minimumPixels = 12) const;

private:
    int percent_ = 100;
    qreal factor_ = 1.0;
};

} // namespace zhu_screen_pet
