#pragma once

#include <QString>

#include <utility>

namespace zhu_screen_pet {

/** 模型消息角色。 */
enum class MessageRole
{
    System,
    User,
    Assistant
};

/** 发送给模型的一条文本消息。 */
struct Message
{
    MessageRole role = MessageRole::User;
    QString content;

    /** 创建一条消息。 */
    static Message create(MessageRole messageRole, QString messageContent)
    {
        return Message{messageRole, std::move(messageContent)};
    }
};

/** 将消息角色转换为 OpenAI-compatible API 使用的字符串。 */
inline QString messageRoleName(MessageRole role)
{
    switch (role) {
    case MessageRole::System: return QStringLiteral("system");
    case MessageRole::User: return QStringLiteral("user");
    case MessageRole::Assistant: return QStringLiteral("assistant");
    }
    return QStringLiteral("user");
}

} // namespace zhu_screen_pet
