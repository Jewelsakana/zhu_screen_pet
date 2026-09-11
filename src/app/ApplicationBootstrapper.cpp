#include "app/ApplicationBootstrapper.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QMessageBox>
#include <QTimer>

#include "app/ChatController.h"
#include "app/AffectionController.h"
#include "app/ConversationController.h"
#include "app/LegacyDataMigrator.h"
#include "app/MemoryMaintenanceService.h"
#include "app/InputActivityController.h"
#include "app/PetEconomyController.h"
#include "app/PetLifecycleController.h"
#include "app/PersonaConfig.h"
#include "app/SatietyController.h"
#include "app/SettingsController.h"
#include "app/AppConfigRepository.h"
#include "infrastructure/AutoStartManager.h"
#include "infrastructure/Database.h"
#include "infrastructure/HttpClient.h"
#include "infrastructure/InputActivityMonitor.h"
#include "model/ModelConfigRepository.h"
#include "infrastructure/SecretStore.h"
#include "infrastructure/SettingsRepository.h"
#include "infrastructure/ShopCatalogRepository.h"
#include "infrastructure/TrayController.h"
#include "infrastructure/WindowManager.h"
#include "memory/MemoryOrchestrator.h"
#include "memory/SqliteConversationRepository.h"
#include "memory/SqliteMemoryRepository.h"
#include "memory/SqliteObservationRepository.h"
#include "model/ChatProviderFactory.h"
#include "model/ProviderManager.h"
#include "ui/FirstRunWizard.h"
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
    autoStartManager_ = std::make_unique<AutoStartManager>();
    inputActivityMonitor_ = std::make_unique<InputActivityMonitor>();
    inputActivity_ = std::make_unique<InputActivityController>(
        inputActivityMonitor_.get(), settings_.get(), this);
    if (!inputActivity_->initialize(&technical)) return fail({
        AppErrorCode::ConfigInvalid, QStringLiteral("无法读取输入统计"), 0,
        ErrorDomain::Configuration, technical,
        QStringLiteral("bootstrap.input_activity"), {}, false}, error);
    connect(inputActivity_.get(), &InputActivityController::persistenceFailed,
            this, [this](const QString& detail) {
                logger_.warning(QStringLiteral("activity"),
                                QStringLiteral("input_count_save_failed"), detail,
                                QStringLiteral("CONFIG_SAVE"));
            });
    affection_ = std::make_unique<AffectionController>(settings_.get(), this);
    if (!affection_->initialize(&technical)) return fail({
        AppErrorCode::ConfigInvalid, QStringLiteral("无法读取好感度进度"), 0,
        ErrorDomain::Configuration, technical,
        QStringLiteral("bootstrap.affection"), {}, false}, error);
    connect(affection_.get(), &AffectionController::persistenceFailed,
            this, [this](const QString& detail) {
                logger_.warning(QStringLiteral("affection"),
                                QStringLiteral("progress_save_failed"), detail,
                                QStringLiteral("CONFIG_SAVE"));
            });
    satiety_ = std::make_unique<SatietyController>(settings_.get(), this);
    if (!satiety_->initialize(&technical)) return fail({
        AppErrorCode::ConfigInvalid, QStringLiteral("无法读取饱食度"), 0,
        ErrorDomain::Configuration, technical,
        QStringLiteral("bootstrap.satiety"), {}, false}, error);
    connect(satiety_.get(), &SatietyController::persistenceFailed,
            this, [this](const QString& detail) {
                logger_.warning(QStringLiteral("satiety"),
                                QStringLiteral("satiety_save_failed"), detail,
                                QStringLiteral("CONFIG_SAVE"));
            });
    if (!ensureConfigFile(paths_.shopCatalogPath(),
                          QStringLiteral("shop-catalog.json"), error)) return false;
    shopCatalog_ = std::make_unique<ShopCatalogRepository>(paths_.shopCatalogPath());
    petEconomy_ = std::make_unique<PetEconomyController>(
        shopCatalog_.get(), settings_.get(), inputActivity_.get(), affection_.get(),
        satiety_.get(), this);
    if (!petEconomy_->initialize(&technical)) return fail({
        AppErrorCode::ConfigInvalid, QStringLiteral("商店配置或背包数据无效"), 0,
        ErrorDomain::Configuration, technical,
        QStringLiteral("bootstrap.pet_economy"), {}, false}, error);
    connect(petEconomy_.get(), &PetEconomyController::persistenceFailed,
            this, [this](const QString& detail) {
                logger_.warning(QStringLiteral("economy"),
                                QStringLiteral("inventory_save_failed"), detail,
                                QStringLiteral("CONFIG_SAVE"));
            });
    database_ = std::make_unique<Database>();
    if (!database_->open(paths_.databasePath(), &technical)) return fail({AppErrorCode::DatabaseUnavailable,
        QStringLiteral("无法打开本地数据库"), 0, ErrorDomain::Database, technical,
        QStringLiteral("bootstrap.database"), {}, false}, error);

    petLifecycle_ = std::make_unique<PetLifecycleController>(settings_.get(), this);
    if (!petLifecycle_->initialize(&technical)) return fail({
        AppErrorCode::ConfigInvalid, QStringLiteral("无法读取宠物等级进度"), 0,
        ErrorDomain::Configuration, technical,
        QStringLiteral("bootstrap.pet_lifecycle"), {}, false}, error);
    connect(petLifecycle_.get(), &PetLifecycleController::persistenceFailed,
            this, [this](const QString& detail) {
                logger_.warning(QStringLiteral("pet"), QStringLiteral("progress_save_failed"),
                                detail, QStringLiteral("CONFIG_SAVE"));
            });

    conversations_ = std::make_unique<SqliteConversationRepository>(database_.get());
    memories_ = std::make_unique<SqliteMemoryRepository>(database_.get());
    observations_ = std::make_unique<SqliteObservationRepository>(database_.get());
    memoryOrchestrator_ = std::make_unique<MemoryOrchestrator>(
        conversations_.get(), memories_.get(), observations_.get());
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
    memoryMaintenance_ = std::make_unique<MemoryMaintenanceService>(
        providerManager_.get(), memoryOrchestrator_.get(), this);
    connect(chatController_.get(), &ChatController::conversationTurnCompleted,
            memoryMaintenance_.get(), &MemoryMaintenanceService::schedule);
    connect(memoryMaintenance_.get(), &MemoryMaintenanceService::maintenanceFailed,
            this, [this](const QString& conversationId, const QString& detail) {
                logger_.warning(QStringLiteral("memory"), QStringLiteral("maintenance_retry_scheduled"),
                                QStringLiteral("%1: %2").arg(conversationId, detail),
                                QStringLiteral("MEMORY_MAINTENANCE"));
            });
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
    petLifecycle_->setUserAddress(persona.userAddress);
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
    memoryMaintenance_->schedule(conversationController_->currentConversationId());
    connect(conversationController_.get(), &ConversationController::operationFailed,
            &errorCenter_, &ErrorCenter::report);
    settingsController_ = std::make_unique<SettingsController>(
        modelConfigs_.get(), appConfigRepository_.get(), providerFactory_.get(), providerManager_.get(),
        chatController_.get(), memoryOrchestrator_.get(), secretStore_.get(), &errorCenter_,
        autoStartManager_.get());
    settingsController_->setInitialUiConfig(uiConfig);
    connect(settingsController_.get(), &SettingsController::settingsApplied,
            petLifecycle_.get(), [this](const ModelProviderConfig&, const PersonaConfig& updated,
                                        const MemoryLimits&) {
                petLifecycle_->setUserAddress(updated.userAddress);
            });
    connect(settingsController_.get(), &SettingsController::operationFailed,
            &errorCenter_, &ErrorCenter::report);
    connect(affection_.get(), &AffectionController::proactivityRewardUnlocked,
            settingsController_.get(), [this](int proactiveLevel) {
                AppError rewardError;
                if (!settingsController_->applyProactivityReward(proactiveLevel,
                                                                  &rewardError)) {
                    logger_.warning(QStringLiteral("affection"),
                                    QStringLiteral("proactivity_reward_failed"),
                                    rewardError.technicalMessage,
                                    QStringLiteral("CONFIG_SAVE"));
                }
            });

    logger_.info(QStringLiteral("bootstrap"), QStringLiteral("main_window_create_started"),
                 QStringLiteral("creating main and attached windows"));
    window_ = std::make_unique<MainWindow>(nullptr, &logger_);
    logger_.info(QStringLiteral("bootstrap"), QStringLiteral("main_window_create_completed"),
                 QStringLiteral("main and attached windows created"));
    window_->setCaptureDirectory(paths_.captureDirectory());
    window_->setModelErrorMessages(modelErrorMessages_);
    window_->setErrorCenter(&errorCenter_);
    window_->setChatController(chatController_.get());
    window_->setConversationController(conversationController_.get());
    window_->setSettingsController(settingsController_.get());
    window_->setPetLifecycleController(petLifecycle_.get());
    window_->setAffectionController(affection_.get());
    window_->setSatietyController(satiety_.get());
    window_->setPetEconomyController(petEconomy_.get());
    window_->setInputActivityController(inputActivity_.get());
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
    logger_.info(QStringLiteral("bootstrap"), QStringLiteral("initialized"),
                 QStringLiteral("application services initialized"));
    return true;
}

