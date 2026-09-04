#include "model/OpenAICompatibleProvider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QUuid>

#include <utility>

namespace zhu_screen_pet {

struct OpenAICompatibleProvider::PendingRequest
{
    QString requestId;
    std::vector<Message> messages;
    ChatOptions options;
    QString apiKey;
    QString transportRequestId;
    int attempts = 0;
    bool cancelled = false;
    bool stream = false;
    bool streamDone = false;
    bool deltaEmitted = false;
    QByteArray sseBuffer;
    QString streamedContent;
    ModelError streamError;
};

OpenAICompatibleProvider::OpenAICompatibleProvider(
    ProviderConfig config, HttpClient* httpClient, SecretStore* secretStore,
    Logger* logger, QObject* parent)
    : ChatProvider(parent), config_(std::move(config)), httpClient_(httpClient),
      secretStore_(secretStore), logger_(logger)
{
    if (httpClient_ == nullptr) {
        httpClient_ = new HttpClient(this);
    }
    connect(httpClient_, &HttpClient::requestFinished, this,
            &OpenAICompatibleProvider::onHttpFinished);
    connect(httpClient_, &HttpClient::dataAvailable, this,
            &OpenAICompatibleProvider::onHttpDataAvailable);
}

QString OpenAICompatibleProvider::providerName() const
{
    return QStringLiteral("OpenAI-compatible");
}

QString OpenAICompatibleProvider::baseUrl() const
{
    return config_.baseUrl;
}

int OpenAICompatibleProvider::lastAttemptCount() const
{
    return lastAttemptCount_;
}

QUrl OpenAICompatibleProvider::completionUrl() const
{
    QUrl url(config_.baseUrl);
    QString path = url.path();
    if (!path.endsWith(QStringLiteral("/chat/completions"))) {
        if (!path.endsWith(QLatin1Char('/'))) {
            path += QLatin1Char('/');
        }
        path += QStringLiteral("chat/completions");
        url.setPath(path);
    }
    return url;
}

bool OpenAICompatibleProvider::resolveApiKey(QString* apiKey,
                                             QString* errorMessage) const
{
    if (apiKey == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("api key output is null");
        }
        return false;
    }
    if (!config_.apiKey.isEmpty()) {
        *apiKey = config_.apiKey;
        return true;
    }
    if (secretStore_ == nullptr || config_.credentialAccount.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("api key is not configured");
        }
        return false;
    }
    return secretStore_->read(config_.credentialService, config_.credentialAccount,
                             apiKey, errorMessage);
}

QString OpenAICompatibleProvider::startChat(const std::vector<Message>& messages,
                                            const ChatOptions& options)
{
    const QString requestId = QUuid::createUuid().toString(QUuid::Id128);
    auto request = std::make_shared<PendingRequest>();
    request->requestId = requestId;
    request->messages = messages;
    request->options = options;
    request->stream = options.stream;
    pendingRequests_.insert(requestId, request);
    emit chatStarted(requestId);

    if (messages.empty()) {
        finishFailure(requestId, {ModelErrorCode::InvalidRequest,
                                  QStringLiteral("message list must not be empty"), 0});
        return requestId;
    }
    if (config_.baseUrl.isEmpty() || !completionUrl().isValid()) {
        finishFailure(requestId, {ModelErrorCode::InvalidRequest,
                                  QStringLiteral("provider base URL is invalid"), 0});
        return requestId;
    }

    QString errorMessage;
    if (!resolveApiKey(&request->apiKey, &errorMessage)) {
        finishFailure(requestId, {ModelErrorCode::Authentication, errorMessage, 0});
        return requestId;
    }

    if (logger_ != nullptr) {
        logger_->info(QStringLiteral("model"), QStringLiteral("request_started"),
                      providerName() + QStringLiteral(" request ") + requestId);
    }
    sendAttempt(requestId);
    return requestId;
}

void OpenAICompatibleProvider::cancel(const QString& requestId)
{
    const auto request = pendingRequests_.value(requestId);
    if (request == nullptr) {
        return;
    }
    request->cancelled = true;
    if (!request->transportRequestId.isEmpty()) {
        httpClient_->cancel(request->transportRequestId);
    }
    finishFailure(requestId, {ModelErrorCode::Cancelled,
                              QStringLiteral("request was cancelled"), 0});
}

