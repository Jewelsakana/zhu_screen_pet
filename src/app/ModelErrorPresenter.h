#pragma once

#include <QString>
#include <QHash>

#include "model/ModelError.h"

namespace zhu_screen_pet {

/** 将模型层稳定错误码转换为适合直接展示给用户的中文提示。 */
class ModelErrorPresenter final
{
public:
    /** 使用配置文件提供的错误文案创建 Presenter。 */
    explicit ModelErrorPresenter(QHash<QString, QString> messages = {});
    /** 将错误码映射为当前配置中的用户提示；缺失时返回底层错误文本。 */
    QString message(const ModelError& error) const;

private:
    QHash<QString, QString> messages_;
};

} // namespace zhu_screen_pet
