#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "app/AppConfigRepository.h"
#include "app/ModelErrorPresenter.h"
#include "app/PersonaConfig.h"
#include "infrastructure/AppPaths.h"
#include "infrastructure/ImageCompressor.h"
#include "infrastructure/SettingsRepository.h"
#include "model/ModelConfigRepository.h"

namespace zhu_screen_pet {

class ConfigRepositoryTest final : public QObject
{
    Q_OBJECT

private:
    static PersonaConfig testPersona(QHash<QString, QString>* messages = nullptr)
    {
        PersonaConfig persona;
        QHash<QString, QString> loadedMessages;
        QString errorMessage;
        const QString path = QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("config/app-settings.json"));
        AppConfigRepository repository(path);
        if (!repository.load(&persona, &loadedMessages, &errorMessage)) {
            qFatal("Cannot load test application configuration: %s", qPrintable(errorMessage));
        }
        if (messages != nullptr) *messages = loadedMessages;
        return persona;
    }

    static MemoryLimits testMemoryLimits()
    {
        PersonaConfig persona;
        QHash<QString, QString> messages;
        MemoryLimits limits;
        QString errorMessage;
        const QString path = QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("config/app-settings.json"));
        AppConfigRepository repository(path);
        if (!repository.load(&persona, &messages, &errorMessage, &limits)) {
            qFatal("Cannot load test memory configuration: %s", qPrintable(errorMessage));
        }
        return limits;
    }

