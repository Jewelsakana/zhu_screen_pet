#pragma once

#include "memory/ConversationRepository.h"

namespace zhu_screen_pet {

class Database;

/** 基于 Database 当前 SQLite 连接的会话仓库。 */
class SqliteConversationRepository final : public ConversationRepository
{
public:
    explicit SqliteConversationRepository(Database* database);

    Result<QString> createConversationResult(const QString& title) override;
    Result<Conversation> getConversationResult(const QString& id) const override;
    Result<QVector<Conversation>> listConversationsResult(bool includeArchived = false) const override;
    Result<void> archiveConversationResult(const QString& id) override;
    Result<void> deleteConversationResult(const QString& id) override;
    Result<void> appendMessageResult(const QString& conversationId, const Message& message,
                                     int tokenCount = 0) override;
    Result<QVector<ConversationMessage>> recentMessagesResult(
        const QString& conversationId, int limit) const override;

private:
    Database* database_ = nullptr;
};

} // namespace zhu_screen_pet
