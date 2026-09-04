#pragma once

#include <QWidget>
#include <QVector>

#include "memory/ConversationTypes.h"

class QLabel;
class QScrollArea;
class QTimer;
class QVBoxLayout;
class QWidget;

namespace zhu_screen_pet {

/** 单个会话的独立历史窗口；助手显示左侧头像和蓝色气泡，用户显示右侧紫色气泡。 */
class ConversationHistoryWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit ConversationHistoryWindow(QWidget* parent = nullptr);

    QString conversationId() const;
    void setConversation(const QString& id, const QString& title,
                         const QVector<ConversationMessage>& messages);
    void appendMessage(MessageRole role, const QString& content);
    void beginAssistantReply();
    void appendAssistantDelta(const QString& delta);
    void finishAssistantReply(const QString& content);
    void setPetAvatarPath(const QString& path);

private:
    QLabel* addMessageBubble(MessageRole role, const QString& content);
    void clearMessages();
    void scrollToBottom();

    QString conversationId_;
    QLabel* title_ = nullptr;
    QScrollArea* scrollArea_ = nullptr;
    QWidget* messageContainer_ = nullptr;
    QVBoxLayout* messageLayout_ = nullptr;
    QLabel* streamingBubble_ = nullptr;
    QTimer* scrollTimer_ = nullptr;
    QString petAvatarPath_;
};

} // namespace zhu_screen_pet
