#pragma once

#include <QMetaType>

namespace zhu_screen_pet {

/** 桌宠在一次聊天交互中的可见状态。 */
enum class PetState
{
    Idle,
    Thinking,
    Speaking,
    Error
};

} // namespace zhu_screen_pet

Q_DECLARE_METATYPE(zhu_screen_pet::PetState)

