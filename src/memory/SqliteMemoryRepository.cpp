#include "memory/SqliteMemoryRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <algorithm>

#include "infrastructure/Database.h"
#include "memory/MemoryContext.h"

namespace zhu_screen_pet {
namespace {

QDateTime parseTime(const QVariant& value)
{
    return QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
}

QString serializeTime(const QDateTime& value)
{
    return value.isValid() ? value.toUTC().toString(Qt::ISODateWithMs) : QString();
}

MessageRole parseRole(const QString& role)
{
    if (role == QStringLiteral("system")) return MessageRole::System;
    if (role == QStringLiteral("assistant")) return MessageRole::Assistant;
    return MessageRole::User;
}

AppError memoryError(AppErrorCode code, const QString& message, const QString& technical,
                     const QString& operation)
{
    AppError error;
    error.code = code; error.message = message; error.domain = ErrorDomain::Database;
    error.technicalMessage = technical; error.operation = operation;
    return error;
}

} // namespace

SqliteMemoryRepository::SqliteMemoryRepository(Database* database)
    : database_(database)
{
}

Result<qint64> SqliteMemoryRepository::saveShortTermResult(
    const QString& content, const QString& sourceEventId, const QDateTime& expiresAt)
{
    MemoryItem item;
    item.kind = QStringLiteral("short_term");
    item.content = content;
    item.sourceEventId = sourceEventId;
    item.expiresAt = expiresAt.isValid() ? expiresAt
        : QDateTime::currentDateTimeUtc().addSecs(MemoryLimits::ShortTermRetentionSeconds);
    return saveResult(item);
}

Result<qint64> SqliteMemoryRepository::saveLongTermResult(
    const QString& content, const QString& sourceEventId)
{
    MemoryItem item;
    item.kind = QStringLiteral("long_term");
    item.content = content;
    item.sourceEventId = sourceEventId;
    return saveResult(item);
}

Result<qint64> SqliteMemoryRepository::saveResult(const MemoryItem& item)
{
    if (database_ == nullptr || !database_->isOpen()) return Result<qint64>::failure(memoryError(
        AppErrorCode::DatabaseUnavailable, QStringLiteral("database is not available"),
        QStringLiteral("database is not open"), QStringLiteral("memory.save")));
    if (item.content.trimmed().isEmpty()) return Result<qint64>::failure(memoryError(
        AppErrorCode::InvalidArgument, QStringLiteral("记忆内容不能为空"),
        QStringLiteral("memory content is empty"), QStringLiteral("memory.save")));
    QSqlQuery query(database_->connection());
    query.prepare(QStringLiteral("INSERT INTO memories(kind,content,source_event_id,created_at,expires_at) "
                                 "VALUES(?,?,?,?,?)"));
    query.addBindValue(item.kind.isEmpty() ? QStringLiteral("short_term") : item.kind);
    query.addBindValue(item.content);
    if (item.sourceEventId.trimmed().isEmpty()) query.addBindValue(QVariant(QVariant::String));
    else query.addBindValue(item.sourceEventId.trimmed());
    query.addBindValue(serializeTime(item.createdAt.isValid()
                                        ? item.createdAt : QDateTime::currentDateTimeUtc()));
    if (item.expiresAt.isValid()) query.addBindValue(serializeTime(item.expiresAt));
    else query.addBindValue(QVariant(QVariant::String));
    if (!query.exec()) return Result<qint64>::failure(memoryError(AppErrorCode::DatabaseQuery,
        QStringLiteral("无法保存记忆"), query.lastError().text(), QStringLiteral("memory.save")));
    return Result<qint64>::success(query.lastInsertId().toLongLong());
}

Result<void> SqliteMemoryRepository::removeResult(qint64 id)
{
    if (database_ == nullptr || !database_->isOpen()) return Result<void>::failure(memoryError(
        AppErrorCode::DatabaseUnavailable, QStringLiteral("database is not available"),
        QStringLiteral("database is not open"), QStringLiteral("memory.remove")));
    QSqlQuery query(database_->connection());
    query.prepare(QStringLiteral("DELETE FROM memories WHERE id=?")); query.addBindValue(id);
    if (!query.exec()) return Result<void>::failure(memoryError(AppErrorCode::DatabaseQuery,
        QStringLiteral("无法删除记忆"), query.lastError().text(), QStringLiteral("memory.remove")));
    if (query.numRowsAffected() <= 0) return Result<void>::failure(memoryError(AppErrorCode::NotFound,
        QStringLiteral("记忆不存在"), QStringLiteral("memory id not found: %1").arg(id),
        QStringLiteral("memory.remove")));
    return Result<void>::success();
}

Result<void> SqliteMemoryRepository::clearKindResult(const QString& kind)
{
    if (database_ == nullptr || !database_->isOpen()) return Result<void>::failure(memoryError(
        AppErrorCode::DatabaseUnavailable, QStringLiteral("database is not available"), QStringLiteral("database is not open"), QStringLiteral("memory.clear")));
    if (kind.trimmed().isEmpty()) return Result<void>::failure(memoryError(
        AppErrorCode::InvalidArgument, QStringLiteral("记忆类型不能为空"), QStringLiteral("kind is empty"), QStringLiteral("memory.clear")));
    QSqlQuery query(database_->connection());
    query.prepare(QStringLiteral("DELETE FROM memories WHERE kind=? OR kind LIKE ?"));
    query.addBindValue(kind); query.addBindValue(kind + QStringLiteral(":%"));
    if (!query.exec()) return Result<void>::failure(memoryError(AppErrorCode::DatabaseQuery,
        QStringLiteral("无法清空记忆"), query.lastError().text(), QStringLiteral("memory.clear")));
    return Result<void>::success();
}

Result<int> SqliteMemoryRepository::cleanupShortTermResult(const QDateTime& now,
                                                           int maxItems)
{
    if (database_ == nullptr || !database_->isOpen()) return Result<int>::failure(memoryError(
        AppErrorCode::DatabaseUnavailable, QStringLiteral("database is not available"),
        QStringLiteral("database is not open"), QStringLiteral("memory.cleanup_short_term")));
    if (!now.isValid() || maxItems < 0) return Result<int>::failure(memoryError(
        AppErrorCode::InvalidArgument, QStringLiteral("短期记忆清理参数无效"),
        QStringLiteral("cleanup time is invalid or maxItems is negative"),
        QStringLiteral("memory.cleanup_short_term")));

    QSqlDatabase db = database_->connection();
    if (!db.transaction()) return Result<int>::failure(memoryError(
        AppErrorCode::DatabaseQuery, QStringLiteral("无法开始短期记忆清理事务"),
        db.lastError().text(), QStringLiteral("memory.cleanup_short_term")));
    int removed = 0;
    QString technical;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "DELETE FROM memories WHERE kind LIKE 'short_term%' AND expires_at IS NOT NULL AND expires_at<=?"));
    query.addBindValue(serializeTime(now));
    bool ok = query.exec();
    if (ok) removed += qMax<qint64>(0, query.numRowsAffected());
    else technical = query.lastError().text();
    if (ok) {
        query.prepare(QStringLiteral(
            "DELETE FROM memories WHERE id IN (SELECT id FROM memories "
            "WHERE kind LIKE 'short_term%' ORDER BY created_at DESC,id DESC LIMIT -1 OFFSET ?)"));
        query.addBindValue(maxItems);
        ok = query.exec();
        if (ok) removed += qMax<qint64>(0, query.numRowsAffected());
        else technical = query.lastError().text();
    }
    if (!ok || !db.commit()) {
        if (technical.isEmpty()) technical = db.lastError().text();
        db.rollback();
        return Result<int>::failure(memoryError(
            AppErrorCode::DatabaseQuery, QStringLiteral("无法清理短期记忆"), technical,
            QStringLiteral("memory.cleanup_short_term")));
    }
    return Result<int>::success(removed);
}

