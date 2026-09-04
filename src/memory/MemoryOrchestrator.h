#pragma once

#include "memory/ConversationRepository.h"
#include "memory/MemoryContext.h"
#include "memory/MemoryRepository.h"

namespace zhu_screen_pet {

/** 统一读取和裁剪记忆，防止 UI 或模型适配器自行拼接无限历史。 */
class MemoryOrchestrator final
{
public:
    explicit MemoryOrchestrator(ConversationRepository* conversations,
                                MemoryRepository* memories = nullptr);

    MemoryContext buildContext(const ContextRequest& request,
                               QString* errorMessage = nullptr) const;
    /** 设置默认上下文预算；单次 ContextRequest 的正数值可以覆盖默认值。 */
    bool setLimits(const MemoryLimits& limits, QString* errorMessage = nullptr);
    MemoryLimits limits() const;
    bool appendMessage(const QString& conversationId, const Message& message,
                       QString* errorMessage = nullptr);
    QVector<MemoryItem> retrieveRelevant(const QString& query, int limit,
                                         QString* errorMessage = nullptr) const;
    /** Phase 6 的长期摘要扩展点；当前版本不修改原始消息。 */
    bool summarizeIfNeeded(const QString& conversationId, QString* errorMessage = nullptr);

    static int estimateTokens(const QString& text);

private:
    ConversationRepository* conversations_ = nullptr;
    MemoryRepository* memories_ = nullptr;
    MemoryLimits limits_;
};

} // namespace zhu_screen_pet
