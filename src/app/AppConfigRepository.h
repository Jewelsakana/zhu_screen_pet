#pragma once

#include <QHash>
#include <QByteArray>
#include <QString>

#include "app/PersonaConfig.h"
#include "app/UiConfig.h"
#include "memory/MemoryContext.h"

namespace zhu_screen_pet {

/** 从独立 JSON 文件读取人格配置和用户可见文案。 */
class AppConfigRepository final
{
public:
    /** 绑定磁盘上的应用配置文件。 */
    explicit AppConfigRepository(QString filePath);

    /** 一次性读取并严格校验人格配置和全部模型错误提示。 */
    bool load(PersonaConfig* persona, QHash<QString, QString>* errorMessages,
              QString* errorMessage = nullptr,
              MemoryLimits* memoryLimits = nullptr,
              UiConfig* uiConfig = nullptr) const;

    /** 原子保存人格和记忆限制；错误提示文案等只读配置保持原样。 */
    bool save(const PersonaConfig& persona, const MemoryLimits& memoryLimits,
              QString* errorMessage = nullptr,
              const UiConfig* uiConfig = nullptr) const;
    /** 获取/恢复完整文件快照，确保隐藏错误文案等字段也能原样回滚。 */
    bool snapshot(QByteArray* data, QString* errorMessage = nullptr) const;
    bool restore(const QByteArray& data, QString* errorMessage = nullptr) const;

private:
    QString filePath_;
};

} // namespace zhu_screen_pet
