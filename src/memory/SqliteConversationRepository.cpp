#include "memory/SqliteConversationRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QUuid>

#include "infrastructure/Database.h"

namespace zhu_screen_pet {
namespace {

QDateTime parseTime(const QVariant& value)
{
    return QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
}

QString nowString()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

AppError dbError(AppErrorCode code, const QString& message, const QString& technical,
                 const QString& operation)
{
    AppError error;
    error.code = code;
    error.message = message;
    error.domain = ErrorDomain::Database;
    error.technicalMessage = technical;
    error.operation = operation;
    return error;
}

MessageRole parseRole(const QString& role)
{
    if (role == QStringLiteral("system")) return MessageRole::System;
    if (role == QStringLiteral("assistant")) return MessageRole::Assistant;
    return MessageRole::User;
}

} // namespace

SqliteConversationRepository::SqliteConversationRepository(Database* database)
    : database_(database)
{
}

Result<QString> SqliteConversationRepository::createConversationResult(const QString& title)
{
    if (database_ == nullptr || !database_->isOpen()) {
        return Result<QString>::failure(dbError(AppErrorCode::DatabaseUnavailable,
            QStringLiteral("database is not available"), QStringLiteral("database is not open"),
            QStringLiteral("conversation.create")));
    }
    const QString id = QUuid::createUuid().toString(QUuid::Id128);
    const QString now = nowString();
    QSqlQuery query(database_->connection());
    query.prepare(QStringLiteral("INSERT INTO conversations(id,title,created_at,updated_at) "
                                 "VALUES(?,?,?,?)"));
    query.addBindValue(id);
    query.addBindValue(title.trimmed().isEmpty() ? QStringLiteral("新会话") : title.trimmed());
    query.addBindValue(now);
    query.addBindValue(now);
    if (!query.exec()) {
        return Result<QString>::failure(dbError(AppErrorCode::DatabaseQuery,
            QStringLiteral("无法创建会话"), query.lastError().text(),
            QStringLiteral("conversation.create")));
    }
    return Result<QString>::success(id);
}

Result<Conversation> SqliteConversationRepository::getConversationResult(const QString& id) const
{
    if (database_ == nullptr || !database_->isOpen()) {
        return Result<Conversation>::failure(dbError(AppErrorCode::DatabaseUnavailable,
            QStringLiteral("database is not available"), QStringLiteral("database is not open"),
            QStringLiteral("conversation.get")));
    }
    QSqlQuery query(database_->connection());
    query.prepare(QStringLiteral("SELECT id,title,created_at,updated_at,archived_at "
                                 "FROM conversations WHERE id=?"));
    query.addBindValue(id);
    if (!query.exec()) return Result<Conversation>::failure(dbError(AppErrorCode::DatabaseQuery,
        QStringLiteral("无法读取会话"), query.lastError().text(), QStringLiteral("conversation.get")));
    if (!query.next()) return Result<Conversation>::failure(dbError(AppErrorCode::NotFound,
        QStringLiteral("会话不存在"), QStringLiteral("conversation id not found: %1").arg(id),
        QStringLiteral("conversation.get")));
    Conversation result;
    result.id = query.value(0).toString();
    result.title = query.value(1).toString();
    result.createdAt = parseTime(query.value(2));
    result.updatedAt = parseTime(query.value(3));
    result.archivedAt = parseTime(query.value(4));
    return Result<Conversation>::success(result);
}

Result<QVector<Conversation>> SqliteConversationRepository::listConversationsResult(bool includeArchived) const
{
    QVector<Conversation> result;
    if (database_ == nullptr || !database_->isOpen()) return Result<QVector<Conversation>>::failure(
        dbError(AppErrorCode::DatabaseUnavailable, QStringLiteral("database is not available"),
                QStringLiteral("database is not open"), QStringLiteral("conversation.list")));
    QSqlQuery query(database_->connection());
    query.prepare(includeArchived
        ? QStringLiteral("SELECT id,title,created_at,updated_at,archived_at FROM conversations "
                         "ORDER BY updated_at DESC")
        : QStringLiteral("SELECT id,title,created_at,updated_at,archived_at FROM conversations "
                         "WHERE archived_at IS NULL ORDER BY updated_at DESC"));
    if (!query.exec()) return Result<QVector<Conversation>>::failure(dbError(
        AppErrorCode::DatabaseQuery, QStringLiteral("无法读取会话列表"), query.lastError().text(),
        QStringLiteral("conversation.list")));
    while (query.next()) {
        Conversation item;
        item.id = query.value(0).toString(); item.title = query.value(1).toString();
        item.createdAt = parseTime(query.value(2)); item.updatedAt = parseTime(query.value(3));
        item.archivedAt = parseTime(query.value(4)); result.append(item);
    }
    return Result<QVector<Conversation>>::success(result);
}

Result<void> SqliteConversationRepository::archiveConversationResult(const QString& id)
{
    if (database_ == nullptr || !database_->isOpen()) return Result<void>::failure(dbError(
        AppErrorCode::DatabaseUnavailable, QStringLiteral("database is not available"),
        QStringLiteral("database is not open"), QStringLiteral("conversation.archive")));
    QSqlQuery query(database_->connection());
    query.prepare(QStringLiteral("UPDATE conversations SET archived_at=?, updated_at=? WHERE id=?"));
    const QString now = nowString(); query.addBindValue(now); query.addBindValue(now); query.addBindValue(id);
    if (!query.exec()) return Result<void>::failure(dbError(AppErrorCode::DatabaseQuery,
        QStringLiteral("无法归档会话"), query.lastError().text(), QStringLiteral("conversation.archive")));
    if (query.numRowsAffected() <= 0) return Result<void>::failure(dbError(AppErrorCode::NotFound,
        QStringLiteral("会话不存在"), QStringLiteral("conversation id not found: %1").arg(id),
        QStringLiteral("conversation.archive")));
    return Result<void>::success();
}

Result<void> SqliteConversationRepository::deleteConversationResult(const QString& id)
{
    if (database_ == nullptr || !database_->isOpen()) return Result<void>::failure(dbError(
        AppErrorCode::DatabaseUnavailable, QStringLiteral("database is not available"),
        QStringLiteral("database is not open"), QStringLiteral("conversation.delete")));
    QSqlQuery query(database_->connection());
    query.prepare(QStringLiteral("DELETE FROM conversations WHERE id=?"));
    query.addBindValue(id);
    if (!query.exec()) return Result<void>::failure(dbError(AppErrorCode::DatabaseQuery,
        QStringLiteral("无法删除会话"), query.lastError().text(), QStringLiteral("conversation.delete")));
    if (query.numRowsAffected() <= 0) return Result<void>::failure(dbError(AppErrorCode::NotFound,
        QStringLiteral("会话不存在"), QStringLiteral("conversation id not found: %1").arg(id),
        QStringLiteral("conversation.delete")));
    return Result<void>::success();
}

Result<void> SqliteConversationRepository::appendMessageResult(const QString& conversationId,
                                                                const Message& message,
                                                                int tokenCount)
{
    if (database_ == nullptr || !database_->isOpen()) return Result<void>::failure(dbError(
        AppErrorCode::DatabaseUnavailable, QStringLiteral("database is not available"),
        QStringLiteral("database is not open"), QStringLiteral("conversation.append_message")));
    QSqlDatabase db = database_->connection();
    if (!db.transaction()) return Result<void>::failure(dbError(AppErrorCode::DatabaseQuery,
        QStringLiteral("无法开始保存消息事务"), db.lastError().text(), QStringLiteral("conversation.append_message")));
    QSqlQuery query(db);
    query.prepare(QStringLiteral("INSERT INTO conversation_messages "
                                 "(conversation_id,role,content,token_count,created_at) VALUES(?,?,?,?,?)"));
    query.addBindValue(conversationId); query.addBindValue(messageRoleName(message.role));
    query.addBindValue(message.content); query.addBindValue(qMax(0, tokenCount)); query.addBindValue(nowString());
    bool ok = query.exec();
    QString technicalError;
    if (!ok) technicalError = query.lastError().text();
    if (ok) {
        query.prepare(QStringLiteral("UPDATE conversations SET updated_at=? WHERE id=?"));
        query.addBindValue(nowString()); query.addBindValue(conversationId); ok = query.exec();
        if (!ok) technicalError = query.lastError().text();
    }
    if (!ok || !db.commit()) {
        if (technicalError.isEmpty()) technicalError = db.lastError().text();
        db.rollback();
        return Result<void>::failure(dbError(AppErrorCode::DatabaseQuery,
            QStringLiteral("无法保存消息"), technicalError, QStringLiteral("conversation.append_message")));
    }
    return Result<void>::success();
}

Result<QVector<ConversationMessage>> SqliteConversationRepository::recentMessagesResult(
    const QString& conversationId, int limit) const
{
    QVector<ConversationMessage> result;
    if (database_ == nullptr || !database_->isOpen()) return Result<QVector<ConversationMessage>>::failure(
        dbError(AppErrorCode::DatabaseUnavailable, QStringLiteral("database is not available"),
                QStringLiteral("database is not open"), QStringLiteral("conversation.recent_messages")));
    if (limit <= 0) return Result<QVector<ConversationMessage>>::failure(dbError(
        AppErrorCode::InvalidArgument, QStringLiteral("消息条数必须大于 0"), QStringLiteral("limit <= 0"),
        QStringLiteral("conversation.recent_messages")));
    QSqlQuery query(database_->connection());
    query.prepare(QStringLiteral("SELECT id,conversation_id,role,content,token_count,created_at,summarized_at "
                                 "FROM conversation_messages WHERE conversation_id=? "
                                 "ORDER BY created_at DESC,id DESC LIMIT ?"));
    query.addBindValue(conversationId); query.addBindValue(limit);
    if (!query.exec()) return Result<QVector<ConversationMessage>>::failure(dbError(
        AppErrorCode::DatabaseQuery, QStringLiteral("无法读取会话消息"), query.lastError().text(),
        QStringLiteral("conversation.recent_messages")));
    while (query.next()) {
        ConversationMessage item;
        item.id = query.value(0).toLongLong(); item.conversationId = query.value(1).toString();
        item.message = Message::create(parseRole(query.value(2).toString()), query.value(3).toString());
        item.tokenCount = query.value(4).toInt(); item.createdAt = parseTime(query.value(5));
        item.summarizedAt = parseTime(query.value(6)); result.prepend(item);
    }
    return Result<QVector<ConversationMessage>>::success(result);
}

} // namespace zhu_screen_pet
