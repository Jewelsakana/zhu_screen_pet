#pragma once

#include <memory>

#include <QObject>

#include "app/ErrorCenter.h"
#include "infrastructure/AppPaths.h"
#include "infrastructure/Logger.h"

class QApplication;

namespace zhu_screen_pet {

class SettingsRepository;
class AppConfigRepository;
class Database;
class HttpClient;
class SecretStore;
class ModelConfigRepository;
class ChatProviderFactory;
class ProviderManager;
class SqliteConversationRepository;
class SqliteMemoryRepository;
class MemoryOrchestrator;
class ChatController;
class ConversationController;
class SettingsController;
class MainWindow;
class WindowManager;
class TrayController;

/** 应用启动编排器：集中初始化基础设施并装配四层依赖，避免 main.cpp 变成业务入口。 */
class ApplicationBootstrapper final : public QObject
{
    Q_OBJECT

public:
    explicit ApplicationBootstrapper(QApplication* application, QObject* parent = nullptr);
    ~ApplicationBootstrapper() override;

    /** 初始化配置、数据库、Provider、会话和窗口；失败时返回结构化错误。 */
    bool initialize(AppError* error = nullptr);
    /** 显示窗口、托盘并进入 Qt 事件循环。 */
    int run();

private:
    bool fail(AppError error, AppError* output);
    bool ensureConfigFile(const QString& path, const QString& shippedName, AppError* error);
    /** 所有退出入口最终调用这里，保证退出动作与清理顺序一致。 */
    void requestApplicationQuit();
    /** 保存退出状态；使用幂等保护避免多个关闭入口重复写入。 */
    void persistApplicationState();

    QApplication* application_ = nullptr;
    AppPaths paths_;
    Logger logger_;
    ErrorCenter errorCenter_;
    std::unique_ptr<SettingsRepository> settings_;
    std::unique_ptr<AppConfigRepository> appConfigRepository_;
    std::unique_ptr<Database> database_;
    std::unique_ptr<SqliteConversationRepository> conversations_;
    std::unique_ptr<SqliteMemoryRepository> memories_;
    std::unique_ptr<MemoryOrchestrator> memoryOrchestrator_;
    std::unique_ptr<HttpClient> httpClient_;
    std::unique_ptr<SecretStore> secretStore_;
    std::unique_ptr<ModelConfigRepository> modelConfigs_;
    std::unique_ptr<ChatProviderFactory> providerFactory_;
    std::unique_ptr<ProviderManager> providerManager_;
    std::unique_ptr<ChatController> chatController_;
    std::unique_ptr<MainWindow> window_;
    std::unique_ptr<WindowManager> windowManager_;
    std::unique_ptr<TrayController> trayController_;
    QHash<QString, QString> modelErrorMessages_;
    std::unique_ptr<ConversationController> conversationController_;
    std::unique_ptr<SettingsController> settingsController_;
    bool initialized_ = false;
    bool quitRequested_ = false;
    bool statePersisted_ = false;
};

} // namespace zhu_screen_pet
