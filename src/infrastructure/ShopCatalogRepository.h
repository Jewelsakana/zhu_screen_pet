#pragma once

#include <QString>
#include <QVector>

#include "app/PetEconomyTypes.h"

namespace zhu_screen_pet {

/** 从用户可编辑 JSON 加载礼物和食物商品目录。 */
class ShopCatalogRepository final
{
public:
    explicit ShopCatalogRepository(QString filePath);

    bool load(QVector<ShopItemDefinition>* items,
              QString* errorMessage = nullptr) const;

private:
    QString filePath_;
};

} // namespace zhu_screen_pet
