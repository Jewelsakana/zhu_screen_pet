#include "memory/SqliteMemoryRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <algorithm>

#include "infrastructure/Database.h"

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
    item.expiresAt = expiresAt;
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
    query.addBindValue(item.content); query.addBindValue(item.sourceEventId);
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

Result<QVector<MemoryItem>> SqliteMemoryRepository::searchResult(
    const QString& text, int limit) const
{
    QVector<MemoryItem> result;
    if (database_ == nullptr || !database_->isOpen()) return Result<QVector<MemoryItem>>::failure(
        memoryError(AppErrorCode::DatabaseUnavailable, QStringLiteral("database is not available"),
                    QStringLiteral("database is not open"), QStringLiteral("memory.search")));
    if (text.trimmed().isEmpty() || limit <= 0) return Result<QVector<MemoryItem>>::failure(
        memoryError(AppErrorCode::InvalidArgument, QStringLiteral("检索参数无效"),
                    QStringLiteral("query is empty or limit <= 0"), QStringLiteral("memory.search")));
    QSqlQuery query(database_->connection());
    query.prepare(QStringLiteral("SELECT id,kind,content,source_event_id,created_at,expires_at FROM memories "
                                 "WHERE content LIKE ? ESCAPE '\\' "
                                 "AND (expires_at IS NULL OR expires_at > ?) "
                                 "ORDER BY created_at DESC,id DESC LIMIT ?"));
    QString pattern = text; pattern.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    pattern.replace(QStringLiteral("%"), QStringLiteral("\\%"));
    pattern.replace(QStringLiteral("_"), QStringLiteral("\\_"));
    query.addBindValue(QStringLiteral("%") + pattern + QStringLiteral("%"));
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    query.addBindValue(limit);
    if (!query.exec()) return Result<QVector<MemoryItem>>::failure(memoryError(
        AppErrorCode::DatabaseQuery, QStringLiteral("无法检索记忆"), query.lastError().text(),
        QStringLiteral("memory.search")));
    while (query.next()) {
        MemoryItem item; item.id = query.value(0).toLongLong(); item.kind = query.value(1).toString();
        item.content = query.value(2).toString(); item.sourceEventId = query.value(3).toString();
        item.createdAt = parseTime(query.value(4)); item.expiresAt = parseTime(query.value(5));
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
    if (text.trimmed().isEmpty() || limit <= 0) return Result<QVector<MemoryItem>>::failure(
        memoryError(AppErrorCode::InvalidArgument, QStringLiteral("检索参数无效"),
                    QStringLiteral("query is empty or limit <= 0"), QStringLiteral("memory.search_by_kind")));
    QSqlQuery query(database_->connection());
    QString pattern = text; pattern.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    pattern.replace(QStringLiteral("%"), QStringLiteral("\\%"));
    pattern.replace(QStringLiteral("_"), QStringLiteral("\\_"));
    const bool asciiQuery = std::all_of(text.cbegin(), text.cend(),
                                        [](const QChar character) { return character.unicode() < 128; });
    if (database_->hasFts5() && asciiQuery) {
        QString ftsText = text.trimmed();
        ftsText.replace(QStringLiteral("\""), QStringLiteral("\"\""));
        query.prepare(QStringLiteral(
            "SELECT m.id,m.kind,m.content,m.source_event_id,m.created_at,m.expires_at "
            "FROM memories_fts f JOIN memories m ON m.id=f.rowid "
            "WHERE memories_fts MATCH ? AND m.kind=? "
            "AND (m.expires_at IS NULL OR m.expires_at > ?) "
            "ORDER BY m.created_at DESC,m.id DESC LIMIT ?"));
        query.addBindValue(QStringLiteral("\"") + ftsText + QStringLiteral("\""));
        query.addBindValue(kind);
        query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        query.addBindValue(limit);
    } else {
        query.prepare(QStringLiteral("SELECT id,kind,content,source_event_id,created_at,expires_at FROM memories "
                                     "WHERE kind=? AND content LIKE ? ESCAPE '\\' "
                                     "AND (expires_at IS NULL OR expires_at > ?) "
                                     "ORDER BY created_at DESC,id DESC LIMIT ?"));
        query.addBindValue(kind);
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
        result.append(item);
    }
    return Result<QVector<MemoryItem>>::success(result);
}

} // namespace zhu_screen_pet
