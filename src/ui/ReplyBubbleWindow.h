#pragma once

#include <QWidget>

#include "infrastructure/WindowPlacement.h"

class QLabel;
class QFrame;
class QHBoxLayout;
class QPushButton;
class QTextBrowser;
class QTimer;

namespace zhu_screen_pet {

/** 只展示最近一次助手回复的临时气泡；流式阶段持续更新同一内容。 */
class ReplyBubbleWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit ReplyBubbleWindow(QWidget* parent = nullptr);

    void setDisplayDuration(int durationMs);
    void beginReply();
    void appendDelta(const QString& delta);
    void finishReply(const QString& completeContent = {});
    /** 根据气泡相对桌宠的实际位置切换尾巴方向。 */
    void setAttachmentSide(AttachmentSide side);
    QString content() const;
    int displayDuration() const;
    bool dismissalTimerActive() const;

protected:
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void restartDismissalTimer();

    QTextBrowser* content_ = nullptr;
    QPushButton* close_ = nullptr;
    QHBoxLayout* rootLayout_ = nullptr;
    QFrame* card_ = nullptr;
    QWidget* tail_ = nullptr;
    QTimer* dismissalTimer_ = nullptr;
    int displayDurationMs_ = 15000;
    int remainingMs_ = 15000;
    bool finished_ = false;
};

} // namespace zhu_screen_pet
