#pragma once

#include <QString>
#include <QStringList>

#include "app/PersonaConfig.h"
#include "app/UiConfig.h"
#include "core/AppError.h"
#include "memory/MemoryContext.h"
#include "model/ModelProviderConfig.h"

namespace zhu_screen_pet {

class AppConfigRepository;
class ChatController;
class MemoryOrchestrator;
class ModelConfigRepository;
class ProviderManager;
class SecretStore;

/** 跨配置文件、凭据和运行时的可恢复设置事务。 */
class SettingsApplyTransaction final
{
public:
    SettingsApplyTransaction(ModelConfigRepository* modelRepository,
                             AppConfigRepository* appRepository,
                             ProviderManager* providerManager,
                             ChatController* chatController,
                             MemoryOrchestrator* memory,
                             SecretStore* secretStore);

    bool execute(const ModelProviderConfig& model,
                 const PersonaConfig& persona,
                 const MemoryLimits& limits,
                 const UiConfig& ui,
                 const QString& apiKey,
                 AppError* error = nullptr);
    /** 复用同一事务与回滚策略，只更新应用 UI 配置。 */
    bool executeUi(const PersonaConfig& persona, const MemoryLimits& limits,
                   const UiConfig& ui, AppError* error = nullptr);

private:
    AppError failure(AppErrorCode code, const QString& message,
                     const QString& technical, const QString& operation,
                     const QStringList& rollbackErrors = {}) const;

    ModelConfigRepository* modelRepository_ = nullptr;
    AppConfigRepository* appRepository_ = nullptr;
    ProviderManager* providerManager_ = nullptr;
    ChatController* chatController_ = nullptr;
    MemoryOrchestrator* memory_ = nullptr;
    SecretStore* secretStore_ = nullptr;
};

} // namespace zhu_screen_pet
