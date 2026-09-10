#include <QtTest/QtTest>

#include <QFile>
#include <QDir>
#include <QFontMetrics>
#include <QGridLayout>
#include <QPushButton>
#include <QTemporaryDir>

#include <algorithm>

#include "app/AffectionController.h"
#include "app/InputActivityController.h"
#include "app/PetEconomyController.h"
#include "app/SatietyController.h"
#include "infrastructure/InputActivityMonitor.h"
#include "infrastructure/SettingsRepository.h"
#include "infrastructure/ShopCatalogRepository.h"
#include "ui/BackpackWindow.h"
#include "ui/ShopWindow.h"

namespace zhu_screen_pet {

class EconomyTest final : public QObject
{
    Q_OBJECT

private:
    static bool writeCatalog(const QString& path, const QByteArray& content)
    {
        QFile file(path);
        return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write(content) == content.size();
    }

private slots:
    void purchaseAndGiveApplyEffectsImmediately()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString catalogPath = directory.filePath(QStringLiteral("shop-catalog.json"));
        QVERIFY(writeCatalog(catalogPath, R"({
            "version": 1,
            "gifts": [{"id":"flower","emoji":"🌸","name":"小花",
                       "price":100,"affection_gain":250}],
            "foods": [{"id":"cake","emoji":"🍰","name":"蛋糕",
                       "price":80,"satiety_gain":20}]
        })"));

        SettingsRepository settings(directory.filePath(QStringLiteral("settings.ini")));
        QVERIFY(settings.load());
        settings.setValue(QStringLiteral("activity/input_count"), 500LL);
        settings.setValue(QStringLiteral("satiety/value"), 50);
        QVERIFY(settings.save());
        InputActivityMonitor monitor;
        InputActivityController activity(&monitor, &settings);
        AffectionController affection(&settings);
        SatietyController satiety(&settings);
        ShopCatalogRepository catalog(catalogPath);
        PetEconomyController economy(&catalog, &settings, &activity, &affection, &satiety);
        QVERIFY(activity.initialize());
        QVERIFY(affection.initialize());
        QVERIFY(satiety.initialize());
        QVERIFY(economy.initialize());

        QCOMPARE(economy.items(ShopItemKind::Gift).size(), 1);
        QCOMPARE(economy.items(ShopItemKind::Food).size(), 1);
        QString message;
        QVERIFY2(economy.purchase(QStringLiteral("flower"), &message), qPrintable(message));
        QCOMPARE(economy.balance(), 400LL);
        QCOMPARE(economy.inventory().size(), 1);
        QCOMPARE(economy.inventory().first().quantity, 1);
        QVERIFY2(economy.give(QStringLiteral("flower"), &message), qPrintable(message));
        QCOMPARE(economy.inventory().size(), 0);
        QCOMPARE(affection.experienceInLevel(), 250);

        QVERIFY2(economy.purchase(QStringLiteral("cake"), &message), qPrintable(message));
        QCOMPARE(economy.balance(), 320LL);
        QVERIFY2(economy.give(QStringLiteral("cake"), &message), qPrintable(message));
        QCOMPARE(satiety.value(), 70);
        QCOMPARE(economy.inventory().size(), 0);
    }

