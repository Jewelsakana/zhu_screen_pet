#pragma once

#include "memory/MemoryRepository.h"

namespace zhu_screen_pet {

class Database;

/** SQLite 记忆仓库；关键词检索使用 LIKE，兼容未启用 FTS5 的 Qt SQLite。 */
class SqliteMemoryRepository final : public MemoryRepository
{
public:
    explicit SqliteMemoryRepository(Database* database);

    Result<qint64> saveResult(const MemoryItem& item) override;
    Result<qint64> saveShortTermResult(const QString& content, const QString& sourceEventId = {},
                                       const QDateTime& expiresAt = {}) override;
    Result<qint64> saveLongTermResult(const QString& content,
                                      const QString& sourceEventId = {}) override;
    Result<void> removeResult(qint64 id) override;
    Result<QVector<MemoryItem>> searchResult(const QString& query, int limit) const override;
    Result<QVector<ConversationMessage>> searchConversationMessagesResult(
        const QString& query, int limit) const override;
    Result<QVector<MemoryItem>> searchShortTermResult(
        const QString& query, int limit) const override;
    Result<QVector<MemoryItem>> searchLongTermResult(
        const QString& query, int limit) const override;

private:
    Result<QVector<MemoryItem>> searchByKindResult(
        const QString& query, const QString& kind, int limit) const;
    Database* database_ = nullptr;
};

} // namespace zhu_screen_pet
