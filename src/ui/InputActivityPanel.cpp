#include "ui/InputActivityPanel.h"

#include <QApplication>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPaintEvent>

#include "infrastructure/DesktopWindowPolicy.h"
#include "ui/UiScaleMetrics.h"

namespace zhu_screen_pet {

InputActivityPanel::InputActivityPanel(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setObjectName(QStringLiteral("inputActivityPanel"));
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    DesktopWindowPolicy::apply(this, {true, true, true, false, false, true});
    setToolTip(QStringLiteral("键盘输入和鼠标点击的统一累计次数，上限 100000"));
    auto* layout = new QHBoxLayout(this);
    inputCount_ = new QLabel(this);
    inputCount_->setObjectName(QStringLiteral("inputActivityCount"));
    inputCount_->setAlignment(Qt::AlignCenter);
    layout->addWidget(inputCount_);
    setCount(0);
    setUiScalePercent(100);
}

void InputActivityPanel::setCount(qint64 inputCount)
{
    inputCount_->setText(QString::number(qBound(qint64{0}, inputCount, qint64{100000})));
    adjustSize();
}

void InputActivityPanel::setUiScalePercent(int percent)
{
    const UiScaleMetrics metrics(percent);
    const int radius = metrics.scaled(14, 7);
    const int padding = metrics.scaled(8, 5);
    cornerRadius_ = radius;
    setStyleSheet(QStringLiteral(
        "QWidget#inputActivityPanel{background:transparent;border:none;}"
        "QLabel{color:#26375d;background:transparent;border:none;padding:%1px;}")
        .arg(padding));
    if (auto* box = qobject_cast<QHBoxLayout*>(layout())) {
        box->setContentsMargins(metrics.scaled(7), metrics.scaled(3),
                                metrics.scaled(7), metrics.scaled(3));
        box->setSpacing(metrics.scaled(5));
    }
    const QFont font = metrics.readableFont(QApplication::font());
    inputCount_->setFont(font);
    const int textWidth = QFontMetrics(font).horizontalAdvance(QStringLiteral("100000"));
    setFixedWidth(textWidth + metrics.scaled(30));
    adjustSize();
    update();
}

void InputActivityPanel::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF card = QRectF(rect()).adjusted(0.75, 0.75, -0.75, -0.75);
    const qreal radius = qMin(cornerRadius_, card.height() / 2.0);
    painter.setPen(QPen(QColor(QStringLiteral("#eadfca")), 1.5));
    painter.setBrush(QColor(QStringLiteral("#fffdf8")));
    painter.drawRoundedRect(card, radius, radius);
}

} // namespace zhu_screen_pet
