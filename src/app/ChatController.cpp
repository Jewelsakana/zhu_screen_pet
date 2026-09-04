#include "app/ChatController.h"

namespace zhu_screen_pet {

ChatController::ChatController(ChatProvider* provider, MemoryOrchestrator* memory, QObject* parent)
    : QObject(parent), provider_(provider), memory_(memory)
{
    qRegisterMetaType<ChatResult>("ChatResult");
    qRegisterMetaType<ModelError>("ModelError");
    qRegisterMetaType<PetState>("PetState");
    if (provider_ == nullptr) return;
    connect(provider_, &ChatProvider::chatStarted,
            this, &ChatController::onChatStarted, Qt::QueuedConnection);
    connect(provider_, &ChatProvider::chatDelta,
            this, &ChatController::onChatDelta, Qt::QueuedConnection);
    connect(provider_, &ChatProvider::chatFinished,
            this, &ChatController::onChatFinished, Qt::QueuedConnection);
}

QString ChatController::sendMessage(const QString& conversationId, const QString& text,
                                    const ChatOptions& options)
{
    lastError_ = AppError{};
    if (provider_ == nullptr || memory_ == nullptr) {
        fail({AppErrorCode::NotReady, QStringLiteral("聊天服务暂不可用"), 0,
              ErrorDomain::Application, QStringLiteral("chat controller dependencies are not available"),
              QStringLiteral("chat.send"), {}, false});
        return {};
    }
    if (!personaConfigured_) {
        fail({AppErrorCode::NotReady, QStringLiteral("人格配置尚未加载"), 0,
              ErrorDomain::Configuration, QStringLiteral("persona configuration has not been loaded"),
              QStringLiteral("chat.send"), {}, false});
        return {};
    }
    if (!pending_.isEmpty()) {
        fail({AppErrorCode::Busy, QStringLiteral("已有聊天请求正在处理中"), 0,
              ErrorDomain::Application, QStringLiteral("another chat request is still running"),
              QStringLiteral("chat.send"), {}, true});
        return {};
    }
    if (conversationId.trimmed().isEmpty() || text.trimmed().isEmpty()) {
        fail({AppErrorCode::InvalidArgument, QStringLiteral("会话和消息内容不能为空"), 0,
              ErrorDomain::Application, QStringLiteral("conversation id and message text must not be empty"),
              QStringLiteral("chat.send"), {}, false});
        return {};
    }

    ContextRequest contextRequest;
    contextRequest.conversationId = conversationId;
    contextRequest.currentInput = text;
    contextRequest.leadingMessages.push_back(
        Message::create(MessageRole::System, persona_.systemInstruction()));
    QString errorMessage;
    const MemoryContext context = memory_->buildContext(contextRequest, &errorMessage);
    if (!errorMessage.isEmpty() || context.messages.empty()) {
        fail({AppErrorCode::DatabaseQuery, QStringLiteral("无法构建聊天上下文"), 0,
              ErrorDomain::Memory, errorMessage, QStringLiteral("chat.build_context"), {}, false});
        return {};
    }
    if (!memory_->appendMessage(conversationId,
                                Message::create(MessageRole::User, text), &errorMessage)) {
        fail({AppErrorCode::DatabaseQuery, QStringLiteral("无法保存用户消息"), 0,
              ErrorDomain::Database, errorMessage, QStringLiteral("chat.save_user_message"), {}, false});
        return {};
    }

    PendingChat pending;
    pending.conversationId = conversationId;
    pending.userText = text;
    pending.context = context.messages;
    pending.options = options;
    // Persona 的回复长度是应用级策略，统一覆盖调用方的临时 maxTokens。
    pending.options.maxTokens = persona_.maxReplyTokens;
    return startPending(std::move(pending));
}

QString ChatController::retryLast()
{
    lastError_ = AppError{};
    if (!pending_.isEmpty()) {
        fail({AppErrorCode::Busy, QStringLiteral("已有聊天请求正在处理中"), 0,
              ErrorDomain::Application, QStringLiteral("another chat request is still running"),
              QStringLiteral("chat.retry"), {}, true});
        return {};
    }
    if (!hasLastFailed_) {
        fail({AppErrorCode::NotReady, QStringLiteral("当前没有可以重试的请求"), 0,
              ErrorDomain::Application, QStringLiteral("there is no failed request to retry"),
              QStringLiteral("chat.retry"), {}, false});
        return {};
    }
    PendingChat pending = lastFailed_;
    pending.accumulatedReply.clear();
    return startPending(std::move(pending));
}

void ChatController::cancel(const QString& requestId)
{
    if (provider_ != nullptr && pending_.contains(requestId)) provider_->cancel(requestId);
}

void ChatController::cancelAll()
{
    if (provider_ == nullptr) return;
    const QStringList requestIds = pending_.keys();
    for (const QString& requestId : requestIds) provider_->cancel(requestId);
}

int ChatController::pendingRequestCount() const
{
    return pending_.size();
}

AppError ChatController::lastAppError() const
{
    return lastError_;
}

PetState ChatController::state() const
{
    return state_;
}

PersonaConfig ChatController::personaConfig() const
{
    return persona_;
}

bool ChatController::setPersonaConfig(const PersonaConfig& config, QString* errorMessage)
{
    if (!config.validate(errorMessage)) return false;
    persona_ = config.normalized();
    personaConfigured_ = true;
    return true;
}

void ChatController::onChatStarted(const QString& requestId)
{
    if (!pending_.contains(requestId)) return;
    setState(PetState::Thinking);
    emit requestStarted(requestId);
}

void ChatController::onChatDelta(const QString& requestId, const QString& delta)
{
    auto it = pending_.find(requestId);
    if (it == pending_.end()) return;
    it->accumulatedReply += delta;
    setState(PetState::Speaking);
    emit replyDelta(requestId, delta);
}

void ChatController::onChatFinished(const QString& requestId, const ChatResult& result)
{
    auto it = pending_.find(requestId);
    if (it == pending_.end()) return;
    const PendingChat pending = it.value();
    pending_.erase(it);

    if (!result.succeeded) {
        lastFailed_ = pending;
        hasLastFailed_ = result.error.code != ModelErrorCode::Cancelled;
        setState(result.error.code == ModelErrorCode::Cancelled
                     ? PetState::Idle : PetState::Error);
        emit requestFailed(requestId, result.error);
        return;
    }

    const QString content = result.content.isEmpty() ? pending.accumulatedReply : result.content;
    QString errorMessage;
    if (content.isEmpty() || !memory_->appendMessage(
            pending.conversationId, Message::create(MessageRole::Assistant, content), &errorMessage)) {
        ModelError error;
        error.code = content.isEmpty() ? ModelErrorCode::InvalidResponse : ModelErrorCode::Unknown;
        error.message = content.isEmpty() ? QStringLiteral("模型返回了空回复") : QStringLiteral("无法保存助手回复");
        error.domain = content.isEmpty() ? ErrorDomain::Model : ErrorDomain::Database;
        error.technicalMessage = errorMessage.isEmpty() ? QStringLiteral("model returned an empty reply")
            : QStringLiteral("failed to save assistant reply: %1").arg(errorMessage);
        error.operation = QStringLiteral("chat.save_assistant_reply");
        lastFailed_ = pending;
        hasLastFailed_ = true;
        setState(PetState::Error);
        emit requestFailed(requestId, error);
        return;
    }
    hasLastFailed_ = false;
    setState(PetState::Idle);
    emit replyFinished(requestId, content);
}

QString ChatController::startPending(PendingChat pending)
{
    if (provider_ == nullptr || pending.context.empty()) {
        fail({AppErrorCode::NotReady, QStringLiteral("聊天请求尚未准备好"), 0,
              ErrorDomain::Application, QStringLiteral("chat request is not ready"),
              QStringLiteral("chat.start"), {}, false});
        setState(PetState::Error);
        return {};
    }
    setState(PetState::Thinking);
    const QString requestId = provider_->startChat(pending.context, pending.options);
    if (requestId.isEmpty()) {
        fail({AppErrorCode::Unknown, QStringLiteral("模型请求未能启动"), 0,
              ErrorDomain::Model, QStringLiteral("chat provider did not return a request id"),
              QStringLiteral("chat.start"), {}, true});
        lastFailed_ = std::move(pending);
        hasLastFailed_ = true;
        setState(PetState::Error);
        return {};
    }
    pending_.insert(requestId, std::move(pending));
    return requestId;
}

void ChatController::fail(const AppError& error)
{
    lastError_ = error;
    setState(PetState::Error);
    emit operationFailed(lastError_);
}

void ChatController::setState(PetState state)
{
    if (state_ == state) return;
    state_ = state;
    emit stateChanged(state_);
}

} // namespace zhu_screen_pet
