#include "model/MockChatProvider.h"

#include <QTimer>
#include <QUuid>

#include <utility>

namespace zhu_screen_pet {

MockChatProvider::MockChatProvider(QString response)
    : ChatProvider(nullptr), response_(std::move(response))
{
}

void MockChatProvider::setResponse(QString response)
{
    response_ = std::move(response);
}

void MockChatProvider::setError(ModelError error)
{
    error_ = std::move(error);
}

void MockChatProvider::clearError()
{
    error_ = ModelError{};
}

int MockChatProvider::requestCount() const
{
    return requestCount_;
}

std::vector<Message> MockChatProvider::lastMessages() const
{
    return lastMessages_;
}

ChatOptions MockChatProvider::lastOptions() const
{
    return lastOptions_;
}

QString MockChatProvider::startChat(const std::vector<Message>& messages,
                                    const ChatOptions& options)
{
    Q_UNUSED(options);

    const QString requestId = QUuid::createUuid().toString(QUuid::Id128);
    lastMessages_ = messages;
    lastOptions_ = options;
    const auto token = std::make_shared<CancellationToken>();
    pendingRequests_.insert(requestId, token);
    emit chatStarted(requestId);

    QTimer::singleShot(0, this, [this, requestId, messages, options, token]() {
        if (!pendingRequests_.contains(requestId)) {
            return;
        }

        ChatResult result;
        if (token->isCancellationRequested()) {
            result = ChatResult::failure({ModelErrorCode::Cancelled,
                                          QStringLiteral("request was cancelled"), 0});
        } else if (messages.empty()) {
            result = ChatResult::failure({ModelErrorCode::InvalidRequest,
                                          QStringLiteral("message list must not be empty"), 0});
        } else {
            ++requestCount_;
            result = error_.code != ModelErrorCode::None
                ? ChatResult::failure(error_)
                : ChatResult::success(response_);
            if (result.succeeded && options.stream) {
                emit chatDelta(requestId, response_);
            }
        }

        pendingRequests_.remove(requestId);
        emit chatFinished(requestId, result);
    });

    return requestId;
}

void MockChatProvider::cancel(const QString& requestId)
{
    const auto token = pendingRequests_.value(requestId);
    if (token != nullptr) {
        token->cancel();
    }
}

} // namespace zhu_screen_pet
