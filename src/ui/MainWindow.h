#pragma once

#include <QHash>
#include <QMainWindow>
#include <QPoint>
#include <QVector>

#include "app/ModelErrorPresenter.h"
#include "app/PetState.h"
#include "app/UiConfig.h"
#include "memory/ConversationTypes.h"
#include "model/ModelError.h"

class QLabel;
class QCloseEvent;
class QEvent;
class QMouseEvent;
class QShowEvent;
class QHideEvent;
class QWidget;

namespace zhu_screen_pet {

class ActionPanel;
class ChatController;
class ChatInputPanel;
class ConversationController;
class ConversationWindow;
class ErrorBannerWindow;
class ErrorCenter;
class HoverRevealController;
class ReplyBubbleWindow;
class SettingsController;
class ScreenCapture;
class WindowAttachmentManager;

/** 透明桌宠主窗口，负责协调附属 UI；聊天、会话和设置业务仍由应用控制器处理。 */
class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void setChatController(ChatController* controller);
    void setErrorCenter(ErrorCenter* errorCenter);
    void setConversationController(ConversationController* controller);
    void setSettingsController(SettingsController* controller);
    void setCaptureDirectory(const QString& directory);
    void setModelErrorMessages(const QHash<QString, QString>& messages);
    void setConversation(const QString& conversationId,
                         const QVector<ConversationMessage>& messages);
    QString conversationId() const;
    ConversationWindow* conversationWindow() const;
    /** 托盘恢复时显示桌宠及自动显隐感应区。 */
    void showPetShell();
    /** 托盘或操作栏调用的设置入口。 */
    void openSettings();
    /** 托盘或操作栏调用的完整会话入口。 */
    void openConversationWindow();

signals:
    /** 操作栏关闭按钮或系统任务栏关闭动作请求结束整个应用。 */
    void applicationExitRequested();

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void createOverlayWindows();
    QWidget* createHotZone(const QString& objectName, const QSize& size);
    void repositionConversationChain();
    void applyUiConfig(const UiConfig& config);
    void sendCurrentMessage();
    void retryLastMessage();
    void cancelCurrentRequest();
    void onRequestStarted(const QString& requestId);
    void onReplyDelta(const QString& requestId, const QString& delta);
    void onReplyFinished(const QString& requestId, const QString& content);
    void onRequestFailed(const QString& requestId, const ModelError& error);
    void onOperationFailed(const AppError& error);
    void onErrorReported(const AppError& error, const QString& userMessage);
    void onCurrentConversationChanged(const QString& conversationId,
                                      const QString& title,
                                      const QVector<ConversationMessage>& messages);
    void hidePetShell();
    void updatePetState(PetState state);
    bool isCurrentRequest(const QString& requestId) const;

    ChatController* chatController_ = nullptr;
    ErrorCenter* errorCenter_ = nullptr;
    ConversationController* conversationController_ = nullptr;
    SettingsController* settingsController_ = nullptr;
    ModelErrorPresenter errorPresenter_;
    QString conversationId_;
    QString currentRequestId_;
    QString lastAssistantReply_;
    bool streamingReplyStarted_ = false;
    bool dragging_ = false;
    QPoint dragOffset_;
    UiConfig uiConfig_;

    QLabel* stateLabel_ = nullptr;
    QLabel* petVisual_ = nullptr;
    ActionPanel* actionPanel_ = nullptr;
    ChatInputPanel* inputPanel_ = nullptr;
    ReplyBubbleWindow* replyBubble_ = nullptr;
    ErrorBannerWindow* errorBanner_ = nullptr;
    ConversationWindow* conversationWindow_ = nullptr;
    QWidget* actionHotZone_ = nullptr;
    QWidget* inputHotZone_ = nullptr;
    HoverRevealController* actionReveal_ = nullptr;
    HoverRevealController* inputReveal_ = nullptr;
    WindowAttachmentManager* attachments_ = nullptr;
    bool repositioningConversationChain_ = false;
    ScreenCapture* screenCapture_ = nullptr;
    QString captureDirectory_;
};

} // namespace zhu_screen_pet
