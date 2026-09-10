#include "ui/LevelProgressWidget.h"

#include <QPainter>
#include <QPaintEvent>

namespace zhu_screen_pet {

LevelProgressWidget::LevelProgressWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("levelProgress"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void LevelProgressWidget::setProgress(int level, int percent)
{
    const int boundedLevel = qBound(0, level, maximumLevel_);
    const int boundedPercent = qBound(0, percent, 100);
    if (level_ == boundedLevel && percent_ == boundedPercent) return;
    level_ = boundedLevel;
    percent_ = boundedPercent;
    update();
}

void LevelProgressWidget::setMaximumLevel(int maximumLevel)
{
    maximumLevel_ = qMax(1, maximumLevel);
    level_ = qBound(0, level_, maximumLevel_);
    update();
}

void LevelProgressWidget::setColors(const QColor& progressColor,
                                    const QColor& levelColor)
{
    progressColor_ = progressColor;
    levelColor_ = levelColor;
    update();
}

void LevelProgressWidget::setShowMaximumLabel(bool enabled)
{
    showMaximumLabel_ = enabled;
    update();
}

void LevelProgressWidget::setShowLevelPrefix(bool enabled)
{
    showLevelPrefix_ = enabled;
    update();
}

int LevelProgressWidget::level() const { return level_; }
int LevelProgressWidget::progressPercent() const { return percent_; }

QSize LevelProgressWidget::sizeHint() const
{
    return QSize(66, 66);
}

void LevelProgressWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal side = qMin(width(), height());
    const qreal penWidth = qMax<qreal>(4.0, side * 0.075);
    const QRectF ring((width() - side) / 2.0 + penWidth,
                      (height() - side) / 2.0 + penWidth,
                      side - penWidth * 2.0, side - penWidth * 2.0);
    QPen backgroundPen(QColor(255, 255, 255, 80), penWidth, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(backgroundPen);
    painter.drawArc(ring, 0, 360 * 16);
    QPen progressPen(progressColor_, penWidth,
                     Qt::SolidLine, Qt::RoundCap);
    painter.setPen(progressPen);
    painter.drawArc(ring, 90 * 16, -percent_ * 360 * 16 / 100);

    QFont font = painter.font();
    font.setBold(true);
    const bool maximum = showMaximumLabel_ && level_ >= maximumLevel_;
    font.setPointSizeF(qMax<qreal>(7.0, side * (maximum ? 0.13 : 0.16)));
    painter.setFont(font);
    painter.setPen(levelColor_);
    const QString text = maximum ? QStringLiteral("LV.MAX")
        : !showLevelPrefix_ ? QString::number(level_)
        : maximumLevel_ <= 10 ? QStringLiteral("LV.%1").arg(level_)
                              : QStringLiteral("LV.%1").arg(level_, 2, 10, QLatin1Char('0'));
    painter.drawText(rect(), Qt::AlignCenter, text);
}

} // namespace zhu_screen_pet
