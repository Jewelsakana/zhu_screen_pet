#include "infrastructure/TaskExecutor.h"

#include <QRunnable>

#include <utility>

namespace zhu_screen_pet {

namespace {

/** 将 std::function 包装为 Qt 线程池可执行任务。 */
class FunctionRunnable final : public QRunnable
{
public:
    FunctionRunnable(std::function<void()> function, TaskExecutor::ErrorHandler errorHandler)
        : function_(std::move(function)), errorHandler_(std::move(errorHandler))
    {
        setAutoDelete(true);
    }

    /** 在线程池工作线程中执行被包装的函数。 */
    void run() override
    {
        try {
            function_();
        } catch (...) {
            if (errorHandler_) errorHandler_(std::current_exception());
        }
    }

private:
    std::function<void()> function_;
    TaskExecutor::ErrorHandler errorHandler_;
};

} // namespace

TaskExecutor::TaskExecutor(int maxThreadCount)
    : pool_(std::make_unique<QThreadPool>())
{
    pool_->setMaxThreadCount(maxThreadCount > 0 ? maxThreadCount : 1);
}

TaskExecutor::~TaskExecutor()
{
    if (!shutdownAndWait(5000)) {
        // QThreadPool 析构会无限等待不协作任务。进程退出路径宁可交给 OS 回收，避免 UI 卡死。
        pool_.release();
    }
}

std::shared_ptr<CancellationToken> TaskExecutor::submit(
    std::function<void(const std::shared_ptr<CancellationToken>&)> task,
    ErrorHandler errorHandler)
{
    auto token = std::make_shared<CancellationToken>();
    if (shuttingDown_.load() || !task) {
        token->cancel();
        return token;
    }

    {
        QMutexLocker locker(&tokensMutex_);
        tokens_.push_back(token);
    }

    pool_->start(new FunctionRunnable([token, task = std::move(task)]() {
        if (!token->isCancellationRequested()) {
            task(token);
        }
    }, std::move(errorHandler)));
    return token;
}

void TaskExecutor::shutdown()
{
    shutdownAndWait(5000);
}

bool TaskExecutor::shutdownAndWait(int timeoutMs)
{
    bool expected = false;
    if (shuttingDown_.compare_exchange_strong(expected, true)) {
        QMutexLocker locker(&tokensMutex_);
        for (const auto& weakToken : tokens_) {
            if (const auto token = weakToken.lock()) token->cancel();
        }
        pool_->clear();
    }
    return pool_ == nullptr || pool_->waitForDone(qMax(0, timeoutMs));
}

} // namespace zhu_screen_pet
