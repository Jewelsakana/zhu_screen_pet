#pragma once

#include <QMetaType>
#include <QString>

namespace zhu_screen_pet {

/** 创建一个聊天模型 Provider 所需的公开配置；不保存 API Key 明文。 */
struct ModelProviderConfig
{
    QString profileId;
    QString providerType;
    QString displayName;
    QString baseUrl;
    QString model;
    QString credentialService;
    QString credentialAccount;
    QString mockReply;
    int timeoutMs = 0;
    int maxRetries = -1;
    int retryBaseDelayMs = 0;

    /** 仅清理字符串空白并统一 Provider 类型大小写，不补任何配置默认值。 */
    ModelProviderConfig normalized() const;
    /** 检查类型、URL、模型名和数值范围是否合法。 */
    bool validate(QString* errorMessage = nullptr) const;
};

} // namespace zhu_screen_pet

Q_DECLARE_METATYPE(zhu_screen_pet::ModelProviderConfig)
