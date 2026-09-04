#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QUuid>

#include "app/LegacyDataMigrator.h"
#include "infrastructure/AppPaths.h"
#include "model/ModelConfigRepository.h"
#include "infrastructure/SecretStore.h"
#include "model/ModelProviderConfig.h"

namespace zhu_screen_pet {

class LegacyMigrationTest final : public QObject
{
    Q_OBJECT

private slots:
    void filesAreCopiedWithoutOverwriting()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const QString legacyRoot = QDir(temporaryDirectory.path()).filePath(QStringLiteral("legacy"));
        const QString newRoot = QDir(temporaryDirectory.path()).filePath(QStringLiteral("current"));
        QVERIFY(QDir().mkpath(QDir(legacyRoot).filePath(QStringLiteral("config"))));
        QVERIFY(QDir().mkpath(QDir(legacyRoot).filePath(QStringLiteral("database"))));
        QVERIFY(QDir().mkpath(QDir(legacyRoot).filePath(QStringLiteral("captures/nested"))));
        const auto writeFile = [](const QString& path, const QByteArray& content) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
        };
        QVERIFY(writeFile(QDir(legacyRoot).filePath(QStringLiteral("config/settings.ini")),
                          QByteArrayLiteral("legacy-settings")));
        QVERIFY(writeFile(QDir(legacyRoot).filePath(QStringLiteral("config/model-providers.json")),
                          QByteArrayLiteral("legacy-models")));
        QVERIFY(writeFile(QDir(legacyRoot).filePath(QStringLiteral("config/app-settings.json")),
                          QByteArrayLiteral("legacy-app")));
        QVERIFY(writeFile(QDir(legacyRoot).filePath(QStringLiteral("database/screen-pet.sqlite")),
                          QByteArrayLiteral("legacy-db")));
        QVERIFY(writeFile(QDir(legacyRoot).filePath(QStringLiteral("captures/nested/image.bin")),
                          QByteArrayLiteral("legacy-capture")));

        AppPaths paths(newRoot);
        QVERIFY(paths.initialize());
        QVERIFY(writeFile(paths.settingsPath(), QByteArrayLiteral("current-settings")));
        bool foundLegacy = false;
        QString errorMessage;
        QVERIFY2(LegacyDataMigrator::migrateFiles(paths, legacyRoot, &foundLegacy, &errorMessage),
                 qPrintable(errorMessage));
        QVERIFY(foundLegacy);
        QFile settings(paths.settingsPath());
        QVERIFY(settings.open(QIODevice::ReadOnly));
        QCOMPARE(settings.readAll(), QByteArrayLiteral("current-settings"));
        QFile database(paths.databasePath());
        QVERIFY(database.open(QIODevice::ReadOnly));
        QCOMPARE(database.readAll(), QByteArrayLiteral("legacy-db"));
        QFile capture(QDir(paths.captureDirectory()).filePath(QStringLiteral("nested/image.bin")));
        QVERIFY(capture.open(QIODevice::ReadOnly));
        QCOMPARE(capture.readAll(), QByteArrayLiteral("legacy-capture"));

        QVERIFY(LegacyDataMigrator::markCompleted(paths, &errorMessage));
        QVERIFY(writeFile(QDir(legacyRoot).filePath(QStringLiteral("database/screen-pet.sqlite")),
                          QByteArrayLiteral("changed-legacy-db")));
        foundLegacy = true;
        QVERIFY(LegacyDataMigrator::migrateFiles(paths, legacyRoot, &foundLegacy, &errorMessage));
        QVERIFY(!foundLegacy);
        database.close();
        QVERIFY(database.open(QIODevice::ReadOnly));
        QCOMPARE(database.readAll(), QByteArrayLiteral("legacy-db"));
    }

    void credentialsAreCopiedWithoutDeletingOldSecret()
    {
#ifdef Q_OS_WIN
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const QString account = QStringLiteral("migration-test-%1")
            .arg(QUuid::createUuid().toString(QUuid::Id128));
        const QString configPath = QDir(temporaryDirectory.path()).filePath(
            QStringLiteral("models.json"));
        ModelConfigRepository models(configPath);
        ModelProviderConfig model;
        model.profileId = QStringLiteral("legacy-remote");
        model.providerType = QStringLiteral("openai-compatible");
        model.displayName = QStringLiteral("Legacy Remote");
        model.baseUrl = QStringLiteral("https://example.invalid/v1");
        model.model = QStringLiteral("test-model");
        model.credentialService = QStringLiteral("ScreenPet");
        model.credentialAccount = account;
        model.timeoutMs = 1000;
        model.maxRetries = 0;
        model.retryBaseDelayMs = 50;
        QVERIFY(models.saveProfile(model));

        SecretStore secrets;
        QVERIFY(secrets.write(QStringLiteral("ScreenPet"), account,
                              QStringLiteral("legacy-secret")));
        QString errorMessage;
        QVERIFY2(LegacyDataMigrator::migrateCredentials(&models, &secrets, &errorMessage),
                 qPrintable(errorMessage));
        QString migratedSecret;
        QVERIFY(secrets.read(QStringLiteral("zhu_screen_pet"), account, &migratedSecret));
        QCOMPARE(migratedSecret, QStringLiteral("legacy-secret"));
        QString oldSecret;
        QVERIFY(secrets.read(QStringLiteral("ScreenPet"), account, &oldSecret));
        QCOMPARE(oldSecret, QStringLiteral("legacy-secret"));
        ModelProviderConfig migratedModel;
        QVERIFY(models.loadProfile(model.profileId, &migratedModel));
        QCOMPARE(migratedModel.credentialService, QStringLiteral("zhu_screen_pet"));
        QVERIFY(secrets.remove(QStringLiteral("ScreenPet"), account));
        QVERIFY(secrets.remove(QStringLiteral("zhu_screen_pet"), account));
#else
        QSKIP("Credential migration is only available on Windows");
#endif
    }
};

} // namespace zhu_screen_pet

QTEST_MAIN(zhu_screen_pet::LegacyMigrationTest)
#include "legacy_migration_test.moc"
