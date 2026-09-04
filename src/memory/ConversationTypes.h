#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

#include "model/Message.h"

namespace zhu_screen_pet {

/** 持久化会话的元数据。 */
struct Conversation
{
    QString id;
    QString title;
    QDateTime createdAt;
    QDateTime updatedAt;
    QDateTime archivedAt;

    bool isArchived() const { return archivedAt.isValid(); }
};

/** 持久化的一条会话消息。 */
struct ConversationMessage
{
    qint64 id = 0;
    QString conversationId;
    Message message;
    int tokenCount = 0;
    QDateTime createdAt;
    QDateTime summarizedAt;
};

/** 可被关键词检索的记忆条目。 */
struct MemoryItem
{
    qint64 id = 0;
    QString kind;
    QString content;
    QString sourceEventId;
    QDateTime createdAt;
    QDateTime expiresAt;
};

} // namespace zhu_screen_pet

