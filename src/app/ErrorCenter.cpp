#include "app/ErrorCenter.h"

#include "app/ModelErrorPresenter.h"
#include "infrastructure/Logger.h"

#include <QRegularExpression>

namespace zhu_screen_pet {

namespace {
QString redact(QString text)
{
    text.replace(QRegularExpression(QStringLiteral("(?i)(authorization\\s*:\\s*bearer\\s+)[^\\s]+")),
                 QStringLiteral("\\1<redacted>"));
    text.replace(QRegularExpression(QStringLiteral("\\bsk-[A-Za-z0-9_-]{8,}\\b")),
                 QStringLiteral("<redacted>"));
    return text;
}
}

ErrorCenter::ErrorCenter(Logger* logger, QObject* parent)
    : QObject(parent), logger_(logger)
{
    qRegisterMetaType<AppError>("AppError");
}

void ErrorCenter::setMessages(const QHash<QString, QString>& messages)
{
    messages_ = messages;
}

QString ErrorCenter::userMessage(const AppError& error) const
{
    return ModelErrorPresenter(messages_).message(error);
}

void ErrorCenter::report(const AppError& source)
{
    AppError error = source;
    if (error.domain == ErrorDomain::None) error.domain = ErrorDomain::Application;
    if (logger_ != nullptr) {
        const QString technical = error.technicalMessage.isEmpty()
            ? error.message : error.technicalMessage;
        logger_->error(error.domainName(), error.operation.isEmpty()
            ? QStringLiteral("operation_failed") : error.operation,
            redact(technical), error.codeName());
    }
    emit errorReported(error, userMessage(error));
}

} // namespace zhu_screen_pet
