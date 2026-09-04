#pragma once

#include <QString>

namespace zhu_screen_pet {

/** 单次聊天请求的可选参数。 */
struct ChatOptions
{
    QString model;
    int maxTokens = 512;
    double temperature = 0.7;
    /** 是否请求 OpenAI-compatible SSE 流式回复。 */
    bool stream = false;
};

} // namespace zhu_screen_pet
