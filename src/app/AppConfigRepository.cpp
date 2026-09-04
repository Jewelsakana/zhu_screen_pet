#include "app/AppConfigRepository.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonArray>
#include <QSaveFile>
#include <QStringList>

#include <utility>

namespace zhu_screen_pet {

AppConfigRepository::AppConfigRepository(QString filePath)
    : filePath_(std::move(filePath))
{
}

bool AppConfigRepository::load(PersonaConfig* persona,
                               QHash<QString, QString>* errorMessages,
                               QString* errorMessage,
                               MemoryLimits* memoryLimits,
                               UiConfig* uiConfig) const
{
    const auto fail = [errorMessage](const QString& message) {
        if (errorMessage != nullptr) *errorMessage = message;
        return false;
    };
    if (persona == nullptr || errorMessages == nullptr || filePath_.isEmpty()) {
        return fail(QStringLiteral("application configuration output or path is invalid"));
    }

    QFile file(filePath_);
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(QStringLiteral("cannot open application configuration: %1")
                        .arg(file.errorString()));
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return fail(QStringLiteral("invalid application configuration JSON: %1")
                        .arg(parseError.errorString()));
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt() != 1) {
        return fail(QStringLiteral("unsupported application configuration schema"));
    }

    const QJsonObject personaObject = root.value(QStringLiteral("persona")).toObject();
    const QJsonObject instructions = personaObject
        .value(QStringLiteral("proactivity_instructions")).toObject();
    PersonaConfig loadedPersona;
    loadedPersona.name = personaObject.value(QStringLiteral("name")).toString();
    loadedPersona.userAddress = personaObject.value(QStringLiteral("user_address"))
        .toString(loadedPersona.userAddress);
    loadedPersona.tone = personaObject.value(QStringLiteral("tone")).toString();
    loadedPersona.maxReplyTokens = personaObject.value(QStringLiteral("max_reply_tokens")).toInt();
    loadedPersona.proactiveLevel = personaObject.value(QStringLiteral("proactive_level")).toInt(-1);
    loadedPersona.systemPromptTemplate = personaObject
        .value(QStringLiteral("system_prompt_template")).toString();
    for (int level = 0; level <= 3; ++level) {
        loadedPersona.proactivityInstructions.append(
            instructions.value(QString::number(level)).toString());
    }
    if (!loadedPersona.validate(errorMessage)) return false;

    MemoryLimits loadedLimits;
    const QJsonObject memoryObject = root.value(QStringLiteral("memory")).toObject();
    loadedLimits.recentMessageLimit = memoryObject.value(
        QStringLiteral("recent_message_limit")).toInt();
    loadedLimits.relevantHistoryLimit = memoryObject.value(
        QStringLiteral("relevant_history_limit")).toInt(-1);
    loadedLimits.longTermMemoryLimit = memoryObject.value(
        QStringLiteral("long_term_memory_limit")).toInt(-1);
    loadedLimits.maxContextTokens = memoryObject.value(
        QStringLiteral("max_context_tokens")).toInt();
    if (!loadedLimits.validate(errorMessage)) return false;

    const QStringList requiredErrorCodes = {
        QStringLiteral("none"), QStringLiteral("authentication"),
        QStringLiteral("timeout"), QStringLiteral("rate_limit"),
        QStringLiteral("network"), QStringLiteral("cancelled"),
        QStringLiteral("invalid_request"), QStringLiteral("invalid_response"),
        QStringLiteral("unknown")
    };
    const QJsonObject messagesObject = root.value(QStringLiteral("model_error_messages")).toObject();
    QHash<QString, QString> loadedMessages;
    for (const QString& code : requiredErrorCodes) {
        const QString message = messagesObject.value(code).toString().trimmed();
        if (message.isEmpty()) {
            return fail(QStringLiteral("model error message is missing: %1").arg(code));
        }
        loadedMessages.insert(code, message);
    }

    *persona = loadedPersona.normalized();
    *errorMessages = loadedMessages;
    if (memoryLimits != nullptr) *memoryLimits = loadedLimits.normalized();
    if (uiConfig != nullptr) {
        UiConfig loadedUi;
        const QJsonObject uiObject = root.value(QStringLiteral("ui")).toObject();
        loadedUi.appIconPath = uiObject.value(QStringLiteral("app_icon_path")).toString();
        loadedUi.petAvatarPath = uiObject.value(QStringLiteral("pet_avatar_path")).toString();
        loadedUi.conversationAvatarPath = uiObject.value(
            QStringLiteral("conversation_avatar_path")).toString();
        if (uiObject.contains(QStringLiteral("screen_capture_enabled"))) {
            loadedUi.screenCaptureEnabled = uiObject.value(
                QStringLiteral("screen_capture_enabled")).toBool();
        }
        if (uiObject.contains(QStringLiteral("screen_capture_interval_ms"))) {
            loadedUi.screenCaptureIntervalMs = uiObject.value(
                QStringLiteral("screen_capture_interval_ms")).toInt();
        }
        if (uiObject.contains(QStringLiteral("capture_on_chat"))) {
            loadedUi.captureOnChat = uiObject.value(QStringLiteral("capture_on_chat")).toBool();
        }
        if (uiObject.contains(QStringLiteral("capture_image_format"))) {
            loadedUi.captureImageFormat = uiObject.value(
                QStringLiteral("capture_image_format")).toString();
        }
        if (uiObject.contains(QStringLiteral("capture_max_width"))) {
            loadedUi.captureMaxWidth = uiObject.value(QStringLiteral("capture_max_width")).toInt();
        }
        if (uiObject.contains(QStringLiteral("capture_quality"))) {
            loadedUi.captureQuality = uiObject.value(QStringLiteral("capture_quality")).toInt();
        }
        if (uiObject.contains(QStringLiteral("reply_bubble_duration_ms"))) {
            loadedUi.replyBubbleDurationMs = uiObject.value(
                QStringLiteral("reply_bubble_duration_ms")).toInt();
        }
        if (uiObject.contains(QStringLiteral("hover_hide_delay_ms"))) {
            loadedUi.hoverHideDelayMs = uiObject.value(
                QStringLiteral("hover_hide_delay_ms")).toInt();
        }
        if (uiObject.contains(QStringLiteral("fade_duration_ms"))) {
            loadedUi.fadeDurationMs = uiObject.value(QStringLiteral("fade_duration_ms")).toInt();
        }
        if (!loadedUi.validate(errorMessage)) return false;
        *uiConfig = loadedUi.normalized();
    }
    return true;
}

