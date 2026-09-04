#include "app/PersonaConfig.h"

#include <QtGlobal>

namespace zhu_screen_pet {

PersonaConfig PersonaConfig::normalized() const
{
    PersonaConfig result = *this;
    result.name = result.name.trimmed();
    result.userAddress = result.userAddress.trimmed();
    result.tone = result.tone.trimmed();
    result.maxReplyTokens = qBound(1, result.maxReplyTokens, 32768);
    result.proactiveLevel = qBound(0, result.proactiveLevel, 3);
    result.systemPromptTemplate = result.systemPromptTemplate.trimmed();
    for (QString& instruction : result.proactivityInstructions) instruction = instruction.trimmed();
    return result;
}

bool PersonaConfig::validate(QString* errorMessage) const
{
    if (name.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("persona name must not be empty");
        return false;
    }
    if (userAddress.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("persona user address must not be empty");
        return false;
    }
    if (tone.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("persona tone must not be empty");
        return false;
    }
    if (maxReplyTokens <= 0 || maxReplyTokens > 32768) {
        if (errorMessage) *errorMessage = QStringLiteral("persona max reply tokens must be between 1 and 32768");
        return false;
    }
    if (proactiveLevel < 0 || proactiveLevel > 3) {
        if (errorMessage) *errorMessage = QStringLiteral("persona proactive level must be between 0 and 3");
        return false;
    }
    if (systemPromptTemplate.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("persona system prompt template must not be empty");
        return false;
    }
    if (proactivityInstructions.size() != 4) {
        if (errorMessage) *errorMessage = QStringLiteral("persona requires four proactivity instructions");
        return false;
    }
    for (const QString& instruction : proactivityInstructions) {
        if (instruction.trimmed().isEmpty()) {
            if (errorMessage) *errorMessage = QStringLiteral("persona proactivity instruction must not be empty");
            return false;
        }
    }
    return true;
}

QString PersonaConfig::proactivityInstruction() const
{
    const PersonaConfig config = normalized();
    return config.proactivityInstructions.value(config.proactiveLevel);
}

QString PersonaConfig::systemInstruction() const
{
    const PersonaConfig config = normalized();
    const QString personaInstruction = config.systemPromptTemplate.arg(
        config.name, config.tone, config.proactivityInstruction());
    return personaInstruction + QStringLiteral(
        " 你对用户的称呼是“%1”；需要称呼用户时使用这一称呼，不要擅自改用其他固定称谓。")
        .arg(config.userAddress);
}

} // namespace zhu_screen_pet
