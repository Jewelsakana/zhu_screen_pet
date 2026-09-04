#include "model/ModelProviderConfig.h"

#include <QUrl>

namespace zhu_screen_pet {

ModelProviderConfig ModelProviderConfig::normalized() const
{
    ModelProviderConfig result = *this;
    result.profileId = result.profileId.trimmed();
    result.providerType = result.providerType.trimmed().toLower();
    result.displayName = result.displayName.trimmed();
    result.baseUrl = result.baseUrl.trimmed();
    result.model = result.model.trimmed();
    result.credentialService = result.credentialService.trimmed();
    result.credentialAccount = result.credentialAccount.trimmed();
    result.mockReply = result.mockReply.trimmed();
    return result;
}

bool ModelProviderConfig::validate(QString* errorMessage) const
{
    const ModelProviderConfig config = normalized();
    const auto fail = [errorMessage](const QString& message) {
        if (errorMessage != nullptr) *errorMessage = message;
        return false;
    };
    if (config.profileId.isEmpty() || config.profileId.contains(QLatin1Char('/'))) {
        return fail(QStringLiteral("model profile id is empty or contains '/'"));
    }
    if (config.providerType != QStringLiteral("mock")
        && config.providerType != QStringLiteral("openai-compatible")
        && config.providerType != QStringLiteral("deepseek")) {
        return fail(QStringLiteral("unsupported model provider type: %1").arg(config.providerType));
    }
    if (config.timeoutMs <= 0 || config.maxRetries < 0 || config.retryBaseDelayMs <= 0) {
        return fail(QStringLiteral("model timeout and retry settings are invalid"));
    }
    if (config.providerType == QStringLiteral("mock")) {
        if (config.displayName.isEmpty()) return fail(QStringLiteral("provider display name must not be empty"));
        return config.mockReply.isEmpty() ? fail(QStringLiteral("mock reply must not be empty")) : true;
    }
    const QUrl url(config.baseUrl);
    if (!url.isValid() || (url.scheme() != QStringLiteral("http")
                           && url.scheme() != QStringLiteral("https"))
        || url.host().isEmpty()) {
        return fail(QStringLiteral("model base URL must be a valid HTTP(S) URL"));
    }
    if (config.model.isEmpty()) return fail(QStringLiteral("model name must not be empty"));
    if (config.displayName.isEmpty()) return fail(QStringLiteral("provider display name must not be empty"));
    if (config.credentialService.isEmpty() || config.credentialAccount.isEmpty()) {
        return fail(QStringLiteral("credential service and account must not be empty"));
    }
    return true;
}

} // namespace zhu_screen_pet