Result<MemoryItem> SqliteMemoryRepository::getResult(qint64 id) const
{
    if (database_ == nullptr || !database_->isOpen()) return Result<MemoryItem>::failure(memoryError(
        AppErrorCode::DatabaseUnavailable, QStringLiteral("database is not available"), QStringLiteral("database is not open"), QStringLiteral("memory.get")));
    QSqlQuery query(database_->connection()); query.prepare(QStringLiteral(
        "SELECT id,kind,content,source_event_id,created_at,expires_at,updated_at,category,confidence,importance,source_kind,source_reference FROM memories WHERE id=?"));
    query.addBindValue(id);
    if (!query.exec()) return Result<MemoryItem>::failure(memoryError(AppErrorCode::DatabaseQuery,
        QStringLiteral("无法读取记忆"), query.lastError().text(), QStringLiteral("memory.get")));
    if (!query.next()) return Result<MemoryItem>::failure(memoryError(AppErrorCode::NotFound,
        QStringLiteral("记忆不存在"), QStringLiteral("memory id not found"), QStringLiteral("memory.get")));
    MemoryItem item; item.id=query.value(0).toLongLong(); item.kind=query.value(1).toString(); item.content=query.value(2).toString();
    item.sourceEventId=query.value(3).toString(); item.createdAt=parseTime(query.value(4)); item.expiresAt=parseTime(query.value(5));
    item.updatedAt=parseTime(query.value(6)); item.category=query.value(7).toString(); item.confidence=query.value(8).toDouble(); item.importance=query.value(9).toDouble();
    item.sourceKind=query.value(10).toString(); item.sourceReference=query.value(11).toString();
    return Result<MemoryItem>::success(item);
}

