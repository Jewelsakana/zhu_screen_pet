#pragma once

#include <QHash>

#include "infrastructure/TaskExecutor.h"
#include "model/ChatProvider.h"

namespace zhu_screen_pet {

/** 不联网的模型实现，用于离线开发、UI 联调和自动化测试。 */
class MockChatProvider final : public ChatProvider
{
public:
    /** 使用配置提供的固定回复创建 Mock Provider。 */
    explicit MockChatProvider(QString response);

    /** 设置下一次及后续成功请求返回的固定文本。 */
    void setResponse(QString response);
    /** 设置下一次及后续请求返回的错误，便于测试失败流程。 */
    void setError(ModelError error);
    /** 清除预设错误，恢复成功返回模式。 */
    void clearError();
    /** 返回已开始处理的请求次数。 */
    int requestCount() const;
    /** 返回最近一次请求收到的完整上下文，供离线联调和测试检查。 */
    std::vector<Message> lastMessages() const;
    /** 返回最近一次请求收到的选项，供离线联调和测试检查。 */
    ChatOptions lastOptions() const;

    QString startChat(const std::vector<Message>& messages,
                      const ChatOptions& options) override;
    void cancel(const QString& requestId) override;

private:
    struct PendingRequest;

    QString response_;
    ModelError error_;
    int requestCount_ = 0;
    std::vector<Message> lastMessages_;
    ChatOptions lastOptions_;
    QHash<QString, std::shared_ptr<CancellationToken>> pendingRequests_;
};

} // namespace zhu_screen_pet
