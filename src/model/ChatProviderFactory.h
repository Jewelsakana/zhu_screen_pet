#pragma once

#include <memory>

#include "model/ChatProvider.h"
#include "model/ModelProviderConfig.h"

namespace zhu_screen_pet {

class HttpClient;
class Logger;
class SecretStore;

/** 根据强类型配置创建 Provider，并统一注入网络、密钥和日志依赖。 */
class ChatProviderFactory final
{
public:
    ChatProviderFactory(HttpClient* httpClient, SecretStore* secretStore, Logger* logger = nullptr);

    /** 创建配置对应的 Provider；配置无效或类型不支持时返回 nullptr。 */
    std::unique_ptr<ChatProvider> create(const ModelProviderConfig& config,
                                         QString* errorMessage = nullptr,
                                         const QString& apiKeyOverride = {}) const;

private:
    HttpClient* httpClient_ = nullptr;
    SecretStore* secretStore_ = nullptr;
    Logger* logger_ = nullptr;
};

} // namespace zhu_screen_pet
