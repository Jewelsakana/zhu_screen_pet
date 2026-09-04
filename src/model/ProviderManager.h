#pragma once

#include <memory>

#include <QSet>

#include "model/ChatProvider.h"
#include "model/ModelProviderConfig.h"

namespace zhu_screen_pet {

class ChatProviderFactory;

/**
 * ChatController 面前稳定不变的模型入口。
 *
 * 管理器拥有当前 Provider，在没有运行中请求时可以原子替换它，并转发聊天信号。
 */
class ProviderManager final : public ChatProvider
{
    Q_OBJECT

public:
    explicit ProviderManager(ChatProviderFactory* factory, QObject* parent = nullptr);

    /** 验证并切换 Provider；有请求运行时拒绝切换以保护回调生命周期。 */
    bool switchProvider(const ModelProviderConfig& config, QString* errorMessage = nullptr);
    QString startChat(const std::vector<Message>& messages,
                      const ChatOptions& options) override;
    void cancel(const QString& requestId) override;

    /** 返回当前配置和 Provider 显示名称。 */
    ModelProviderConfig activeConfiguration() const;
    QString activeProviderName() const;
    int activeRequestCount() const;
    bool hasProvider() const;

signals:
    /** Provider 成功切换后通知设置界面和状态栏。 */
    void activeProviderChanged(const QString& profileId, const QString& providerName);

private:
    void connectProvider(ChatProvider* provider);

    ChatProviderFactory* factory_ = nullptr;
    std::unique_ptr<ChatProvider> activeProvider_;
    ModelProviderConfig activeConfig_;
    QSet<QString> activeRequests_;
};

} // namespace zhu_screen_pet
