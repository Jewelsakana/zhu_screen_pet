#pragma once

#include <QString>
#include <QStringList>

namespace zhu_screen_pet {

/** 控制桌宠名称、用户称呼、语气和默认回复长度的轻量人格配置。 */
struct PersonaConfig
{
    QString name;
    QString userAddress = QStringLiteral("主人大人");
    QString tone;
    int maxReplyTokens = 0;
    int proactiveLevel = 0;
    QString systemPromptTemplate;
    QStringList proactivityInstructions;

    /** 返回经过范围约束的配置副本。 */
    PersonaConfig normalized() const;
    /** 校验名称、用户称呼、语气、回复长度和主动程度。 */
    bool validate(QString* errorMessage = nullptr) const;
    /** 返回主动程度对应的明确行为指令。 */
    QString proactivityInstruction() const;
    /** 生成人格对应的 system 消息文本。 */
    QString systemInstruction() const;
};

} // namespace zhu_screen_pet
