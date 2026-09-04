#pragma once

#include <QString>
#include <QMetaType>

namespace zhu_screen_pet {

/** 桌宠窗口、气泡和自动显隐所需的可持久化 UI 参数。 */
struct UiConfig
{
    int replyBubbleDurationMs = 15000;
    int hoverHideDelayMs = 600;
    int fadeDurationMs = 180;
    QString appIconPath;
    QString petAvatarPath;
    QString conversationAvatarPath;
    bool screenCaptureEnabled = false;
    int screenCaptureIntervalMs = 5000;
    bool captureOnChat = false;
    QString captureImageFormat = QStringLiteral("jpeg");
    int captureMaxWidth = 1280;
    int captureQuality = 75;

    UiConfig normalized() const;
    bool validate(QString* errorMessage = nullptr) const;
};

} // namespace zhu_screen_pet

Q_DECLARE_METATYPE(zhu_screen_pet::UiConfig)
