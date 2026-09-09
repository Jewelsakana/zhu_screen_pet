#pragma once

#include "memory/ConversationRepository.h"
#include "memory/MemoryContext.h"
#include "memory/MemoryRepository.h"
#include "memory/ObservationRepository.h"

namespace zhu_screen_pet {

/** 统一读取和裁剪记忆，防止 UI 或模型适配器自行拼接无限历史。 */
class MemoryOrchestrator final
{
public:
    explicit MemoryOrchestrator(ConversationRepository* conversations,
                                MemoryRepository* memories = nullptr,
                                ObservationRepository* observations = nullptr);

    MemoryContext buildContext(const ContextRequest& request,
                               QString* errorMessage = nullptr) const;
    /** 设置默认上下文预算；单次 ContextRequest 的正数值可以覆盖默认值。 */
    bool setLimits(const MemoryLimits& limits, QString* errorMessage = nullptr);
    MemoryLimits limits() const;
    bool appendMessage(const QString& conversationId, const Message& message,
                       QString* errorMessage = nullptr);
    bool appendObservation(const ObservationEvent& observation,
                           QString* errorMessage = nullptr);
    QVector<MemoryItem> retrieveRelevant(const QString& query, int limit,
                                         QString* errorMessage = nullptr) const;
    /** 判断指定会话是否达到后台摘要阈值；实际模型任务由 MemoryMaintenanceService 执行。 */
    bool summarizeIfNeeded(const QString& conversationId, QString* errorMessage = nullptr);
    QVector<ConversationMessage> unsummarizedMessages(const QString& conversationId, int limit,
                                                      QString* errorMessage = nullptr) const;
    bool markMessagesSummarized(const QVector<qint64>& messageIds,
                                const QDateTime& summarizedAt = {},
                                QString* errorMessage = nullptr);
    bool saveExtractedFact(const QString& content, const QString& category,
                           double confidence, double importance,
                           const QString& sourceReference, QString* errorMessage = nullptr);
    bool clearMemoryKind(const QString& kind, QString* errorMessage = nullptr);
    /** 清理过期短期记忆并限制总条数；返回删除数量，失败返回 -1。 */
    int cleanupShortTermMemories(QString* errorMessage = nullptr);
    QVector<MemoryItem> listMemories(const QString& kind, const QString& query,
                                     int limit, QString* errorMessage = nullptr) const;
    MemoryItem getMemory(qint64 id, QString* errorMessage = nullptr) const;
    bool updateMemory(const MemoryItem& item, QString* errorMessage = nullptr);
    bool removeMemory(qint64 id, QString* errorMessage = nullptr);
    std::optional<MemoryItem> conversationSummary(const QString& conversationId,
                                                  QString* errorMessage = nullptr) const;

    static int estimateTokens(const QString& text);
    /** 按常见 OpenAI 兼容视觉输入的 512px 分块规则估算图片 token。 */
    static int estimateImageTokens(const QSize& size,
                                   const QString& detail = QStringLiteral("original"));

private:
    ConversationRepository* conversations_ = nullptr;
    MemoryRepository* memories_ = nullptr;
    ObservationRepository* observations_ = nullptr;
    MemoryLimits limits_;
};

} // namespace zhu_screen_pet
