#pragma once

#include <QString>

namespace zhu_screen_pet {

class AppPaths;
class ModelConfigRepository;
class SecretStore;

/** 将 ScreenPet 版本的本地数据和凭据安全复制到重命名后的应用位置。 */
class LegacyDataMigrator final
{
public:
    /** 返回旧版 ScreenPet/Screen Pet 的系统应用数据目录。 */
    static QString defaultLegacyRootDirectory();

    /**
     * 复制目标端尚不存在的旧配置、数据库、日志和截图。
     * 不覆盖或删除任何文件；foundLegacy 用于决定初始化成功后是否写完成标记。
     */
    static bool migrateFiles(const AppPaths& paths, const QString& legacyRootDirectory,
                             bool* foundLegacy = nullptr,
                             QString* errorMessage = nullptr);

    /** 将旧 service 下的远程 Provider 密钥复制到新 service，且不覆盖新密钥。 */
    static bool migrateCredentials(ModelConfigRepository* models, SecretStore* secrets,
                                   QString* errorMessage = nullptr);

    /** 所有新位置数据均已成功打开后，原子写入幂等完成标记。 */
    static bool markCompleted(const AppPaths& paths, QString* errorMessage = nullptr);
};

} // namespace zhu_screen_pet
