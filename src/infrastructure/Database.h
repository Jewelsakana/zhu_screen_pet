#pragma once

#include <QSqlDatabase>
#include <QString>

#include "core/Result.h"

namespace zhu_screen_pet {

/** SQLite 连接和数据库迁移服务；当前连接设计为单线程使用。 */
class Database final
{
public:
    /** 创建具有独立 Qt 连接名的数据库服务。 */
    Database();
    /** 关闭数据库并移除 Qt 连接。 */
    ~Database();

    /** 打开数据库、启用 WAL/外键并执行数据库迁移。 */
    bool open(const QString& path, QString* errorMessage = nullptr);
    /** 类型安全的打开接口；不会把失败与空值混淆。 */
    Result<void> openResult(const QString& path);
    /** 关闭当前数据库连接。 */
    void close();
    /** 返回数据库当前是否处于打开状态。 */
    bool isOpen() const;
    /** 返回当前 SQLite 连接；调用方不得跨线程使用该连接。 */
    QSqlDatabase connection() const;
    /** 返回 schema 版本；数据库未打开或查询失败时返回 -1。 */
    int schemaVersion() const;
    /** 返回 schema 版本，查询失败时返回结构化错误。 */
    Result<int> schemaVersionResult() const;
    /** 返回当前 SQLite 连接是否已成功启用 FTS5 索引。 */
    bool hasFts5() const;
    /** 返回本次 open 是否执行过全量 FTS rebuild，供诊断和回归测试使用。 */
    bool ftsRebuiltDuringOpen() const;

private:
    /** 执行一条 SQL 语句，并在失败时返回数据库错误。 */
    bool execute(const QString& sql, QString* errorMessage = nullptr);
    /** 在事务中创建或升级数据库 schema。 */
    bool migrate(QString* errorMessage);
    /** 尝试安装和重建 FTS5 索引；失败时记录降级能力但不阻止数据库启动。 */
    void configureFts5();

    QString connectionName_;
    QSqlDatabase database_;
    bool ftsRebuiltDuringOpen_ = false;
};

} // namespace zhu_screen_pet