    void insufficientBalanceDoesNotCreateInventory()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString catalogPath = directory.filePath(QStringLiteral("shop-catalog.json"));
        QVERIFY(writeCatalog(catalogPath, R"({
            "version":1,
            "gifts":[{"id":"gift","emoji":"🎁","name":"礼物",
                      "price":100,"affection_gain":10}],
            "foods":[]
        })"));
        SettingsRepository settings(directory.filePath(QStringLiteral("settings.ini")));
        QVERIFY(settings.load());
        settings.setValue(QStringLiteral("activity/input_count"), 99LL);
        QVERIFY(settings.save());
        InputActivityMonitor monitor;
        InputActivityController activity(&monitor, &settings);
        AffectionController affection(&settings);
        SatietyController satiety(&settings);
        ShopCatalogRepository catalog(catalogPath);
        PetEconomyController economy(&catalog, &settings, &activity, &affection, &satiety);
        QVERIFY(activity.initialize());
        QVERIFY(affection.initialize());
        QVERIFY(satiety.initialize());
        QVERIFY(economy.initialize());
        QString message;
        QVERIFY(!economy.purchase(QStringLiteral("gift"), &message));
        QCOMPARE(message, QStringLiteral("余额不足"));
        QCOMPARE(economy.balance(), 99LL);
        QVERIFY(economy.inventory().isEmpty());
    }

    void inventoryAndSatietyRestoreFromSettings()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString catalogPath = directory.filePath(QStringLiteral("shop-catalog.json"));
        QVERIFY(writeCatalog(catalogPath, R"({
            "version":1,
            "gifts":[{"id":"gift","emoji":"🎁","name":"礼物",
                      "price":100,"affection_gain":10}],
            "foods":[]
        })"));
        SettingsRepository settings(directory.filePath(QStringLiteral("settings.ini")));
        QVERIFY(settings.load());
        settings.setValue(QStringLiteral("activity/input_count"), 200LL);
        settings.setValue(QStringLiteral("satiety/value"), 50);
        settings.setValue(QStringLiteral("satiety/elapsed_seconds"),
                          SatietyController::DecayIntervalSeconds);
        QVERIFY(settings.save());
        InputActivityMonitor monitor;
        InputActivityController activity(&monitor, &settings);
        AffectionController affection(&settings);
        SatietyController satiety(&settings);
        ShopCatalogRepository catalog(catalogPath);
        PetEconomyController economy(&catalog, &settings, &activity, &affection, &satiety);
        QVERIFY(activity.initialize());
        QVERIFY(affection.initialize());
        QVERIFY(satiety.initialize());
        QCOMPARE(satiety.value(), 45);
        QVERIFY(economy.initialize());
        QVERIFY(economy.purchase(QStringLiteral("gift")));
        QVERIFY(economy.shutdown());

        PetEconomyController restored(&catalog, &settings, &activity, &affection, &satiety);
        QVERIFY(restored.initialize());
        QCOMPARE(restored.inventory().size(), 1);
        QCOMPARE(restored.inventory().first().quantity, 1);
        QCOMPARE(SatietyController::DecayAmount, 5);
        QCOMPARE(SatietyController::DecayIntervalSeconds, 30 * 60);
    }

    void emptyCatalogIsValidButWrongTypesAreRejected()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString catalogPath = directory.filePath(QStringLiteral("shop-catalog.json"));
        QVector<ShopItemDefinition> items;
        QString error;
        QVERIFY(writeCatalog(catalogPath,
            R"({"version":1,"gifts":[],"foods":[]})"));
        ShopCatalogRepository repository(catalogPath);
        QVERIFY2(repository.load(&items, &error), qPrintable(error));
        QVERIFY(items.isEmpty());

        QVERIFY(writeCatalog(catalogPath,
            R"({"version":1,"gifts":{},"foods":[]})"));
        QVERIFY(!repository.load(&items, &error));
        QVERIFY(error.contains(QStringLiteral("must be arrays")));
    }

    void emptyCatalogCanBeAttachedToShopAndBackpackWindows()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString catalogPath = directory.filePath(QStringLiteral("shop-catalog.json"));
        QVERIFY(writeCatalog(catalogPath,
            R"({"version":1,"gifts":[],"foods":[]})"));
        SettingsRepository settings(directory.filePath(QStringLiteral("settings.ini")));
        QVERIFY(settings.load());
        InputActivityMonitor monitor;
        InputActivityController activity(&monitor, &settings);
        AffectionController affection(&settings);
        SatietyController satiety(&settings);
        ShopCatalogRepository catalog(catalogPath);
        PetEconomyController economy(&catalog, &settings, &activity, &affection, &satiety);
        QVERIFY(activity.initialize());
        QVERIFY(affection.initialize());
        QVERIFY(satiety.initialize());
        QVERIFY(economy.initialize());

        ShopWindow shop;
        BackpackWindow backpack;
        shop.setController(&economy);
        backpack.setController(&economy);
        QVERIFY(!shop.isVisible());
        QVERIFY(!backpack.isVisible());
    }

    void shippedCatalogContainsInitialGiftAndFoodLineup()
    {
        const QString path = QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("config/shop-catalog.json"));
        ShopCatalogRepository repository(path);
        QVector<ShopItemDefinition> items;
        QString error;
        QVERIFY2(repository.load(&items, &error), qPrintable(error));
        QCOMPARE(items.size(), 11);
        QCOMPARE(std::count_if(items.cbegin(), items.cend(), [](const auto& item) {
            return item.kind == ShopItemKind::Gift;
        }), 6);
        QCOMPARE(std::count_if(items.cbegin(), items.cend(), [](const auto& item) {
            return item.kind == ShopItemKind::Food;
        }), 5);

        const auto chocolate = std::find_if(items.cbegin(), items.cend(), [](const auto& item) {
            return item.id == QStringLiteral("chocolate");
        });
        QVERIFY(chocolate != items.cend());
        QCOMPARE(chocolate->price, 500);
        QCOMPARE(chocolate->affectionGain, 50);
        const auto steak = std::find_if(items.cbegin(), items.cend(), [](const auto& item) {
            return item.id == QStringLiteral("steak");
        });
        QVERIFY(steak != items.cend());
        QCOMPARE(steak->price, 4000);
        QCOMPARE(steak->satietyGain, 100);
    }

    void shippedCatalogRendersEveryItemAndItsEffect()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        SettingsRepository settings(directory.filePath(QStringLiteral("settings.ini")));
        QVERIFY(settings.load());
        InputActivityMonitor monitor;
        InputActivityController activity(&monitor, &settings);
        AffectionController affection(&settings);
        SatietyController satiety(&settings);
        const QString catalogPath = QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("config/shop-catalog.json"));
        ShopCatalogRepository catalog(catalogPath);
        PetEconomyController economy(&catalog, &settings, &activity, &affection, &satiety);
        QVERIFY(activity.initialize());
        QVERIFY(affection.initialize());
        QVERIFY(satiety.initialize());
        QVERIFY(economy.initialize());

        ShopWindow shop;
        shop.setController(&economy);
        QVector<QPushButton*> itemButtons;
        for (QPushButton* button : shop.findChildren<QPushButton*>()) {
            if (!button->property("itemId").toString().isEmpty()) itemButtons.append(button);
        }
        QCOMPARE(itemButtons.size(), 11);
        const auto chocolate = std::find_if(itemButtons.cbegin(), itemButtons.cend(),
            [](const QPushButton* button) {
                return button->property("itemId").toString() == QStringLiteral("chocolate");
            });
        QVERIFY(chocolate != itemButtons.cend());
        QVERIFY((*chocolate)->text().contains(QStringLiteral("价格：500")));
        QVERIFY((*chocolate)->text().contains(QStringLiteral("好感度：+50")));
        const auto steak = std::find_if(itemButtons.cbegin(), itemButtons.cend(),
            [](const QPushButton* button) {
                return button->property("itemId").toString() == QStringLiteral("steak");
            });
        QVERIFY(steak != itemButtons.cend());
        QVERIFY((*steak)->text().contains(QStringLiteral("价格：4000")));
        QVERIFY((*steak)->text().contains(QStringLiteral("饱食度：+100")));

        shop.setUiScalePercent(50);
        for (QPushButton* button : std::as_const(itemButtons)) {
            const QFontMetrics fontMetrics(button->font());
            const QStringList lines = button->text().split(QLatin1Char('\n'));
            int widestLine = 0;
            for (const QString& line : lines) {
                widestLine = qMax(widestLine, fontMetrics.horizontalAdvance(line));
            }
            const int textHeight = lines.size() * fontMetrics.lineSpacing();
            QVERIFY2(button->minimumWidth() >= widestLine + 16,
                     qPrintable(QStringLiteral("商品卡片宽度不足：%1").arg(button->text())));
            QVERIFY2(button->minimumHeight() >= textHeight + 16,
                     qPrintable(QStringLiteral("商品卡片高度不足：%1").arg(button->text())));
        }

        const auto giftColumnCount = [&]() {
            auto* content = (*chocolate)->parentWidget();
            auto* grid = content == nullptr
                ? nullptr : qobject_cast<QGridLayout*>(content->layout());
            if (grid == nullptr) return 0;
            int maximumColumn = -1;
            const auto giftButtons = content->findChildren<QPushButton*>(
                QString{}, Qt::FindDirectChildrenOnly);
            for (QPushButton* button : giftButtons) {
                int row = 0;
                int column = 0;
                int rowSpan = 0;
                int columnSpan = 0;
                grid->getItemPosition(grid->indexOf(button), &row, &column,
                                      &rowSpan, &columnSpan);
                maximumColumn = qMax(maximumColumn, column);
            }
            return maximumColumn + 1;
        };
        shop.show();
        shop.resize(360, 480);
        QTest::qWait(30);
        const int narrowColumns = giftColumnCount();
        shop.resize(900, 480);
        QTest::qWait(30);
        const int wideColumns = giftColumnCount();
        QVERIFY(narrowColumns >= 1);
        QVERIFY2(wideColumns > narrowColumns,
                 qPrintable(QStringLiteral("responsive columns did not grow: %1 -> %2")
                                .arg(narrowColumns).arg(wideColumns)));
    }
};

} // namespace zhu_screen_pet

QTEST_MAIN(zhu_screen_pet::EconomyTest)
#include "economy_test.moc"
