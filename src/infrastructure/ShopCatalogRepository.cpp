#include "infrastructure/ShopCatalogRepository.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>

#include <utility>

namespace zhu_screen_pet {

ShopCatalogRepository::ShopCatalogRepository(QString filePath)
    : filePath_(std::move(filePath))
{
}

bool ShopCatalogRepository::load(QVector<ShopItemDefinition>* items,
                                 QString* errorMessage) const
{
    const auto fail = [errorMessage](const QString& message) {
        if (errorMessage != nullptr) *errorMessage = message;
        return false;
    };
    if (items == nullptr || filePath_.isEmpty()) {
        return fail(QStringLiteral("shop catalog output or path is invalid"));
    }
    QFile file(filePath_);
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(QStringLiteral("cannot open shop catalog: %1").arg(file.errorString()));
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return fail(QStringLiteral("invalid shop catalog JSON: %1")
                        .arg(parseError.errorString()));
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt() != 1) {
        return fail(QStringLiteral("unsupported shop catalog schema"));
    }
    if (!root.contains(QStringLiteral("gifts"))
        || !root.value(QStringLiteral("gifts")).isArray()
        || !root.contains(QStringLiteral("foods"))
        || !root.value(QStringLiteral("foods")).isArray()) {
        return fail(QStringLiteral("shop catalog gifts and foods must be arrays"));
    }

    QVector<ShopItemDefinition> loaded;
    QSet<QString> ids;
    const auto appendItems = [&](const QJsonArray& array, ShopItemKind kind,
                                 const QString& group) -> bool {
        for (const QJsonValue& value : array) {
            if (!value.isObject()) return false;
            const QJsonObject object = value.toObject();
            ShopItemDefinition item;
            item.id = object.value(QStringLiteral("id")).toString().trimmed();
            item.kind = kind;
            item.emoji = object.value(QStringLiteral("emoji")).toString().trimmed();
            item.name = object.value(QStringLiteral("name")).toString().trimmed();
            item.price = object.value(QStringLiteral("price")).toInt();
            item.affectionGain = object.value(QStringLiteral("affection_gain")).toInt();
            item.satietyGain = object.value(QStringLiteral("satiety_gain")).toInt();
            const int effect = kind == ShopItemKind::Gift
                ? item.affectionGain : item.satietyGain;
            if (item.id.isEmpty() || item.id.contains(QLatin1Char('/'))
                || ids.contains(item.id) || item.emoji.isEmpty() || item.name.isEmpty()
                || item.price <= 0 || item.price > 100000 || effect <= 0) {
                if (errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("invalid or duplicate %1 item: %2")
                        .arg(group, item.id);
                }
                return false;
            }
            ids.insert(item.id);
            loaded.append(item);
        }
        return true;
    };
    if (!appendItems(root.value(QStringLiteral("gifts")).toArray(),
                     ShopItemKind::Gift, QStringLiteral("gift"))
        || !appendItems(root.value(QStringLiteral("foods")).toArray(),
                        ShopItemKind::Food, QStringLiteral("food"))) {
        return false;
    }
    *items = loaded;
    return true;
}

} // namespace zhu_screen_pet
