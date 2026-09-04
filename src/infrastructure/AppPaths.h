#pragma once

#include <QString>

namespace zhu_screen_pet {

/** 统一管理应用数据目录及其子目录，避免各模块自行拼接路径。 */
class AppPaths final
{
public:
    /** 使用指定根目录；为空时使用系统推荐的应用数据目录。 */
    explicit AppPaths(QString rootDirectory = {});

    /** 创建应用所需目录；失败时返回 false 并写入错误信息。 */
    bool initialize(QString* errorMessage = nullptr);

    /** 返回应用数据根目录。 */
    QString rootDirectory() const;
    /** 返回配置文件目录。 */
    QString configDirectory() const;
    /** 返回数据库目录。 */
    QString databaseDirectory() const;
    /** 返回 SQLite 数据库文件路径。 */
    QString databasePath() const;
    /** 返回设置文件路径。 */
    QString settingsPath() const;
    /** 返回用户可编辑的模型 Provider JSON 配置路径。 */
    QString modelConfigPath() const;
    /** 返回人格和用户提示 JSON 配置路径。 */
    QString appConfigPath() const;
    /** 返回日志目录。 */
    QString logDirectory() const;
    /** 返回截图目录。 */
    QString captureDirectory() const;

private:
    QString rootDirectory_;
};

} // namespace zhu_screen_pet
