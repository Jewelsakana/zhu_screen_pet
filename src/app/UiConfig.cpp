#include "app/UiConfig.h"

#include <QtGlobal>

namespace zhu_screen_pet {

UiConfig UiConfig::normalized() const
{
    UiConfig result = *this;
    result.appIconPath = result.appIconPath.trimmed();
    result.petAvatarPath = result.petAvatarPath.trimmed();
    result.conversationAvatarPath = result.conversationAvatarPath.trimmed();
    result.replyBubbleDurationMs = qBound(1000, result.replyBubbleDurationMs, 300000);
    result.hoverHideDelayMs = qBound(100, result.hoverHideDelayMs, 5000);
    result.fadeDurationMs = qBound(0, result.fadeDurationMs, 2000);
    return result;
}

bool UiConfig::validate(QString* errorMessage) const
{
    if (replyBubbleDurationMs < 1000 || replyBubbleDurationMs > 300000
        || hoverHideDelayMs < 100 || hoverHideDelayMs > 5000
        || fadeDurationMs < 0 || fadeDurationMs > 2000) {
        if (errorMessage) *errorMessage = QStringLiteral("UI timing configuration is out of range");
        return false;
    }
    return true;
}

} // namespace zhu_screen_pet