Result<qint64> SqliteMemoryRepository::upsertLongTermResult(const MemoryItem& source)
{
    if (database_ == nullptr || !database_->isOpen()) return Result<qint64>::failure(memoryError(
        AppErrorCode::DatabaseUnavailable, QStringLiteral("database is not available"), QStringLiteral("database is not open"), QStringLiteral("memory.upsert")));
    const QString content=source.content.trimmed();
    if (content.isEmpty()) return Result<qint64>::failure(memoryError(AppErrorCode::InvalidArgument,
        QStringLiteral("记忆内容不能为空"), QStringLiteral("content is empty"), QStringLiteral("memory.upsert")));
    QSqlQuery find(database_->connection()); find.prepare(QStringLiteral(
        "SELECT id FROM memories WHERE kind LIKE 'long_term%' AND (lower(trim(content))=lower(trim(?)) "
        "OR (?<>'' AND ?='conversation_summary' AND source_kind=? AND source_reference=? AND category=?)) LIMIT 1"));
    find.addBindValue(content); find.addBindValue(source.sourceReference); find.addBindValue(source.category);
    find.addBindValue(source.sourceKind); find.addBindValue(source.sourceReference); find.addBindValue(source.category);
    if (!find.exec()) return Result<qint64>::failure(memoryError(AppErrorCode::DatabaseQuery,
        QStringLiteral("无法检查重复记忆"), find.lastError().text(), QStringLiteral("memory.upsert")));
    const QString now=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    const double confidence=qBound(0.0,source.confidence,1.0), importance=qBound(0.0,source.importance,1.0);
    if (source.id > 0) {
        QSqlQuery update(database_->connection());
        update.prepare(QStringLiteral("UPDATE memories SET kind=?,content=?,source_event_id=?,updated_at=?,category=?,confidence=?,importance=?,source_kind=?,source_reference=? WHERE id=?"));
        update.addBindValue(source.kind.isEmpty()?QStringLiteral("long_term"):source.kind); update.addBindValue(content);
        update.addBindValue(source.sourceEventId); update.addBindValue(now); update.addBindValue(source.category);
        update.addBindValue(confidence); update.addBindValue(importance); update.addBindValue(source.sourceKind); update.addBindValue(source.sourceReference); update.addBindValue(source.id);
        if (!update.exec()) return Result<qint64>::failure(memoryError(AppErrorCode::DatabaseQuery,
            QStringLiteral("无法更新记忆"), update.lastError().text(), QStringLiteral("memory.upsert")));
        if (update.numRowsAffected() <= 0) return Result<qint64>::failure(memoryError(AppErrorCode::NotFound,
            QStringLiteral("记忆不存在"), QStringLiteral("memory id not found"), QStringLiteral("memory.upsert")));
        return Result<qint64>::success(source.id);
    }
    if (find.next()) {
        const qint64 id=find.value(0).toLongLong(); QSqlQuery update(database_->connection());
        update.prepare(QStringLiteral("UPDATE memories SET content=?,source_event_id=COALESCE(NULLIF(?,''),source_event_id),updated_at=?,category=?,confidence=?,importance=?,source_kind=?,source_reference=? WHERE id=?"));
        update.addBindValue(content); update.addBindValue(source.sourceEventId); update.addBindValue(now); update.addBindValue(source.category); update.addBindValue(confidence); update.addBindValue(importance); update.addBindValue(source.sourceKind); update.addBindValue(source.sourceReference); update.addBindValue(id);
        if (!update.exec()) return Result<qint64>::failure(memoryError(AppErrorCode::DatabaseQuery,
            QStringLiteral("无法更新重复记忆"), update.lastError().text(), QStringLiteral("memory.upsert")));
        return Result<qint64>::success(id);
    }
    QSqlQuery insert(database_->connection()); insert.prepare(QStringLiteral(
        "INSERT INTO memories(kind,content,source_event_id,created_at,expires_at,updated_at,category,confidence,importance,source_kind,source_reference) VALUES(?,?,?,?,NULL,?,?,?,?,?,?)"));
    insert.addBindValue(source.kind.isEmpty()?QStringLiteral("long_term"):source.kind); insert.addBindValue(content); insert.addBindValue(source.sourceEventId); insert.addBindValue(now); insert.addBindValue(now); insert.addBindValue(source.category); insert.addBindValue(confidence); insert.addBindValue(importance); insert.addBindValue(source.sourceKind); insert.addBindValue(source.sourceReference);
    if (!insert.exec()) return Result<qint64>::failure(memoryError(AppErrorCode::DatabaseQuery,
        QStringLiteral("无法保存长期记忆"), insert.lastError().text(), QStringLiteral("memory.upsert")));
    return Result<qint64>::success(insert.lastInsertId().toLongLong());
}

