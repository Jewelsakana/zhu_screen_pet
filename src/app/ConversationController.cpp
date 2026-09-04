#include "app/ConversationController.h"

#include "app/ChatController.h"
#include "infrastructure/SettingsRepository.h"
#include "memory/ConversationRepository.h"

#include <algorithm>

namespace zhu_screen_pet {

ConversationController::ConversationController(ConversationRepository* repository,
                                               SettingsRepository* settings,
                                               QObject* parent)
    : QObject(parent), repository_(repository), settings_(settings)
{
    qRegisterMetaType<QVector<Conversation>>("QVector<Conversation>");
    qRegisterMetaType<QVector<ConversationMessage>>("QVector<ConversationMessage>");
    qRegisterMetaType<AppError>("AppError");
}

void ConversationController::setChatController(ChatController* controller)
{
    chatController_ = controller;
}

AppError ConversationController::makeError(AppErrorCode code, const QString& message,
                                            const QString& technical,
                                            const QString& operation) const
{
    AppError error;
    error.code = code;
    error.message = message;
    error.domain = ErrorDomain::Application;
    error.technicalMessage = technical;
    error.operation = operation;
    return error;
}

bool ConversationController::fail(const AppError& error, AppError* output)
{
    if (output) *output = error;
    emit operationFailed(error);
    return false;
}

bool ConversationController::initialize(AppError* error)
{
    if (repository_ == nullptr) {
        return fail(makeError(AppErrorCode::NotReady, QStringLiteral("会话服务暂不可用"),
                              QStringLiteral("conversation repository is unavailable"),
                              QStringLiteral("conversation.initialize")), error);
    }

    QString wanted;
    if (settings_ != nullptr) {
        wanted = settings_->value(QStringLiteral("chat/current_conversation_id")).toString().trimmed();
    }
    if (!wanted.isEmpty()) {
        const auto current = repository_->getConversationResult(wanted);
        if (current && !current.value().isArchived()) {
            if (!switchConversation(wanted, error)) return false;
            return true;
        }
        if (!current && current.error().code != AppErrorCode::NotFound) {
            return fail(current.error(), error);
        }
    }

    const auto active = repository_->listConversationsResult(false);
    if (!active) return fail(active.error(), error);
    conversations_ = active.value();
    if (conversations_.isEmpty()) {
        const auto created = repository_->createConversationResult(QStringLiteral("默认会话"));
        if (!created) return fail(created.error(), error);
        return switchConversation(created.value(), error);
    }
    return switchConversation(conversations_.first().id, error);
}

bool ConversationController::refresh(AppError* error)
{
    if (repository_ == nullptr) {
        return fail(makeError(AppErrorCode::NotReady, QStringLiteral("会话服务暂不可用"),
                              QStringLiteral("conversation repository is unavailable"),
                              QStringLiteral("conversation.refresh")), error);
    }
    const auto result = repository_->listConversationsResult(true);
    if (!result) return fail(result.error(), error);
    conversations_.clear();
    archivedConversations_.clear();
    for (const Conversation& conversation : result.value()) {
        if (conversation.isArchived()) archivedConversations_.append(conversation);
        else conversations_.append(conversation);
    }
    emit conversationsChanged(conversations_);
    return true;
}

bool ConversationController::createConversation(const QString& title, AppError* error)
{
    if (!ensureChatIdle(QStringLiteral("conversation.create"), error)) return false;
    if (repository_ == nullptr) {
        return fail(makeError(AppErrorCode::NotReady, QStringLiteral("会话服务暂不可用"),
                              QStringLiteral("conversation repository is unavailable"),
                              QStringLiteral("conversation.create")), error);
    }
    const auto created = repository_->createConversationResult(title.trimmed().isEmpty()
        ? QStringLiteral("新会话") : title.trimmed());
    if (!created) return fail(created.error(), error);
    if (switchConversation(created.value(), error)) return true;
    repository_->deleteConversationResult(created.value());
    return false;
}

bool ConversationController::switchConversation(const QString& conversationId, AppError* error)
{
    if (!ensureChatIdle(QStringLiteral("conversation.switch"), error)) return false;
    if (repository_ == nullptr || conversationId.trimmed().isEmpty()) {
        return fail(makeError(AppErrorCode::InvalidArgument, QStringLiteral("会话 ID 不能为空"),
                              QStringLiteral("conversation id is empty"),
                              QStringLiteral("conversation.switch")), error);
    }
    const QString id = conversationId.trimmed();
    const auto conversation = repository_->getConversationResult(id);
    if (!conversation) return fail(conversation.error(), error);
    if (conversation.value().isArchived()) {
        return fail(makeError(AppErrorCode::InvalidArgument, QStringLiteral("已归档的会话不能切换"),
                              QStringLiteral("conversation is archived: %1").arg(id),
                              QStringLiteral("conversation.switch")), error);
    }
    const auto messages = repository_->recentMessagesResult(id, 200);
    if (!messages) return fail(messages.error(), error);
    const auto all = repository_->listConversationsResult(true);
    if (!all) return fail(all.error(), error);
    if (!persistCurrentId(id, error)) return false;

    commitCurrentConversation(conversation.value(), messages.value(), all.value());
    return true;
}

bool ConversationController::archiveConversation(const QString& conversationId, AppError* error)
{
    if (!ensureChatIdle(QStringLiteral("conversation.archive"), error)) return false;
    if (repository_ == nullptr || conversationId.trimmed().isEmpty()) {
        return fail(makeError(AppErrorCode::InvalidArgument, QStringLiteral("会话 ID 不能为空"),
                              QStringLiteral("conversation id is empty"),
                              QStringLiteral("conversation.archive")), error);
    }
    const QString id = conversationId.trimmed();
    const auto allResult = repository_->listConversationsResult(true);
    if (!allResult) return fail(allResult.error(), error);
    QVector<Conversation> all = allResult.value();
    if (id != currentConversationId_) {
        const auto archived = repository_->archiveConversationResult(id);
        if (!archived) return fail(archived.error(), error);
        for (Conversation& item : all) {
            if (item.id == id) item.archivedAt = QDateTime::currentDateTimeUtc();
        }
        applyConversationSnapshot(all);
        return true;
    }

    Conversation successor;
    bool createdSuccessor = false;
    for (const Conversation& item : all) {
        if (!item.isArchived() && item.id != id) { successor = item; break; }
    }
    if (successor.id.isEmpty()) {
        const auto created = repository_->createConversationResult(QStringLiteral("新会话"));
        if (!created) return fail(created.error(), error);
        createdSuccessor = true;
        const auto loaded = repository_->getConversationResult(created.value());
        if (!loaded) {
            repository_->deleteConversationResult(created.value());
            return fail(loaded.error(), error);
        }
        successor = loaded.value();
        all.append(successor);
    }
    const auto messages = repository_->recentMessagesResult(successor.id, 200);
    if (!messages) {
        if (createdSuccessor) repository_->deleteConversationResult(successor.id);
        return fail(messages.error(), error);
    }
    if (!persistCurrentId(successor.id, error)) {
        if (createdSuccessor) repository_->deleteConversationResult(successor.id);
        return false;
    }
    const auto archived = repository_->archiveConversationResult(id);
    if (!archived) {
        persistCurrentId(id, nullptr);
        if (createdSuccessor) repository_->deleteConversationResult(successor.id);
        return fail(archived.error(), error);
    }
    for (Conversation& item : all) {
        if (item.id == id) item.archivedAt = QDateTime::currentDateTimeUtc();
    }
    commitCurrentConversation(successor, messages.value(), all);
    return true;
}

bool ConversationController::archiveCurrentConversation(AppError* error)
{
    return archiveConversation(currentConversationId_, error);
}

bool ConversationController::deleteConversation(const QString& conversationId, AppError* error)
{
    if (!ensureChatIdle(QStringLiteral("conversation.delete"), error)) return false;
    if (repository_ == nullptr || conversationId.trimmed().isEmpty()) {
        return fail(makeError(AppErrorCode::InvalidArgument, QStringLiteral("会话 ID 不能为空"),
                              QStringLiteral("conversation id is empty"),
                              QStringLiteral("conversation.delete")), error);
    }
    const QString id = conversationId.trimmed();
    const auto allResult = repository_->listConversationsResult(true);
    if (!allResult) return fail(allResult.error(), error);
    QVector<Conversation> all = allResult.value();
    if (id != currentConversationId_) {
        const auto removed = repository_->deleteConversationResult(id);
        if (!removed) return fail(removed.error(), error);
        all.erase(std::remove_if(all.begin(), all.end(), [&id](const Conversation& item) {
            return item.id == id;
        }), all.end());
        applyConversationSnapshot(all);
        return true;
    }

    Conversation successor;
    bool createdSuccessor = false;
    for (const Conversation& item : all) {
        if (!item.isArchived() && item.id != id) { successor = item; break; }
    }
    if (successor.id.isEmpty()) {
        const auto created = repository_->createConversationResult(QStringLiteral("新会话"));
        if (!created) return fail(created.error(), error);
        createdSuccessor = true;
        const auto loaded = repository_->getConversationResult(created.value());
        if (!loaded) {
            repository_->deleteConversationResult(created.value());
            return fail(loaded.error(), error);
        }
        successor = loaded.value();
        all.append(successor);
    }
    const auto messages = repository_->recentMessagesResult(successor.id, 200);
    if (!messages) {
        if (createdSuccessor) repository_->deleteConversationResult(successor.id);
        return fail(messages.error(), error);
    }
    if (!persistCurrentId(successor.id, error)) {
        if (createdSuccessor) repository_->deleteConversationResult(successor.id);
        return false;
    }
    const auto removed = repository_->deleteConversationResult(id);
    if (!removed) {
        persistCurrentId(id, nullptr);
        if (createdSuccessor) repository_->deleteConversationResult(successor.id);
        return fail(removed.error(), error);
    }
    all.erase(std::remove_if(all.begin(), all.end(), [&id](const Conversation& item) {
        return item.id == id;
    }), all.end());
    commitCurrentConversation(successor, messages.value(), all);
    return true;
}

bool ConversationController::deleteCurrentConversation(AppError* error)
{
    return deleteConversation(currentConversationId_, error);
}

QString ConversationController::currentConversationId() const { return currentConversationId_; }
QString ConversationController::currentConversationTitle() const { return currentConversationTitle_; }
QVector<ConversationMessage> ConversationController::currentConversationMessages() const
{ return currentConversationMessages_; }
QVector<Conversation> ConversationController::archivedConversations() const
{ return archivedConversations_; }
QVector<Conversation> ConversationController::conversations() const { return conversations_; }

bool ConversationController::persistCurrentId(const QString& conversationId, AppError* error)
{
    if (settings_ == nullptr) return true;
    const QString key = QStringLiteral("chat/current_conversation_id");
    const QVariant previous = settings_->value(key);
    settings_->setValue(key, conversationId);
    QString technical;
    if (settings_->save(&technical)) return true;
    settings_->setValue(key, previous);
    settings_->save(nullptr);
    return fail(makeError(AppErrorCode::Io, QStringLiteral("无法保存当前会话"), technical,
                          QStringLiteral("conversation.persist_current")), error);
}

void ConversationController::applyConversationSnapshot(const QVector<Conversation>& all)
{
    conversations_.clear();
    archivedConversations_.clear();
    for (const Conversation& item : all) {
        if (item.isArchived()) archivedConversations_.append(item);
        else conversations_.append(item);
    }
    emit conversationsChanged(conversations_);
}

void ConversationController::commitCurrentConversation(
    const Conversation& conversation, const QVector<ConversationMessage>& messages,
    const QVector<Conversation>& all)
{
    currentConversationId_ = conversation.id;
    currentConversationTitle_ = conversation.title;
    currentConversationMessages_ = messages;
    applyConversationSnapshot(all);
    emit currentConversationChanged(currentConversationId_, currentConversationTitle_, messages);
}

bool ConversationController::ensureChatIdle(const QString& operation, AppError* error)
{
    if (chatController_ == nullptr || chatController_->pendingRequestCount() == 0) return true;
    return fail(makeError(AppErrorCode::Busy, QStringLiteral("回复生成期间不能变更会话"),
                          QStringLiteral("a chat request is still running"), operation), error);
}

} // namespace zhu_screen_pet
