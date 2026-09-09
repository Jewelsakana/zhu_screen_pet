#include "memory/ConversationRepository.h"

namespace zhu_screen_pet {

namespace {
void copyError(const AppError& error, QString* output)
{
    if (output != nullptr) {
        *output = error.technicalMessage.isEmpty() ? error.message : error.technicalMessage;
    }
}
}

QString ConversationRepository::createConversation(const QString& title, QString* errorMessage)
{
    const auto result = createConversationResult(title);
    if (!result) copyError(result.error(), errorMessage);
    return result ? result.value() : QString{};
}

bool ConversationRepository::getConversation(const QString& id, Conversation* result,
                                              QString* errorMessage) const
{
    const auto value = getConversationResult(id);
    if (!value) {
        copyError(value.error(), errorMessage);
        return false;
    }
    if (result == nullptr) return false;
    *result = value.value();
    return true;
}

QVector<Conversation> ConversationRepository::listConversations(bool includeArchived,
                                                                QString* errorMessage) const
{
    const auto result = listConversationsResult(includeArchived);
    if (!result) {
        copyError(result.error(), errorMessage);
        return {};
    }
    return result.value();
}

bool ConversationRepository::archiveConversation(const QString& id, QString* errorMessage)
{
    const auto result = archiveConversationResult(id);
    if (!result) copyError(result.error(), errorMessage);
    return result.succeeded();
}

bool ConversationRepository::deleteConversation(const QString& id, QString* errorMessage)
{
    const auto result = deleteConversationResult(id);
    if (!result) copyError(result.error(), errorMessage);
    return result.succeeded();
}

bool ConversationRepository::appendMessage(const QString& conversationId, const Message& message,
                                           int tokenCount, QString* errorMessage)
{
    const auto result = appendMessageResult(conversationId, message, tokenCount);
    if (!result) copyError(result.error(), errorMessage);
    return result.succeeded();
}

QVector<ConversationMessage> ConversationRepository::recentMessages(const QString& conversationId,
                                                                    int limit,
                                                                    QString* errorMessage) const
{
    const auto result = recentMessagesResult(conversationId, limit);
    if (!result) {
        copyError(result.error(), errorMessage);
        return {};
    }
    return result.value();
}

QVector<ConversationMessage> ConversationRepository::unsummarizedMessages(const QString& conversationId,
                                                                            int limit,
                                                                            QString* errorMessage) const
{
    const auto result = unsummarizedMessagesResult(conversationId, limit);
    if (!result) { copyError(result.error(), errorMessage); return {}; }
    return result.value();
}

bool ConversationRepository::markMessagesSummarized(const QVector<qint64>& messageIds,
                                                    const QDateTime& summarizedAt,
                                                    QString* errorMessage)
{
    const auto result = markMessagesSummarizedResult(
        messageIds, summarizedAt.isValid() ? summarizedAt : QDateTime::currentDateTimeUtc());
    if (!result) copyError(result.error(), errorMessage);
    return result.succeeded();
}

int ConversationRepository::removeSummarizedBefore(const QDateTime& cutoff,
                                                    QString* errorMessage)
{
    const auto result = removeSummarizedBeforeResult(cutoff);
    if (!result) copyError(result.error(), errorMessage);
    return result ? result.value() : -1;
}

} // namespace zhu_screen_pet
