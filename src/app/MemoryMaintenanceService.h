#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QVector>

#include "memory/ConversationTypes.h"
#include "model/ChatResult.h"

namespace zhu_screen_pet {

class ChatProvider;
class MemoryOrchestrator;

/** 异步压缩旧会话并提取可长期保存的用户事实。 */
class MemoryMaintenanceService final : public QObject
{
    Q_OBJECT

public:
    MemoryMaintenanceService(ChatProvider* provider, MemoryOrchestrator* memory,
                             QObject* parent = nullptr);
    void schedule(const QString& conversationId);
    bool isRunning(const QString& conversationId) const;

signals:
    void maintenanceCompleted(const QString& conversationId, int messageCount, int factCount);
    void maintenanceFailed(const QString& conversationId, const QString& detail);

private:
    struct PendingBatch {
        QString conversationId;
        QVector<ConversationMessage> messages;
        QString sourcePrefix;
    };

    void startIfNeeded(const QString& conversationId);
    void finish(const QString& requestId, const ChatResult& result);
    void retryLater(const QString& conversationId, const QString& detail);
    static QString buildPrompt(const QVector<ConversationMessage>& messages,
                               const QString& previousSummary,
                               const QVector<MemoryItem>& existingMemories);

    ChatProvider* provider_ = nullptr;
    MemoryOrchestrator* memory_ = nullptr;
    QSet<QString> runningConversations_;
    QSet<QString> retryScheduled_;
    QHash<QString, QDateTime> retryNotBefore_;
    QHash<QString, PendingBatch> pending_;
};

} // namespace zhu_screen_pet
