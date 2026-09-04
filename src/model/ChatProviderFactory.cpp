#include "model/ChatProviderFactory.h"

#include "model/MockChatProvider.h"
#include "model/OpenAICompatibleProvider.h"

namespace zhu_screen_pet {

ChatProviderFactory::ChatProviderFactory(HttpClient* httpClient, SecretStore* secretStore,
                                         Logger* logger)
    : httpClient_(httpClient), secretStore_(secretStore), logger_(logger)
{
}

std::unique_ptr<ChatProvider> ChatProviderFactory::create(
    const ModelProviderConfig& source, QString* errorMessage,
    const QString& apiKeyOverride) const
{
    ModelProviderConfig config = source.normalized();
    if (!config.validate(errorMessage)) return nullptr;

    if (config.providerType == QStringLiteral("mock")) {
        return std::make_unique<MockChatProvider>(config.mockReply);
    }
    if (httpClient_ == nullptr || secretStore_ == nullptr) {
        if (errorMessage) *errorMessage = QStringLiteral("network or secret store is unavailable");
        return nullptr;
    }

    ProviderConfig providerConfig;
    providerConfig.baseUrl = config.baseUrl;
    providerConfig.model = config.model;
    providerConfig.apiKey = apiKeyOverride;
    providerConfig.credentialService = config.credentialService;
    providerConfig.credentialAccount = config.credentialAccount;
    providerConfig.timeoutMs = config.timeoutMs;
    providerConfig.maxRetries = config.maxRetries;
    providerConfig.retryBaseDelayMs = config.retryBaseDelayMs;

    if (config.providerType == QStringLiteral("deepseek")) {
        return std::make_unique<DeepSeekAICompatibleProvider>(
            providerConfig, httpClient_, secretStore_, logger_);
    }
    return std::make_unique<OpenAICompatibleProvider>(
        providerConfig, httpClient_, secretStore_, logger_);
}

} // namespace zhu_screen_pet
