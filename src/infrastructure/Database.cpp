#include "infrastructure/Database.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QUuid>

namespace zhu_screen_pet {

Database::Database()
    : connectionName_(QStringLiteral("zhu_screen_pet_%1").arg(QUuid::createUuid().toString(QUuid::Id128)))
{
}

Database::~Database()
{
    close();
    if (QSqlDatabase::contains(connectionName_)) {
        QSqlDatabase::removeDatabase(connectionName_);
    }
}

bool Database::open(const QString& path, QString* errorMessage)
{
    const Result<void> result = openResult(path);
    if (!result && errorMessage != nullptr) *errorMessage = result.error().technicalMessage;
    return result.succeeded();
}

Result<void> Database::openResult(const QString& path)
{
    ftsRebuiltDuringOpen_ = false;
    if (database_.isValid()) {
        close();
    }
    database_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    database_.setDatabaseName(path);
    if (!database_.open()) {
        return Result<void>::failure({AppErrorCode::DatabaseUnavailable,
                                      QStringLiteral("unable to open database"), 0,
                                      ErrorDomain::Database, database_.lastError().text(),
                                      QStringLiteral("database.open"), {}, false});
    }

    QString technicalError;
    if (!execute(QStringLiteral("PRAGMA journal_mode=WAL"), &technicalError)
        || !execute(QStringLiteral("PRAGMA foreign_keys=ON"), &technicalError)) {
        close();
        return Result<void>::failure({AppErrorCode::DatabaseQuery,
                                      QStringLiteral("database initialization failed"), 0,
                                      ErrorDomain::Database, technicalError,
                                      QStringLiteral("database.pragma"), {}, false});
    }
    if (!migrate(&technicalError)) {
        close();
        return Result<void>::failure({AppErrorCode::DatabaseQuery,
                                      QStringLiteral("database migration failed"), 0,
                                      ErrorDomain::Database, technicalError,
                                      QStringLiteral("database.migrate"), {}, false});
    }
    configureFts5();
    return Result<void>::success();
}

void Database::close()
{
    if (database_.isOpen()) {
        database_.close();
    }
    database_ = QSqlDatabase();
}

bool Database::isOpen() const
{
    return database_.isOpen();
}

int Database::schemaVersion() const
{
    const Result<int> result = schemaVersionResult();
    return result ? result.value() : -1;
}

Result<int> Database::schemaVersionResult() const
{
    if (!database_.isOpen()) {
        return Result<int>::failure({AppErrorCode::DatabaseUnavailable,
                                     QStringLiteral("database is not open"), 0,
                                     ErrorDomain::Database, {},
                                     QStringLiteral("database.schema_version"), {}, false});
    }
    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral("SELECT version FROM schema_version LIMIT 1")) || !query.next()) {
        return Result<int>::failure({AppErrorCode::DatabaseQuery,
                                     QStringLiteral("unable to read database schema version"), 0,
                                     ErrorDomain::Database, query.lastError().text(),
                                     QStringLiteral("database.schema_version"), {}, false});
    }
    return Result<int>::success(query.value(0).toInt());
}

QSqlDatabase Database::connection() const
{
    return database_;
}

bool Database::hasFts5() const
{
    if (!database_.isOpen()) return false;
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "SELECT enabled FROM database_capabilities WHERE name='fts5' LIMIT 1"));
    return query.exec() && query.next() && query.value(0).toInt() == 1;
}

bool Database::ftsRebuiltDuringOpen() const
{
    return ftsRebuiltDuringOpen_;
}

bool Database::execute(const QString& sql, QString* errorMessage)
{
    QSqlQuery query(database_);
    if (query.exec(sql)) {
        return true;
    }
    if (errorMessage != nullptr) {
        *errorMessage = query.lastError().text();
    }
    return false;
}

bool Database::migrate(QString* errorMessage)
{
    if (!database_.transaction()) {
        if (errorMessage != nullptr) {
            *errorMessage = database_.lastError().text();
        }
        return false;
    }

    bool ok = execute(
        QStringLiteral("CREATE TABLE IF NOT EXISTS schema_version (version INTEGER NOT NULL)"),
        errorMessage);
    ok = ok && execute(
        QStringLiteral("INSERT INTO schema_version(version) SELECT 1 WHERE NOT EXISTS "
                       "(SELECT 1 FROM schema_version)"), errorMessage);
    if (!ok) {
        database_.rollback();
        return false;
    }

    QSqlQuery versionQuery(database_);
    if (!versionQuery.exec(QStringLiteral("SELECT version FROM schema_version LIMIT 1"))
        || !versionQuery.next()) {
        if (errorMessage != nullptr) *errorMessage = versionQuery.lastError().text();
        database_.rollback();
        return false;
    }
    int version = versionQuery.value(0).toInt();
    if (version < 2) {
        ok = execute(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS conversations ("
            "id TEXT PRIMARY KEY, title TEXT NOT NULL, created_at TEXT NOT NULL, "
            "updated_at TEXT NOT NULL, archived_at TEXT)"), errorMessage);
        ok = ok && execute(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS conversation_messages ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, conversation_id TEXT NOT NULL, "
            "role TEXT NOT NULL, content TEXT NOT NULL, token_count INTEGER NOT NULL DEFAULT 0, "
            "created_at TEXT NOT NULL, summarized_at TEXT, "
            "FOREIGN KEY (conversation_id) REFERENCES conversations(id) ON DELETE CASCADE)"),
                           errorMessage);
        ok = ok && execute(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_conversation_messages_conversation_time "
            "ON conversation_messages(conversation_id, created_at, id)"), errorMessage);
        ok = ok && execute(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS memories ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, kind TEXT NOT NULL, content TEXT NOT NULL, "
            "source_event_id TEXT, created_at TEXT NOT NULL, expires_at TEXT)"), errorMessage);
        ok = ok && execute(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_memories_kind_time ON memories(kind, created_at)"),
                           errorMessage);
        ok = ok && execute(QStringLiteral(
            "UPDATE schema_version SET version = 2"), errorMessage);
        version = 2;
    }
    if (ok && version < 3) {
        ok = execute(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS database_capabilities ("
            "name TEXT PRIMARY KEY, enabled INTEGER NOT NULL, detail TEXT)"), errorMessage);
        ok = ok && execute(QStringLiteral("UPDATE schema_version SET version = 3"), errorMessage);
        version = 3;
    }
    if (!ok || !database_.commit()) {
        database_.rollback();
        if (errorMessage != nullptr && errorMessage->isEmpty()) {
            *errorMessage = database_.lastError().text();
        }
        return false;
    }
    return true;
}

