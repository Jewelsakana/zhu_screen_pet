#include "memory/MemoryOrchestrator.h"

#include <algorithm>

#include <QSet>
#include <QtMath>

namespace zhu_screen_pet {

MemoryOrchestrator::MemoryOrchestrator(ConversationRepository* conversations,
                                       MemoryRepository* memories,
                                       ObservationRepository* observations)
    : conversations_(conversations), memories_(memories), observations_(observations)
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
    int usedTokens = estimateTokens(current.content) + qMax(0, request.reservedInputTokens);
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

    Message latestObservationMessage;
    bool hasLatestObservation = false;
    if (request.includeLatestObservation && observations_ != nullptr) {
        const auto observationResult = observations_->latestValidResult(
            request.conversationId, QDateTime::currentDateTimeUtc());
        if (!observationResult) {
            if (errorMessage) {
                *errorMessage = observationResult.error().technicalMessage.isEmpty()
                    ? observationResult.error().message
                    : observationResult.error().technicalMessage;
            }
            return {};
        }
        if (observationResult.value().has_value()) {
            const ObservationEvent& observation = *observationResult.value();
            const int observationLimit = qMax(1, request.latestObservationMaxTokens);
            const QString prefix = QStringLiteral("[不可信屏幕观察][%1] ")
                .arg(observation.capturedAt.toLocalTime().toString(Qt::ISODate));
            const QString summary = observation.summary.trimmed();
            int low = 0;
            int high = summary.size();
            while (low < high) {
                const int middle = (low + high + 1) / 2;
                if (estimateTokens(prefix + summary.left(middle)) <= observationLimit) low = middle;
                else high = middle - 1;
            }
            const QString content = prefix + summary.left(low);
            const int tokens = estimateTokens(content);
            if (low > 0 && usedTokens + tokens <= maxTokens) {
                latestObservationMessage = Message::create(MessageRole::User, content);
                hasLatestObservation = true;
                usedTokens += tokens;
            } else {
                context.truncated = true;
            }
        }
    }

    const auto recentResult = conversations_->recentMessagesResult(request.conversationId, maxMessages);
    if (!recentResult) {
        if (errorMessage) *errorMessage = recentResult.error().technicalMessage.isEmpty()
            ? recentResult.error().message : recentResult.error().technicalMessage;
        return {};
    }
    const QVector<ConversationMessage> recent = recentResult.value();

    std::vector<Message> selectedRecent;
    selectedRecent.reserve(static_cast<std::size_t>(recent.size()));
    for (int end = recent.size() - 1; end >= 0;) {
        int start = end;
        if (recent.at(end).message.role == MessageRole::Assistant) {
            if (end <= 0 || recent.at(end - 1).message.role != MessageRole::User) {
                context.truncated = true;
                break;
            }
            start = end - 1;
        }
        int turnTokens = 0;
        for (int index = start; index <= end; ++index) {
            const ConversationMessage& item = recent.at(index);
            turnTokens += item.tokenCount > 0
                ? item.tokenCount : estimateTokens(item.message.content);
        }
        if (usedTokens + turnTokens > maxTokens) {
            // 不跨过较新的完整轮次继续挑选旧消息，避免上下文出现孤立回复或时间断层。
            context.truncated = true;
            break;
        }
        for (int index = end; index >= start; --index) {
            selectedRecent.insert(selectedRecent.begin(), recent.at(index).message);
        }
        usedTokens += turnTokens;
        end = start - 1;
    }
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
        for (const Message& message : selectedRecent) {
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
    context.messages.insert(context.messages.end(), selectedRecent.begin(),
                            selectedRecent.end());
    context.messages.insert(context.messages.end(), relatedMessages.begin(), relatedMessages.end());
    context.messages.insert(context.messages.end(), longTermMessages.begin(), longTermMessages.end());
    if (hasLatestObservation) context.messages.push_back(latestObservationMessage);
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

bool MemoryOrchestrator::appendObservation(const ObservationEvent& observation,
                                           QString* errorMessage)
{
    if (observations_ == nullptr) {
        if (errorMessage) *errorMessage = QStringLiteral("observation repository is not available");
        return false;
    }
    const auto cleanup = observations_->removeExpiredResult(QDateTime::currentDateTimeUtc());
    if (!cleanup) {
        if (errorMessage) {
            *errorMessage = cleanup.error().technicalMessage.isEmpty()
                ? cleanup.error().message : cleanup.error().technicalMessage;
        }
        return false;
    }
    const auto result = observations_->saveResult(observation);
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

int MemoryOrchestrator::estimateImageTokens(const QSize& sourceSize, const QString& detail)
{
    if (!sourceSize.isValid() || sourceSize.isEmpty()) return 0;
    if (detail.compare(QStringLiteral("low"), Qt::CaseInsensitive) == 0) return 85;

    QSize size = sourceSize;
    if (size.width() > 2048 || size.height() > 2048) {
        size.scale(2048, 2048, Qt::KeepAspectRatio);
    }
    if (qMin(size.width(), size.height()) > 768) {
        const qreal factor = 768.0 / qMin(size.width(), size.height());
        size = QSize(qMax(1, qRound(size.width() * factor)),
                     qMax(1, qRound(size.height() * factor)));
    }
    const int tiles = qCeil(size.width() / 512.0) * qCeil(size.height() / 512.0);
    return 85 + 170 * qMax(1, tiles);
}

} // namespace zhu_screen_pet
