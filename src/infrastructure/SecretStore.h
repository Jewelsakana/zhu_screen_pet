#pragma once

#include <QString>

namespace zhu_screen_pet {

/** 敏感凭据存储抽象；Windows 使用 Credential Manager。 */
class SecretStore final
{
public:
    /** 保存服务账号对应的密钥。 */
    bool write(const QString& service, const QString& account,
               const QString& secret, QString* errorMessage = nullptr) const;
    /** 读取服务账号对应的密钥。 */
    bool read(const QString& service, const QString& account,
              QString* secret, QString* errorMessage = nullptr) const;
    /** 删除服务账号对应的密钥。 */
    bool remove(const QString& service, const QString& account,
                QString* errorMessage = nullptr) const;
};

} // namespace zhu_screen_pet
