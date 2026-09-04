#pragma once

#include <QSettings>
#include <QString>

namespace zhu_screen_pet {

/** 基于 QSettings 的本地配置仓库。 */
class SettingsRepository final
{
public:
    /** 绑定一个 INI 配置文件路径。 */
    explicit SettingsRepository(QString filePath);

    /** 同步读取配置并检查 QSettings 状态。 */
    bool load(QString* errorMessage = nullptr);
    /** 将当前配置同步写入磁盘。 */
    bool save(QString* errorMessage = nullptr);
    /** 读取配置值；键不存在时返回 defaultValue。 */
    QVariant value(const QString& key, const QVariant& defaultValue = {}) const;
    /** 修改配置值；修改后需要调用 save() 才会显式同步到磁盘。 */
    void setValue(const QString& key, const QVariant& value);

private:
    QSettings settings_;
};

} // namespace zhu_screen_pet
