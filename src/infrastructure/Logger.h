#pragma once

#include <QMutex>
#include <QString>
#include <QDateTime>

class QFile;

namespace zhu_screen_pet {

/** 线程安全的结构化 JSONL 文件日志服务。 */
class Logger final
{
public:
    /** 日志严重级别。 */
    enum class Level { Debug, Info, Warning, Error };

    /** 创建尚未打开日志文件的日志服务。 */
    Logger() = default;
    /** 关闭并释放日志文件。 */
    ~Logger();

    /** 在目录中打开当天的日志文件。 */
    bool initialize(const QString& directory, QString* errorMessage = nullptr,
                    qint64 maxBytes = 5 * 1024 * 1024, int maxFiles = 7);
    /** 写入一条结构化日志；API Key 等敏感内容由调用方负责脱敏。 */
    void log(Level level, const QString& module, const QString& event,
             const QString& message, const QString& errorCode = {});
    /** 写入 Debug 级别日志。 */
    void debug(const QString& module, const QString& event, const QString& message);
    /** 写入 Info 级别日志。 */
    void info(const QString& module, const QString& event, const QString& message);
    /** 写入 Warning 级别日志。 */
    void warning(const QString& module, const QString& event, const QString& message,
                 const QString& errorCode = {});
    /** 写入 Error 级别日志。 */
    void error(const QString& module, const QString& event, const QString& message,
               const QString& errorCode = {});
    /** 返回最近一次日志写入是否成功。 */
    bool lastWriteSucceeded() const;
    /** 返回最近一次日志写入失败的技术详情。 */
    QString lastWriteError() const;

private:
    bool rotateIfNeeded();
    void pruneRotatedFiles();
    QFile* file_ = nullptr;
    QString filePath_;
    QString directory_;
    qint64 maxBytes_ = 5 * 1024 * 1024;
    int maxFiles_ = 7;
    bool lastWriteSucceeded_ = true;
    QString lastWriteError_;
    mutable QMutex mutex_;
};

} // namespace zhu_screen_pet
