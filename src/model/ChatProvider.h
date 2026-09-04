#pragma once

#include <vector>
#include <QObject>
#include <QString>

#include "model/ChatOptions.h"
#include "model/ChatResult.h"
#include "model/Message.h"

namespace zhu_screen_pet {

/** 文本模型统一异步接口；具体 Provider 不应直接操作 UI 或记忆数据库。 */
class ChatProvider : public QObject
{
    Q_OBJECT

public:
    explicit ChatProvider(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    ~ChatProvider() override = default;

    /**
     * 启动一次聊天请求并立即返回请求 ID。
     *
     * 请求结果通过 chatFinished() 异步通知。Provider 不应在此函数中阻塞
     * UI 线程，也不应直接修改任何 QWidget。
     */
    virtual QString startChat(
        const std::vector<Message>& messages,
        const ChatOptions& options) = 0;

    /** 取消指定请求；请求已结束时调用不会产生额外结果信号。 */
    virtual void cancel(const QString& requestId) = 0;

signals:
    /** 请求正式进入 Provider 后发出。 */
    void chatStarted(const QString& requestId);
    /** 流式响应到达的一段增量文本；非流式请求不会发出。 */
    void chatDelta(const QString& requestId, const QString& delta);
    /** 请求完成；成功或失败都通过统一 ChatResult 返回。 */
    void chatFinished(const QString& requestId, const ChatResult& result);
};

} // namespace zhu_screen_pet
