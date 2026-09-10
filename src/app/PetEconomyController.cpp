#include "app/PetEconomyController.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <limits>

#include "app/AffectionController.h"
#include "app/InputActivityController.h"
#include "app/SatietyController.h"
#include "infrastructure/SettingsRepository.h"
#include "infrastructure/ShopCatalogRepository.h"

namespace zhu_screen_pet {
namespace {
constexpr auto InventoryKey = "inventory/items_json";
}

PetEconomyController::PetEconomyController(
    ShopCatalogRepository* catalogRepository, SettingsRepository* settings,
    InputActivityController* inputActivity, AffectionController* affection,
    SatietyController* satiety, QObject* parent)
    : QObject(parent), catalogRepository_(catalogRepository), settings_(settings),
      inputActivity_(inputActivity), affection_(affection), satiety_(satiety)
{
    if (inputActivity_ != nullptr) {
        connect(inputActivity_, &InputActivityController::countChanged,
                this, &PetEconomyController::balanceChanged);
    }
}

bool PetEconomyController::initialize(QString* errorMessage)
{
    if (initialized_) return true;
    if (catalogRepository_ == nullptr || settings_ == nullptr || inputActivity_ == nullptr
        || affection_ == nullptr || satiety_ == nullptr) {
        if (errorMessage != nullptr) *errorMessage = QStringLiteral("pet economy dependencies are unavailable");
        return false;
    }
    if (!catalogRepository_->load(&items_, errorMessage)) return false;
    const QByteArray stored = settings_->value(QString::fromLatin1(InventoryKey)).toByteArray();
    if (!stored.isEmpty()) {
        const QJsonDocument document = QJsonDocument::fromJson(stored);
        if (!document.isObject()) {
            if (errorMessage != nullptr) *errorMessage = QStringLiteral("inventory JSON is invalid");
            return false;
        }
        const QJsonObject object = document.object();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            const int quantity = it.value().toInt();
            if (quantity > 0 && findItem(it.key()) != nullptr) inventory_.insert(it.key(), quantity);
        }
    }
    initialized_ = true;
    return true;
}

bool PetEconomyController::shutdown(QString* errorMessage)
{
    return !initialized_ || saveInventory(errorMessage);
}

QVector<ShopItemDefinition> PetEconomyController::items(ShopItemKind kind) const
{
    QVector<ShopItemDefinition> result;
    for (const ShopItemDefinition& item : items_) {
        if (item.kind == kind) result.append(item);
    }
    return result;
}

QVector<InventoryEntry> PetEconomyController::inventory() const
{
    QVector<InventoryEntry> result;
    for (const ShopItemDefinition& item : items_) {
        const int quantity = inventory_.value(item.id);
        if (quantity > 0) result.append({item, quantity});
    }
    return result;
}

qint64 PetEconomyController::balance() const
{
    return inputActivity_ == nullptr ? 0 : inputActivity_->inputCount();
}

bool PetEconomyController::purchase(const QString& itemId, QString* userMessage)
{
    const ShopItemDefinition* item = findItem(itemId);
    if (!initialized_ || item == nullptr) {
        if (userMessage != nullptr) *userMessage = QStringLiteral("商品不存在");
        return false;
    }
    QString error;
    if (!inputActivity_->trySpend(item->price, &error)) {
        if (userMessage != nullptr) {
            *userMessage = error == QStringLiteral("余额不足") ? error
                : QStringLiteral("购买失败：%1").arg(error);
        }
        return false;
    }
    const int previous = inventory_.value(itemId);
    if (previous >= std::numeric_limits<int>::max()) {
        inputActivity_->refund(item->price, nullptr);
        if (userMessage != nullptr) *userMessage = QStringLiteral("该物品数量已达到上限");
        return false;
    }
    inventory_.insert(itemId, previous + 1);
    if (!saveInventory(&error)) {
        if (previous > 0) inventory_.insert(itemId, previous);
        else inventory_.remove(itemId);
        inputActivity_->refund(item->price, nullptr);
        if (userMessage != nullptr) *userMessage = QStringLiteral("购买失败：%1").arg(error);
        return false;
    }
    emit inventoryChanged();
    emit itemPurchased(itemId);
    if (userMessage != nullptr) *userMessage = QStringLiteral("购买成功");
    return true;
}

bool PetEconomyController::give(const QString& itemId, QString* userMessage)
{
    const ShopItemDefinition* item = findItem(itemId);
    const int previous = inventory_.value(itemId);
    if (!initialized_ || item == nullptr || previous <= 0) {
        if (userMessage != nullptr) *userMessage = QStringLiteral("背包中没有这个物品");
        return false;
    }
    if (previous == 1) inventory_.remove(itemId);
    else inventory_.insert(itemId, previous - 1);
    QString error;
    if (!saveInventory(&error)) {
        inventory_.insert(itemId, previous);
        if (userMessage != nullptr) *userMessage = QStringLiteral("赠与失败：%1").arg(error);
        return false;
    }
    const bool effectApplied = item->kind == ShopItemKind::Gift
        ? affection_->addExperience(item->affectionGain, &error)
        : satiety_->increase(item->satietyGain, &error);
    if (!effectApplied) {
        inventory_.insert(itemId, previous);
        saveInventory(nullptr);
        if (userMessage != nullptr) *userMessage = QStringLiteral("赠与失败：%1").arg(error);
        return false;
    }
    emit inventoryChanged();
    emit itemGiven(item->name);
    if (userMessage != nullptr) *userMessage = QStringLiteral("已赠与%1").arg(item->name);
    return true;
}

const ShopItemDefinition* PetEconomyController::findItem(const QString& itemId) const
{
    for (const ShopItemDefinition& item : items_) {
        if (item.id == itemId) return &item;
    }
    return nullptr;
}

bool PetEconomyController::saveInventory(QString* errorMessage)
{
    if (settings_ == nullptr) return false;
    QJsonObject object;
    for (auto it = inventory_.constBegin(); it != inventory_.constEnd(); ++it) {
        if (it.value() > 0) object.insert(it.key(), it.value());
    }
    settings_->setValue(QString::fromLatin1(InventoryKey),
                        QJsonDocument(object).toJson(QJsonDocument::Compact));
    return settings_->save(errorMessage);
}

} // namespace zhu_screen_pet