QByteArray OpenAICompatibleProvider::buildRequestBody(const PendingRequest& request) const
{
    QJsonArray messages;
    for (const Message& message : request.messages) {
        QJsonObject item;
        item.insert(QStringLiteral("role"), messageRoleName(message.role));
        item.insert(QStringLiteral("content"), message.content);
        messages.append(item);
    }

    const QString model = request.options.model.isEmpty()
        ? config_.model : request.options.model;
    QJsonObject root;
    root.insert(QStringLiteral("model"), model);
    root.insert(QStringLiteral("messages"), messages);
    root.insert(QStringLiteral("temperature"), request.options.temperature);
    root.insert(QStringLiteral("max_tokens"), request.options.maxTokens);
    root.insert(QStringLiteral("stream"), request.stream);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

void OpenAICompatibleProvider::sendAttempt(const QString& requestId)
{
    const auto request = pendingRequests_.value(requestId);
    if (request == nullptr || request->cancelled) {
        return;
    }
    const QString model = request->options.model.isEmpty()
        ? config_.model : request->options.model;
    if (model.isEmpty()) {
        finishFailure(requestId, {ModelErrorCode::InvalidRequest,
                                  QStringLiteral("model is not configured"), 0});
        return;
    }

    ++request->attempts;
    const QList<QPair<QByteArray, QByteArray>> headers = {
        {QByteArrayLiteral("Authorization"),
         QByteArrayLiteral("Bearer ") + request->apiKey.toUtf8()},
        {QByteArrayLiteral("Accept"), request->stream
            ? QByteArrayLiteral("text/event-stream") : QByteArrayLiteral("application/json")}
    };
    request->transportRequestId = httpClient_->postJson(
        completionUrl(), buildRequestBody(*request), headers, config_.timeoutMs);
    transportToRequest_.insert(request->transportRequestId, requestId);

    if (logger_ != nullptr) {
        logger_->debug(QStringLiteral("model"), QStringLiteral("request_attempt"),
                       QStringLiteral("%1 attempt %2").arg(requestId).arg(request->attempts));
    }
}

void OpenAICompatibleProvider::onHttpFinished(const QString& transportRequestId,
                                              const HttpResponse& response)
{
    const QString requestId = transportToRequest_.take(transportRequestId);
    if (requestId.isEmpty()) {
        return;
    }
    const auto request = pendingRequests_.value(requestId);
    if (request == nullptr || request->cancelled) {
        return;
    }
    request->transportRequestId.clear();

    if (request->stream) {
        if (request->streamError.code != ModelErrorCode::None) {
            finishFailure(requestId, request->streamError);
        } else if (!response.succeeded() || response.statusCode < 200
                   || response.statusCode >= 300) {
            // 流式内容一旦发送给上层便不可撤回，不能把第二次响应透明拼接到第一次响应。
            if (!request->deltaEmitted && shouldRetry(response)
                && request->attempts <= config_.maxRetries) {
                request->sseBuffer.clear();
                request->streamedContent.clear();
                request->streamDone = false;
                scheduleRetry(requestId);
            } else {
                finishFailure(requestId, errorFromResponse(response));
            }
        } else if (!request->streamDone || request->streamedContent.isEmpty()) {
            finishFailure(requestId, {ModelErrorCode::InvalidResponse,
                                      QStringLiteral("stream response is incomplete or contains no content"),
                                      response.statusCode});
        } else {
            finishSuccess(requestId, request->streamedContent);
        }
        return;
    }

    if (response.succeeded() && response.statusCode >= 200 && response.statusCode < 300) {
        const ChatResult result = parseChatResponse(response);
        if (result.succeeded) {
            finishSuccess(requestId, result.content);
        } else {
            finishFailure(requestId, result.error);
        }
        return;
    }

    if (shouldRetry(response) && request->attempts <= config_.maxRetries) {
        scheduleRetry(requestId);
        return;
    }
    finishFailure(requestId, errorFromResponse(response));
}

void OpenAICompatibleProvider::onHttpDataAvailable(
    const QString& transportRequestId, const QByteArray& data)
{
    const QString requestId = transportToRequest_.value(transportRequestId);
    const auto request = pendingRequests_.value(requestId);
    if (request == nullptr || request->cancelled || !request->stream) {
        return;
    }
    parseSseData(requestId, data);
}

bool OpenAICompatibleProvider::parseSseData(const QString& requestId,
                                            const QByteArray& data)
{
    const auto request = pendingRequests_.value(requestId);
    if (request == nullptr) {
        return false;
    }
    request->sseBuffer.append(data);

    while (true) {
        const int newlineIndex = request->sseBuffer.indexOf('\n');
        if (newlineIndex < 0) {
            break;
        }
        QByteArray line = request->sseBuffer.left(newlineIndex);
        request->sseBuffer.remove(0, newlineIndex + 1);
        if (line.endsWith('\r')) {
            line.chop(1);
        }
        if (line.isEmpty() || line.startsWith(':')) {
            continue;
        }
        if (!line.startsWith("data:")) {
            continue;
        }

        const QByteArray payload = line.mid(5).trimmed();
        if (payload == QByteArrayLiteral("[DONE]")) {
            request->streamDone = true;
            continue;
        }

        QJsonParseError parseError{};
        const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            request->streamError = {ModelErrorCode::InvalidResponse,
                                    QStringLiteral("invalid SSE JSON response"), 200};
            return false;
        }

        const QJsonArray choices = document.object().value(QStringLiteral("choices")).toArray();
        if (choices.isEmpty() || !choices.first().isObject()) {
            request->streamError = {ModelErrorCode::InvalidResponse,
                                    QStringLiteral("SSE response does not contain choices"), 200};
            return false;
        }
        const QJsonObject delta = choices.first().toObject()
            .value(QStringLiteral("delta")).toObject();
        const QString content = delta.value(QStringLiteral("content")).toString();
        if (!content.isEmpty()) {
            request->streamedContent += content;
            request->deltaEmitted = true;
            emit chatDelta(requestId, content);
        }
    }
    return true;
}

