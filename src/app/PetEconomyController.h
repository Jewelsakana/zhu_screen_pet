#pragma once

#include <QHash>
#include <QObject>

#include "app/PetEconomyTypes.h"

namespace zhu_screen_pet {

class AffectionController;
class InputActivityController;
class SatietyController;
class SettingsRepository;
class ShopCatalogRepository;

/** 商店与背包应用服务：协调余额、库存、好感度和饱食度。 */
class PetEconomyController final : public QObject
{
    Q_OBJECT

public:
    PetEconomyController(ShopCatalogRepository* catalogRepository,
                         SettingsRepository* settings,
                         InputActivityController* inputActivity,
                         AffectionController* affection,
                         SatietyController* satiety,
                         QObject* parent = nullptr);

    bool initialize(QString* errorMessage = nullptr);
    bool shutdown(QString* errorMessage = nullptr);
    QVector<ShopItemDefinition> items(ShopItemKind kind) const;
    QVector<InventoryEntry> inventory() const;
    qint64 balance() const;
    bool purchase(const QString& itemId, QString* userMessage = nullptr);
    bool give(const QString& itemId, QString* userMessage = nullptr);

signals:
    void balanceChanged(qint64 balance);
    void inventoryChanged();
    void itemPurchased(const QString& itemId);
    void itemGiven(const QString& itemName);
    void persistenceFailed(const QString& detail);

private:
    const ShopItemDefinition* findItem(const QString& itemId) const;
    bool saveInventory(QString* errorMessage = nullptr);

    ShopCatalogRepository* catalogRepository_ = nullptr;
    SettingsRepository* settings_ = nullptr;
    InputActivityController* inputActivity_ = nullptr;
    AffectionController* affection_ = nullptr;
    SatietyController* satiety_ = nullptr;
    QVector<ShopItemDefinition> items_;
    QHash<QString, int> inventory_;
    bool initialized_ = false;
};

} // namespace zhu_screen_pet
