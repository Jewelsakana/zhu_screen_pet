#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

namespace zhu_screen_pet {

/** 桌宠上方的持久错误提示；仅展示用户文案，必须由用户主动关闭。 */
class ErrorBannerWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit ErrorBannerWindow(QWidget* parent = nullptr);
    void showError(const QString& userMessage, bool retryable);
    /** 用户确认或开始重试时清除当前错误；普通 hide() 只用于暂时隐藏窗口。 */
    void dismiss();
    /** 桌宠从最小化状态恢复时，重新显示仍未被用户关闭的错误。 */
    void restoreIfActive();
    QString message() const;
    bool hasActiveError() const;

signals:
    void retryRequested();
    void settingsRequested();

private:
    QLabel* message_ = nullptr;
    QPushButton* retry_ = nullptr;
    bool hasActiveError_ = false;
};

} // namespace zhu_screen_pet