bool OpenAICompatibleProvider::shouldRetry(const HttpResponse& response) const
{
    if (response.timedOut) return true;
    // QNetworkReply 会为 HTTP 4xx/5xx 同时设置 NetworkError。只要服务端已经
    // 返回明确的失败状态，就必须先按协议状态判断，不能把 401/403 等误当成
    // 可恢复的传输故障。2xx 响应中途断开仍按网络错误处理。
    if (response.statusCode >= 400) {
        return response.statusCode == 408 || response.statusCode == 429
            || response.statusCode >= 500;
    }
    return response.networkError != 0;
}

void OpenAICompatibleProvider::scheduleRetry(const QString& requestId)
{
    const auto request = pendingRequests_.value(requestId);
    if (request == nullptr || request->cancelled) {
        return;
    }
    const int exponent = qMin(request->attempts - 1, 10);
    const int delay = qMin(config_.retryBaseDelayMs * (1 << exponent), 30000);
    if (logger_ != nullptr) {
        logger_->warning(QStringLiteral("model"), QStringLiteral("retry_scheduled"),
                         QStringLiteral("%1 retry in %2 ms").arg(requestId).arg(delay));
    }
    QTimer::singleShot(delay, this, [this, requestId]() { sendAttempt(requestId); });
}

ChatResult OpenAICompatibleProvider::parseChatResponse(const HttpResponse& response) const
{
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(response.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return ChatResult::failure({ModelErrorCode::InvalidResponse,
                                    QStringLiteral("invalid JSON response"), response.statusCode});
    }
    const QJsonArray choices = document.object().value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty() || !choices.first().isObject()) {
        return ChatResult::failure({ModelErrorCode::InvalidResponse,
                                    QStringLiteral("response does not contain choices"), response.statusCode});
    }
    const QJsonObject message = choices.first().toObject()
        .value(QStringLiteral("message")).toObject();
    const QString content = message.value(QStringLiteral("content")).toString();
    if (content.isEmpty()) {
        return ChatResult::failure({ModelErrorCode::InvalidResponse,
                                    QStringLiteral("response content is empty"), response.statusCode});
    }
    return ChatResult::success(content);
}

