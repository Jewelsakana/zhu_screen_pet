#include "app/UiConfig.h"

#include <QtGlobal>

namespace zhu_screen_pet {

UiConfig UiConfig::normalized() const
{
    UiConfig result = *this;
    result.appIconPath = result.appIconPath.trimmed();
    result.petAvatarPath = result.petAvatarPath.trimmed();
    result.conversationAvatarPath = result.conversationAvatarPath.trimmed();
    result.captureImageFormat = result.captureImageFormat.trimmed().toLower();
    if (result.captureImageFormat == QStringLiteral("jpg")) {
        result.captureImageFormat = QStringLiteral("jpeg");
    }
    result.replyBubbleDurationMs = qBound(1000, result.replyBubbleDurationMs, 300000);
    result.hoverHideDelayMs = qBound(100, result.hoverHideDelayMs, 5000);
    result.fadeDurationMs = qBound(0, result.fadeDurationMs, 2000);
    result.windowScalePercent = qBound(MinimumWindowScalePercent,
                                       result.windowScalePercent,
                                       MaximumWindowScalePercent);
    result.screenCaptureIntervalMs = qBound(MinimumScreenCaptureIntervalMs,
                                             result.screenCaptureIntervalMs,
                                             MaximumScreenCaptureIntervalMs);
    return result;
}

bool UiConfig::validate(QString* errorMessage) const
{
    if (replyBubbleDurationMs < 1000 || replyBubbleDurationMs > 300000
        || hoverHideDelayMs < 100 || hoverHideDelayMs > 5000
        || fadeDurationMs < 0 || fadeDurationMs > 2000
        || windowScalePercent < MinimumWindowScalePercent
        || windowScalePercent > MaximumWindowScalePercent) {
        if (errorMessage) *errorMessage = QStringLiteral("UI timing configuration is out of range");
        return false;
    }
    if (screenCaptureIntervalMs < MinimumScreenCaptureIntervalMs
        || screenCaptureIntervalMs > MaximumScreenCaptureIntervalMs
        || captureMaxWidth < 1 || captureMaxWidth > 8192
        || captureQuality < 1 || captureQuality > 100) {
        if (errorMessage) *errorMessage = QStringLiteral("截图配置超出范围");
        return false;
    }
    if (captureImageFormat != QStringLiteral("jpeg")
        && captureImageFormat != QStringLiteral("webp")) {
        if (errorMessage) *errorMessage = QStringLiteral("不支持的截图格式");
        return false;
    }
    if (!screenCaptureEnabled
        && (automaticScreenAnalysisEnabled || captureOnChat)) {
        if (errorMessage) *errorMessage = QStringLiteral(
            "启用自动屏幕分析或随消息附图前，必须先允许向模型发送截图");
        return false;
    }
    return true;
}

} // namespace zhu_screen_pet
