#include "app/ModelErrorPresenter.h"

#include <utility>

namespace zhu_screen_pet {

ModelErrorPresenter::ModelErrorPresenter(QHash<QString, QString> messages)
    : messages_(std::move(messages))
{
}

QString ModelErrorPresenter::message(const ModelError& error) const
{
    QString configured = messages_.value(
        error.domainName() + QLatin1Char('.') + error.codeName()).trimmed();
    if (configured.isEmpty()) configured = messages_.value(error.codeName()).trimmed();
    if (!configured.isEmpty()) return configured;
    const QString unknown = messages_.value(QStringLiteral("unknown")).trimmed();
    if (!unknown.isEmpty()) return unknown;
    return error.message.trimmed().isEmpty() ? QStringLiteral("操作失败，请稍后重试。") : error.message;
}

} // namespace zhu_screen_pet