ModelError OpenAICompatibleProvider::errorFromResponse(const HttpResponse& response) const
{
    ModelError error;
    error.domain = ErrorDomain::Model;
    error.operation = QStringLiteral("model.chat");
    error.httpStatus = response.statusCode;
    if (response.timedOut) {
        error.code = ModelErrorCode::Timeout;
        error.message = QStringLiteral("model request timed out");
        error.retryable = true;
        return error;
    }
    const bool hasHttpFailure = response.statusCode >= 400;
    if (hasHttpFailure && (response.statusCode == 401 || response.statusCode == 403)) {
        error.code = ModelErrorCode::Authentication;
    } else if (hasHttpFailure && response.statusCode == 408) {
        error.code = ModelErrorCode::Timeout;
    } else if (hasHttpFailure && response.statusCode == 429) {
        error.code = ModelErrorCode::RateLimit;
    } else if (hasHttpFailure && response.statusCode < 500) {
        error.code = ModelErrorCode::InvalidRequest;
    } else if (hasHttpFailure) {
        error.code = ModelErrorCode::Network;
    } else if (response.networkError != 0) {
        error.code = ModelErrorCode::Network;
        error.message = response.errorString.isEmpty()
            ? QStringLiteral("model network request failed") : response.errorString;
        error.technicalMessage = error.message;
        error.retryable = true;
        return error;
    } else {
        error.code = ModelErrorCode::Unknown;
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(response.body, &parseError);
    if (parseError.error == QJsonParseError::NoError && document.isObject()) {
        error.message = document.object().value(QStringLiteral("error")).toObject()
            .value(QStringLiteral("message")).toString();
    }
    if (error.message.isEmpty()) {
        error.message = QStringLiteral("model request failed (HTTP %1)").arg(response.statusCode);
    }
    error.technicalMessage = error.message;
    error.retryable = error.code == ModelErrorCode::Timeout
        || error.code == ModelErrorCode::Network || error.code == ModelErrorCode::RateLimit;
    return error;
}

void OpenAICompatibleProvider::finishSuccess(const QString& requestId,
                                             const QString& content)
{
    if (!pendingRequests_.contains(requestId)) {
        return;
    }
    if (logger_ != nullptr) {
        logger_->info(QStringLiteral("model"), QStringLiteral("request_finished"),
                      QStringLiteral("%1 succeeded").arg(requestId));
    }
    const auto request = pendingRequests_.value(requestId);
    lastAttemptCount_ = request == nullptr ? 0 : request->attempts;
    pendingRequests_.remove(requestId);
    emit chatFinished(requestId, ChatResult::success(content));
}

void OpenAICompatibleProvider::finishFailure(const QString& requestId,
                                             const ModelError& error)
{
    if (!pendingRequests_.contains(requestId)) {
        return;
    }
    const auto request = pendingRequests_.value(requestId);
    if (request != nullptr && !request->transportRequestId.isEmpty()) {
        transportToRequest_.remove(request->transportRequestId);
        httpClient_->cancel(request->transportRequestId);
    }
    // 失败日志由应用层 ErrorCenter 统一记录，避免 Provider 和 UI 重复落盘。
    lastAttemptCount_ = request == nullptr ? 0 : request->attempts;
    pendingRequests_.remove(requestId);
    emit chatFinished(requestId, ChatResult::failure(error));
}

DeepSeekAICompatibleProvider::DeepSeekAICompatibleProvider(
    ProviderConfig config, HttpClient* httpClient, SecretStore* secretStore,
    Logger* logger, QObject* parent)
    : OpenAICompatibleProvider(std::move(config), httpClient, secretStore, logger, parent)
{
}

QString DeepSeekAICompatibleProvider::providerName() const
{
    return QStringLiteral("DeepSeek");
}

} // namespace zhu_screen_pet
