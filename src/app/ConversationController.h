#pragma once

#include <QMetaType>
#include <QObject>
#include <QVector>

#include "core/AppError.h"
#include "memory/ConversationTypes.h"

namespace zhu_screen_pet {

class ConversationRepository;
class SettingsRepository;
class ChatController;

/** 会话应用服务：管理会话列表、当前会话、创建和归档，不让 UI 直接接触数据库。 */
class ConversationController final : public QObject
{
    Q_OBJECT

public:
    explicit ConversationController(ConversationRepository* repository,
                                     SettingsRepository* settings = nullptr,
                                     QObject* parent = nullptr);

    /** 绑定聊天控制器，使所有会话变更在模型请求进行期间由应用层统一拒绝。 */
    void setChatController(ChatController* controller);

    /** 从设置恢复当前会话；记录失效或归档时自动选择/创建可用会话。 */
    bool initialize(AppError* error = nullptr);
    /** 重新读取未归档会话列表并通知 UI。 */
    bool refresh(AppError* error = nullptr);
    /** 创建会话并立即切换到该会话。标题为空时使用默认标题。 */
    bool createConversation(const QString& title = {}, AppError* error = nullptr);
    /** 切换到指定的未归档会话并加载其历史消息。 */
    bool switchConversation(const QString& conversationId, AppError* error = nullptr);
    /** 归档指定会话；归档当前会话后自动切换到其他会话或创建新会话。 */
    bool archiveConversation(const QString& conversationId, AppError* error = nullptr);
    /** 归档当前会话。 */
    bool archiveCurrentConversation(AppError* error = nullptr);
    /** 永久删除指定会话及其消息；删除当前会话后自动切换或创建会话。 */
    bool deleteConversation(const QString& conversationId, AppError* error = nullptr);
    bool deleteCurrentConversation(AppError* error = nullptr);

    QString currentConversationId() const;
    QString currentConversationTitle() const;
    QVector<ConversationMessage> currentConversationMessages() const;
    /** 返回最近一次刷新得到的已归档会话，供归档管理界面使用。 */
    QVector<Conversation> archivedConversations() const;
    /** 返回最近一次刷新得到的未归档会话。 */
    QVector<Conversation> conversations() const;

signals:
    /** 当前可选会话列表发生变化。 */
    void conversationsChanged(const QVector<Conversation>& conversations);
    /** 当前会话及其历史消息已加载。 */
    void currentConversationChanged(const QString& conversationId,
                                    const QString& title,
                                    const QVector<ConversationMessage>& messages);
    /** 会话操作失败，统一交给 ErrorCenter 路由。 */
    void operationFailed(const AppError& error);

private:
    bool fail(const AppError& error, AppError* output);
    AppError makeError(AppErrorCode code, const QString& message,
                       const QString& technical, const QString& operation) const;
    bool persistCurrentId(const QString& conversationId, AppError* error);
    void applyConversationSnapshot(const QVector<Conversation>& all);
    void commitCurrentConversation(const Conversation& conversation,
                                   const QVector<ConversationMessage>& messages,
                                   const QVector<Conversation>& all);
    bool ensureChatIdle(const QString& operation, AppError* error);

    ConversationRepository* repository_ = nullptr;
    SettingsRepository* settings_ = nullptr;
    ChatController* chatController_ = nullptr;
    QVector<Conversation> conversations_;
    QVector<Conversation> archivedConversations_;
    QString currentConversationId_;
    QString currentConversationTitle_;
    QVector<ConversationMessage> currentConversationMessages_;
};

} // namespace zhu_screen_pet

Q_DECLARE_METATYPE(QVector<zhu_screen_pet::Conversation>)
Q_DECLARE_METATYPE(QVector<zhu_screen_pet::ConversationMessage>)
