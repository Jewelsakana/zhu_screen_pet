#pragma once

#include <memory>
#include <QStringList>

#include <QObject>

#include "core/AppError.h"
#include "app/PersonaConfig.h"
#include "app/UiConfig.h"
#include "memory/MemoryContext.h"
#include "model/ModelProviderConfig.h"
#include "model/ChatResult.h"

namespace zhu_screen_pet {

class AppConfigRepository;
class ChatController;
class ChatProvider;
class ChatProviderFactory;
class ErrorCenter;
class MemoryOrchestrator;
class ModelConfigRepository;
class ProviderManager;
class SecretStore;

/** 设置应用服务：管理草稿校验、连接测试、原子持久化和运行时热更新。 */
class SettingsController final : public QObject
{
    Q_OBJECT

public:
    SettingsController(ModelConfigRepository* modelRepository,
                       AppConfigRepository* appRepository,
                       ChatProviderFactory* factory,
                       ProviderManager* providerManager,
                       ChatController* chatController,
                       MemoryOrchestrator* memory,
                       SecretStore* secretStore,
                       ErrorCenter* errorCenter,
                       QObject* parent = nullptr);

    ModelProviderConfig activeModel() const;
    PersonaConfig persona() const;
    MemoryLimits memoryLimits() const;
    UiConfig uiConfig() const;
    QStringList modelProfileIds(QString* errorMessage = nullptr) const;
    bool loadModelProfile(const QString& id, ModelProviderConfig* config,
                          QString* errorMessage = nullptr) const;

    /** 异步发送最小请求测试候选配置，不改变当前 Provider 和磁盘文件。 */
    bool testConnection(const ModelProviderConfig& config, const QString& apiKey,
                        AppError* error = nullptr);
    /** 校验、热切换并原子保存；任何失败都会尽力恢复旧运行时和磁盘配置。 */
    bool apply(const ModelProviderConfig& model, const PersonaConfig& persona,
               const MemoryLimits& limits, const QString& apiKey,
               AppError* error = nullptr);
    bool apply(const ModelProviderConfig& model, const PersonaConfig& persona,
               const MemoryLimits& limits, const UiConfig& uiConfig,
               const QString& apiKey, AppError* error = nullptr);
    void setInitialUiConfig(const UiConfig& uiConfig);
    /** 由 UI 在用户确认后取消活动请求；会话和历史消息不会关闭或删除。 */
    void cancelActiveChat();

signals:
    void settingsApplied(const ModelProviderConfig& model,
                         const PersonaConfig& persona,
                         const MemoryLimits& limits);
    void connectionTestFinished(bool succeeded, const AppError& error);
    void uiConfigurationChanged(const UiConfig& uiConfig);
    void operationFailed(const AppError& error);

private:
    AppError makeError(AppErrorCode code, const QString& message,
                       const QString& technical, const QString& operation) const;
    bool fail(const AppError& error, AppError* output);
    bool ensureIdle(AppError* error) const;
    void finishConnectionTest(const QString& requestId, const ChatResult& result);

    ModelConfigRepository* modelRepository_ = nullptr;
    AppConfigRepository* appRepository_ = nullptr;
    ChatProviderFactory* factory_ = nullptr;
    ProviderManager* providerManager_ = nullptr;
    ChatController* chatController_ = nullptr;
    MemoryOrchestrator* memory_ = nullptr;
    SecretStore* secretStore_ = nullptr;
    ErrorCenter* errorCenter_ = nullptr;
    std::unique_ptr<ChatProvider> testProvider_;
    QString testRequestId_;
    UiConfig uiConfig_;
};

} // namespace zhu_screen_pet
