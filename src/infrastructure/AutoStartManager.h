#pragma once

#include <QString>

namespace zhu_screen_pet {

/** 管理当前 Windows 用户的登录启动项；注册表是该设置的唯一事实来源。 */
class AutoStartManager final
{
public:
    explicit AutoStartManager(QString executablePath = {},
                              QString registryPath = {},
                              QString valueName = {});

    bool isSupported() const;
    bool isEnabled() const;
    bool setEnabled(bool enabled, QString* errorMessage = nullptr);

    /** 生成 Windows Run 注册表所需的、带引号的可执行文件命令。 */
    static QString startupCommand(const QString& executablePath);

private:
    QString executablePath_;
    QString registryPath_;
    QString valueName_;
};

} // namespace zhu_screen_pet