int ApplicationBootstrapper::run()
{
    if (!initialized_) return 1;
    QString activityError;
    if (!inputActivity_->start(&activityError)) {
        logger_.warning(QStringLiteral("activity"),
                        QStringLiteral("global_input_monitor_unavailable"), activityError,
                        QStringLiteral("INPUT_MONITOR"));
    } else {
        logger_.info(QStringLiteral("activity"),
                     QStringLiteral("global_input_monitor_started"),
                     QStringLiteral("keyboard and mouse click monitoring started"));
    }
    if (!settings_->value(QStringLiteral("onboarding/completed"), false).toBool()) {
        logger_.info(QStringLiteral("bootstrap"), QStringLiteral("onboarding_started"),
                     QStringLiteral("showing first-run wizard"));
        FirstRunWizard wizard(settingsController_.get());
        if (wizard.exec() == QDialog::Accepted) {
            settings_->setValue(QStringLiteral("onboarding/completed"), true);
            QString saveError;
            if (!settings_->save(&saveError)) {
                logger_.warning(QStringLiteral("onboarding"),
                                QStringLiteral("completion_marker_save_failed"), saveError,
                                QStringLiteral("CONFIG_SAVE"));
                QMessageBox::warning(nullptr, QStringLiteral("小珠看着你"),
                                     QStringLiteral("首次启动配置已应用，但完成标记保存失败，"
                                                    "下次启动可能再次显示向导。"));
            }
        }
    }
    logger_.info(QStringLiteral("bootstrap"), QStringLiteral("pet_shell_show_started"),
                 QStringLiteral("showing main pet shell"));
    window_->showPetShell();
    trayController_->show();
    logger_.info(QStringLiteral("bootstrap"), QStringLiteral("event_loop_started"),
                 QStringLiteral("entering application event loop"));
    // 首次创建透明气泡必须等 Windows/Qt 完成顶层窗口的样式和透明合成，
    // 否则启动问候的第一帧可能短暂显示成未裁剪的方形背景。
    QTimer::singleShot(0, this, [this]() {
        if (quitRequested_) return;
        if (petLifecycle_ != nullptr) petLifecycle_->start();
        if (satiety_ != nullptr) satiety_->start();
    });
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
    QString saveError;
    if (petEconomy_ != nullptr && !petEconomy_->shutdown(&saveError)
        && !saveError.isEmpty()) {
        logger_.error(QStringLiteral("economy"), QStringLiteral("inventory_save_failed"),
                      saveError, QStringLiteral("CONFIG_SAVE"));
    }
    saveError.clear();
    if (satiety_ != nullptr && !satiety_->shutdown(&saveError)
        && !saveError.isEmpty()) {
        logger_.error(QStringLiteral("satiety"), QStringLiteral("satiety_save_failed"),
                      saveError, QStringLiteral("CONFIG_SAVE"));
    }
    saveError.clear();
    if (petLifecycle_ != nullptr && !petLifecycle_->shutdown(&saveError)
        && !saveError.isEmpty()) {
        logger_.error(QStringLiteral("pet"), QStringLiteral("progress_save_failed"), saveError,
                      QStringLiteral("CONFIG_SAVE"));
    }
    saveError.clear();
    if (affection_ != nullptr && !affection_->shutdown(&saveError)
        && !saveError.isEmpty()) {
        logger_.error(QStringLiteral("affection"), QStringLiteral("progress_save_failed"),
                      saveError, QStringLiteral("CONFIG_SAVE"));
    }
    saveError.clear();
    if (inputActivity_ != nullptr && !inputActivity_->shutdown(&saveError)
        && !saveError.isEmpty()) {
        logger_.error(QStringLiteral("activity"), QStringLiteral("input_count_save_failed"),
                      saveError, QStringLiteral("CONFIG_SAVE"));
    }
    saveError.clear();
    if (windowManager_ != nullptr && window_ != nullptr
        && !windowManager_->save(window_.get(), &saveError) && !saveError.isEmpty()) {
        logger_.error(QStringLiteral("window"), QStringLiteral("position_save_failed"), saveError,
                      QStringLiteral("CONFIG_SAVE"));
    }
}

} // namespace zhu_screen_pet
