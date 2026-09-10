#pragma once

#include <QString>
#include <QVector>

namespace zhu_screen_pet {

enum class ShopItemKind
{
    Gift,
    Food
};

struct ShopItemDefinition
{
    QString id;
    ShopItemKind kind = ShopItemKind::Gift;
    QString emoji;
    QString name;
    int price = 0;
    int affectionGain = 0;
    int satietyGain = 0;
};

struct InventoryEntry
{
    ShopItemDefinition item;
    int quantity = 0;
};

} // namespace zhu_screen_pet
