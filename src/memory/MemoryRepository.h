#pragma once

#include <QVector>
#include <optional>

#include "core/Result.h"
#include "memory/ConversationTypes.h"

namespace zhu_screen_pet {

/** 观察、短期和长期记忆的持久化与关键词检索抽象。 */
class MemoryRepository
{
public:
    virtual ~MemoryRepository() = default;

    virtual Result<qint64> saveResult(const MemoryItem& item) = 0;
    virtual Result<qint64> saveShortTermResult(const QString& content,
        const QString& sourceEventId = {}, const QDateTime& expiresAt = {}) = 0;
    virtual Result<qint64> saveLongTermResult(const QString& content,
        const QString& sourceEventId = {}) = 0;
    virtual Result<void> removeResult(qint64 id) = 0;
    virtual Result<void> clearKindResult(const QString& kind) = 0;
    /** 删除已过期的短期记忆，并只保留最近的 maxItems 条。 */
    virtual Result<int> cleanupShortTermResult(const QDateTime& now, int maxItems) = 0;
    virtual Result<MemoryItem> getResult(qint64 id) const = 0;
    virtual Result<qint64> upsertLongTermResult(const MemoryItem& item) = 0;
    virtual Result<std::optional<MemoryItem>> conversationSummaryResult(
        const QString& conversationId) const = 0;
    virtual Result<QVector<MemoryItem>> searchResult(const QString& query, int limit) const = 0;
    virtual Result<QVector<ConversationMessage>> searchConversationMessagesResult(
        const QString& query, int limit) const = 0;
    virtual Result<QVector<MemoryItem>> searchShortTermResult(
        const QString& query, int limit) const = 0;
    virtual Result<QVector<MemoryItem>> searchLongTermResult(
        const QString& query, int limit) const = 0;

    qint64 save(const MemoryItem& item, QString* errorMessage = nullptr);
    qint64 saveShortTerm(const QString& content, const QString& sourceEventId = {},
                         const QDateTime& expiresAt = {}, QString* errorMessage = nullptr);
    qint64 saveLongTerm(const QString& content, const QString& sourceEventId = {},
                        QString* errorMessage = nullptr);
    bool remove(qint64 id, QString* errorMessage = nullptr);
    bool clearKind(const QString& kind, QString* errorMessage = nullptr);
    int cleanupShortTerm(const QDateTime& now, int maxItems,
                         QString* errorMessage = nullptr);
    MemoryItem get(qint64 id, QString* errorMessage = nullptr) const;
    qint64 upsertLongTerm(const MemoryItem& item, QString* errorMessage = nullptr);
    std::optional<MemoryItem> conversationSummary(const QString& conversationId,
                                                  QString* errorMessage = nullptr) const;
    QVector<MemoryItem> search(const QString& query, int limit, QString* errorMessage = nullptr) const;
    QVector<ConversationMessage> searchConversationMessages(
        const QString& query, int limit, QString* errorMessage = nullptr) const;
    QVector<MemoryItem> searchShortTerm(
        const QString& query, int limit, QString* errorMessage = nullptr) const;
    QVector<MemoryItem> searchLongTerm(
        const QString& query, int limit, QString* errorMessage = nullptr) const;
};

} // namespace zhu_screen_pet
