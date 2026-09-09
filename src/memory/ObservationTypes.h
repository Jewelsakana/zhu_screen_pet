#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QSize>

namespace zhu_screen_pet {

/** 一次成功的屏幕观察；只保存文字摘要和不可逆指纹，不保存截图或 Base64。 */
struct ObservationEvent
{
    static constexpr int DefaultTtlSeconds = 600;

    QString id;
    QString captureId;
    QString conversationId;
    QString summary;
    QByteArray fingerprint;
    QString source;
    QString appHint;
    QString modelRequestId;
    QString modelProvider;
    QString imageFormat;
    QSize imageSize;
    int durationMs = 0;
    QDateTime capturedAt;
    QDateTime expiresAt;
};

} // namespace zhu_screen_pet
