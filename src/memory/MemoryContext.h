#pragma once

#include <vector>

#include <QString>
#include <QVector>

#include "memory/ConversationTypes.h"

namespace zhu_screen_pet {

/** 记忆上下文的条数和 token 配额；由 app-settings.json 配置。 */
struct MemoryLimits
{
    static constexpr int MinimumRecentMessages = 1;
    static constexpr int MaximumRecentMessages = 1000;
    static constexpr int MinimumRetrievedItems = 0;
    static constexpr int MaximumRetrievedItems = 100;
    static constexpr int MinimumContextTokens = 1;
    static constexpr int MaximumContextTokens = 128000;

    int recentMessageLimit = 20;
    int relevantHistoryLimit = 5;
    int longTermMemoryLimit = 5;
    int maxContextTokens = 8000;

    MemoryLimits normalized() const;
    bool validate(QString* errorMessage = nullptr) const;
};

/** 构建一次模型上下文所需的参数。 */
struct ContextRequest
{
    QString conversationId;
    QString currentInput;
    int maxMessages = 0;
    int maxTokens = 0;
    bool includeRelevantHistory = true;
    int relevantHistoryLimit = -1;
    int longTermMemoryLimit = -1;
    /** 必须保留并计入总 token 预算的 system 消息，例如 Persona。 */
    std::vector<Message> leadingMessages;
};

/** 已按条数和 token 预算裁剪后的模型上下文。 */
struct MemoryContext
{
    std::vector<Message> messages;
    QVector<ConversationMessage> relatedHistory;
    QVector<MemoryItem> relatedMemories;
    int estimatedTokens = 0;
    bool truncated = false;
};

} // namespace zhu_screen_pet
