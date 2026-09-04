#pragma once

#include <QVector>

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
    QVector<MemoryItem> search(const QString& query, int limit, QString* errorMessage = nullptr) const;
    QVector<ConversationMessage> searchConversationMessages(
        const QString& query, int limit, QString* errorMessage = nullptr) const;
    QVector<MemoryItem> searchShortTerm(
        const QString& query, int limit, QString* errorMessage = nullptr) const;
    QVector<MemoryItem> searchLongTerm(
        const QString& query, int limit, QString* errorMessage = nullptr) const;
};

} // namespace zhu_screen_pet
