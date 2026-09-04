#include "ui/ReplyBubbleWindow.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QPainter>
#include <QPushButton>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>

#include "infrastructure/DesktopWindowPolicy.h"

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

} // namespace

ReplyBubbleWindow::ReplyBubbleWindow(QWidget* parent)
    : QWidget(parent, Qt::Window), dismissalTimer_(new QTimer(this))
{
    setObjectName(QStringLiteral("replyBubble"));
    setAttribute(Qt::WA_StyledBackground, true);
    DesktopWindowPolicy::apply(this, {true, true, true, false, false, false});
    setStyleSheet(QStringLiteral(
        "QWidget#replyBubble{background:transparent;border:none;}"
        "QFrame#replyBubbleCard{background:#fffaf0;border:1px solid #ddcfb7;border-radius:24px;}"
        "QTextBrowser{background:transparent;border:none;color:#263047;padding:7px;}"
        "QPushButton{border:none;background:#e7f0ff;color:#36558f;border-radius:9px;padding:4px 8px;}"
        "QPushButton:hover{background:#cfe0ff;color:#253c80;}"));
    rootLayout_ = new QHBoxLayout(this);
    rootLayout_->setContentsMargins(1, 1, 1, 1);
    rootLayout_->setSpacing(-1);
    card_ = new QFrame(this);
    card_->setObjectName(QStringLiteral("replyBubbleCard"));
    card_->setAttribute(Qt::WA_StyledBackground, true);
    auto* cardLayout = new QVBoxLayout(card_);
    cardLayout->setContentsMargins(15, 10, 11, 13);
    auto* actions = new QHBoxLayout();
    actions->addStretch();
    close_ = new QPushButton(QStringLiteral("✕"), card_);
    close_->setObjectName(QStringLiteral("replyBubbleClose"));
    actions->addWidget(close_);
    content_ = new QTextBrowser(card_);
    content_->setObjectName(QStringLiteral("replyBubbleContent"));
    content_->setOpenLinks(false);
    content_->setMaximumHeight(220);
    content_->setMinimumHeight(60);
    content_->setCursor(Qt::IBeamCursor);
    cardLayout->addLayout(actions);
    cardLayout->addWidget(content_);
    tail_ = new BubbleTail(this);
    rootLayout_->addWidget(card_, 1);
    rootLayout_->addWidget(tail_, 0, Qt::AlignVCenter);
    setFixedWidth(352);
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

void ReplyBubbleWindow::beginReply()
{
    dismissalTimer_->stop();
    content_->clear();
    finished_ = false;
    remainingMs_ = displayDurationMs_;
    show();
    raise();
}

void ReplyBubbleWindow::appendDelta(const QString& delta)
{
    if (!isVisible() && content_->toPlainText().isEmpty()) beginReply();
    QTextCursor cursor(content_->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(delta);
    content_->setTextCursor(cursor);
    content_->ensureCursorVisible();
}

void ReplyBubbleWindow::finishReply(const QString& completeContent)
{
    if (!completeContent.isEmpty()) content_->setPlainText(completeContent);
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

void ReplyBubbleWindow::enterEvent(QEvent* event)
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

} // namespace zhu_screen_pet