bool AppConfigRepository::save(const PersonaConfig& source, const MemoryLimits& sourceLimits,
                               QString* errorMessage, const UiConfig* uiConfig) const
{
    const PersonaConfig persona = source.normalized();
    const MemoryLimits limits = sourceLimits.normalized();
    if (!persona.validate(errorMessage) || !limits.validate(errorMessage)) return false;
    QFile file(filePath_);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) *errorMessage = QStringLiteral("cannot open application configuration: %1")
            .arg(file.errorString());
        return false;
    }
    QJsonParseError parseError{};
    QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) *errorMessage = QStringLiteral("invalid application configuration JSON: %1")
            .arg(parseError.errorString());
        return false;
    }
    QJsonObject root = document.object();
    QJsonObject personaObject = root.value(QStringLiteral("persona")).toObject();
    personaObject.insert(QStringLiteral("name"), persona.name);
    personaObject.insert(QStringLiteral("user_address"), persona.userAddress);
    personaObject.insert(QStringLiteral("tone"), persona.tone);
    personaObject.insert(QStringLiteral("max_reply_tokens"), persona.maxReplyTokens);
    personaObject.insert(QStringLiteral("proactive_level"), persona.proactiveLevel);
    personaObject.insert(QStringLiteral("system_prompt_template"), persona.systemPromptTemplate);
    QJsonObject instructions;
    const QStringList defaults = persona.proactivityInstructions;
    for (int level = 0; level <= 3; ++level) {
        instructions.insert(QString::number(level),
                            level < defaults.size() ? defaults.at(level) : QString{});
    }
    personaObject.insert(QStringLiteral("proactivity_instructions"), instructions);
    root.insert(QStringLiteral("persona"), personaObject);
    QJsonObject memoryObject = root.value(QStringLiteral("memory")).toObject();
    memoryObject.insert(QStringLiteral("recent_message_limit"), limits.recentMessageLimit);
    memoryObject.insert(QStringLiteral("relevant_history_limit"), limits.relevantHistoryLimit);
    memoryObject.insert(QStringLiteral("long_term_memory_limit"), limits.longTermMemoryLimit);
    memoryObject.insert(QStringLiteral("max_context_tokens"), limits.maxContextTokens);
    root.insert(QStringLiteral("memory"), memoryObject);
    if (uiConfig != nullptr) {
        const UiConfig normalizedUi = uiConfig->normalized();
        if (!normalizedUi.validate(errorMessage)) return false;
        QJsonObject uiObject;
        uiObject.insert(QStringLiteral("app_icon_path"), normalizedUi.appIconPath);
        uiObject.insert(QStringLiteral("pet_avatar_path"), normalizedUi.petAvatarPath);
        uiObject.insert(QStringLiteral("conversation_avatar_path"),
                        normalizedUi.conversationAvatarPath);
        uiObject.insert(QStringLiteral("screen_capture_enabled"), normalizedUi.screenCaptureEnabled);
        uiObject.insert(QStringLiteral("screen_capture_interval_ms"),
                        normalizedUi.screenCaptureIntervalMs);
        uiObject.insert(QStringLiteral("capture_on_chat"), normalizedUi.captureOnChat);
        uiObject.insert(QStringLiteral("capture_image_format"), normalizedUi.captureImageFormat);
        uiObject.insert(QStringLiteral("capture_max_width"), normalizedUi.captureMaxWidth);
        uiObject.insert(QStringLiteral("capture_quality"), normalizedUi.captureQuality);
        uiObject.insert(QStringLiteral("reply_bubble_duration_ms"),
                        normalizedUi.replyBubbleDurationMs);
        uiObject.insert(QStringLiteral("hover_hide_delay_ms"), normalizedUi.hoverHideDelayMs);
        uiObject.insert(QStringLiteral("fade_duration_ms"), normalizedUi.fadeDurationMs);
        root.insert(QStringLiteral("ui"), uiObject);
    }

    QSaveFile output(filePath_);
    if (!output.open(QIODevice::WriteOnly)
        || output.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0
        || !output.commit()) {
        if (errorMessage) *errorMessage = QStringLiteral("cannot commit application configuration: %1")
            .arg(output.errorString());
        return false;
    }
    return true;
}

bool AppConfigRepository::snapshot(QByteArray* data, QString* errorMessage) const
{
    if (data == nullptr) {
        if (errorMessage) *errorMessage = QStringLiteral("application configuration snapshot output is null");
        return false;
    }
    QFile file(filePath_);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) *errorMessage = QStringLiteral("cannot read application configuration snapshot: %1")
            .arg(file.errorString());
        return false;
    }
    *data = file.readAll();
    return true;
}

bool AppConfigRepository::restore(const QByteArray& data, QString* errorMessage) const
{
    QSaveFile file(filePath_);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        if (errorMessage) *errorMessage = QStringLiteral("cannot restore application configuration: %1")
            .arg(file.errorString());
        return false;
    }
    return true;
}

} // namespace zhu_screen_pet
