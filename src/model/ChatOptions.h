#pragma once

#include <QString>

namespace zhu_screen_pet {

enum class ChatRequestKind
{
    Normal,
    Screenshot
};

/** 单次聊天请求的可选参数。 */
struct ChatOptions
{
    QString model;
    int maxTokens = 512;
    double temperature = 0.7;
    /** 是否请求 OpenAI-compatible SSE 流式回复。 */
    bool stream = false;
    ChatRequestKind requestKind = ChatRequestKind::Normal;
    /** DeepSeek 扩展：请求直接回答，避免短输出预算被推理内容耗尽。 */
    bool disableThinking = false;
};

} // namespace zhu_screen_pet
