#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <exception>
#include <vector>

#include <QMutex>
#include <QThreadPool>

namespace zhu_screen_pet {

/** 后台任务使用的协作式取消标记；任务需要主动检查该标记。 */
class CancellationToken final
{
public:
    /** 请求任务取消。 */
    void cancel() { cancelled_.store(true); }
    /** 返回任务是否已收到取消请求。 */
    bool isCancellationRequested() const { return cancelled_.load(); }

private:
    std::atomic_bool cancelled_{false};
};

/** 基于 QThreadPool 的后台任务执行器。 */
class TaskExecutor final
{
public:
    using ErrorHandler = std::function<void(std::exception_ptr)>;
    /** 创建执行器；maxThreadCount 小于等于 0 时使用 1 个线程。 */
    explicit TaskExecutor(int maxThreadCount = 2);
    /** 等待已提交任务完成并释放线程池。 */
    ~TaskExecutor();

    /** 提交后台任务并返回其取消标记；关闭后提交的任务不会执行。 */
    std::shared_ptr<CancellationToken> submit(
        std::function<void(const std::shared_ptr<CancellationToken>&)> task,
        ErrorHandler errorHandler = {});
    /** 停止接收新任务并等待已提交任务完成。 */
    void shutdown();
    /** 请求协作式取消并在限定时间内等待；返回是否全部任务已退出。 */
    bool shutdownAndWait(int timeoutMs);

private:
    std::unique_ptr<QThreadPool> pool_;
    std::vector<std::weak_ptr<CancellationToken>> tokens_;
    QMutex tokensMutex_;
    std::atomic_bool shuttingDown_{false};
};

} // namespace zhu_screen_pet