Result<std::optional<MemoryItem>> SqliteMemoryRepository::conversationSummaryResult(
    const QString& conversationId) const
{
    if (database_ == nullptr || !database_->isOpen()) return Result<std::optional<MemoryItem>>::failure(memoryError(
        AppErrorCode::DatabaseUnavailable, QStringLiteral("database is not available"), QStringLiteral("database is not open"), QStringLiteral("memory.summary")));
    QSqlQuery query(database_->connection()); query.prepare(QStringLiteral(
        "SELECT id,kind,content,source_event_id,created_at,expires_at,updated_at,category,confidence,importance,source_kind,source_reference "
        "FROM memories WHERE kind='long_term' AND category='conversation_summary' AND source_kind='conversation' AND source_reference=? LIMIT 1"));
    query.addBindValue(conversationId);
    if (!query.exec()) return Result<std::optional<MemoryItem>>::failure(memoryError(AppErrorCode::DatabaseQuery,
        QStringLiteral("无法读取会话摘要"), query.lastError().text(), QStringLiteral("memory.summary")));
    if (!query.next()) return Result<std::optional<MemoryItem>>::success(std::nullopt);
    MemoryItem item; item.id=query.value(0).toLongLong(); item.kind=query.value(1).toString(); item.content=query.value(2).toString(); item.sourceEventId=query.value(3).toString();
    item.createdAt=parseTime(query.value(4)); item.expiresAt=parseTime(query.value(5)); item.updatedAt=parseTime(query.value(6)); item.category=query.value(7).toString();
    item.confidence=query.value(8).toDouble(); item.importance=query.value(9).toDouble(); item.sourceKind=query.value(10).toString(); item.sourceReference=query.value(11).toString();
    return Result<std::optional<MemoryItem>>::success(item);
}

