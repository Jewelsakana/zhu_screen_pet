#pragma once

#include <QHash>
#include <QObject>

#include "memory/MemoryOrchestrator.h"
#include "app/PersonaConfig.h"
#include "app/PetState.h"
#include "model/ChatOptions.h"
#include "model/ChatProvider.h"
#include "core/AppError.h"

namespace zhu_screen_pet {

/** 编排用户输入、上下文构建、模型请求及回复持久化。 */
class ChatController final : public QObject
{
    Q_OBJECT

public:
    explicit ChatController(ChatProvider* provider, MemoryOrchestrator* memory,
                            QObject* parent = nullptr);

    /**
     * 保存用户消息并启动异步模型请求。
     *
     * 参数无效、上下文构建失败或用户消息无法保存时返回空字符串。
     */
    QString sendMessage(const QString& conversationId, const QString& text,
                        const ChatOptions& options = ChatOptions{});
    /** 重新发送最近一次失败请求，不会重复写入用户消息。 */
    QString retryLast();
    /** 请求 Provider 取消指定聊天；最终状态仍由 requestFailed 通知。 */
    void cancel(const QString& requestId);
    /** 取消当前全部聊天请求，供设置热切换前由用户主动选择。 */
    void cancelAll();
    /** 返回当前仍在等待 Provider 完成的请求数量。 */
    int pendingRequestCount() const;
    /** 返回最近一次同步操作的结构化错误。 */
    AppError lastAppError() const;
    /** 返回当前桌宠状态。 */
    PetState state() const;
    /** 返回当前生效的人格配置，供设置界面生成编辑草稿。 */
    PersonaConfig personaConfig() const;
    /** 校验并更新后续请求使用的人格配置。 */
    bool setPersonaConfig(const PersonaConfig& config, QString* errorMessage = nullptr);

signals:
    /** Provider 已接受请求，可用于将 UI 切换到 thinking 状态。 */
    void requestStarted(const QString& requestId);
    /** 流式回复的一段增量文本。 */
    void replyDelta(const QString& requestId, const QString& delta);
    /** 回复已完成且已成功写入会话。 */
    void replyFinished(const QString& requestId, const QString& content);
    /** 模型请求或回复持久化失败。 */
    void requestFailed(const QString& requestId, const ModelError& error);
    /** 同步启动、上下文或持久化失败时发出，与异步错误共用同一错误类型。 */
    void operationFailed(const AppError& error);
    /** 聊天生命周期导致桌宠状态变化时发出。 */
    void stateChanged(PetState state);

private:
    struct PendingChat
    {
        QString conversationId;
        QString userText;
        QString accumulatedReply;
        std::vector<Message> context;
        ChatOptions options;
    };

    void onChatStarted(const QString& requestId);
    void onChatDelta(const QString& requestId, const QString& delta);
    void onChatFinished(const QString& requestId, const ChatResult& result);
    QString startPending(PendingChat pending);
    void setState(PetState state);
    void fail(const AppError& error);

    ChatProvider* provider_ = nullptr;
    MemoryOrchestrator* memory_ = nullptr;
    QHash<QString, PendingChat> pending_;
    PendingChat lastFailed_;
    bool hasLastFailed_ = false;
    AppError lastError_;
    PersonaConfig persona_;
    bool personaConfigured_ = false;
    PetState state_ = PetState::Idle;
};

} // namespace zhu_screen_pet
