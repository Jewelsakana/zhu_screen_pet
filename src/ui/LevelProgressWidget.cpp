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
    const int boundedLevel = qBound(0, level, 100);
    const int boundedPercent = qBound(0, percent, 100);
    if (level_ == boundedLevel && percent_ == boundedPercent) return;
    level_ = boundedLevel;
    percent_ = boundedPercent;
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
    QPen progressPen(QColor(QStringLiteral("#35b66a")), penWidth,
                     Qt::SolidLine, Qt::RoundCap);
    painter.setPen(progressPen);
    painter.drawArc(ring, 90 * 16, -percent_ * 360 * 16 / 100);

    QFont font = painter.font();
    font.setBold(true);
    font.setPointSizeF(qMax<qreal>(8.0, side * 0.16));
    painter.setFont(font);
    painter.setPen(QColor(QStringLiteral("#20242c")));
    painter.drawText(rect(), Qt::AlignCenter,
                     QStringLiteral("LV.%1").arg(level_, 2, 10, QLatin1Char('0')));
}

} // namespace zhu_screen_pet
