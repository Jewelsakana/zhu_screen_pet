#pragma once

#include <QString>
#include <QMetaType>

#include "infrastructure/ScreenCapturePolicy.h"

namespace zhu_screen_pet {

/** 桌宠窗口、气泡和自动显隐所需的可持久化 UI 参数。 */
struct UiConfig
{
    static constexpr int MinimumWindowScalePercent = 50;
    static constexpr int MaximumWindowScalePercent = 200;
    static constexpr int MinimumScreenCaptureIntervalMs =
        ScreenCapturePolicy::MinimumAutomaticIntervalMs;
    static constexpr int MaximumScreenCaptureIntervalMs =
        ScreenCapturePolicy::MaximumAutomaticIntervalMs;
    static constexpr int RecommendedScreenCaptureIntervalMs =
        ScreenCapturePolicy::RecommendedAutomaticIntervalMs;

    int replyBubbleDurationMs = 15000;
    int hoverHideDelayMs = 600;
    int fadeDurationMs = 180;
    /** 主窗口及其附属窗口相对设计尺寸的统一缩放比例。 */
    int windowScalePercent = 100;
    QString appIconPath;
    QString petAvatarPath;
    QString conversationAvatarPath;
    /** 允许在用户明确触发时把屏幕图像发送给当前模型。 */
    bool screenCaptureEnabled = false;
    /** 定时捕获并主动发送屏幕图像；必须同时启用 screenCaptureEnabled。 */
    bool automaticScreenAnalysisEnabled = false;
    int screenCaptureIntervalMs = RecommendedScreenCaptureIntervalMs;
    /** 将截图附加到用户本次消息，而不是另起一次模型请求。 */
    bool captureOnChat = false;
    /** 默认保护隐私；关闭后允许桌宠及其附属窗口出现在截图中。 */
    bool excludeOwnWindowsFromCapture = true;
    QString captureImageFormat = QStringLiteral("jpeg");
    int captureMaxWidth = 1280;
    int captureQuality = 75;

    UiConfig normalized() const;
    bool validate(QString* errorMessage = nullptr) const;
};

} // namespace zhu_screen_pet

Q_DECLARE_METATYPE(zhu_screen_pet::UiConfig)
