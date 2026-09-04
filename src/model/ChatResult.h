#pragma once

#include <QString>
#include <QMetaType>

#include <utility>

#include "model/ModelError.h"

namespace zhu_screen_pet {

/** 一次聊天请求的统一结果。 */
struct ChatResult
{
    bool succeeded = false;
    QString content;
    ModelError error;

    /** 创建成功结果。 */
    static ChatResult success(QString response)
    {
        ChatResult result;
        result.succeeded = true;
        result.content = std::move(response);
        return result;
    }

    /** 创建失败结果。 */
    static ChatResult failure(ModelError modelError)
    {
        ChatResult result;
        result.error = std::move(modelError);
        return result;
    }
};

} // namespace zhu_screen_pet

Q_DECLARE_METATYPE(zhu_screen_pet::ChatResult)
