#include "app/LegacyDataMigrator.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

#include "infrastructure/AppPaths.h"
#include "model/ModelConfigRepository.h"
#include "infrastructure/SecretStore.h"
#include "model/ModelProviderConfig.h"

namespace zhu_screen_pet {

namespace {

QString markerPath(const AppPaths& paths)
{
    return QDir(paths.configDirectory()).filePath(QStringLiteral("legacy-migration-v1.complete"));
}

bool copyFileIfMissing(const QString& sourcePath, const QString& targetPath,
                       QString* errorMessage)
{
    if (!QFileInfo::exists(sourcePath) || QFileInfo::exists(targetPath)) return true;
    if (!QDir().mkpath(QFileInfo(targetPath).absolutePath())) {
        if (errorMessage) *errorMessage = QStringLiteral("cannot create migration directory: %1")
            .arg(QFileInfo(targetPath).absolutePath());
        return false;
    }
    QFile source(sourcePath);
    QSaveFile target(targetPath);
    if (!source.open(QIODevice::ReadOnly) || !target.open(QIODevice::WriteOnly)) {
        if (errorMessage) *errorMessage = QStringLiteral("cannot open migration file: %1 -> %2")
            .arg(sourcePath, targetPath);
        return false;
    }
    while (!source.atEnd()) {
        const QByteArray chunk = source.read(1024 * 1024);
        if (chunk.isEmpty() && source.error() != QFile::NoError) {
            if (errorMessage) *errorMessage = QStringLiteral("cannot read legacy file: %1")
                .arg(sourcePath);
            target.cancelWriting();
            return false;
        }
        if (target.write(chunk) != chunk.size()) {
            if (errorMessage) *errorMessage = QStringLiteral("cannot write migrated file: %1")
                .arg(targetPath);
            target.cancelWriting();
            return false;
        }
    }
    if (!target.commit()) {
        if (errorMessage) *errorMessage = QStringLiteral("cannot commit migrated file: %1")
            .arg(targetPath);
        return false;
    }
    if (QFileInfo(sourcePath).size() != QFileInfo(targetPath).size()) {
        if (errorMessage) *errorMessage = QStringLiteral("migrated file size mismatch: %1")
            .arg(targetPath);
        return false;
    }
    return true;
}

bool copyTreeIfMissing(const QString& sourceDirectory, const QString& targetDirectory,
                       QString* errorMessage)
{
    QDir source(sourceDirectory);
    if (!source.exists()) return true;
    for (const QFileInfo& entry : source.entryInfoList(
             QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs)) {
        const QString targetPath = QDir(targetDirectory).filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyTreeIfMissing(entry.absoluteFilePath(), targetPath, errorMessage)) return false;
        } else if (!copyFileIfMissing(entry.absoluteFilePath(), targetPath, errorMessage)) {
            return false;
        }
    }
    return true;
}

} // namespace

QString LegacyDataMigrator::defaultLegacyRootDirectory()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
        .filePath(QStringLiteral("ScreenPet/Screen Pet"));
}

bool LegacyDataMigrator::migrateFiles(const AppPaths& paths,
                                      const QString& legacyRootDirectory,
                                      bool* foundLegacy, QString* errorMessage)
{
    if (foundLegacy) *foundLegacy = false;
    if (QFileInfo::exists(markerPath(paths))) return true;
    const QDir legacy(legacyRootDirectory);
    if (!legacy.exists()) return true;
    if (foundLegacy) *foundLegacy = true;

    const QList<QPair<QString, QString>> files{
        {legacy.filePath(QStringLiteral("config/settings.ini")), paths.settingsPath()},
        {legacy.filePath(QStringLiteral("config/model-providers.json")), paths.modelConfigPath()},
        {legacy.filePath(QStringLiteral("config/app-settings.json")), paths.appConfigPath()},
        {legacy.filePath(QStringLiteral("database/screen-pet.sqlite")), paths.databasePath()},
        {legacy.filePath(QStringLiteral("database/screen-pet.sqlite-wal")),
         paths.databasePath() + QStringLiteral("-wal")},
        {legacy.filePath(QStringLiteral("database/screen-pet.sqlite-shm")),
         paths.databasePath() + QStringLiteral("-shm")},
    };
    for (const auto& file : files) {
        if (!copyFileIfMissing(file.first, file.second, errorMessage)) return false;
    }
    return copyTreeIfMissing(legacy.filePath(QStringLiteral("captures")),
                             paths.captureDirectory(), errorMessage)
        && copyTreeIfMissing(legacy.filePath(QStringLiteral("logs")),
                             paths.logDirectory(), errorMessage);
}

bool LegacyDataMigrator::migrateCredentials(ModelConfigRepository* models,
                                            SecretStore* secrets,
                                            QString* errorMessage)
{
    if (models == nullptr || secrets == nullptr) {
        if (errorMessage) *errorMessage = QStringLiteral("migration services are unavailable");
        return false;
    }
    QString repositoryError;
    const QStringList profileIds = models->profileIds(&repositoryError);
    if (!repositoryError.isEmpty()) {
        if (errorMessage) *errorMessage = repositoryError;
        return false;
    }
    for (const QString& profileId : profileIds) {
        ModelProviderConfig config;
        if (!models->loadProfile(profileId, &config, &repositoryError)) {
            if (errorMessage) *errorMessage = repositoryError;
            return false;
        }
        if (config.providerType == QStringLiteral("mock") || config.credentialAccount.isEmpty()) {
            continue;
        }

        QString newSecret;
        const bool hasNewSecret = secrets->read(QStringLiteral("zhu_screen_pet"),
                                                config.credentialAccount, &newSecret, nullptr)
            && !newSecret.isEmpty();
        QString oldSecret;
        const bool hasOldSecret = secrets->read(QStringLiteral("ScreenPet"),
                                                config.credentialAccount, &oldSecret, nullptr)
            && !oldSecret.isEmpty();
        if (!hasNewSecret && hasOldSecret
            && !secrets->write(QStringLiteral("zhu_screen_pet"), config.credentialAccount,
                               oldSecret, errorMessage)) {
            return false;
        }
        // 只有新凭据已经存在或复制成功时才改配置；读取失败时仍保留旧 service 退路。
        if ((hasNewSecret || hasOldSecret)
            && config.credentialService == QStringLiteral("ScreenPet")) {
            config.credentialService = QStringLiteral("zhu_screen_pet");
            if (!models->saveProfile(config, false, errorMessage)) return false;
        }
    }
    return true;
}

bool LegacyDataMigrator::markCompleted(const AppPaths& paths, QString* errorMessage)
{
    QSaveFile marker(markerPath(paths));
    if (!marker.open(QIODevice::WriteOnly)
        || marker.write("legacy-migration-v1\n") < 0 || !marker.commit()) {
        if (errorMessage) *errorMessage = QStringLiteral("cannot write migration marker: %1")
            .arg(marker.fileName());
        return false;
    }
    return true;
}

} // namespace zhu_screen_pet