Result<QVector<MemoryItem>> SqliteMemoryRepository::searchResult(
    const QString& text, int limit) const
{
    QVector<MemoryItem> result;
    if (database_ == nullptr || !database_->isOpen()) return Result<QVector<MemoryItem>>::failure(
        memoryError(AppErrorCode::DatabaseUnavailable, QStringLiteral("database is not available"),
                    QStringLiteral("database is not open"), QStringLiteral("memory.search")));
    if (limit <= 0) return Result<QVector<MemoryItem>>::failure(
        memoryError(AppErrorCode::InvalidArgument, QStringLiteral("检索参数无效"),
                    QStringLiteral("query is empty or limit <= 0"), QStringLiteral("memory.search")));
    QSqlQuery query(database_->connection());
    query.prepare(QStringLiteral("SELECT id,kind,content,source_event_id,created_at,expires_at,updated_at,category,confidence,importance,source_kind,source_reference FROM memories "
                                 "WHERE (?='' OR content LIKE ? ESCAPE '\\') "
                                 "AND (expires_at IS NULL OR expires_at > ?) "
                                 "ORDER BY importance DESC,updated_at DESC,id DESC LIMIT ?"));
    QString pattern = text; pattern.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    pattern.replace(QStringLiteral("%"), QStringLiteral("\\%"));
    pattern.replace(QStringLiteral("_"), QStringLiteral("\\_"));
    query.addBindValue(text.trimmed()); query.addBindValue(QStringLiteral("%") + pattern + QStringLiteral("%"));
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    query.addBindValue(limit);
    if (!query.exec()) return Result<QVector<MemoryItem>>::failure(memoryError(
        AppErrorCode::DatabaseQuery, QStringLiteral("无法检索记忆"), query.lastError().text(),
        QStringLiteral("memory.search")));
    while (query.next()) {
        MemoryItem item; item.id = query.value(0).toLongLong(); item.kind = query.value(1).toString();
        item.content = query.value(2).toString(); item.sourceEventId = query.value(3).toString();
        item.createdAt = parseTime(query.value(4)); item.expiresAt = parseTime(query.value(5));
        item.updatedAt = parseTime(query.value(6)); item.category = query.value(7).toString();
        item.confidence = query.value(8).toDouble(); item.importance = query.value(9).toDouble();
        item.sourceKind = query.value(10).toString(); item.sourceReference = query.value(11).toString();
        result.append(item);
    }
    return Result<QVector<MemoryItem>>::success(result);
}

Result<QVector<ConversationMessage>> SqliteMemoryRepository::searchConversationMessagesResult(
    const QString& text, int limit) const
{
    QVector<ConversationMessage> result;
    if (database_ == nullptr || !database_->isOpen()) return Result<QVector<ConversationMessage>>::failure(
        memoryError(AppErrorCode::DatabaseUnavailable, QStringLiteral("database is not available"),
                    QStringLiteral("database is not open"), QStringLiteral("memory.search_messages")));
    if (text.trimmed().isEmpty() || limit <= 0) return Result<QVector<ConversationMessage>>::failure(
        memoryError(AppErrorCode::InvalidArgument, QStringLiteral("检索参数无效"),
                    QStringLiteral("query is empty or limit <= 0"), QStringLiteral("memory.search_messages")));
    QString pattern = text; pattern.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    pattern.replace(QStringLiteral("%"), QStringLiteral("\\%"));
    pattern.replace(QStringLiteral("_"), QStringLiteral("\\_"));
    QSqlQuery query(database_->connection());
    const bool asciiQuery = std::all_of(text.cbegin(), text.cend(),
                                        [](const QChar character) { return character.unicode() < 128; });
    if (database_->hasFts5() && asciiQuery) {
        QString ftsText = text.trimmed();
        ftsText.replace(QStringLiteral("\""), QStringLiteral("\"\""));
        query.prepare(QStringLiteral(
            "SELECT m.id,m.conversation_id,m.role,m.content,m.token_count,m.created_at,m.summarized_at "
            "FROM conversation_messages_fts f JOIN conversation_messages m ON m.id=f.rowid "
            "WHERE conversation_messages_fts MATCH ? ORDER BY m.created_at DESC,m.id DESC LIMIT ?"));
        query.addBindValue(QStringLiteral("\"") + ftsText + QStringLiteral("\""));
        query.addBindValue(limit);
    } else {
        query.prepare(QStringLiteral("SELECT id,conversation_id,role,content,token_count,created_at,summarized_at "
                                     "FROM conversation_messages WHERE content LIKE ? ESCAPE '\\' "
                                     "ORDER BY created_at DESC,id DESC LIMIT ?"));
        query.addBindValue(QStringLiteral("%") + pattern + QStringLiteral("%")); query.addBindValue(limit);
    }
    if (!query.exec()) return Result<QVector<ConversationMessage>>::failure(memoryError(
        AppErrorCode::DatabaseQuery, QStringLiteral("无法检索历史消息"), query.lastError().text(),
        QStringLiteral("memory.search_messages")));
    while (query.next()) {
        ConversationMessage item; item.id = query.value(0).toLongLong();
        item.conversationId = query.value(1).toString();
        item.message = Message::create(parseRole(query.value(2).toString()), query.value(3).toString());
        item.tokenCount = query.value(4).toInt(); item.createdAt = parseTime(query.value(5));
        item.summarizedAt = parseTime(query.value(6)); result.append(item);
    }
    return Result<QVector<ConversationMessage>>::success(result);
}

