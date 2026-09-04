#include "app/ApplicationBootstrapper.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QMessageBox>

#include "app/ChatController.h"
#include "app/ConversationController.h"
#include "app/LegacyDataMigrator.h"
#include "app/PersonaConfig.h"
#include "app/SettingsController.h"
#include "app/AppConfigRepository.h"
#include "infrastructure/Database.h"
#include "infrastructure/HttpClient.h"
#include "model/ModelConfigRepository.h"
#include "infrastructure/SecretStore.h"
#include "infrastructure/SettingsRepository.h"
#include "infrastructure/TrayController.h"
#include "infrastructure/WindowManager.h"
#include "memory/MemoryOrchestrator.h"
#include "memory/SqliteConversationRepository.h"
#include "memory/SqliteMemoryRepository.h"
#include "model/ChatProviderFactory.h"
#include "model/ProviderManager.h"
#include "ui/MainWindow.h"

namespace zhu_screen_pet {

namespace {
QString resolveConfiguredAssetPath(const QString& configuredPath)
{
    const QString path = configuredPath.trimmed();
    if (path.isEmpty() || QDir::isAbsolutePath(path)) return path;
    return QDir(QCoreApplication::applicationDirPath()).filePath(path);
}
}

ApplicationBootstrapper::ApplicationBootstrapper(QApplication* application, QObject* parent)
    : QObject(parent), application_(application), errorCenter_(&logger_, this)
{
}

ApplicationBootstrapper::~ApplicationBootstrapper() = default;

bool ApplicationBootstrapper::fail(AppError error, AppError* output)
{
    if (error.domain == ErrorDomain::None) error.domain = ErrorDomain::Application;
    errorCenter_.report(error);
    if (output) *output = error;
    QMessageBox::critical(nullptr, QStringLiteral("小珠看着你"), errorCenter_.userMessage(error));
    return false;
}

bool ApplicationBootstrapper::ensureConfigFile(const QString& path, const QString& shippedName,
                                               AppError* error)
{
    if (QFile::exists(path)) return true;
    const QString shipped = QDir(QCoreApplication::applicationDirPath()).filePath(
        QStringLiteral("config/") + shippedName);
    if (QFile::copy(shipped, path)) return true;
    return fail({AppErrorCode::Io, QStringLiteral("无法创建应用配置文件"), 0,
                 ErrorDomain::Configuration,
                 QStringLiteral("cannot copy %1 to %2").arg(shipped, path),
                 QStringLiteral("bootstrap.copy_config"), {}, false}, error);
}

bool ApplicationBootstrapper::initialize(AppError* error)
{
    if (initialized_) return true;
    if (!paths_.initialize()) return fail({AppErrorCode::Io, QStringLiteral("无法创建应用数据目录"), 0,
        ErrorDomain::Infrastructure, QStringLiteral("app paths initialization failed"),
        QStringLiteral("bootstrap.paths"), {}, false}, error);
    bool foundLegacyData = false;
    QString technical;
    if (!LegacyDataMigrator::migrateFiles(paths_,
            LegacyDataMigrator::defaultLegacyRootDirectory(), &foundLegacyData, &technical)) {
        return fail({AppErrorCode::Io, QStringLiteral("无法迁移旧版应用数据"), 0,
            ErrorDomain::Infrastructure, technical,
            QStringLiteral("bootstrap.migrate_files"), {}, false}, error);
    }
    if (!logger_.initialize(paths_.logDirectory(), &technical)) return fail({AppErrorCode::Io,
        QStringLiteral("无法初始化日志服务"), 0, ErrorDomain::Infrastructure, technical,
        QStringLiteral("bootstrap.logger"), {}, false}, error);

    settings_ = std::make_unique<SettingsRepository>(paths_.settingsPath());
    if (!settings_->load(&technical)) return fail({AppErrorCode::Io, QStringLiteral("无法读取应用设置"), 0,
        ErrorDomain::Configuration, technical, QStringLiteral("bootstrap.settings"), {}, false}, error);
    database_ = std::make_unique<Database>();
    if (!database_->open(paths_.databasePath(), &technical)) return fail({AppErrorCode::DatabaseUnavailable,
        QStringLiteral("无法打开本地数据库"), 0, ErrorDomain::Database, technical,
        QStringLiteral("bootstrap.database"), {}, false}, error);

    conversations_ = std::make_unique<SqliteConversationRepository>(database_.get());
    memories_ = std::make_unique<SqliteMemoryRepository>(database_.get());
    memoryOrchestrator_ = std::make_unique<MemoryOrchestrator>(conversations_.get(), memories_.get());
    httpClient_ = std::make_unique<HttpClient>();
    secretStore_ = std::make_unique<SecretStore>();

    QString modelConfigPath = qEnvironmentVariable("ZHU_SCREEN_PET_MODEL_CONFIG").trimmed();
    if (modelConfigPath.isEmpty()) modelConfigPath = paths_.modelConfigPath();
    if (!ensureConfigFile(modelConfigPath, QStringLiteral("model-providers.json"), error)) return false;
    modelConfigs_ = std::make_unique<ModelConfigRepository>(modelConfigPath);
    bool credentialMigrationSucceeded = true;
    if (foundLegacyData
        && !LegacyDataMigrator::migrateCredentials(modelConfigs_.get(), secretStore_.get(),
                                                   &technical)) {
        credentialMigrationSucceeded = false;
        logger_.warning(QStringLiteral("migration"), QStringLiteral("credential_migration_failed"),
                        technical, QStringLiteral("CREDENTIAL_MIGRATION"));
    }
    ModelProviderConfig modelConfig;
    if (!modelConfigs_->loadActive(&modelConfig, &technical)) return fail({AppErrorCode::ConfigInvalid,
        QStringLiteral("模型配置无效"), 0, ErrorDomain::Configuration, technical,
        QStringLiteral("bootstrap.model_config"), {}, false}, error);
    providerFactory_ = std::make_unique<ChatProviderFactory>(httpClient_.get(), secretStore_.get(), &logger_);
    providerManager_ = std::make_unique<ProviderManager>(providerFactory_.get());
    if (!providerManager_->switchProvider(modelConfig, &technical)) return fail({AppErrorCode::ConfigInvalid,
        QStringLiteral("无法启用模型"), 0, ErrorDomain::Model, technical,
        QStringLiteral("bootstrap.provider"), {}, false}, error);

    chatController_ = std::make_unique<ChatController>(providerManager_.get(), memoryOrchestrator_.get());
    PersonaConfig persona;
    QString appConfigPath = qEnvironmentVariable("ZHU_SCREEN_PET_APP_CONFIG").trimmed();
    if (appConfigPath.isEmpty()) appConfigPath = paths_.appConfigPath();
    if (!ensureConfigFile(appConfigPath, QStringLiteral("app-settings.json"), error)) return false;
    appConfigRepository_ = std::make_unique<AppConfigRepository>(appConfigPath);
    MemoryLimits limits;
    UiConfig uiConfig;
    if (!appConfigRepository_->load(&persona, &modelErrorMessages_, &technical, &limits, &uiConfig)) return fail({AppErrorCode::ConfigInvalid,
        QStringLiteral("应用配置无效"), 0, ErrorDomain::Configuration, technical,
        QStringLiteral("bootstrap.app_config"), {}, false}, error);
    if (!memoryOrchestrator_->setLimits(limits, &technical)) return fail({AppErrorCode::ConfigInvalid,
        QStringLiteral("记忆配置无效"), 0, ErrorDomain::Memory, technical,
        QStringLiteral("bootstrap.memory_config"), {}, false}, error);
    if (!chatController_->setPersonaConfig(persona, &technical)) return fail({AppErrorCode::ConfigInvalid,
        QStringLiteral("人格配置无效"), 0, ErrorDomain::Configuration, technical,
        QStringLiteral("bootstrap.persona"), {}, false}, error);
    const QString configuredIconPath = resolveConfiguredAssetPath(uiConfig.appIconPath);
    if (!configuredIconPath.isEmpty()) {
        const QIcon configuredIcon(configuredIconPath);
        if (!configuredIcon.isNull()) application_->setWindowIcon(configuredIcon);
        else logger_.warning(QStringLiteral("ui"), QStringLiteral("app_icon_load_failed"),
                             configuredIconPath, QStringLiteral("UI_ASSET"));
    }
    errorCenter_.setMessages(modelErrorMessages_);
    connect(chatController_.get(), &ChatController::operationFailed,
            &errorCenter_, &ErrorCenter::report);
    connect(chatController_.get(), &ChatController::requestFailed, &errorCenter_,
            [this](const QString&, const ModelError& modelError) { errorCenter_.report(modelError); });

    AppError errorValue;
    conversationController_ = std::make_unique<ConversationController>(conversations_.get(), settings_.get());
    conversationController_->setChatController(chatController_.get());
    if (!conversationController_->initialize(&errorValue)) return fail(errorValue, error);
    connect(conversationController_.get(), &ConversationController::operationFailed,
            &errorCenter_, &ErrorCenter::report);
    settingsController_ = std::make_unique<SettingsController>(
        modelConfigs_.get(), appConfigRepository_.get(), providerFactory_.get(), providerManager_.get(),
        chatController_.get(), memoryOrchestrator_.get(), secretStore_.get(), &errorCenter_);
    settingsController_->setInitialUiConfig(uiConfig);
    connect(settingsController_.get(), &SettingsController::operationFailed,
            &errorCenter_, &ErrorCenter::report);

    window_ = std::make_unique<MainWindow>();
    window_->setModelErrorMessages(modelErrorMessages_);
    window_->setErrorCenter(&errorCenter_);
    window_->setChatController(chatController_.get());
    window_->setConversationController(conversationController_.get());
    window_->setSettingsController(settingsController_.get());
    windowManager_ = std::make_unique<WindowManager>(settings_.get());
    windowManager_->restore(window_.get());
    trayController_ = std::make_unique<TrayController>();
    trayController_->initialize(window_.get(), application_->windowIcon());
    connect(window_.get(), &MainWindow::applicationExitRequested,
            this, &ApplicationBootstrapper::requestApplicationQuit);
    connect(trayController_.get(), &TrayController::showRequested, window_.get(), [this]() {
        window_->showPetShell();
    });
    connect(trayController_.get(), &TrayController::settingsRequested, window_.get(), [this]() {
        window_->showPetShell();
        window_->openSettings();
    });
    connect(trayController_.get(), &TrayController::conversationsRequested,
            window_.get(), [this]() {
                window_->showPetShell();
                window_->openConversationWindow();
            });
    connect(trayController_.get(), &TrayController::quitRequested,
            this, &ApplicationBootstrapper::requestApplicationQuit);
    connect(application_, &QApplication::aboutToQuit,
             this, &ApplicationBootstrapper::persistApplicationState);
    if (foundLegacyData && credentialMigrationSucceeded
        && !LegacyDataMigrator::markCompleted(paths_, &technical)) {
        logger_.warning(QStringLiteral("migration"), QStringLiteral("marker_write_failed"),
                        technical, QStringLiteral("CONFIG_SAVE"));
    }
    initialized_ = true;
    return true;
}

int ApplicationBootstrapper::run()
{
    if (!initialized_) return 1;
    window_->showPetShell();
    trayController_->show();
    return application_->exec();
}

void ApplicationBootstrapper::requestApplicationQuit()
{
    if (quitRequested_) return;
    quitRequested_ = true;
    persistApplicationState();
    if (trayController_ != nullptr) trayController_->hide();
    application_->quit();
}

void ApplicationBootstrapper::persistApplicationState()
{
    if (statePersisted_) return;
    statePersisted_ = true;
    if (windowManager_ == nullptr || window_ == nullptr) return;
    QString saveError;
    if (!windowManager_->save(window_.get(), &saveError) && !saveError.isEmpty()) {
        logger_.error(QStringLiteral("window"), QStringLiteral("position_save_failed"), saveError,
                      QStringLiteral("CONFIG_SAVE"));
    }
}

} // namespace zhu_screen_pet
