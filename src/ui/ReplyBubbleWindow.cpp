#include "ui/ReplyBubbleWindow.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

#include "infrastructure/DesktopWindowPolicy.h"
#include "ui/UiScaleMetrics.h"

namespace zhu_screen_pet {
namespace {

/** 绘制指向桌宠的气泡尾巴；主体窗口通常位于桌宠左侧。 */
class BubbleTail final : public QWidget
{
public:
    explicit BubbleTail(QWidget* parent = nullptr) : QWidget(parent)
    {
        setFixedSize(22, 38);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setObjectName(QStringLiteral("replyBubbleTail"));
        setPointsRight(true);
    }

    /** true 表示尾巴尖端朝右，false 表示朝左。 */
    void setPointsRight(bool pointsRight)
    {
        if (pointsRight_ == pointsRight && property("pointsRight").isValid()) return;
        pointsRight_ = pointsRight;
        setProperty("pointsRight", pointsRight_);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        QPolygonF triangle;
        if (pointsRight_) {
            triangle << QPointF(0, 7) << QPointF(width() - 2, height() / 2.0)
                     << QPointF(0, height() - 7);
        } else {
            triangle << QPointF(width(), 7) << QPointF(2, height() / 2.0)
                     << QPointF(width(), height() - 7);
        }
        painter.setBrush(QColor(QStringLiteral("#fffaf0")));
        painter.setPen(QPen(QColor(QStringLiteral("#ddcfb7")), 1.0));
        painter.drawPolygon(triangle);
    }

private:
    bool pointsRight_ = true;
};

/** 自行绘制卡片圆角，避免首次显示时依赖平台对 QSS 背景的裁剪时序。 */
class BubbleCard final : public QFrame
{
public:
    explicit BubbleCard(QWidget* parent = nullptr) : QFrame(parent) {}

