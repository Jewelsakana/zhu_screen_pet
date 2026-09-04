#pragma once

#include <QHash>
#include <QUrl>

#include <memory>
#include <vector>

#include "infrastructure/HttpClient.h"
#include "infrastructure/Logger.h"
#include "infrastructure/SecretStore.h"
#include "model/ChatProvider.h"

namespace zhu_screen_pet {

/** OpenAI-compatible Provider 的连接、鉴权和重试配置。 */
struct ProviderConfig
{
    QString baseUrl;
    QString model;
    QString apiKey;
    QString credentialService = QStringLiteral("zhu_screen_pet");
    QString credentialAccount;
    int timeoutMs = 30000;
    int maxRetries = 3;
    int retryBaseDelayMs = 1000;
};

/** 使用 OpenAI-compatible Chat Completions 协议的通用模型 Provider。 */
class OpenAICompatibleProvider : public ChatProvider
{
    Q_OBJECT

public:
    explicit OpenAICompatibleProvider(
        ProviderConfig config,
        HttpClient* httpClient = nullptr,
        SecretStore* secretStore = nullptr,
        Logger* logger = nullptr,
        QObject* parent = nullptr);

    /** 启动一次非流式聊天请求。 */
    QString startChat(const std::vector<Message>& messages,
                      const ChatOptions& options) override;
    /** 取消请求或其后续重试。 */
    void cancel(const QString& requestId) override;

    /** 返回 Provider 显示名称。 */
    virtual QString providerName() const;
    /** 返回当前配置的 API 地址。 */
    QString baseUrl() const;
    /** 返回最近完成请求的传输尝试次数，主要用于诊断自动重试。 */
    int lastAttemptCount() const;

protected:
    /** 生成最终的 `/chat/completions` URL。 */
    QUrl completionUrl() const;
    /** 为请求解析 API Key；显式配置优先于 SecretStore。 */
    bool resolveApiKey(QString* apiKey, QString* errorMessage) const;

private:
    struct PendingRequest;

    void sendAttempt(const QString& requestId);
    void scheduleRetry(const QString& requestId);
    void finishSuccess(const QString& requestId, const QString& content);
    void finishFailure(const QString& requestId, const ModelError& error);
    void onHttpFinished(const QString& transportRequestId,
                        const HttpResponse& response);
    void onHttpDataAvailable(const QString& transportRequestId,
                             const QByteArray& data);
    ModelError errorFromResponse(const HttpResponse& response) const;
    bool shouldRetry(const HttpResponse& response) const;
    ChatResult parseChatResponse(const HttpResponse& response) const;
    QByteArray buildRequestBody(const PendingRequest& request) const;
    bool parseSseData(const QString& requestId, const QByteArray& data);

    ProviderConfig config_;
    HttpClient* httpClient_ = nullptr;
    SecretStore* secretStore_ = nullptr;
    Logger* logger_ = nullptr;
    QHash<QString, std::shared_ptr<PendingRequest>> pendingRequests_;
    QHash<QString, QString> transportToRequest_;
    int lastAttemptCount_ = 0;
};

/** DeepSeek 的 OpenAI-compatible Provider，复用通用协议实现。 */
class DeepSeekAICompatibleProvider final : public OpenAICompatibleProvider
{
    Q_OBJECT

public:
    explicit DeepSeekAICompatibleProvider(
        ProviderConfig config = {},
        HttpClient* httpClient = nullptr,
        SecretStore* secretStore = nullptr,
        Logger* logger = nullptr,
        QObject* parent = nullptr);

    /** 返回 DeepSeek 显示名称。 */
    QString providerName() const override;
};

} // namespace zhu_screen_pet