Result<QVector<MemoryItem>> SqliteMemoryRepository::searchShortTermResult(
    const QString& query, int limit) const
{
    return searchByKindResult(query, QStringLiteral("short_term"), limit);
}

Result<QVector<MemoryItem>> SqliteMemoryRepository::searchLongTermResult(
    const QString& query, int limit) const
{
    return searchByKindResult(query, QStringLiteral("long_term"), limit);
}

Result<QVector<MemoryItem>> SqliteMemoryRepository::searchByKindResult(
    const QString& text, const QString& kind, int limit) const
{
    QVector<MemoryItem> result;
    if (database_ == nullptr || !database_->isOpen()) return Result<QVector<MemoryItem>>::failure(
        memoryError(AppErrorCode::DatabaseUnavailable, QStringLiteral("database is not available"),
                    QStringLiteral("database is not open"), QStringLiteral("memory.search_by_kind")));
    if (limit <= 0) return Result<QVector<MemoryItem>>::failure(
        memoryError(AppErrorCode::InvalidArgument, QStringLiteral("检索参数无效"),
                    QStringLiteral("query is empty or limit <= 0"), QStringLiteral("memory.search_by_kind")));
    QSqlQuery query(database_->connection());
    QString pattern = text; pattern.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    pattern.replace(QStringLiteral("%"), QStringLiteral("\\%"));
    pattern.replace(QStringLiteral("_"), QStringLiteral("\\_"));
    const bool asciiQuery = std::all_of(text.cbegin(), text.cend(),
                                        [](const QChar character) { return character.unicode() < 128; });
    if (!text.trimmed().isEmpty() && database_->hasFts5() && asciiQuery) {
        QString ftsText = text.trimmed();
        ftsText.replace(QStringLiteral("\""), QStringLiteral("\"\""));
        query.prepare(QStringLiteral(
            "SELECT m.id,m.kind,m.content,m.source_event_id,m.created_at,m.expires_at,m.updated_at,m.category,m.confidence,m.importance,m.source_kind,m.source_reference "
            "FROM memories_fts f JOIN memories m ON m.id=f.rowid "
            "WHERE memories_fts MATCH ? AND m.kind=? "
            "AND (m.expires_at IS NULL OR m.expires_at > ?) "
            "ORDER BY m.created_at DESC,m.id DESC LIMIT ?"));
        query.addBindValue(QStringLiteral("\"") + ftsText + QStringLiteral("\""));
        query.addBindValue(kind);
        query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        query.addBindValue(limit);
    } else {
        query.prepare(QStringLiteral("SELECT id,kind,content,source_event_id,created_at,expires_at,updated_at,category,confidence,importance,source_kind,source_reference FROM memories "
                                     "WHERE kind=? AND (?='' OR content LIKE ? ESCAPE '\\') "
                                     "AND (expires_at IS NULL OR expires_at > ?) "
                                     "ORDER BY created_at DESC,id DESC LIMIT ?"));
        query.addBindValue(kind);
        query.addBindValue(text.trimmed());
        query.addBindValue(QStringLiteral("%") + pattern + QStringLiteral("%"));
        query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        query.addBindValue(limit);
    }
    if (!query.exec()) return Result<QVector<MemoryItem>>::failure(memoryError(
        AppErrorCode::DatabaseQuery, QStringLiteral("无法按类型检索记忆"), query.lastError().text(),
        QStringLiteral("memory.search_by_kind")));
    while (query.next()) {
        MemoryItem item;
        item.id = query.value(0).toLongLong(); item.kind = query.value(1).toString();
        item.content = query.value(2).toString(); item.sourceEventId = query.value(3).toString();
        item.createdAt = parseTime(query.value(4)); item.expiresAt = parseTime(query.value(5));
        item.updatedAt = parseTime(query.value(6)); item.category = query.value(7).toString();
        item.confidence = query.value(8).toDouble(); item.importance = query.value(9).toDouble();
        item.sourceKind = query.value(10).toString(); item.sourceReference = query.value(11).toString();
        result.append(item);
    }
    return Result<QVector<MemoryItem>>::success(result);
}

} // namespace zhu_screen_pet
