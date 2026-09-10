#include <QtTest/QtTest>

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QImage>

#include "app/AffectionController.h"
#include "app/InputActivityController.h"
#include "infrastructure/InputActivityMonitor.h"
#include "infrastructure/SettingsRepository.h"
#include "ui/InputActivityPanel.h"

namespace zhu_screen_pet {

class ActivityAffectionTest final : public QObject
{
    Q_OBJECT

private slots:
    void inputCountPanelPaintsOpaqueRoundedCard()
    {
        InputActivityPanel panel;
        panel.setCount(368);
        panel.resize(panel.sizeHint());
        QImage image(panel.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        panel.render(&image);
        QVERIFY(image.pixelColor(image.width() / 2, image.height() / 2).alpha() > 0);
        QVERIFY(image.pixelColor(image.width() / 2, 1).alpha() > 0);
        QVERIFY(image.pixelColor(0, 0).alpha() < 255);
    }

    void inputCountsRestoreAndPersist()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        SettingsRepository settings(directory.filePath(QStringLiteral("settings.ini")));
        QVERIFY(settings.load());
        settings.setValue(QStringLiteral("activity/keyboard_count"), 123LL);
        settings.setValue(QStringLiteral("activity/mouse_count"), 45LL);
        QVERIFY(settings.save());

        InputActivityMonitor monitor;
        InputActivityController controller(&monitor, &settings);
        QVERIFY(controller.initialize());
        QCOMPARE(controller.inputCount(), 168LL);
        QCOMPARE(InputActivityController::SaveIntervalMs, 10 * 60 * 1000);
        QCOMPARE(InputActivityController::MaximumCount, 100000LL);
        QVERIFY(controller.shutdown());

        SettingsRepository restored(directory.filePath(QStringLiteral("settings.ini")));
        QVERIFY(restored.load());
        QCOMPARE(restored.value(QStringLiteral("activity/input_count")).toLongLong(),
                 168LL);
    }

    void affectionUsesFixedExperienceAndMaximumLevel()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        SettingsRepository settings(directory.filePath(QStringLiteral("settings.ini")));
        QVERIFY(settings.load());
        AffectionController controller(&settings);
        QVERIFY(controller.initialize());
        QCOMPARE(controller.level(), 0);
        QCOMPARE(controller.progressPercent(), 0);

        QSignalSpy levelSpy(&controller, &AffectionController::levelUp);
        QSignalSpy rewardSpy(&controller,
                             &AffectionController::proactivityRewardUnlocked);
        QVERIFY(controller.addExperience(999));
        QCOMPARE(controller.level(), 0);
        QCOMPARE(controller.progressPercent(), 99);
        QVERIFY(controller.addExperience(1));
        QCOMPARE(controller.level(), 1);
        QCOMPARE(controller.experienceInLevel(), 0);
        QCOMPARE(levelSpy.count(), 1);
        QCOMPARE(rewardSpy.count(), 1);
        QCOMPARE(rewardSpy.first().first().toInt(), 1);

        QVERIFY(controller.addExperience(9000));
        QCOMPARE(controller.level(), AffectionController::MaximumLevel);
        QCOMPARE(controller.progressPercent(), 100);
        QVERIFY(controller.isMaximumLevel());
        QCOMPARE(AffectionController::ExperiencePerLevel, 1000);
        QCOMPARE(AffectionController::proactivityRewardForLevel(4), 2);
        QCOMPARE(AffectionController::proactivityRewardForLevel(7), 3);
        QVERIFY(controller.shutdown());

        AffectionController restored(&settings);
        QVERIFY(restored.initialize());
        QCOMPARE(restored.level(), AffectionController::MaximumLevel);
        QCOMPARE(restored.experienceInLevel(), 0);
    }
};

} // namespace zhu_screen_pet

QTEST_MAIN(zhu_screen_pet::ActivityAffectionTest)
#include "activity_affection_test.moc"
