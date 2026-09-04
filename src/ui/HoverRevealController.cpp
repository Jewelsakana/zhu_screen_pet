#include "ui/HoverRevealController.h"

#include <QApplication>
#include <QCursor>
#include <QEvent>
#include <QPropertyAnimation>
#include <QTimer>
#include <QWidget>

namespace zhu_screen_pet {

HoverRevealController::HoverRevealController(QObject* parent)
    : QObject(parent), hideTimer_(new QTimer(this)), cursorPollTimer_(new QTimer(this))
{
    hideTimer_->setSingleShot(true);
    connect(hideTimer_, &QTimer::timeout, this, [this]() {
        if (canHideNow()) animateTo(0.0, true);
        else scheduleHide();
    });
    // Windows 对近乎透明的分层窗口不总能稳定产生 Enter 事件，因此同时轮询全局鼠标位置。
    cursorPollTimer_->setInterval(50);
    connect(cursorPollTimer_, &QTimer::timeout, this, &HoverRevealController::inspectCursor);
}

void HoverRevealController::bind(QWidget* panel, QWidget* hotZone)
{
    if (panel_ != nullptr) panel_->removeEventFilter(this);
    if (hotZone_ != nullptr) hotZone_->removeEventFilter(this);
    panel_ = panel;
    hotZone_ = hotZone;
    if (panel_ == nullptr || hotZone_ == nullptr) return;
    panel_->installEventFilter(this);
    hotZone_->installEventFilter(this);
    // 顶层原生窗口使用 windowOpacity；QGraphicsOpacityEffect 在 Windows 首次显示时
    // 可能先渲染出黑色 backing store，导致奶白面板第一次出现为黑色。
    panel_->setWindowOpacity(0.0);
    animation_ = new QPropertyAnimation(panel_, "windowOpacity", this);
    // 热区只提供全局几何范围，不显示原生透明窗口，避免阻挡桌面点击。
    hotZone_->hide();
    panel_->hide();
    revealed_ = false;
    if (active_) cursorPollTimer_->start();
}

void HoverRevealController::setTimings(int hideDelayMs, int fadeDurationMs)
{
    hideDelayMs_ = qMax(100, hideDelayMs);
    fadeDurationMs_ = qMax(0, fadeDurationMs);
}

void HoverRevealController::setActive(bool active)
{
    if (active_ == active) {
        if (active_ && !cursorPollTimer_->isActive()) cursorPollTimer_->start();
        return;
    }
    active_ = active;
    hideTimer_->stop();
    if (!active_) {
        cursorPollTimer_->stop();
        if (animation_ != nullptr) animation_->stop();
        if (panel_ != nullptr) panel_->setWindowOpacity(0.0);
        if (panel_ != nullptr) panel_->hide();
        if (hotZone_ != nullptr) hotZone_->hide();
        revealed_ = false;
        return;
    }
    cursorPollTimer_->start();
    if (hotZone_ != nullptr) hotZone_->hide();
    inspectCursor();
}

bool HoverRevealController::isActive() const
{
    return active_;
}

void HoverRevealController::setCanHidePredicate(std::function<bool()> predicate)
{
    canHidePredicate_ = std::move(predicate);
}

void HoverRevealController::reveal()
{
    if (!active_ || panel_ == nullptr || hotZone_ == nullptr) return;
    hideTimer_->stop();
    hotZone_->hide();
    panel_->ensurePolished();
    panel_->show();
    panel_->raise();
    revealed_ = true;
    animateTo(1.0, false);
}

void HoverRevealController::hideNow()
{
    hideTimer_->stop();
    if (panel_ == nullptr || hotZone_ == nullptr) return;
    if (animation_ != nullptr) animation_->stop();
    panel_->setWindowOpacity(0.0);
    panel_->hide();
    hotZone_->hide();
    revealed_ = false;
}

void HoverRevealController::showTemporarily()
{
    reveal();
    scheduleHide();
}

bool HoverRevealController::isRevealed() const
{
    return revealed_;
}

bool HoverRevealController::eventFilter(QObject* watched, QEvent* event)
{
    if (active_ && (watched == panel_ || watched == hotZone_) && event->type() == QEvent::Enter) {
        reveal();
    } else if (watched == panel_ && event->type() == QEvent::Leave) {
        scheduleHide();
    } else if (watched == panel_ && event->type() == QEvent::FocusOut) {
        scheduleHide();
    }
    return QObject::eventFilter(watched, event);
}

void HoverRevealController::scheduleHide()
{
    if (active_ && revealed_ && !hideTimer_->isActive()) hideTimer_->start(hideDelayMs_);
}

void HoverRevealController::inspectCursor()
{
    if (!active_ || panel_ == nullptr || hotZone_ == nullptr) return;
    const QPoint cursor = QCursor::pos();
    const bool nearHiddenPanel = containsGlobalPoint(panel_, cursor, activationMargin_);
    const bool inHotZone = containsGlobalPoint(hotZone_, cursor);
    if (nearHiddenPanel || inHotZone) {
        hideTimer_->stop();
        if (!revealed_) reveal();
    } else if (revealed_) {
        scheduleHide();
    }
}

bool HoverRevealController::containsGlobalPoint(const QWidget* widget, const QPoint& point,
                                                int margin) const
{
    if (widget == nullptr) return false;
    return widget->frameGeometry().adjusted(-margin, -margin, margin, margin).contains(point);
}

void HoverRevealController::animateTo(qreal opacity, bool hideWhenFinished)
{
    if (animation_ == nullptr || panel_ == nullptr) return;
    animation_->stop();
    QObject::disconnect(animation_, nullptr, this, nullptr);
    animation_->setDuration(fadeDurationMs_);
    animation_->setStartValue(panel_->windowOpacity());
    animation_->setEndValue(opacity);
    if (hideWhenFinished) {
        connect(animation_, &QPropertyAnimation::finished, this, [this]() {
            if (panel_ != nullptr) panel_->hide();
            if (hotZone_ != nullptr) hotZone_->hide();
            revealed_ = false;
        });
    }
    animation_->start();
}

bool HoverRevealController::canHideNow() const
{
    if (!active_ || panel_ == nullptr) return false;
    if (containsGlobalPoint(panel_, QCursor::pos(), activationMargin_)) return false;
    if (containsGlobalPoint(hotZone_, QCursor::pos())) return false;
    QWidget* focused = QApplication::focusWidget();
    if (focused != nullptr && (focused == panel_ || panel_->isAncestorOf(focused))) return false;
    return !canHidePredicate_ || canHidePredicate_();
}

} // namespace zhu_screen_pet