    void setCornerRadius(qreal radius)
    {
        cornerRadius_ = qMax<qreal>(1.0, radius);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF cardRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        const qreal radius = qMin(cornerRadius_, cardRect.height() / 2.0);
        painter.setPen(QPen(QColor(QStringLiteral("#ddcfb7")), 1.0));
        painter.setBrush(QColor(QStringLiteral("#fffaf0")));
        painter.drawRoundedRect(cardRect, radius, radius);
    }

private:
    qreal cornerRadius_ = 24.0;
};

} // namespace

ReplyBubbleWindow::ReplyBubbleWindow(QWidget* parent)
    : QWidget(parent, Qt::Window), dismissalTimer_(new QTimer(this))
{
    setObjectName(QStringLiteral("replyBubble"));
    setAttribute(Qt::WA_StyledBackground, true);
    DesktopWindowPolicy::apply(this, {true, true, true, false, false, false});
    setStyleSheet(QStringLiteral(
        "QWidget#replyBubble{background:transparent;border:none;}"
        "QFrame#replyBubbleCard{background:transparent;border:none;}"
        "QTextBrowser{background:transparent;border:none;color:#263047;padding:7px;}"
        "QPushButton{border:none;background:#e7f0ff;color:#36558f;border-radius:9px;padding:4px 8px;}"
        "QPushButton:hover{background:#cfe0ff;color:#253c80;}"));
    rootLayout_ = new QHBoxLayout(this);
    rootLayout_->setContentsMargins(1, 1, 1, 1);
    rootLayout_->setSpacing(-1);
    card_ = new BubbleCard(this);
    card_->setObjectName(QStringLiteral("replyBubbleCard"));
    card_->setAttribute(Qt::WA_StyledBackground, true);
    auto* cardLayout = new QVBoxLayout(card_);
    // 关闭按钮悬浮于右上角，避免独占一整行而把正文推到气泡中部。
    cardLayout->setContentsMargins(15, 10, 50, 13);
    close_ = new QPushButton(QStringLiteral("✕"), card_);
    close_->setObjectName(QStringLiteral("replyBubbleClose"));
    close_->setFixedSize(40, 36);
    content_ = new QTextBrowser(card_);
    content_->setObjectName(QStringLiteral("replyBubbleContent"));
    content_->setOpenLinks(false);
    content_->document()->setDocumentMargin(2.0);
    content_->setAlignment(Qt::AlignLeft);
    content_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    content_->setFixedHeight(minimumContentHeight_);
    content_->setCursor(Qt::IBeamCursor);
    cardLayout->addWidget(content_, 0, Qt::AlignTop);
    close_->raise();
    tail_ = new BubbleTail(this);
    rootLayout_->addWidget(card_, 1);
    rootLayout_->addWidget(tail_, 0, Qt::AlignVCenter);
    setFixedWidth(380);
    dismissalTimer_->setSingleShot(true);
    connect(dismissalTimer_, &QTimer::timeout, this, &QWidget::hide);
    connect(close_, &QPushButton::clicked, this, &QWidget::hide);
    adjustSize();
    // 首次 replyDelta 到来前不能随桌宠主窗口一起显示空气泡。
    hide();
}

void ReplyBubbleWindow::setDisplayDuration(int durationMs)
{
    displayDurationMs_ = qBound(1000, durationMs, 300000);
}

void ReplyBubbleWindow::setUiScalePercent(int percent)
{
    const UiScaleMetrics metrics(percent);
    rootLayout_->setContentsMargins(metrics.scaled(1), metrics.scaled(1),
                                    metrics.scaled(1), metrics.scaled(1));
    if (auto* cardLayout = qobject_cast<QVBoxLayout*>(card_->layout())) {
        cardLayout->setContentsMargins(metrics.scaled(15), metrics.scaled(10),
                                       metrics.scaled(50), metrics.scaled(13));
    }
    static_cast<BubbleCard*>(card_)->setCornerRadius(metrics.scaled(24));
    close_->setFixedSize(metrics.scaled(40), metrics.scaled(36));
    tail_->setFixedSize(metrics.scaled(22), metrics.scaled(38));
    minimumContentHeight_ = metrics.scaled(48);
    maximumContentHeight_ = metrics.scaled(220);
    setFixedWidth(metrics.scaled(380));
    updateContentHeight();
    positionCloseButton();
}

void ReplyBubbleWindow::beginReply()
{
    dismissalTimer_->stop();
    content_->clear();
    finished_ = false;
    remainingMs_ = displayDurationMs_;
    show();
    raise();
    updateContentHeight();
}

void ReplyBubbleWindow::appendDelta(const QString& delta)
{
    if (!isVisible() && content_->toPlainText().isEmpty()) beginReply();
    QTextCursor cursor(content_->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(delta);
    content_->setTextCursor(cursor);
    content_->ensureCursorVisible();
    updateContentHeight();
}

void ReplyBubbleWindow::finishReply(const QString& completeContent)
{
    if (!completeContent.isEmpty()) content_->setPlainText(completeContent);
    updateContentHeight();
    if (!isVisible()) show();
    finished_ = true;
    remainingMs_ = displayDurationMs_;
    restartDismissalTimer();
}

void ReplyBubbleWindow::setAttachmentSide(AttachmentSide side)
{
    const bool pointsRight = side != AttachmentSide::Right;
    auto* tail = static_cast<BubbleTail*>(tail_);
    // 桌宠拖动会频繁触发重新定位；方向未变化时不重排布局，避免视觉闪烁。
    if (tail->property("pointsRight").toBool() == pointsRight) return;
    tail->setPointsRight(pointsRight);

    rootLayout_->removeWidget(card_);
    rootLayout_->removeWidget(tail_);
    if (pointsRight) {
        rootLayout_->addWidget(card_, 1);
        rootLayout_->addWidget(tail_, 0, Qt::AlignVCenter);
    } else {
        rootLayout_->addWidget(tail_, 0, Qt::AlignVCenter);
        rootLayout_->addWidget(card_, 1);
    }
}

QString ReplyBubbleWindow::content() const { return content_->toPlainText(); }
int ReplyBubbleWindow::displayDuration() const { return displayDurationMs_; }
bool ReplyBubbleWindow::dismissalTimerActive() const { return dismissalTimer_->isActive(); }

void ReplyBubbleWindow::enterEvent(QEnterEvent* event)
{
    if (dismissalTimer_->isActive()) {
        remainingMs_ = qMax(1, dismissalTimer_->remainingTime());
        dismissalTimer_->stop();
    }
    QWidget::enterEvent(event);
}

void ReplyBubbleWindow::leaveEvent(QEvent* event)
{
    if (finished_) restartDismissalTimer();
    QWidget::leaveEvent(event);
}

void ReplyBubbleWindow::restartDismissalTimer()
{
    if (finished_ && !underMouse()) dismissalTimer_->start(qMax(1, remainingMs_));
}

void ReplyBubbleWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    positionCloseButton();
}

void ReplyBubbleWindow::updateContentHeight()
{
    if (content_ == nullptr || card_ == nullptr) return;
    const auto resizeToCurrentContent = [this]() {
        if (card_->layout() != nullptr) {
            card_->layout()->invalidate();
            card_->layout()->activate();
        }
        rootLayout_->invalidate();
        rootLayout_->activate();
        // 顶层窗口曾显示过长内容后，普通 resize() 可能受旧布局缓存影响而拒绝缩小。
        // 动态固定高度可同时覆盖增长和缩短，宽度仍由既有固定宽度控制。
        setFixedHeight(qMax(1, rootLayout_->sizeHint().height()));
    };
    const int textWidth = qMax(80, content_->viewport()->width());
    content_->document()->setTextWidth(textWidth);
    const int documentHeight = qCeil(content_->document()->size().height());
    const int targetHeight = qBound(minimumContentHeight_, documentHeight + 12,
                                    maximumContentHeight_);
    content_->setFixedHeight(targetHeight);
    card_->updateGeometry();
    resizeToCurrentContent();
    QTimer::singleShot(0, this, [this, resizeToCurrentContent]() {
        if (content_ == nullptr) return;
        const int width = qMax(80, content_->viewport()->width());
        if (!qFuzzyCompare(content_->document()->textWidth(), qreal(width))) {
            content_->document()->setTextWidth(width);
            const int height = qBound(minimumContentHeight_,
                qCeil(content_->document()->size().height()) + 12,
                maximumContentHeight_);
            content_->setFixedHeight(height);
            card_->updateGeometry();
            resizeToCurrentContent();
        }
    });
}

void ReplyBubbleWindow::positionCloseButton()
{
    if (card_ == nullptr || close_ == nullptr) return;
    const int rightMargin = qMax(8, (card_->layout() == nullptr
        ? 11 : card_->layout()->contentsMargins().right()) / 5);
    const int topMargin = card_->layout() == nullptr
        ? 10 : card_->layout()->contentsMargins().top();
    close_->move(qMax(0, card_->width() - close_->width() - rightMargin), topMargin);
    close_->raise();
}

} // namespace zhu_screen_pet
