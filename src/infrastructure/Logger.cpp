#include "infrastructure/Logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QFileInfoList>

namespace zhu_screen_pet {

namespace {

/** 将日志级别转换为 JSON 中使用的稳定字符串。 */
QString levelName(Logger::Level level)
{
    switch (level) {
    case Logger::Level::Debug: return QStringLiteral("debug");
    case Logger::Level::Info: return QStringLiteral("info");
    case Logger::Level::Warning: return QStringLiteral("warning");
    case Logger::Level::Error: return QStringLiteral("error");
    }
    return QStringLiteral("unknown");
}

} // namespace

Logger::~Logger()
{
    if (file_ != nullptr) {
        file_->close();
        delete file_;
    }
}

bool Logger::initialize(const QString& directory, QString* errorMessage,
                        qint64 maxBytes, int maxFiles)
{
    if (!QDir().mkpath(directory)) {
        if (errorMessage) *errorMessage = QStringLiteral("cannot create log directory: %1").arg(directory);
        return false;
    }
    directory_ = directory;
    maxBytes_ = qMax<qint64>(1024, maxBytes);
    maxFiles_ = qMax(1, maxFiles);
    filePath_ = QDir(directory).filePath(
        QStringLiteral("zhu_screen_pet-%1.jsonl").arg(QDate::currentDate().toString(Qt::ISODate)));

    auto* file = new QFile(filePath_);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = file->errorString();
        }
        delete file;
        return false;
    }
    file_ = file;
    lastWriteSucceeded_ = true;
    lastWriteError_.clear();
    info(QStringLiteral("logger"), QStringLiteral("initialized"), filePath_);
    return true;
}

void Logger::log(Level level, const QString& module, const QString& event,
                 const QString& message, const QString& errorCode)
{
    QMutexLocker locker(&mutex_);
    if (file_ == nullptr) {
        lastWriteSucceeded_ = false;
        lastWriteError_ = QStringLiteral("logger is not initialized");
        return;
    }

    if (!rotateIfNeeded()) {
        lastWriteSucceeded_ = false;
        return;
    }

    QJsonObject object;
    object.insert(QStringLiteral("time"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    object.insert(QStringLiteral("level"), levelName(level));
    object.insert(QStringLiteral("module"), module);
    object.insert(QStringLiteral("event"), event);
    object.insert(QStringLiteral("message"), message);
    if (!errorCode.isEmpty()) {
        object.insert(QStringLiteral("error_code"), errorCode);
    }

    const QByteArray line = QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
    const qint64 written = file_->write(line);
    if (written != line.size() || !file_->flush()) {
        lastWriteSucceeded_ = false;
        lastWriteError_ = file_->errorString();
        return;
    }
    lastWriteSucceeded_ = true;
    lastWriteError_.clear();
}

void Logger::debug(const QString& module, const QString& event, const QString& message)
{ log(Level::Debug, module, event, message); }
void Logger::info(const QString& module, const QString& event, const QString& message)
{ log(Level::Info, module, event, message); }
void Logger::warning(const QString& module, const QString& event, const QString& message,
                     const QString& errorCode)
{ log(Level::Warning, module, event, message, errorCode); }
void Logger::error(const QString& module, const QString& event, const QString& message,
                   const QString& errorCode)
{ log(Level::Error, module, event, message, errorCode); }

bool Logger::lastWriteSucceeded() const
{
    QMutexLocker locker(&mutex_);
    return lastWriteSucceeded_;
}

QString Logger::lastWriteError() const
{
    QMutexLocker locker(&mutex_);
    return lastWriteError_;
}

bool Logger::rotateIfNeeded()
{
    if (file_ == nullptr || file_->size() < maxBytes_) return true;
    file_->close();
    const QString baseRotated = QStringLiteral("%1.%2").arg(filePath_,
        QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddHHmmsszzz")));
    QString rotated = baseRotated;
    int suffix = 1;
    while (QFile::exists(rotated)) rotated = baseRotated + QStringLiteral("-%1").arg(suffix++);
    if (!QFile::rename(filePath_, rotated)) {
        file_->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
        lastWriteError_ = QStringLiteral("cannot rotate log file: %1").arg(filePath_);
        return false;
    }
    if (!file_->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        lastWriteError_ = file_->errorString();
        return false;
    }
    pruneRotatedFiles();
    return true;
}

void Logger::pruneRotatedFiles()
{
    const QFileInfoList files = QDir(directory_).entryInfoList(
        QStringList() << QStringLiteral("zhu_screen_pet-*.jsonl.*"),
        QDir::Files, QDir::Time | QDir::Reversed);
    const int keep = qMax(0, maxFiles_ - 1);
    for (int i = 0; i < files.size() - keep; ++i) QFile::remove(files.at(i).absoluteFilePath());
}

} // namespace zhu_screen_pet
