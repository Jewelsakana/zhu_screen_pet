#pragma once

#include <QPointer>
#include <QWidget>
#include <QVector>

#include "memory/ConversationTypes.h"

class QListWidget;
class QListWidgetItem;

namespace zhu_screen_pet {

class ConversationController;
class ConversationHistoryWindow;

/** 会话列表窗口；负责创建、选择和管理会话，并为每个会话打开独立历史窗口。 */
class ConversationWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit ConversationWindow(QWidget* parent = nullptr);
    ~ConversationWindow() override;
    void setController(ConversationController* controller);
    void setConversation(const QString& id, const QString& title,
                         const QVector<ConversationMessage>& messages);
    /** 将当前聊天流同步给已经打开的当前会话历史窗口。 */
    void appendMessage(MessageRole role, const QString& content);
    void beginAssistantReply();
    void appendAssistantDelta(const QString& delta);
    void finishAssistantReply(const QString& content);
    void setConversationAvatarPath(const QString& path);
    /** 隐藏列表持有的全部历史窗口，用于桌宠整体最小化。 */
    void hideAllHistoryWindows();
    ConversationHistoryWindow* historyWindow() const;

signals:
    void hidden();
    /** 历史窗口显示或尺寸变化后，请求上层重新排列窗口链。 */
    void historyWindowShown();

protected:
    void hideEvent(QHideEvent* event) override;

private:
    void refreshList(const QVector<Conversation>& conversations);
    void openConversationItem(QListWidgetItem* item);
    void showCurrentHistory();
    ConversationHistoryWindow* currentHistoryWindow() const;
    QString selectedConversationId() const;
    void createConversation();
    void archiveConversation();
    void deleteConversation();
    void manageArchived();

    ConversationController* controller_ = nullptr;
    QString currentId_;
    QString currentTitle_;
    QVector<ConversationMessage> currentMessages_;
    QString streamingAssistantContent_;
    bool streamingAssistantActive_ = false;
    QListWidget* list_ = nullptr;
    QPointer<ConversationHistoryWindow> historyWindow_;
    QString conversationAvatarPath_;
};

} // namespace zhu_screen_pet
