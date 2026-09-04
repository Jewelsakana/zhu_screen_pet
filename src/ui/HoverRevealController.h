#pragma once

#include <functional>

#include <QObject>
#include <QPointer>
#include <QPoint>

class QPropertyAnimation;
class QTimer;
class QWidget;

namespace zhu_screen_pet {

/** 控制一块悬浮 UI 的感应显示、延迟隐藏和淡入淡出动画。 */
class HoverRevealController final : public QObject
{
    Q_OBJECT

public:
    explicit HoverRevealController(QObject* parent = nullptr);

    /** 绑定实际面板和隐藏后保留的感应热区，两者所有权不转移。 */
    void bind(QWidget* panel, QWidget* hotZone);
    void setTimings(int hideDelayMs, int fadeDurationMs);
    /** 启停鼠标位置监听；桌宠整体隐藏时必须停用，防止附属窗口被单独唤醒。 */
    void setActive(bool active);
    bool isActive() const;
    /** 返回 false 时禁止隐藏，例如输入框有内容或输入法正在组合文字。 */
    void setCanHidePredicate(std::function<bool()> predicate);
    void reveal();
    void hideNow();
    void showTemporarily();
    bool isRevealed() const;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void scheduleHide();
    void inspectCursor();
    bool containsGlobalPoint(const QWidget* widget, const QPoint& point, int margin = 0) const;
    void animateTo(qreal opacity, bool hideWhenFinished);
    bool canHideNow() const;

    QPointer<QWidget> panel_;
    QPointer<QWidget> hotZone_;
    QPropertyAnimation* animation_ = nullptr;
    QTimer* hideTimer_ = nullptr;
    QTimer* cursorPollTimer_ = nullptr;
    std::function<bool()> canHidePredicate_;
    int hideDelayMs_ = 600;
    int fadeDurationMs_ = 180;
    int activationMargin_ = 18;
    bool active_ = true;
    bool revealed_ = false;
};

} // namespace zhu_screen_pet