void Database::configureFts5()
{
    if (!database_.isOpen()) return;
    QSqlQuery existing(database_);
    existing.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name IN (?,?)"));
    existing.addBindValue(QStringLiteral("conversation_messages_fts"));
    existing.addBindValue(QStringLiteral("memories_fts"));
    const bool indexesExisted = existing.exec() && existing.next() && existing.value(0).toInt() == 2;
    if (!database_.transaction()) return;

    bool ok = execute(QStringLiteral(
        "CREATE VIRTUAL TABLE IF NOT EXISTS conversation_messages_fts USING fts5("
        "content, content='conversation_messages', content_rowid='id', tokenize='unicode61')"));
    ok = ok && execute(QStringLiteral(
        "CREATE VIRTUAL TABLE IF NOT EXISTS memories_fts USING fts5("
        "content, content='memories', content_rowid='id', tokenize='unicode61')"));
    ok = ok && execute(QStringLiteral(
        "CREATE TRIGGER IF NOT EXISTS conversation_messages_fts_ai AFTER INSERT ON conversation_messages BEGIN "
        "INSERT INTO conversation_messages_fts(rowid,content) VALUES(new.id,new.content); END"));
    ok = ok && execute(QStringLiteral(
        "CREATE TRIGGER IF NOT EXISTS conversation_messages_fts_ad AFTER DELETE ON conversation_messages BEGIN "
        "INSERT INTO conversation_messages_fts(conversation_messages_fts,rowid,content) "
        "VALUES('delete',old.id,old.content); END"));
    ok = ok && execute(QStringLiteral(
        "CREATE TRIGGER IF NOT EXISTS conversation_messages_fts_au AFTER UPDATE OF content ON conversation_messages BEGIN "
        "INSERT INTO conversation_messages_fts(conversation_messages_fts,rowid,content) "
        "VALUES('delete',old.id,old.content); "
        "INSERT INTO conversation_messages_fts(rowid,content) VALUES(new.id,new.content); END"));
    ok = ok && execute(QStringLiteral(
        "CREATE TRIGGER IF NOT EXISTS memories_fts_ai AFTER INSERT ON memories BEGIN "
        "INSERT INTO memories_fts(rowid,content) VALUES(new.id,new.content); END"));
    ok = ok && execute(QStringLiteral(
        "CREATE TRIGGER IF NOT EXISTS memories_fts_ad AFTER DELETE ON memories BEGIN "
        "INSERT INTO memories_fts(memories_fts,rowid,content) VALUES('delete',old.id,old.content); END"));
    ok = ok && execute(QStringLiteral(
        "CREATE TRIGGER IF NOT EXISTS memories_fts_au AFTER UPDATE OF content ON memories BEGIN "
        "INSERT INTO memories_fts(memories_fts,rowid,content) VALUES('delete',old.id,old.content); "
        "INSERT INTO memories_fts(rowid,content) VALUES(new.id,new.content); END"));
    // 仅首次创建 FTS 表时重建；避免每次启动扫描全部历史数据。
    if (!indexesExisted) {
        ftsRebuiltDuringOpen_ = true;
        ok = ok && execute(QStringLiteral(
            "INSERT INTO conversation_messages_fts(conversation_messages_fts) VALUES('rebuild')"));
        ok = ok && execute(QStringLiteral("INSERT INTO memories_fts(memories_fts) VALUES('rebuild')"));
    }

    if (ok && database_.commit()) {
        QSqlQuery capability(database_);
        capability.exec(QStringLiteral(
            "INSERT OR REPLACE INTO database_capabilities(name,enabled,detail) "
            "VALUES('fts5',1,'unicode61')"));
        return;
    }
    database_.rollback();
    QSqlQuery capability(database_);
    capability.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO database_capabilities(name,enabled,detail) VALUES('fts5',0,?)"));
    capability.addBindValue(QStringLiteral("SQLite FTS5 is unavailable; using LIKE fallback"));
    capability.exec();
}

} // namespace zhu_screen_pet
