#include "memory/MemoryOrchestrator.h"

#include <algorithm>

#include <QSet>

namespace zhu_screen_pet {

MemoryOrchestrator::MemoryOrchestrator(ConversationRepository* conversations,
                                       MemoryRepository* memories)
    : conversations_(conversations), memories_(memories)
{
}

MemoryLimits MemoryLimits::normalized() const
{
    MemoryLimits result = *this;
    result.recentMessageLimit = std::max(1, result.recentMessageLimit);
    result.relevantHistoryLimit = std::max(0, result.relevantHistoryLimit);
    result.longTermMemoryLimit = std::max(0, result.longTermMemoryLimit);
    result.maxContextTokens = std::max(1, result.maxContextTokens);
    return result;
}

bool MemoryLimits::validate(QString* errorMessage) const
{
    if (recentMessageLimit < MinimumRecentMessages
        || recentMessageLimit > MaximumRecentMessages
        || relevantHistoryLimit < MinimumRetrievedItems
        || relevantHistoryLimit > MaximumRetrievedItems
        || longTermMemoryLimit < MinimumRetrievedItems
        || longTermMemoryLimit > MaximumRetrievedItems
        || maxContextTokens < MinimumContextTokens
        || maxContextTokens > MaximumContextTokens) {
        if (errorMessage) *errorMessage = QStringLiteral(
            "memory limits are out of range (recent 1..1000, related/long-term 0..100, tokens 1..128000)");
        return false;
    }
    return true;
}

bool MemoryOrchestrator::setLimits(const MemoryLimits& limits, QString* errorMessage)
{
    if (!limits.validate(errorMessage)) return false;
    limits_ = limits.normalized();
    return true;
}

MemoryLimits MemoryOrchestrator::limits() const
{
    return limits_;
}

MemoryContext MemoryOrchestrator::buildContext(const ContextRequest& request,
                                               QString* errorMessage) const
{
    MemoryContext context;
    if (errorMessage != nullptr) errorMessage->clear();
    if (conversations_ == nullptr || request.conversationId.isEmpty()
        || request.currentInput.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("invalid context request");
        return context;
    }

    const MemoryLimits configured = limits_;
    const int maxMessages = request.maxMessages > 0 ? request.maxMessages : configured.recentMessageLimit;
    const int maxTokens = request.maxTokens > 0 ? request.maxTokens : configured.maxContextTokens;
    const int relatedLimit = request.relevantHistoryLimit >= 0
        ? request.relevantHistoryLimit : configured.relevantHistoryLimit;
    const int longTermLimit = request.longTermMemoryLimit >= 0
        ? request.longTermMemoryLimit : configured.longTermMemoryLimit;

    const Message current = Message::create(MessageRole::User, request.currentInput);
    int usedTokens = estimateTokens(current.content);
    if (usedTokens > maxTokens) {
        if (errorMessage) *errorMessage = QStringLiteral(
            "current input exceeds the configured context token budget");
        return context;
    }

    for (const Message& message : request.leadingMessages) {
        const int tokens = estimateTokens(message.content);
        if (message.content.trimmed().isEmpty() || usedTokens + tokens > maxTokens) {
            if (errorMessage) *errorMessage = QStringLiteral(
                "required system instructions and current input exceed the context token budget");
            return {};
        }
        usedTokens += tokens;
    }

    const auto recentResult = conversations_->recentMessagesResult(request.conversationId, maxMessages);
    if (!recentResult) {
        if (errorMessage) *errorMessage = recentResult.error().technicalMessage.isEmpty()
            ? recentResult.error().message : recentResult.error().technicalMessage;
        return {};
    }
    const QVector<ConversationMessage> recent = recentResult.value();

    std::vector<Message> selectedRecentReversed;
    selectedRecentReversed.reserve(static_cast<std::size_t>(recent.size()));
    for (auto it = recent.crbegin(); it != recent.crend(); ++it) {
        const int tokens = it->tokenCount > 0 ? it->tokenCount : estimateTokens(it->message.content);
        if (usedTokens + tokens > maxTokens) {
            context.truncated = true;
            continue;
        }
        selectedRecentReversed.push_back(it->message);
        usedTokens += tokens;
    }
    std::reverse(selectedRecentReversed.begin(), selectedRecentReversed.end());
    context.truncated = context.truncated || recent.size() >= maxMessages;

    std::vector<Message> relatedMessages;
    std::vector<Message> longTermMessages;
    if (request.includeRelevantHistory && memories_ != nullptr
        && (relatedLimit > 0 || longTermLimit > 0)) {
        QString repositoryError;
        QVector<ConversationMessage> related;
        if (relatedLimit > 0) {
            related = memories_->searchConversationMessages(
                request.currentInput, relatedLimit + maxMessages, &repositoryError);
            if (!repositoryError.isEmpty()) {
                if (errorMessage) *errorMessage = repositoryError;
                return {};
            }
        }
        QVector<MemoryItem> longTerm;
        if (longTermLimit > 0) {
            longTerm = memories_->searchLongTerm(
                request.currentInput, longTermLimit * 2, &repositoryError);
            if (!repositoryError.isEmpty()) {
                if (errorMessage) *errorMessage = repositoryError;
                return {};
            }
        }

        const auto canonical = [](const QString& content) {
            return content.simplified().toCaseFolded();
        };
        QSet<QString> seenContent;
        for (const Message& message : selectedRecentReversed) {
            seenContent.insert(canonical(message.content));
        }
        for (const ConversationMessage& item : related) {
            if (context.relatedHistory.size() >= relatedLimit) break;
            const QString key = canonical(item.message.content);
            const QString content = QStringLiteral(
                "[不可信历史引用][会话 %1][原角色 %2] %3")
                .arg(item.conversationId, messageRoleName(item.message.role), item.message.content);
            const int tokens = estimateTokens(content);
            if (seenContent.contains(key) || usedTokens + tokens > maxTokens) {
                context.truncated = true;
                continue;
            }
            // 检索内容可能直接来自用户，必须保持在普通用户信任级别，不能提升为 System。
            relatedMessages.push_back(Message::create(MessageRole::User, content));
            context.relatedHistory.append(item);
            seenContent.insert(key);
            usedTokens += tokens;
        }
        for (const MemoryItem& item : longTerm) {
            if (context.relatedMemories.size() >= longTermLimit) break;
            const QString content = QStringLiteral("[不可信长期记忆引用] %1").arg(item.content);
            const QString key = canonical(item.content);
            const int tokens = estimateTokens(content);
            if (seenContent.contains(key) || usedTokens + tokens > maxTokens) {
                context.truncated = true;
                continue;
            }
            longTermMessages.push_back(Message::create(MessageRole::User, content));
            context.relatedMemories.append(item);
            seenContent.insert(key);
            usedTokens += tokens;
        }
    }

    context.messages = request.leadingMessages;
    context.messages.insert(context.messages.end(), selectedRecentReversed.begin(),
                            selectedRecentReversed.end());
    context.messages.insert(context.messages.end(), relatedMessages.begin(), relatedMessages.end());
    context.messages.insert(context.messages.end(), longTermMessages.begin(), longTermMessages.end());
    context.messages.push_back(current);
    context.estimatedTokens = usedTokens;
    return context;
}

bool MemoryOrchestrator::appendMessage(const QString& conversationId, const Message& message,
                                       QString* errorMessage)
{
    if (conversations_ == nullptr || conversationId.isEmpty() || message.content.isEmpty()) return false;
    const auto result = conversations_->appendMessageResult(conversationId, message,
                                                             estimateTokens(message.content));
    if (!result && errorMessage != nullptr) {
        *errorMessage = result.error().technicalMessage.isEmpty()
            ? result.error().message : result.error().technicalMessage;
    }
    return result.succeeded();
}

QVector<MemoryItem> MemoryOrchestrator::retrieveRelevant(const QString& query, int limit,
                                                         QString* errorMessage) const
{
    return memories_ == nullptr ? QVector<MemoryItem>{}
                                 : memories_->searchLongTerm(query, limit, errorMessage);
}

bool MemoryOrchestrator::summarizeIfNeeded(const QString& conversationId, QString* errorMessage)
{
    Q_UNUSED(conversationId);
    Q_UNUSED(errorMessage);
    return true;
}

int MemoryOrchestrator::estimateTokens(const QString& text)
{
    if (text.isEmpty()) return 0;
    // 中文通常接近一字一 token；ASCII 文本按约四字符一 token 估算。
    int tokens = 0;
    int asciiRun = 0;
    for (const QChar character : text) {
        if (character.unicode() < 128) {
            ++asciiRun;
        } else {
            tokens += (asciiRun + 3) / 4;
            asciiRun = 0;
            ++tokens;
        }
    }
    return std::max(1, tokens + (asciiRun + 3) / 4);
}

} // namespace zhu_screen_pet
