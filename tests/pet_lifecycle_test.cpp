#include <QtTest/QtTest>

#include <QSignalSpy>
#include <QTemporaryDir>

#include "app/PetLifecycleController.h"
#include "infrastructure/SettingsRepository.h"

namespace zhu_screen_pet {

class PetLifecycleTest final : public QObject
{
    Q_OBJECT

private slots:
    void greetingUsesExpectedTimeRanges_data()
    {
        QTest::addColumn<int>("hour");
        QTest::addColumn<QString>("expected");
        QTest::newRow("midnight") << 0 << QStringLiteral("凌晨好呀，小珠！");
        QTest::newRow("before-dawn") << 5 << QStringLiteral("凌晨好呀，小珠！");
        QTest::newRow("morning-start") << 6 << QStringLiteral("早上好呀，小珠！");
        QTest::newRow("morning-end") << 10 << QStringLiteral("早上好呀，小珠！");
        QTest::newRow("noon-start") << 11 << QStringLiteral("中午好呀，小珠！");
        QTest::newRow("noon-end") << 12 << QStringLiteral("中午好呀，小珠！");
        QTest::newRow("afternoon-start") << 13 << QStringLiteral("下午好呀，小珠！");
        QTest::newRow("unspecified-gap") << 18 << QStringLiteral("下午好呀，小珠！");
        QTest::newRow("evening-start") << 19 << QStringLiteral("晚上好呀，小珠！");
        QTest::newRow("evening-end") << 23 << QStringLiteral("晚上好呀，小珠！");
    }

    void greetingUsesExpectedTimeRanges()
    {
        QFETCH(int, hour);
        QFETCH(QString, expected);
        QCOMPARE(PetLifecycleController::greetingForHour(
                     hour, QStringLiteral(" 小珠 ")), expected);
    }

    void levelDurationCurveMatchesConfiguredBands()
    {
        QCOMPARE(PetLifecycleController::secondsRequiredForLevel(0), 3 * 60LL);
        QCOMPARE(PetLifecycleController::secondsRequiredForLevel(9), 9 * 60LL);
        QCOMPARE(PetLifecycleController::secondsRequiredForLevel(10), 10 * 60LL);
        QCOMPARE(PetLifecycleController::secondsRequiredForLevel(49), 59 * 60LL);
        QCOMPARE(PetLifecycleController::secondsRequiredForLevel(50), 60 * 60LL);
        QCOMPARE(PetLifecycleController::secondsRequiredForLevel(89), 9 * 60 * 60LL);
        QCOMPARE(PetLifecycleController::secondsRequiredForLevel(90), 10 * 60 * 60LL);
        QCOMPARE(PetLifecycleController::secondsRequiredForLevel(99), 19 * 60 * 60LL);
        QCOMPARE(PetLifecycleController::secondsRequiredForLevel(100), 0LL);

        qint64 previous = 0;
        for (int level = 0; level < PetLifecycleController::MaximumLevel; ++level) {
            const qint64 current = PetLifecycleController::secondsRequiredForLevel(level);
            QVERIFY(current >= previous);
            previous = current;
        }
    }

    void initializeRestoresAndNormalizesPersistedProgress()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        SettingsRepository settings(directory.filePath(QStringLiteral("settings.ini")));
        QVERIFY(settings.load());
        settings.setValue(QStringLiteral("pet/level"), 0);
        // 足够升两级，并在第 2 级保留 10 秒进度。
        settings.setValue(QStringLiteral("pet/level_elapsed_seconds"), 370);
        QVERIFY(settings.save());

        PetLifecycleController controller(&settings);
        QSignalSpy levelUpSpy(&controller, &PetLifecycleController::levelUp);
        QVERIFY(controller.initialize());
        QCOMPARE(controller.level(), 2);
        QCOMPARE(controller.progressPercent(), 4);
        QCOMPARE(levelUpSpy.count(), 2);

        QVERIFY(controller.shutdown());
        SettingsRepository restored(directory.filePath(QStringLiteral("settings.ini")));
        QVERIFY(restored.load());
        QCOMPARE(restored.value(QStringLiteral("pet/level")).toInt(), 2);
        QCOMPARE(restored.value(QStringLiteral("pet/level_elapsed_seconds")).toLongLong(),
                 10LL);
    }

    void startPublishesGreetingIdleTextAndProgress()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        SettingsRepository settings(directory.filePath(QStringLiteral("settings.ini")));
        QVERIFY(settings.load());
        PetLifecycleController controller(&settings);
        QVERIFY(controller.initialize());

        QSignalSpy greetingSpy(
            &controller, &PetLifecycleController::startupGreetingRequested);
        QSignalSpy idleSpy(&controller, &PetLifecycleController::idleDescriptionChanged);
        QSignalSpy progressSpy(&controller, &PetLifecycleController::progressChanged);
        controller.setUserAddress(QStringLiteral("配置称呼"));
        controller.start();
        QCOMPARE(greetingSpy.count(), 1);
        QCOMPARE(idleSpy.count(), 1);
        QCOMPARE(progressSpy.count(), 1);
        QVERIFY(greetingSpy.first().first().toString().contains(QStringLiteral("配置称呼")));
        QCOMPARE(PetLifecycleController::ProgressRefreshIntervalMs, 60 * 1000);
        const QStringList allowed = {
            QStringLiteral("摸鱼中~"), QStringLiteral("视奸中~"),
            QStringLiteral("吃饭中~"), QStringLiteral("睡觉中~"),
            QStringLiteral("无聊中~")};
        QVERIFY(allowed.contains(idleSpy.first().first().toString()));
        QVERIFY(controller.shutdown());
    }
};

} // namespace zhu_screen_pet

QTEST_MAIN(zhu_screen_pet::PetLifecycleTest)
#include "pet_lifecycle_test.moc"
