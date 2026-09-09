#include "memory/SqliteObservationRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

#include "infrastructure/Database.h"

namespace zhu_screen_pet {
namespace {

QString serializeTime(const QDateTime& value)
{
    return value.toUTC().toString(Qt::ISODateWithMs);
}

QDateTime parseTime(const QVariant& value)
{
    return QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
}

AppError observationError(AppErrorCode code, const QString& message,
                          const QString& technical, const QString& operation)
{
    return {code, message, 0, ErrorDomain::Database, technical, operation, {}, false};
}

ObservationEvent readObservation(const QSqlQuery& query)
{
    ObservationEvent observation;
    observation.id = query.value(0).toString();
    observation.conversationId = query.value(1).toString();
    observation.summary = query.value(2).toString(); observation.fingerprint = query.value(3).toByteArray();
    observation.capturedAt = parseTime(query.value(4)); observation.expiresAt = parseTime(query.value(5));
    observation.captureId = query.value(6).toString(); observation.source = query.value(7).toString();
    observation.appHint = query.value(8).toString(); observation.modelRequestId = query.value(9).toString();
    observation.modelProvider = query.value(10).toString(); observation.imageFormat = query.value(11).toString();
    observation.imageSize = QSize(query.value(12).toInt(), query.value(13).toInt()); observation.durationMs = query.value(14).toInt();
    return observation;
}

} // namespace

SqliteObservationRepository::SqliteObservationRepository(Database* database)
    : database_(database)
{
}

Result<QString> SqliteObservationRepository::saveResult(const ObservationEvent& observation)
{
    if (database_ == nullptr || !database_->isOpen()) {
        return Result<QString>::failure(observationError(
            AppErrorCode::DatabaseUnavailable, QStringLiteral("观察数据库不可用"),
            QStringLiteral("database is not open"), QStringLiteral("observation.save")));
    }
    const QDateTime capturedAt = observation.capturedAt.isValid()
        ? observation.capturedAt.toUTC() : QDateTime::currentDateTimeUtc();
    const QDateTime expiresAt = observation.expiresAt.isValid()
        ? observation.expiresAt.toUTC() : capturedAt.addSecs(ObservationEvent::DefaultTtlSeconds);
    if (observation.conversationId.trimmed().isEmpty()
        || observation.summary.trimmed().isEmpty() || observation.fingerprint.isEmpty()
        || expiresAt <= capturedAt) {
        return Result<QString>::failure(observationError(
            AppErrorCode::InvalidArgument, QStringLiteral("观察事件参数无效"),
            QStringLiteral("conversation, summary, fingerprint and a future expiry are required"),
            QStringLiteral("observation.save")));
    }

    const QString id = observation.id.trimmed().isEmpty()
        ? QUuid::createUuid().toString(QUuid::Id128) : observation.id.trimmed();
    QSqlQuery query(database_->connection());
    query.prepare(QStringLiteral(
        "INSERT INTO observation_events(id,conversation_id,summary,fingerprint,captured_at,expires_at,capture_id,source,app_hint,model_request_id,model_provider,image_format,image_width,image_height,duration_ms) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
    query.addBindValue(id);
    query.addBindValue(observation.conversationId);
    query.addBindValue(observation.summary.trimmed());
    query.addBindValue(observation.fingerprint);
    query.addBindValue(serializeTime(capturedAt));
    query.addBindValue(serializeTime(expiresAt));
    query.addBindValue(observation.captureId.trimmed().isEmpty() ? id : observation.captureId.trimmed());
    query.addBindValue(observation.source.trimmed().isEmpty() ? QStringLiteral("primary_screen") : observation.source.trimmed());
    query.addBindValue(observation.appHint.isNull() ? QStringLiteral("") : observation.appHint);
    query.addBindValue(observation.modelRequestId.isNull() ? QStringLiteral("") : observation.modelRequestId);
    query.addBindValue(observation.modelProvider.isNull() ? QStringLiteral("") : observation.modelProvider);
    query.addBindValue(observation.imageFormat.isNull() ? QStringLiteral("") : observation.imageFormat);
    query.addBindValue(observation.imageSize.width()); query.addBindValue(observation.imageSize.height());
    query.addBindValue(qMax(0, observation.durationMs));
    if (!query.exec()) {
        return Result<QString>::failure(observationError(
            AppErrorCode::DatabaseQuery, QStringLiteral("无法保存屏幕观察"),
            query.lastError().text(), QStringLiteral("observation.save")));
    }
    return Result<QString>::success(id);
}

Result<std::optional<ObservationEvent>> SqliteObservationRepository::latestValidResult(
    const QString& conversationId, const QDateTime& at) const
{
    if (database_ == nullptr || !database_->isOpen()) {
        return Result<std::optional<ObservationEvent>>::failure(observationError(
            AppErrorCode::DatabaseUnavailable, QStringLiteral("观察数据库不可用"),
            QStringLiteral("database is not open"), QStringLiteral("observation.latest")));
    }
    if (conversationId.trimmed().isEmpty() || !at.isValid()) {
        return Result<std::optional<ObservationEvent>>::failure(observationError(
            AppErrorCode::InvalidArgument, QStringLiteral("观察查询参数无效"),
            QStringLiteral("conversation id and query time are required"),
            QStringLiteral("observation.latest")));
    }
    QSqlQuery query(database_->connection());
    query.prepare(QStringLiteral(
        "SELECT id,conversation_id,summary,fingerprint,captured_at,expires_at,capture_id,source,app_hint,model_request_id,model_provider,image_format,image_width,image_height,duration_ms "
        "FROM observation_events WHERE conversation_id=? AND expires_at>? "
        "ORDER BY captured_at DESC,id DESC LIMIT 1"));
    query.addBindValue(conversationId);
    query.addBindValue(serializeTime(at));
    if (!query.exec()) {
        return Result<std::optional<ObservationEvent>>::failure(observationError(
            AppErrorCode::DatabaseQuery, QStringLiteral("无法读取最近屏幕观察"),
            query.lastError().text(), QStringLiteral("observation.latest")));
    }
    if (!query.next()) {
        return Result<std::optional<ObservationEvent>>::success(std::nullopt);
    }
    return Result<std::optional<ObservationEvent>>::success(readObservation(query));
}

Result<void> SqliteObservationRepository::removeExpiredResult(const QDateTime& at)
{
    if (database_ == nullptr || !database_->isOpen()) {
        return Result<void>::failure(observationError(
            AppErrorCode::DatabaseUnavailable, QStringLiteral("观察数据库不可用"),
            QStringLiteral("database is not open"), QStringLiteral("observation.remove_expired")));
    }
    if (!at.isValid()) {
        return Result<void>::failure(observationError(
            AppErrorCode::InvalidArgument, QStringLiteral("观察清理时间无效"),
            QStringLiteral("cleanup time is invalid"), QStringLiteral("observation.remove_expired")));
    }
    QSqlQuery query(database_->connection());
    query.prepare(QStringLiteral("DELETE FROM observation_events WHERE expires_at<=?"));
    query.addBindValue(serializeTime(at));
    if (!query.exec()) {
        return Result<void>::failure(observationError(
            AppErrorCode::DatabaseQuery, QStringLiteral("无法清理过期屏幕观察"),
            query.lastError().text(), QStringLiteral("observation.remove_expired")));
    }
    return Result<void>::success();
}

} // namespace zhu_screen_pet
