#pragma once

#include <QVector>

#include "core/Result.h"
#include "memory/ConversationTypes.h"

namespace zhu_screen_pet {

/** 会话和消息的持久化抽象；实现可以使用 SQLite 或测试替身。 */
class ConversationRepository
{
public:
    virtual ~ConversationRepository() = default;

    virtual Result<QString> createConversationResult(const QString& title) = 0;
    virtual Result<Conversation> getConversationResult(const QString& id) const = 0;
    virtual Result<QVector<Conversation>> listConversationsResult(bool includeArchived = false) const = 0;
    virtual Result<void> archiveConversationResult(const QString& id) = 0;
    virtual Result<void> deleteConversationResult(const QString& id) = 0;
    virtual Result<void> appendMessageResult(const QString& conversationId, const Message& message,
                                             int tokenCount = 0) = 0;
    virtual Result<QVector<ConversationMessage>> recentMessagesResult(
        const QString& conversationId, int limit) const = 0;
    virtual Result<QVector<ConversationMessage>> unsummarizedMessagesResult(
        const QString& conversationId, int limit) const = 0;
    virtual Result<void> markMessagesSummarizedResult(
        const QVector<qint64>& messageIds, const QDateTime& summarizedAt) = 0;
    /** 删除摘要已成功保存且超过短期保留期的原始消息。 */
    virtual Result<int> removeSummarizedBeforeResult(const QDateTime& cutoff) = 0;

    // 兼容旧调用方的薄适配层；新代码应优先使用 Result 接口。
    QString createConversation(const QString& title, QString* errorMessage = nullptr);
    bool getConversation(const QString& id, Conversation* result, QString* errorMessage = nullptr) const;
    QVector<Conversation> listConversations(bool includeArchived = false,
                                            QString* errorMessage = nullptr) const;
    bool archiveConversation(const QString& id, QString* errorMessage = nullptr);
    bool deleteConversation(const QString& id, QString* errorMessage = nullptr);
    bool appendMessage(const QString& conversationId, const Message& message,
                       int tokenCount = 0, QString* errorMessage = nullptr);
    QVector<ConversationMessage> recentMessages(const QString& conversationId, int limit,
                                                QString* errorMessage = nullptr) const;
    QVector<ConversationMessage> unsummarizedMessages(const QString& conversationId, int limit,
                                                      QString* errorMessage = nullptr) const;
    bool markMessagesSummarized(const QVector<qint64>& messageIds,
                                const QDateTime& summarizedAt = {},
                                QString* errorMessage = nullptr);
    int removeSummarizedBefore(const QDateTime& cutoff,
                               QString* errorMessage = nullptr);
};

} // namespace zhu_screen_pet