private slots:
    void appPathsCreateExpectedDirectories()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        AppPaths paths(temporaryDirectory.path() + QStringLiteral("/zhu_screen_pet"));
        QString errorMessage;
        QVERIFY2(paths.initialize(&errorMessage), qPrintable(errorMessage));
        QVERIFY(QFileInfo::exists(paths.configDirectory()));
        QCOMPARE(paths.modelConfigPath(),
                 paths.configDirectory() + QStringLiteral("/model-providers.json"));
        QCOMPARE(paths.appConfigPath(),
                 paths.configDirectory() + QStringLiteral("/app-settings.json"));
        QVERIFY(QFileInfo::exists(paths.databaseDirectory()));
        QVERIFY(QFileInfo::exists(paths.logDirectory()));
        QVERIFY(QFileInfo::exists(paths.captureDirectory()));
    }

    void settingsRoundTrip()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString settingsPath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("settings.ini"));
        SettingsRepository settings(settingsPath);
        settings.setValue(QStringLiteral("capture/interval_seconds"), 30);
        QString errorMessage;
        QVERIFY2(settings.save(&errorMessage), qPrintable(errorMessage));

        SettingsRepository loaded(settingsPath);
        QVERIFY2(loaded.load(&errorMessage), qPrintable(errorMessage));
        QCOMPARE(loaded.value(QStringLiteral("capture/interval_seconds")).toInt(), 30);
    }

    void modelConfigurationProfilesRoundTripWithoutSecret()
    {
        QTemporaryDir temporaryDirectory;
        const QString path = QDir(temporaryDirectory.path()).filePath(
            QStringLiteral("model-providers.json"));
        const QString shippedPath = QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("config/model-providers.json"));
        QVERIFY2(QFile::copy(shippedPath, path), qPrintable(shippedPath));
        ModelConfigRepository repository(path);

        ModelProviderConfig defaults;
        QVERIFY(repository.loadActive(&defaults));
        QCOMPARE(defaults.providerType, QStringLiteral("mock"));

        ModelProviderConfig deepSeek;
        deepSeek.profileId = QStringLiteral("deepseek-chat");
        deepSeek.providerType = QStringLiteral("deepseek");
        deepSeek.displayName = QStringLiteral("DeepSeek Chat");
        deepSeek.baseUrl = QStringLiteral("https://example.invalid/v1");
        deepSeek.model = QStringLiteral("configured-model");
        deepSeek.credentialService = QStringLiteral("zhu_screen_pet");
        deepSeek.credentialAccount = QStringLiteral("deepseek-api-key");
        deepSeek.timeoutMs = 15000;
        deepSeek.maxRetries = 2;
        deepSeek.retryBaseDelayMs = 500;
        QString errorMessage;
        QVERIFY2(repository.saveProfile(deepSeek, true, &errorMessage), qPrintable(errorMessage));

        ModelConfigRepository reloaded(path);
        ModelProviderConfig loaded;
        QVERIFY2(reloaded.loadActive(&loaded, &errorMessage), qPrintable(errorMessage));
        QCOMPARE(loaded.profileId, deepSeek.profileId);
        QCOMPARE(loaded.providerType, deepSeek.providerType);
        QCOMPARE(loaded.credentialAccount, deepSeek.credentialAccount);

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray serialized = file.readAll();
        QVERIFY(!serialized.contains("sk-test-secret"));
        QVERIFY(!serialized.contains("api_key"));
        QVERIFY(serialized.contains("https://example.invalid/v1"));
        QVERIFY(serialized.contains("configured-model"));
    }

    void modelConfigurationDoesNotInventProviderDefaults()
    {
        ModelProviderConfig incomplete;
        incomplete.profileId = QStringLiteral("missing-values");
        incomplete.providerType = QStringLiteral("deepseek");
        incomplete.displayName = QStringLiteral("DeepSeek");
        incomplete.credentialService = QStringLiteral("zhu_screen_pet");
        incomplete.credentialAccount = QStringLiteral("deepseek-key");
        incomplete.timeoutMs = 30000;
        incomplete.maxRetries = 3;
        incomplete.retryBaseDelayMs = 1000;
        QString errorMessage;
        QVERIFY(!incomplete.validate(&errorMessage));
        QVERIFY(errorMessage.contains(QStringLiteral("URL")));
        QVERIFY(incomplete.normalized().baseUrl.isEmpty());
        QVERIFY(incomplete.normalized().model.isEmpty());
    }

    void modelConfigurationLimitsAutomaticRetries()
    {
        ModelProviderConfig config;
        config.profileId = QStringLiteral("retry-limits");
        config.providerType = QStringLiteral("mock");
        config.displayName = QStringLiteral("Retry Limits");
        config.mockReply = QStringLiteral("ok");
        config.timeoutMs = ModelProviderConfig::MaximumTimeoutMs;
        config.maxRetries = ModelProviderConfig::MaximumRetries;
        config.retryBaseDelayMs = ModelProviderConfig::MaximumRetryBaseDelayMs;
        QVERIFY(config.validate());

        QString errorMessage;
        config.maxRetries = ModelProviderConfig::MaximumRetries + 1;
        QVERIFY(!config.validate(&errorMessage));
        QVERIFY(errorMessage.contains(QStringLiteral("retries 0..5")));
    }

    void modelConfigurationRejectsRemotePlainHttp()
    {
        ModelProviderConfig remote;
        remote.profileId = QStringLiteral("remote-http");
        remote.providerType = QStringLiteral("openai-compatible");
        remote.displayName = QStringLiteral("Remote HTTP");
        remote.baseUrl = QStringLiteral("http://example.com/v1");
        remote.model = QStringLiteral("model");
        remote.credentialService = QStringLiteral("service");
        remote.credentialAccount = QStringLiteral("account");
        remote.timeoutMs = 30000;
        remote.maxRetries = 3;
        remote.retryBaseDelayMs = 1000;
        QString error;
        QVERIFY(!remote.validate(&error));
        QVERIFY(error.contains(QStringLiteral("HTTPS")));

        remote.baseUrl = QStringLiteral("http://127.0.0.1:11434/v1");
        QVERIFY2(remote.validate(&error), qPrintable(error));
    }

    void modelErrorsHaveFriendlyUserMessages()
    {
        QHash<QString, QString> messages;
        testPersona(&messages);
        const ModelErrorPresenter presenter(messages);
        QCOMPARE(presenter.message(
                     {ModelErrorCode::Authentication, QStringLiteral("secret missing"), 401}),
                 messages.value(QStringLiteral("authentication")));
        QCOMPARE(presenter.message(
                     {ModelErrorCode::Timeout, QStringLiteral("socket timeout"), 0}),
                 messages.value(QStringLiteral("timeout")));
        QCOMPARE(presenter.message(
                     {ModelErrorCode::RateLimit, QStringLiteral("too many requests"), 429}),
                 messages.value(QStringLiteral("rate_limit")));
        QCOMPARE(presenter.message(
                     {ModelErrorCode::Network, QStringLiteral("connection refused"), 0}),
                 messages.value(QStringLiteral("network")));
    }

    void personaConfigControlsReplyLengthAndProactivity()
    {
        PersonaConfig config = testPersona();
        config.name = QStringLiteral("小猫");
        config.tone = QStringLiteral("温柔");
        config.maxReplyTokens = 1234;
        config.proactiveLevel = 3;
        QString errorMessage;
        QVERIFY(config.validate(&errorMessage));
        QCOMPARE(config.normalized().maxReplyTokens, 1234);
        QVERIFY(config.systemInstruction().contains(QStringLiteral("小猫")));
        QVERIFY(config.systemInstruction().contains(config.userAddress));
        QVERIFY(config.systemInstruction().contains(QStringLiteral("积极发现")));
        QVERIFY(config.proactivityInstruction().contains(QStringLiteral("主动")));
        config.proactiveLevel = 4;
        QVERIFY(!config.validate(&errorMessage));
    }

    void applicationConfigDefaultsMissingUserAddress()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("app-settings.json"));
        const QString shippedPath = QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("config/app-settings.json"));
        QVERIFY(QFile::copy(shippedPath, path));

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        file.close();
        QVERIFY(document.isObject());
        QJsonObject root = document.object();
        QJsonObject personaObject = root.value(QStringLiteral("persona")).toObject();
        personaObject.remove(QStringLiteral("user_address"));
        root.insert(QStringLiteral("persona"), personaObject);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QVERIFY(file.write(QJsonDocument(root).toJson()) > 0);
        file.close();

        PersonaConfig persona;
        QHash<QString, QString> messages;
        QString errorMessage;
        AppConfigRepository repository(path);
        QVERIFY2(repository.load(&persona, &messages, &errorMessage), qPrintable(errorMessage));
        QCOMPARE(persona.userAddress, QStringLiteral("主人大人"));
        QVERIFY(persona.systemInstruction().contains(QStringLiteral("主人大人")));
    }

    void applicationConfigLoadsMemoryLimits()
    {
        const MemoryLimits limits = testMemoryLimits();
        QCOMPARE(limits.recentMessageLimit, 20);
        QCOMPARE(limits.relevantHistoryLimit, 5);
        QCOMPARE(limits.longTermMemoryLimit, 5);
        QCOMPARE(limits.maxContextTokens, 8000);
        QCOMPARE(limits.summaryMessageThreshold, 20);
        QCOMPARE(limits.summaryTokenThreshold, 4000);

        PersonaConfig persona;
        QHash<QString, QString> messages;
        UiConfig ui;
        const QString path = QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("config/app-settings.json"));
        AppConfigRepository repository(path);
        QVERIFY(repository.load(&persona, &messages, nullptr, nullptr, &ui));
        QCOMPARE(ui.replyBubbleDurationMs, 15000);
        QCOMPARE(ui.hoverHideDelayMs, 600);
        QCOMPARE(ui.fadeDurationMs, 180);
        QCOMPARE(ui.windowScalePercent, 100);
        QCOMPARE(ui.appIconPath, QStringLiteral("app-icon.png"));
        QCOMPARE(ui.petAvatarPath, QStringLiteral("pet_avatar.png"));
        QCOMPARE(ui.conversationAvatarPath, QStringLiteral("conversation_avatar.png"));
        QVERIFY(!ui.screenCaptureEnabled);
        QCOMPARE(ui.screenCaptureIntervalMs, 60000);
        QVERIFY(!ui.captureOnChat);
        QCOMPARE(ui.captureImageFormat, QStringLiteral("jpeg"));
        QCOMPARE(ui.captureMaxWidth, 1280);
        QCOMPARE(ui.captureQuality, 75);
    }

    void applicationConfigClampsLegacyCaptureIntervalToThirtySeconds()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("app-settings.json"));
        const QString shippedPath = QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("config/app-settings.json"));
        QVERIFY(QFile::copy(shippedPath, path));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        file.close();
        QJsonObject root = document.object();
        QJsonObject uiObject = root.value(QStringLiteral("ui")).toObject();
        uiObject.insert(QStringLiteral("screen_capture_interval_ms"), 5000);
        root.insert(QStringLiteral("ui"), uiObject);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QVERIFY(file.write(QJsonDocument(root).toJson()) > 0);
        file.close();

        PersonaConfig persona;
        QHash<QString, QString> messages;
        UiConfig ui;
        QString error;
        AppConfigRepository repository(path);
        QVERIFY2(repository.load(&persona, &messages, &error, nullptr, &ui), qPrintable(error));
        QCOMPARE(ui.screenCaptureIntervalMs, UiConfig::MinimumScreenCaptureIntervalMs);
    }

    void imageCompressorScalesAndEncodesJpeg()
    {
        QImage source(2400, 1200, QImage::Format_RGB32);
        source.fill(Qt::blue);
        ImageCompressionOptions options;
        options.format = QStringLiteral("jpeg");
        options.maxWidth = 800;
        options.quality = 80;
        QByteArray data;
        QString format;
        QSize outputSize;
        QString error;
        QVERIFY2(ImageCompressor::compress(source, options, &data, &format,
                                           &outputSize, &error), qPrintable(error));
        QVERIFY(!data.isEmpty());
        QCOMPARE(format, QStringLiteral("jpeg"));
        QCOMPARE(outputSize, QSize(800, 400));
        QImage decoded;
        QVERIFY(decoded.loadFromData(data, "JPEG"));
        QCOMPARE(decoded.size(), outputSize);
    }

    void applicationConfigPersistsAssetPaths()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("app-settings.json"));
        const QString shippedPath = QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("config/app-settings.json"));
        QVERIFY(QFile::copy(shippedPath, path));

        AppConfigRepository repository(path);
        PersonaConfig persona;
        QHash<QString, QString> messages;
        MemoryLimits limits;
        UiConfig ui;
        QString errorMessage;
        QVERIFY2(repository.load(&persona, &messages, &errorMessage, &limits, &ui),
                 qPrintable(errorMessage));
        ui.appIconPath = QStringLiteral("assets/app-icon.png");
        ui.petAvatarPath = QStringLiteral("assets/pet-avatar.png");
        ui.conversationAvatarPath = QStringLiteral("assets/history-avatar.png");
        ui.screenCaptureEnabled = true;
        ui.screenCaptureIntervalMs = 45000;
        ui.captureOnChat = true;
        ui.captureImageFormat = QStringLiteral("webp");
        ui.captureMaxWidth = 1024;
        ui.captureQuality = 68;
        ui.windowScalePercent = 135;
        ui.excludeOwnWindowsFromCapture = false;
        QVERIFY2(repository.save(persona, limits, &errorMessage, &ui),
                 qPrintable(errorMessage));

        UiConfig reloadedUi;
        QVERIFY2(repository.load(&persona, &messages, &errorMessage, &limits, &reloadedUi),
                 qPrintable(errorMessage));
        QCOMPARE(reloadedUi.appIconPath, QStringLiteral("assets/app-icon.png"));
        QCOMPARE(reloadedUi.petAvatarPath, QStringLiteral("assets/pet-avatar.png"));
        QCOMPARE(reloadedUi.conversationAvatarPath, QStringLiteral("assets/history-avatar.png"));
        QVERIFY(reloadedUi.screenCaptureEnabled);
        QCOMPARE(reloadedUi.screenCaptureIntervalMs, 45000);
        QVERIFY(reloadedUi.captureOnChat);
        QCOMPARE(reloadedUi.captureImageFormat, QStringLiteral("webp"));
        QCOMPARE(reloadedUi.captureMaxWidth, 1024);
        QCOMPARE(reloadedUi.captureQuality, 68);
        QCOMPARE(reloadedUi.windowScalePercent, 135);
        QVERIFY(!reloadedUi.excludeOwnWindowsFromCapture);
    }

};

} // namespace zhu_screen_pet

QTEST_MAIN(zhu_screen_pet::ConfigRepositoryTest)
#include "config_repository_test.moc"
