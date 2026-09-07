#pragma once

#include <optional>

#include "core/Result.h"
#include "memory/ObservationTypes.h"

namespace zhu_screen_pet {

/** 屏幕观察的持久化接口；观察与普通会话消息使用不同存储边界。 */
class ObservationRepository
{
public:
    virtual ~ObservationRepository() = default;

    virtual Result<QString> saveResult(const ObservationEvent& observation) = 0;
    virtual Result<std::optional<ObservationEvent>> latestValidResult(
        const QString& conversationId, const QDateTime& at) const = 0;
    virtual Result<void> removeExpiredResult(const QDateTime& at) = 0;
};

} // namespace zhu_screen_pet
