#include "app/SettingsController.h"

#include "app/ChatController.h"
#include "app/ErrorCenter.h"
#include "app/AppConfigRepository.h"
#include "model/ModelConfigRepository.h"
#include "infrastructure/SecretStore.h"
#include "memory/MemoryOrchestrator.h"
#include "model/ChatProvider.h"
#include "model/ChatProviderFactory.h"
#include "model/ChatResult.h"
#include "model/ProviderManager.h"

namespace zhu_screen_pet {

SettingsController::SettingsController(ModelConfigRepository* modelRepository,
                                       AppConfigRepository* appRepository,
                                       ChatProviderFactory* factory,
                                       ProviderManager* providerManager,
                                       ChatController* chatController,
                                       MemoryOrchestrator* memory,
                                       SecretStore* secretStore,
                                       ErrorCenter* errorCenter,
                                       QObject* parent)
    : QObject(parent), modelRepository_(modelRepository), appRepository_(appRepository),
      factory_(factory), providerManager_(providerManager), chatController_(chatController),
      memory_(memory), secretStore_(secretStore), errorCenter_(errorCenter)
{
    qRegisterMetaType<ChatResult>("ChatResult");
}

ModelProviderConfig SettingsController::activeModel() const
{
    return providerManager_ == nullptr ? ModelProviderConfig{} : providerManager_->activeConfiguration();
}

PersonaConfig SettingsController::persona() const
{
    return chatController_ == nullptr ? PersonaConfig{} : chatController_->personaConfig();
}

MemoryLimits SettingsController::memoryLimits() const
{
    return memory_ == nullptr ? MemoryLimits{} : memory_->limits();
}

UiConfig SettingsController::uiConfig() const
{
    return uiConfig_;
}

void SettingsController::setInitialUiConfig(const UiConfig& uiConfig)
{
    uiConfig_ = uiConfig.normalized();
}

QStringList SettingsController::modelProfileIds(QString* errorMessage) const
{
    return modelRepository_ == nullptr ? QStringList{} : modelRepository_->profileIds(errorMessage);
}

bool SettingsController::loadModelProfile(const QString& id, ModelProviderConfig* config,
                                          QString* errorMessage) const
{
    return modelRepository_ != nullptr && modelRepository_->loadProfile(id, config, errorMessage);
}

AppError SettingsController::makeError(AppErrorCode code, const QString& message,
                                       const QString& technical, const QString& operation) const
{
    AppError result;
    result.code = code;
    result.message = message;
    result.domain = ErrorDomain::Application;
    result.technicalMessage = technical;
    result.operation = operation;
    return result;
}

bool SettingsController::fail(const AppError& error, AppError* output)
{
    if (output) *output = error;
    emit operationFailed(error);
    return false;
}

bool SettingsController::ensureIdle(AppError* error) const
{
    const bool providerIdle = providerManager_ == nullptr || providerManager_->activeRequestCount() == 0;
    const bool chatIdle = chatController_ == nullptr || chatController_->pendingRequestCount() == 0;
    if (providerIdle && chatIdle) return true;
    const AppError value = makeError(AppErrorCode::Busy, QStringLiteral("回复生成期间不能修改模型设置"),
                                     QStringLiteral("a model request is still running"),
                                     QStringLiteral("settings.apply"));
    if (error) *error = value;
    return false;
}

bool SettingsController::testConnection(const ModelProviderConfig& source, const QString& apiKey,
                                        AppError* error)
{
    if (testProvider_ != nullptr) {
        return fail(makeError(AppErrorCode::Busy, QStringLiteral("已有连接测试正在进行"),
                              QStringLiteral("a connection test is already running"),
                              QStringLiteral("settings.test_connection")), error);
    }
    if (factory_ == nullptr) {
        return fail(makeError(AppErrorCode::NotReady, QStringLiteral("模型服务暂不可用"),
                              QStringLiteral("provider factory is unavailable"),
                              QStringLiteral("settings.test_connection")), error);
    }
    const ModelProviderConfig config = source.normalized();
    QString technical;
    testProvider_ = factory_->create(config, &technical, apiKey);
    if (testProvider_ == nullptr) {
        return fail(makeError(AppErrorCode::ConfigInvalid, QStringLiteral("模型配置无效"), technical,
                              QStringLiteral("settings.test_connection")), error);
    }
    connect(testProvider_.get(), &ChatProvider::chatFinished, this,
            [this](const QString& requestId, const ChatResult& result) {
                if (requestId == testRequestId_) finishConnectionTest(requestId, result);
            }, Qt::QueuedConnection);
    ChatOptions options;
    options.model = config.model;
    options.maxTokens = 8;
    options.temperature = 0.0;
    options.stream = false;
    testRequestId_ = testProvider_->startChat(
        {Message::create(MessageRole::User, QStringLiteral("ping"))}, options);
    if (testRequestId_.isEmpty()) {
        testProvider_.reset();
        return fail(makeError(AppErrorCode::Network, QStringLiteral("无法启动连接测试"),
                              QStringLiteral("provider returned an empty request id"),
                              QStringLiteral("settings.test_connection")), error);
    }
    return true;
}

void SettingsController::cancelActiveChat()
{
    if (chatController_ != nullptr) chatController_->cancelAll();
}

void SettingsController::finishConnectionTest(const QString& requestId, const ChatResult& result)
{
    Q_UNUSED(requestId);
    AppError error;
    if (result.succeeded) {
        error = makeError(AppErrorCode::None, QStringLiteral("连接测试成功"), {},
                          QStringLiteral("settings.test_connection"));
    } else {
        error = result.error;
        error.message = QStringLiteral("连接测试失败：%1").arg(result.error.message);
        error.operation = QStringLiteral("settings.test_connection");
    }
    testProvider_.reset();
    testRequestId_.clear();
    emit connectionTestFinished(result.succeeded, error);
}

bool SettingsController::apply(const ModelProviderConfig& sourceModel,
                               const PersonaConfig& sourcePersona,
                               const MemoryLimits& sourceLimits,
                               const QString& apiKey, AppError* error)
{
    return apply(sourceModel, sourcePersona, sourceLimits, uiConfig_, apiKey, error);
}

bool SettingsController::apply(const ModelProviderConfig& sourceModel,
                               const PersonaConfig& sourcePersona,
                               const MemoryLimits& sourceLimits,
                               const UiConfig& sourceUi,
                               const QString& apiKey, AppError* error)
{
    if (!ensureIdle(error)) {
        if (error && error->code != AppErrorCode::None) { emit operationFailed(*error); }
        return false;
    }
    if (modelRepository_ == nullptr || appRepository_ == nullptr || providerManager_ == nullptr
        || chatController_ == nullptr || memory_ == nullptr) {
        return fail(makeError(AppErrorCode::NotReady, QStringLiteral("设置服务暂不可用"),
                              QStringLiteral("settings dependencies are unavailable"),
                              QStringLiteral("settings.apply")), error);
    }
    const ModelProviderConfig model = sourceModel.normalized();
    const PersonaConfig persona = sourcePersona.normalized();
    const MemoryLimits limits = sourceLimits.normalized();
    const UiConfig ui = sourceUi.normalized();
    QString technical;
    if (!model.validate(&technical) || !persona.validate(&technical) || !limits.validate(&technical)
        || !ui.validate(&technical)) {
        return fail(makeError(AppErrorCode::ConfigInvalid, QStringLiteral("设置内容校验失败"), technical,
                              QStringLiteral("settings.validate")), error);
    }
    if (model.providerType != QStringLiteral("mock") && apiKey.trimmed().isEmpty()) {
        QString storedKey;
        QString secretError;
        if (secretStore_ == nullptr
            || !secretStore_->read(model.credentialService, model.credentialAccount,
                                   &storedKey, &secretError)
            || storedKey.trimmed().isEmpty()) {
            return fail(makeError(AppErrorCode::ConfigInvalid,
                                  QStringLiteral("请先填写并保存模型 API Key"),
                                  secretError.isEmpty()
                                      ? QStringLiteral("model API key is not configured")
                                      : secretError,
                                  QStringLiteral("settings.validate_secret")), error);
        }
    }
    if (factory_ == nullptr || factory_->create(model, &technical, apiKey.trimmed()) == nullptr) {
        return fail(makeError(AppErrorCode::ConfigInvalid, QStringLiteral("无法创建候选模型"), technical,
                              QStringLiteral("settings.validate_provider")), error);
    }

    QByteArray oldModelFile;
    QByteArray oldAppFile;
    if (!modelRepository_->snapshot(&oldModelFile, &technical)
        || !appRepository_->snapshot(&oldAppFile, &technical)) {
        return fail(makeError(AppErrorCode::Io, QStringLiteral("无法读取旧配置，未进行修改"), technical,
                              QStringLiteral("settings.snapshot")), error);
    }
    const ModelProviderConfig oldModel = providerManager_->activeConfiguration();
    const PersonaConfig oldPersona = chatController_->personaConfig();
    const MemoryLimits oldLimits = memory_->limits();
    QString oldSecret;
    bool hadOldSecret = false;
    if (secretStore_ != nullptr && !model.credentialService.isEmpty()
        && !model.credentialAccount.isEmpty()) {
        hadOldSecret = secretStore_->read(model.credentialService, model.credentialAccount,
                                          &oldSecret, nullptr);
    }
    if (!apiKey.trimmed().isEmpty() && secretStore_ != nullptr
        && !model.credentialService.isEmpty() && !model.credentialAccount.isEmpty()
        && !secretStore_->write(model.credentialService, model.credentialAccount,
                                apiKey.trimmed(), &technical)) {
        return fail(makeError(AppErrorCode::Io, QStringLiteral("无法保存模型密钥"), technical,
                              QStringLiteral("settings.save_secret")), error);
    }

    if (!modelRepository_->saveProfile(model, true, &technical)
        || !appRepository_->save(persona, limits, &technical, &ui)) {
        modelRepository_->restore(oldModelFile, nullptr);
        appRepository_->restore(oldAppFile, nullptr);
        if (!apiKey.trimmed().isEmpty() && secretStore_ != nullptr) {
            if (hadOldSecret) secretStore_->write(model.credentialService, model.credentialAccount,
                                                   oldSecret, nullptr);
            else secretStore_->remove(model.credentialService, model.credentialAccount, nullptr);
        }
        return fail(makeError(AppErrorCode::Io, QStringLiteral("设置保存失败，已保留旧配置"), technical,
                              QStringLiteral("settings.persist")), error);
    }
    if (!providerManager_->switchProvider(model, &technical)
        || !chatController_->setPersonaConfig(persona, &technical)
        || !memory_->setLimits(limits, &technical)) {
        providerManager_->switchProvider(oldModel, nullptr);
        chatController_->setPersonaConfig(oldPersona, nullptr);
        memory_->setLimits(oldLimits, nullptr);
        modelRepository_->restore(oldModelFile, nullptr);
        appRepository_->restore(oldAppFile, nullptr);
        if (!apiKey.trimmed().isEmpty() && secretStore_ != nullptr) {
            if (hadOldSecret) secretStore_->write(model.credentialService, model.credentialAccount,
                                                   oldSecret, nullptr);
            else secretStore_->remove(model.credentialService, model.credentialAccount, nullptr);
        }
        return fail(makeError(AppErrorCode::ConfigInvalid, QStringLiteral("无法应用运行时设置，已恢复旧配置"),
                              technical, QStringLiteral("settings.apply_runtime")), error);
    }
    uiConfig_ = ui;
    emit uiConfigurationChanged(uiConfig_);
    emit settingsApplied(model, persona, limits);
    return true;
}

} // namespace zhu_screen_pet
