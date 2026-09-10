#include <QtTest/QtTest>

#include <QDir>
#include <QImage>
#include <QLabel>
#include <QTemporaryDir>

#include "app/PetState.h"
#include "infrastructure/AutoStartManager.h"
#include "ui/PetAnimationPlayer.h"

namespace zhu_screen_pet {

class OnboardingAnimationTest final : public QObject
{
    Q_OBJECT

private slots:
    void startupCommandQuotesExecutablePath()
    {
        QCOMPARE(AutoStartManager::startupCommand(
                     QStringLiteral("C:/Program Files/Zhu/小珠看着你.exe")),
                 QStringLiteral("\"C:\\Program Files\\Zhu\\小珠看着你.exe\""));
    }

    void animationPlayerLoadsSequencesAndFallsBackSafely()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(QDir().mkpath(directory.filePath(QStringLiteral("idle"))));
        QVERIFY(QDir().mkpath(directory.filePath(QStringLiteral("thinking"))));
        QVERIFY(QDir().mkpath(directory.filePath(QStringLiteral("speaking"))));
        QVERIFY(QDir().mkpath(directory.filePath(QStringLiteral("level_up"))));

        const auto saveFrame = [&directory](const QString& relativePath, QRgb color) {
            QImage image(8, 8, QImage::Format_ARGB32);
            image.fill(color);
            return image.save(directory.filePath(relativePath), "PNG");
        };
        QVERIFY(saveFrame(QStringLiteral("idle/frame_00.png"), qRgba(1, 2, 3, 255)));
        QVERIFY(saveFrame(QStringLiteral("idle/frame_01.png"), qRgba(4, 5, 6, 255)));
        QVERIFY(saveFrame(QStringLiteral("thinking/frame_00.png"), qRgba(7, 8, 9, 255)));
        QVERIFY(saveFrame(QStringLiteral("speaking/frame_00.png"), qRgba(10, 11, 12, 255)));
        QVERIFY(saveFrame(QStringLiteral("level_up/frame_00.png"), qRgba(13, 14, 15, 255)));

        QLabel label;
        label.resize(64, 64);
        PetAnimationPlayer player(&label);
        QPixmap fallback(8, 8);
        fallback.fill(Qt::white);
        player.setFallbackPixmap(fallback);
        player.setTargetSize(label.size());
        player.setAnimationRoot(directory.path());
        QCOMPARE(player.frameCount(PetState::Idle), 2);
        QCOMPARE(player.frameCount(PetState::Thinking), 1);
        QCOMPARE(player.frameCount(PetState::Speaking), 1);
        QCOMPARE(player.levelUpFrameCount(), 1);
        player.setState(PetState::Thinking);
        QVERIFY(!label.pixmap().isNull());
        player.playLevelUp();
        QVERIFY(!label.pixmap().isNull());

        player.setAnimationRoot(directory.filePath(QStringLiteral("missing")));
        QCOMPARE(player.frameCount(PetState::Idle), 0);
        player.stopLevelUp();
        QVERIFY(!label.pixmap().isNull());
    }
};

} // namespace zhu_screen_pet

QTEST_MAIN(zhu_screen_pet::OnboardingAnimationTest)
#include "onboarding_animation_test.moc"
