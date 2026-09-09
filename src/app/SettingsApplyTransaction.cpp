#include "app/SettingsApplyTransaction.h"

#include "app/AppConfigRepository.h"
#include "app/ChatController.h"
#include "infrastructure/SecretStore.h"
#include "memory/MemoryOrchestrator.h"
#include "model/ModelConfigRepository.h"
#include "model/ProviderManager.h"

namespace zhu_screen_pet {

SettingsApplyTransaction::SettingsApplyTransaction(
    ModelConfigRepository* modelRepository, AppConfigRepository* appRepository,
    ProviderManager* providerManager, ChatController* chatController,
    MemoryOrchestrator* memory, SecretStore* secretStore)
    : modelRepository_(modelRepository), appRepository_(appRepository),
      providerManager_(providerManager), chatController_(chatController),
      memory_(memory), secretStore_(secretStore)
{
}

AppError SettingsApplyTransaction::failure(
    AppErrorCode code, const QString& message, const QString& technical,
    const QString& operation, const QStringList& rollbackErrors) const
{
    AppError result;
    result.code = code;
    result.message = rollbackErrors.isEmpty()
        ? message : QStringLiteral("设置失败且未能完全恢复，请检查配置文件和密钥");
    result.domain = ErrorDomain::Application;
    result.technicalMessage = technical;
    if (!rollbackErrors.isEmpty()) {
        result.technicalMessage += QStringLiteral("; rollback failures: ")
            + rollbackErrors.join(QStringLiteral(" | "));
    }
    result.operation = operation;
    return result;
}

bool SettingsApplyTransaction::execute(
    const ModelProviderConfig& model, const PersonaConfig& persona,
    const MemoryLimits& limits, const UiConfig& ui, const QString& apiKey,
    AppError* error)
{
    QByteArray oldModelFile;
    QByteArray oldAppFile;
    QString technical;
    if (!modelRepository_->snapshot(&oldModelFile, &technical)
        || !appRepository_->snapshot(&oldAppFile, &technical)) {
        if (error) *error = failure(AppErrorCode::Io, QStringLiteral("无法读取旧配置，未进行修改"),
                                    technical, QStringLiteral("settings.snapshot"));
        return false;
    }

    const ModelProviderConfig oldModel = providerManager_->activeConfiguration();
    const PersonaConfig oldPersona = chatController_->personaConfig();
    const MemoryLimits oldLimits = memory_->limits();
    QString oldSecret;
    bool hadOldSecret = false;
    const bool changesSecret = !apiKey.trimmed().isEmpty() && secretStore_ != nullptr
        && !model.credentialService.isEmpty() && !model.credentialAccount.isEmpty();
    if (changesSecret) {
        hadOldSecret = secretStore_->read(model.credentialService, model.credentialAccount,
                                          &oldSecret, nullptr);
        if (!secretStore_->write(model.credentialService, model.credentialAccount,
                                 apiKey.trimmed(), &technical)) {
            if (error) *error = failure(AppErrorCode::Io, QStringLiteral("无法保存模型密钥"),
                                        technical, QStringLiteral("settings.save_secret"));
            return false;
        }
    }

    auto rollback = [&](bool restoreRuntime) {
        QStringList failures;
        QString detail;
        if (restoreRuntime) {
            if (!providerManager_->switchProvider(oldModel, &detail))
                failures.append(QStringLiteral("provider: %1").arg(detail));
            detail.clear();
            if (!chatController_->setPersonaConfig(oldPersona, &detail))
                failures.append(QStringLiteral("persona: %1").arg(detail));
            detail.clear();
            if (!memory_->setLimits(oldLimits, &detail))
                failures.append(QStringLiteral("memory: %1").arg(detail));
        }
        detail.clear();
        if (!modelRepository_->restore(oldModelFile, &detail))
            failures.append(QStringLiteral("model file: %1").arg(detail));
        detail.clear();
        if (!appRepository_->restore(oldAppFile, &detail))
            failures.append(QStringLiteral("app file: %1").arg(detail));
        if (changesSecret) {
            detail.clear();
            const bool restored = hadOldSecret
                ? secretStore_->write(model.credentialService, model.credentialAccount,
                                      oldSecret, &detail)
                : secretStore_->remove(model.credentialService, model.credentialAccount, &detail);
            if (!restored) failures.append(QStringLiteral("secret: %1").arg(detail));
        }
        return failures;
    };

    if (!modelRepository_->saveProfile(model, true, &technical)
        || !appRepository_->save(persona, limits, &technical, &ui)) {
        const QStringList rollbackErrors = rollback(false);
        if (error) *error = failure(AppErrorCode::Io, QStringLiteral("设置保存失败，已恢复旧配置"),
                                    technical, QStringLiteral("settings.persist"), rollbackErrors);
        return false;
    }
    if (!providerManager_->switchProvider(model, &technical)
        || !chatController_->setPersonaConfig(persona, &technical)
        || !memory_->setLimits(limits, &technical)) {
        const QStringList rollbackErrors = rollback(true);
        if (error) *error = failure(AppErrorCode::ConfigInvalid,
                                    QStringLiteral("无法应用运行时设置，已恢复旧配置"),
                                    technical, QStringLiteral("settings.apply_runtime"),
                                    rollbackErrors);
        return false;
    }
    return true;
}

bool SettingsApplyTransaction::executeUi(const PersonaConfig& persona,
                                         const MemoryLimits& limits,
                                         const UiConfig& ui, AppError* error)
{
    QByteArray oldAppFile;
    QString technical;
    if (appRepository_ == nullptr
        || !appRepository_->snapshot(&oldAppFile, &technical)) {
        if (error) *error = failure(AppErrorCode::Io,
                                    QStringLiteral("无法读取旧配置，未进行修改"),
                                    technical, QStringLiteral("settings.snapshot_ui"));
        return false;
    }
    if (appRepository_->save(persona, limits, &technical, &ui)) return true;

    QString rollbackError;
    QStringList rollbackErrors;
    if (!appRepository_->restore(oldAppFile, &rollbackError)) {
        rollbackErrors.append(QStringLiteral("app file: %1").arg(rollbackError));
    }
    if (error) *error = failure(AppErrorCode::Io,
                                QStringLiteral("界面设置保存失败，已恢复旧配置"),
                                technical, QStringLiteral("settings.persist_ui"),
                                rollbackErrors);
    return false;
}

} // namespace zhu_screen_pet
