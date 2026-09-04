#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QSemaphore>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCursor>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScreen>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTimer>
#include <QSpinBox>
#include <QElapsedTimer>
#include <QImage>
#include <QThread>
#include <QUuid>

#include <algorithm>
#include <stdexcept>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "infrastructure/AppPaths.h"
#include "app/AppConfigRepository.h"
#include "infrastructure/Database.h"
#include "infrastructure/SettingsRepository.h"
#include "model/ModelConfigRepository.h"
#include "infrastructure/TaskExecutor.h"
#include "infrastructure/WindowManager.h"
#include "infrastructure/WindowPlacement.h"
#include "infrastructure/WindowAttachmentManager.h"
#include "infrastructure/DesktopWindowPolicy.h"
#include "infrastructure/HttpClient.h"
#include "infrastructure/ImageCompressor.h"
#include "infrastructure/ScreenCapture.h"
#include "infrastructure/Logger.h"
#include "infrastructure/SecretStore.h"
#include "model/MockChatProvider.h"
#include "model/ChatProviderFactory.h"
#include "model/ProviderManager.h"
#include "memory/SqliteConversationRepository.h"
#include "memory/SqliteMemoryRepository.h"
#include "memory/MemoryOrchestrator.h"
#include "app/ChatController.h"
#include "app/ConversationController.h"
#include "app/SettingsController.h"
#include "app/ModelErrorPresenter.h"
#include "app/ErrorCenter.h"
#include "app/PersonaConfig.h"
#include "ui/MainWindow.h"
#include "ui/SettingsDialog.h"
#include "ui/ReplyBubbleWindow.h"
#include "ui/ErrorBannerWindow.h"
#include "ui/HoverRevealController.h"
#include "ui/ActionPanel.h"
#include "ui/ChatInputPanel.h"
#include "ui/ConversationHistoryWindow.h"
#include "ui/ConversationWindow.h"

namespace zhu_screen_pet {

class SmokeTest final : public QObject
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
    void initTestCase()
    {
        qRegisterMetaType<ChatResult>("ChatResult");
        qRegisterMetaType<HttpResponse>("HttpResponse");
        qRegisterMetaType<CapturedImage>("CapturedImage");
    }

    void applicationWindowHasExpectedTitle()
    {
        MainWindow window;
        QCOMPARE(window.windowTitle(), QStringLiteral("小珠看着你"));
        QVERIFY(window.centralWidget() != nullptr);
    }

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
        QVERIFY(ui.appIconPath.isEmpty());
        QVERIFY(ui.petAvatarPath.isEmpty());
        QVERIFY(ui.conversationAvatarPath.isEmpty());
        QVERIFY(!ui.screenCaptureEnabled);
        QCOMPARE(ui.screenCaptureIntervalMs, 5000);
        QVERIFY(!ui.captureOnChat);
        QCOMPARE(ui.captureImageFormat, QStringLiteral("jpeg"));
        QCOMPARE(ui.captureMaxWidth, 1280);
        QCOMPARE(ui.captureQuality, 75);
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

    void screenCaptureCanCaptureAndPersistCompressedImage()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        ScreenCapture capture;
        ImageCompressionOptions options;
        options.maxWidth = 640;
        options.quality = 60;
        capture.configure(false, 5000, directory.path(), options);
        QSignalSpy capturedSpy(&capture, &ScreenCapture::captured);
        QString error;
        if (!capture.captureNow(&error)) {
            QSKIP(qPrintable(QStringLiteral("screen capture unavailable in test environment: %1")
                                 .arg(error)));
        }
        QCOMPARE(capturedSpy.count(), 1);
        const CapturedImage image = qvariant_cast<CapturedImage>(capturedSpy.at(0).at(0));
        QVERIFY(!image.data.isEmpty());
        QVERIFY(!image.filePath.isEmpty());
        QVERIFY(QFileInfo::exists(image.filePath));
        QVERIFY(image.size.width() <= 640);
        QVERIFY(capture.clearCaptures(&error));
        QVERIFY(!QFileInfo::exists(image.filePath));
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
        ui.screenCaptureIntervalMs = 7000;
        ui.captureOnChat = true;
        ui.captureImageFormat = QStringLiteral("webp");
        ui.captureMaxWidth = 1024;
        ui.captureQuality = 68;
        QVERIFY2(repository.save(persona, limits, &errorMessage, &ui),
                 qPrintable(errorMessage));

        UiConfig reloadedUi;
        QVERIFY2(repository.load(&persona, &messages, &errorMessage, &limits, &reloadedUi),
                 qPrintable(errorMessage));
        QCOMPARE(reloadedUi.appIconPath, QStringLiteral("assets/app-icon.png"));
        QCOMPARE(reloadedUi.petAvatarPath, QStringLiteral("assets/pet-avatar.png"));
        QCOMPARE(reloadedUi.conversationAvatarPath, QStringLiteral("assets/history-avatar.png"));
        QVERIFY(reloadedUi.screenCaptureEnabled);
        QCOMPARE(reloadedUi.screenCaptureIntervalMs, 7000);
        QVERIFY(reloadedUi.captureOnChat);
        QCOMPARE(reloadedUi.captureImageFormat, QStringLiteral("webp"));
        QCOMPARE(reloadedUi.captureMaxWidth, 1024);
        QCOMPARE(reloadedUi.captureQuality, 68);
    }

    void databaseInitializesSchema()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString databasePath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("zhu_screen_pet.sqlite"));
        Database database;
        QString errorMessage;
        QVERIFY2(database.open(databasePath, &errorMessage), qPrintable(errorMessage));
        QVERIFY(database.isOpen());
        QCOMPARE(database.schemaVersion(), 3);
    }

    void databaseResultsDistinguishNotFoundFromFailure()
    {
        Database unopened;
        SqliteConversationRepository unavailable(&unopened);
        const auto unavailableResult = unavailable.getConversationResult(QStringLiteral("missing"));
        QVERIFY(!unavailableResult);
        QCOMPARE(unavailableResult.error().code, AppErrorCode::DatabaseUnavailable);

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Database database;
        const auto openResult = database.openResult(directory.filePath(QStringLiteral("result.sqlite")));
        QVERIFY2(openResult.succeeded(), qPrintable(openResult.error().technicalMessage));
        SqliteConversationRepository conversations(&database);
        const auto missing = conversations.getConversationResult(QStringLiteral("missing"));
        QVERIFY(!missing);
        QCOMPARE(missing.error().code, AppErrorCode::NotFound);
        const auto emptyList = conversations.listConversationsResult(false);
        QVERIFY(emptyList.succeeded());
        QVERIFY(emptyList.value().isEmpty());
    }

    void ftsIndexesAreNotRebuiltOnEveryOpen()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("fts.sqlite"));
        bool ftsAvailable = false;
        {
            Database database;
            QString errorMessage;
            QVERIFY2(database.open(path, &errorMessage), qPrintable(errorMessage));
            ftsAvailable = database.hasFts5();
            if (ftsAvailable) QVERIFY(database.ftsRebuiltDuringOpen());
        }
        {
            Database database;
            QString errorMessage;
            QVERIFY2(database.open(path, &errorMessage), qPrintable(errorMessage));
            if (ftsAvailable) QVERIFY(!database.ftsRebuiltDuringOpen());
        }
    }

    void errorCenterRoutesStructuredErrors()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Logger logger;
        QString errorMessage;
        QVERIFY(logger.initialize(directory.path(), &errorMessage));
        ErrorCenter center(&logger);
        center.setMessages({{QStringLiteral("database.database_query"),
                             QStringLiteral("本地数据暂时无法读取。")}});
        QSignalSpy spy(&center, &ErrorCenter::errorReported);
        AppError error;
        error.code = AppErrorCode::DatabaseQuery;
        error.domain = ErrorDomain::Database;
        error.message = QStringLiteral("safe");
        error.technicalMessage = QStringLiteral("SQL internal detail sk-test-secret123");
        error.operation = QStringLiteral("test.query");
        center.report(error);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("本地数据暂时无法读取。"));
        QVERIFY(logger.lastWriteSucceeded());
        const QStringList logs = QDir(directory.path()).entryList(
            QStringList() << QStringLiteral("*.jsonl"), QDir::Files);
        QCOMPARE(logs.size(), 1);
        QFile logFile(directory.filePath(logs.first()));
        QVERIFY(logFile.open(QIODevice::ReadOnly));
        const QByteArray logged = logFile.readAll();
        QVERIFY(logged.contains("<redacted>"));
        QVERIFY(!logged.contains("sk-test-secret123"));
    }

    void taskExecutorRunsBackgroundTask()
    {
        TaskExecutor executor(1);
        QSemaphore completed;
        const auto token = executor.submit([&completed](
            const std::shared_ptr<CancellationToken>& taskToken) {
            if (!taskToken->isCancellationRequested()) {
                completed.release();
            }
        });
        QVERIFY(!token->isCancellationRequested());
        QVERIFY(completed.tryAcquire(1, 2000));
        executor.shutdown();
    }

    void taskExecutorContainsExceptionsAndStopsCooperatively()
    {
        TaskExecutor executor(1);
        QSemaphore exceptionSeen;
        executor.submit([](const std::shared_ptr<CancellationToken>&) {
            throw std::runtime_error("expected test exception");
        }, [&](std::exception_ptr error) {
            QVERIFY(error != nullptr);
            exceptionSeen.release();
        });
        QVERIFY(exceptionSeen.tryAcquire(1, 1000));

        QSemaphore started;
        executor.submit([&](const std::shared_ptr<CancellationToken>& token) {
            started.release();
            while (!token->isCancellationRequested()) QThread::msleep(1);
        });
        QVERIFY(started.tryAcquire(1, 1000));
        QElapsedTimer timer;
        timer.start();
        QVERIFY(executor.shutdownAndWait(1000));
        QVERIFY(timer.elapsed() < 1000);
    }

    void httpResponseSuccessRequiresTwoHundredStatus()
    {
        HttpResponse ok;
        ok.statusCode = 204;
        QVERIFY(ok.succeeded());
        HttpResponse serverError;
        serverError.statusCode = 500;
        QVERIFY(!serverError.succeeded());
        HttpResponse missingStatus;
        QVERIFY(!missingStatus.succeeded());
    }

    void loggerRotatesAndReportsWriteState()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Logger logger;
        QString errorMessage;
        QVERIFY2(logger.initialize(directory.path(), &errorMessage, 128, 3), qPrintable(errorMessage));
        for (int i = 0; i < 8; ++i) {
            logger.info(QStringLiteral("test"), QStringLiteral("large"), QString(200, QLatin1Char('x')));
            QVERIFY2(logger.lastWriteSucceeded(), qPrintable(logger.lastWriteError()));
        }
        const QStringList rotated = QDir(directory.path()).entryList(
            QStringList() << QStringLiteral("zhu_screen_pet-*.jsonl.*"), QDir::Files);
        QVERIFY(!rotated.isEmpty());
        QVERIFY(rotated.size() <= 2);
    }

    void loggerInitializationReportsFailure()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString blockingFile = directory.filePath(QStringLiteral("not-a-directory"));
        QFile file(blockingFile);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        Logger logger;
        QString errorMessage;
        QVERIFY(!logger.initialize(blockingFile, &errorMessage));
        QVERIFY(!errorMessage.isEmpty());
    }

    void windowManagerReportsSettingsSaveFailure()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString blockingFile = directory.filePath(QStringLiteral("blocking"));
        QFile file(blockingFile);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        SettingsRepository settings(blockingFile + QStringLiteral("/settings.ini"));
        WindowManager manager(&settings);
        QWidget window;
        QString errorMessage;
        QVERIFY(!manager.save(&window, &errorMessage));
        QVERIFY(!errorMessage.isEmpty());
    }

    void windowPositionIsClampedToAvailableGeometry()
    {
        const QRect available(0, 0, 1920, 1080);
        const QPoint position = WindowManager::clampPosition(
            available, QSize(420, 240), QPoint(1800, 1000));
        QCOMPARE(position, QPoint(1500, 840));
    }

    void windowPlacementUsesPreferredSideAndFlipsAtScreenEdge()
    {
        WindowPlacementRequest request;
        request.availableGeometry = QRect(0, 0, 1000, 800);
        request.anchorGeometry = QRect(400, 300, 200, 200);
        request.windowSize = QSize(100, 80);
        request.preferredSide = AttachmentSide::Right;
        request.alignment = AttachmentAlignment::Center;
        request.gap = 12;
        WindowPlacementResult result = WindowPlacement::adjacent(request);
        QCOMPARE(result.position, QPoint(612, 359));
        QCOMPARE(result.actualSide, AttachmentSide::Right);
        QVERIFY(!result.flipped);

        request.anchorGeometry = QRect(900, 300, 90, 200);
        result = WindowPlacement::adjacent(request);
        QCOMPARE(result.position, QPoint(788, 359));
        QCOMPARE(result.actualSide, AttachmentSide::Left);
        QVERIFY(result.flipped);
    }

    void horizontalWindowChainFlipsAsOneUnit()
    {
        HorizontalWindowChainRequest request;
        request.availableGeometry = QRect(0, 0, 1600, 900);
        request.anchorGeometry = QRect(180, 380, 100, 50);
        request.windowSizes = {QSize(320, 500), QSize(620, 600)};
        request.preferredSide = AttachmentSide::Right;
        request.gap = 12;
        HorizontalWindowChainResult result = WindowPlacement::horizontalChain(request);
        QCOMPARE(result.actualSide, AttachmentSide::Right);
        QCOMPARE(result.positions.size(), 2);
        QVERIFY(result.positions.at(0).x() > request.anchorGeometry.right());
        QVERIFY(result.positions.at(1).x() > result.positions.at(0).x() + 320);

        request.anchorGeometry = QRect(1320, 380, 100, 50);
        result = WindowPlacement::horizontalChain(request);
        QCOMPARE(result.actualSide, AttachmentSide::Left);
        QVERIFY(result.flipped);
        QVERIFY(result.positions.at(0).x() + 320 < request.anchorGeometry.left());
        QVERIFY(result.positions.at(1).x() + 620 < result.positions.at(0).x());
    }

    void horizontalWindowChainKeepsSpacingWhenNeitherSideFits()
    {
        HorizontalWindowChainRequest request;
        request.availableGeometry = QRect(0, 0, 1000, 800);
        request.anchorGeometry = QRect(450, 350, 100, 100);
        request.windowSizes = {QSize(300, 400), QSize(300, 500)};
        request.preferredSide = AttachmentSide::Right;
        request.gap = 10;
        const HorizontalWindowChainResult result = WindowPlacement::horizontalChain(request);
        QCOMPARE(result.positions.size(), 2);
        const QRect first(result.positions.at(0), request.windowSizes.at(0));
        const QRect second(result.positions.at(1), request.windowSizes.at(1));
        QVERIFY(request.availableGeometry.contains(first));
        QVERIFY(request.availableGeometry.contains(second));
        QVERIFY(!first.intersects(second));
        QCOMPARE(qAbs(second.left() - first.right()) - 1, request.gap);
    }

    void windowManagerPersistsNamedWindowsIndependently()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        SettingsRepository settings(directory.filePath(QStringLiteral("settings.ini")));
        QVERIFY(settings.load());
        WindowManager manager(&settings);
        QWidget pet;
        QWidget conversation;
        pet.resize(100, 100);
        conversation.resize(100, 100);
        pet.move(120, 140);
        conversation.move(360, 280);
        QVERIFY(manager.save(&pet, QStringLiteral("pet")));
        QVERIFY(manager.save(&conversation, QStringLiteral("conversation")));
        pet.move(0, 0);
        conversation.move(0, 0);
        manager.restore(&pet, QStringLiteral("pet"));
        manager.restore(&conversation, QStringLiteral("conversation"));
        QCOMPARE(pet.pos(), QPoint(120, 140));
        QCOMPARE(conversation.pos(), QPoint(360, 280));
    }

    void attachedWindowFollowsAnchorMovement()
    {
        QWidget anchor;
        QWidget bubble;
        anchor.resize(200, 200);
        bubble.resize(100, 80);
        anchor.move(300, 240);
        anchor.show();
        bubble.show();
        QTest::qWait(20);
        WindowAttachmentManager manager;
        manager.setAnchor(&anchor);
        manager.attach(&bubble, {AttachmentSide::Left, AttachmentAlignment::Center, 10});
        const QPoint initial = bubble.pos();
        anchor.move(340, 270);
        QTRY_COMPARE_WITH_TIMEOUT(bubble.pos() - initial, QPoint(40, 30), 1000);
    }

    void desktopWindowPolicyAppliesTransparentOverlayFlags()
    {
        QWidget overlay;
        DesktopWindowOptions options;
        options.frameless = true;
        options.translucentBackground = true;
        options.alwaysOnTop = true;
        options.showInTaskbar = false;
        options.acceptFocus = false;
        options.mouseInputTransparent = true;
        QString error;
        QVERIFY2(DesktopWindowPolicy::apply(&overlay, options, &error), qPrintable(error));
        QVERIFY(overlay.windowFlags().testFlag(Qt::FramelessWindowHint));
        QVERIFY(overlay.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
        QCOMPARE(overlay.windowType(), Qt::Tool);
        QVERIFY(overlay.testAttribute(Qt::WA_TranslucentBackground));
        QVERIFY(overlay.testAttribute(Qt::WA_ShowWithoutActivating));
        QVERIFY(overlay.testAttribute(Qt::WA_TransparentForMouseEvents));
        QVERIFY(DesktopWindowPolicy::setMouseInputTransparent(&overlay, false, &error));
        QVERIFY(!overlay.testAttribute(Qt::WA_TransparentForMouseEvents));
    }

    void hoverRevealControllerDelaysHidingAndHonorsGuard()
    {
        QWidget panel;
        QWidget hotZone;
        panel.resize(180, 80);
        hotZone.resize(20, 80);
        // 远离测试运行器的默认鼠标坐标，避免全局轮询主动触发显示。
        panel.move(5000, 5000);
        hotZone.move(5200, 5000);
        HoverRevealController controller;
        controller.setTimings(100, 0);
        bool allowHide = false;
        controller.setCanHidePredicate([&allowHide]() { return allowHide; });
        controller.bind(&panel, &hotZone);
        QVERIFY(!panel.isVisible());
        QVERIFY(!hotZone.isVisible());

        controller.setActive(false);
        QVERIFY(!controller.isActive());
        QVERIFY(!panel.isVisible());
        QVERIFY(!hotZone.isVisible());
        controller.setActive(true);
        QVERIFY(controller.isActive());
        QVERIFY(!hotZone.isVisible());

        QEvent enter(QEvent::Enter);
        QApplication::sendEvent(&hotZone, &enter);
        QVERIFY(controller.isRevealed());
        QVERIFY(panel.isVisible());
        QCOMPARE(panel.graphicsEffect(), nullptr);
        QTRY_COMPARE_WITH_TIMEOUT(panel.windowOpacity(), 1.0, 500);
        QVERIFY(!hotZone.isVisible());

        QEvent leaveBlocked(QEvent::Leave);
        QApplication::sendEvent(&panel, &leaveBlocked);
        QTest::qWait(140);
        QVERIFY(controller.isRevealed());
        QVERIFY(panel.isVisible());

        allowHide = true;
        QEvent leaveAllowed(QEvent::Leave);
        QApplication::sendEvent(&panel, &leaveAllowed);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.isRevealed(), 500);
        QVERIFY(!panel.isVisible());
        QVERIFY(!hotZone.isVisible());
    }

    void hoverRevealControllerPollsInvisibleActivationRegion()
    {
        QWidget panel;
        QWidget hotZone;
        panel.resize(180, 80);
        hotZone.resize(44, 120);
        panel.move(5000, 5000);
        // 不移动真实鼠标，而是把隐藏感应区放到当前鼠标位置验证轮询回退路径。
        hotZone.move(QCursor::pos() - QPoint(22, 60));
        HoverRevealController controller;
        controller.setTimings(100, 0);
        controller.bind(&panel, &hotZone);
        QVERIFY(!hotZone.isVisible());
        QTRY_VERIFY_WITH_TIMEOUT(controller.isRevealed(), 500);
        QVERIFY(panel.isVisible());
        controller.setActive(false);
        QVERIFY(!panel.isVisible());
    }

    void replyBubbleMaintainsOneStreamAndStartsTimerAfterFinish()
    {
        ReplyBubbleWindow bubble;
        bubble.setDisplayDuration(1200);
        bubble.beginReply();
        bubble.appendDelta(QStringLiteral("前半"));
        bubble.appendDelta(QStringLiteral("后半"));
        QCOMPARE(bubble.content(), QStringLiteral("前半后半"));
        QVERIFY(bubble.isVisible());
        QVERIFY(!bubble.dismissalTimerActive());

        bubble.finishReply();
        QVERIFY(bubble.dismissalTimerActive());
        QEvent enter(QEvent::Enter);
        QApplication::sendEvent(&bubble, &enter);
        QVERIFY(!bubble.dismissalTimerActive());
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(&bubble, &leave);
        QVERIFY(bubble.dismissalTimerActive());

        bubble.beginReply();
        bubble.appendDelta(QStringLiteral("新回复"));
        QCOMPARE(bubble.content(), QStringLiteral("新回复"));
        QVERIFY(!bubble.dismissalTimerActive());
        auto* content = bubble.findChild<QTextBrowser*>(QStringLiteral("replyBubbleContent"));
        QVERIFY(content != nullptr);
        QTest::mouseClick(content->viewport(), Qt::LeftButton);
        QVERIFY(bubble.isVisible());
        QVERIFY(bubble.findChild<QPushButton*>(QStringLiteral("replyBubbleExpand")) == nullptr);
        QVERIFY(bubble.styleSheet().contains(QStringLiteral("background:transparent")));
        QVERIFY(bubble.styleSheet().contains(QStringLiteral("border-radius:24px")));
        auto* tail = bubble.findChild<QWidget*>(QStringLiteral("replyBubbleTail"));
        QVERIFY(tail != nullptr);
        QCOMPARE(tail->property("pointsRight").toBool(), true);
        bubble.setAttachmentSide(AttachmentSide::Right);
        QCOMPARE(tail->property("pointsRight").toBool(), false);
        bubble.finishReply();
        auto* close = bubble.findChild<QPushButton*>(QStringLiteral("replyBubbleClose"));
        QVERIFY(close != nullptr);
        close->click();
        QVERIFY(!bubble.isVisible());
    }

    void errorBannerPersistsAndReplacesOnlyUserMessage()
    {
        ErrorBannerWindow banner;
        banner.showError(QStringLiteral("网络好像断开了，请稍后重试。"), true);
        QVERIFY(banner.isVisible());
        QVERIFY(banner.hasActiveError());
        QCOMPARE(banner.message(), QStringLiteral("网络好像断开了，请稍后重试。"));
        QTest::qWait(120);
        QVERIFY(banner.isVisible());

        banner.showError(QStringLiteral("我还没有拿到模型密钥，请先到设置里配置一下。"), false);
        QCOMPARE(banner.message(), QStringLiteral("我还没有拿到模型密钥，请先到设置里配置一下。"));
        banner.hide();
        QVERIFY(banner.hasActiveError());
        banner.restoreIfActive();
        QVERIFY(banner.isVisible());
        auto* close = banner.findChild<QPushButton*>(QStringLiteral("errorBannerClose"));
        QVERIFY(close != nullptr);
        close->click();
        QVERIFY(!banner.isVisible());
        QVERIFY(!banner.hasActiveError());
    }

    void mainWindowBuildsTransparentShellAndMinimizesAllWindows()
    {
        MainWindow window;
        QVERIFY(window.windowFlags().testFlag(Qt::FramelessWindowHint));
        QVERIFY(window.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
        QCOMPARE(window.windowType(), Qt::Window);
        QVERIFY(window.testAttribute(Qt::WA_TranslucentBackground));
        QVERIFY(window.findChild<ActionPanel*>(QStringLiteral("actionPanel")) != nullptr);
        QVERIFY(window.findChild<ChatInputPanel*>(QStringLiteral("chatInputPanel")) != nullptr);
        QVERIFY(window.findChild<ReplyBubbleWindow*>(QStringLiteral("replyBubble")) != nullptr);
        QVERIFY(window.findChild<ErrorBannerWindow*>(QStringLiteral("errorBanner")) != nullptr);
        auto* conversations = window.conversationWindow();
        QVERIFY(conversations != nullptr);

        window.showPetShell();
        QVERIFY(window.isVisible());
        QVERIFY(!window.findChild<ReplyBubbleWindow*>(QStringLiteral("replyBubble"))->isVisible());
        QVERIFY(!window.findChild<ErrorBannerWindow*>(QStringLiteral("errorBanner"))->isVisible());
        QVERIFY(!conversations->isVisible());

        auto* openConversations = window.findChild<QPushButton*>(
            QStringLiteral("conversationManagerButton"));
        QVERIFY(openConversations != nullptr);
        openConversations->click();
        QVERIFY(conversations->isVisible());

        auto* minimize = window.findChild<QPushButton*>(QStringLiteral("petMinimizeButton"));
        QVERIFY(minimize != nullptr);
        minimize->click();
        QVERIFY(!window.isVisible());
        QVERIFY(!window.findChild<ActionPanel*>(QStringLiteral("actionPanel"))->isVisible());
        QVERIFY(!window.findChild<ChatInputPanel*>(QStringLiteral("chatInputPanel"))->isVisible());
        QVERIFY(!window.findChild<ReplyBubbleWindow*>(QStringLiteral("replyBubble"))->isVisible());
        QVERIFY(!window.findChild<ErrorBannerWindow*>(QStringLiteral("errorBanner"))->isVisible());
        QVERIFY(!conversations->isVisible());
    }

    void mainWindowDragIsClampedToAvailableScreen()
    {
        MainWindow window;
        window.show();
        QTest::qWait(20);
        QScreen* screen = QGuiApplication::primaryScreen();
        QVERIFY(screen != nullptr);
        const QRect available = screen->availableGeometry();
        window.move(available.center() - QPoint(window.width() / 2, window.height() / 2));
        const QPoint localPress = window.rect().center();
        QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, localPress);
        const QPoint targetGlobal = available.topLeft() - QPoint(1000, 1000);
        QMouseEvent moveEvent(QEvent::MouseMove, QPointF(-1000, -1000),
                              QPointF(targetGlobal), Qt::NoButton, Qt::LeftButton,
                              Qt::NoModifier);
        QApplication::sendEvent(&window, &moveEvent);
        QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, QPoint(0, 0));
        QVERIFY(available.contains(window.frameGeometry()));
    }

    void mainWindowCloseEntrypointsRequestApplicationExit()
    {
        MainWindow window;
        QSignalSpy exitSpy(&window, &MainWindow::applicationExitRequested);

        window.showPetShell();
        QVERIFY(window.close());
        QCOMPARE(exitSpy.count(), 1);
        QVERIFY(!window.isVisible());

        window.showPetShell();
        auto* closeButton = window.findChild<QPushButton*>(QStringLiteral("petCloseButton"));
        QVERIFY(closeButton != nullptr);
        closeButton->click();
        QCOMPARE(exitSpy.count(), 2);
    }

    void floatingControlsUseTransparentBasesAndOpaqueRoundedControls()
    {
        MainWindow window;
        auto* actions = window.findChild<ActionPanel*>(QStringLiteral("actionPanel"));
        auto* input = window.findChild<ChatInputPanel*>(QStringLiteral("chatInputPanel"));
        auto* bubble = window.findChild<ReplyBubbleWindow*>(QStringLiteral("replyBubble"));
        auto* error = window.findChild<ErrorBannerWindow*>(QStringLiteral("errorBanner"));
        QVERIFY(actions != nullptr && input != nullptr && bubble != nullptr && error != nullptr);
        QVERIFY(actions->testAttribute(Qt::WA_TranslucentBackground));
        QVERIFY(input->testAttribute(Qt::WA_TranslucentBackground));
        QVERIFY(actions->styleSheet().contains(QStringLiteral("background:transparent")));
        QVERIFY(input->styleSheet().contains(QStringLiteral("background:transparent")));
        QVERIFY(actions->styleSheet().contains(QStringLiteral("border-radius:16px")));
        QVERIFY(input->styleSheet().contains(QStringLiteral("border-radius:17px")));
        for (QPushButton* button : actions->findChildren<QPushButton*>()) {
            QVERIFY(button->minimumHeight() >= 38);
        }
        auto* editor = input->findChild<QTextEdit*>(QStringLiteral("chatInput"));
        QVERIFY(editor != nullptr);
        QVERIFY(input->styleSheet().contains(QStringLiteral("background:#fffdf8")));

        QVERIFY(bubble->testAttribute(Qt::WA_TranslucentBackground));
        QVERIFY(bubble->styleSheet().contains(QStringLiteral("background:transparent")));
        QVERIFY(bubble->styleSheet().contains(QStringLiteral("#fffaf0")));
        QVERIFY(!error->testAttribute(Qt::WA_TranslucentBackground));
        QVERIFY(error->styleSheet().contains(QStringLiteral("#fffaf0")));

        SettingsDialog settings(nullptr);
        QVERIFY(!settings.testAttribute(Qt::WA_TranslucentBackground));
        QVERIFY(settings.styleSheet().contains(QStringLiteral("#fffaf0")));
    }

    void mainWindowRunsStreamingChatAndShowsReplyBubble()
    {
        QTemporaryDir temporaryDirectory;
        Database database;
        QVERIFY(database.open(QDir(temporaryDirectory.path()).filePath(QStringLiteral("ui.sqlite"))));
        SqliteConversationRepository conversations(&database);
        const QString conversationId = conversations.createConversation(QStringLiteral("界面"));
        MemoryOrchestrator memory(&conversations);
        MockChatProvider provider(QStringLiteral("界面回复"));
        ChatController controller(&provider, &memory);
        QHash<QString, QString> errorMessages;
        QVERIFY(controller.setPersonaConfig(testPersona(&errorMessages)));
        MainWindow window;
        window.setModelErrorMessages(errorMessages);
        window.setChatController(&controller);
        window.setConversation(conversationId, {});
        auto* input = window.findChild<QTextEdit*>(QStringLiteral("chatInput"));
        auto* send = window.findChild<QPushButton*>(QStringLiteral("sendButton"));
        auto* bubble = window.findChild<ReplyBubbleWindow*>(QStringLiteral("replyBubble"));
        QVERIFY(input != nullptr && send != nullptr && bubble != nullptr);

        input->setPlainText(QStringLiteral("界面消息"));
        QTest::mouseClick(send, Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(bubble->content(), QStringLiteral("界面回复"), 1000);
        QCOMPARE(conversations.recentMessages(conversationId, 10).size(), 2);
        QCOMPARE(bubble->content(), QStringLiteral("界面回复"));
        QVERIFY(bubble->dismissalTimerActive());
        QVERIFY(!window.conversationWindow()->isVisible());
        auto* bubbleContent = bubble->findChild<QTextBrowser*>(
            QStringLiteral("replyBubbleContent"));
        QVERIFY(bubbleContent != nullptr);
        QTest::mouseClick(bubbleContent->viewport(), Qt::LeftButton);
        QVERIFY(!window.conversationWindow()->isVisible());
    }

    void conversationHistoryKeepsTransientMessagesWhileHidden()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        SettingsRepository settings(temporaryDirectory.filePath(QStringLiteral("settings.ini")));
        QVERIFY(settings.load());
        Database database;
        QVERIFY(database.open(temporaryDirectory.filePath(QStringLiteral("history-cache.sqlite"))));
        SqliteConversationRepository repository(&database);
        ConversationController controller(&repository, &settings);
        QVERIFY(controller.initialize());

        ConversationWindow window;
        window.setController(&controller);
        window.show();
        window.appendMessage(MessageRole::User, QStringLiteral("即时用户消息"));
        window.beginAssistantReply();
        window.appendAssistantDelta(QStringLiteral("部分"));
        auto* list = window.findChild<QListWidget*>(QStringLiteral("conversationList"));
        QVERIFY(list != nullptr && list->count() == 1);
        list->setCurrentRow(0);
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                          list->visualItemRect(list->item(0)).center());
        QTRY_VERIFY(window.historyWindow() != nullptr);
        auto* history = window.historyWindow();
        QCOMPARE(history->findChildren<QLabel*>(QStringLiteral("userMessageBubble")).size(), 1);
        auto assistant = history->findChildren<QLabel*>(QStringLiteral("assistantMessageBubble"));
        QCOMPARE(assistant.size(), 1);
        QCOMPARE(assistant.first()->text(), QStringLiteral("部分"));

        history->hide();
        window.appendAssistantDelta(QStringLiteral("回复"));
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                          list->visualItemRect(list->item(0)).center());
        QTRY_VERIFY(history->isVisible());
        assistant = history->findChildren<QLabel*>(QStringLiteral("assistantMessageBubble"));
        QCOMPARE(assistant.size(), 1);
        QCOMPARE(assistant.first()->text(), QStringLiteral("部分回复"));

        history->hide();
        window.finishAssistantReply(QStringLiteral("最终回复"));
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                          list->visualItemRect(list->item(0)).center());
        QTRY_VERIFY(history->isVisible());
        assistant = history->findChildren<QLabel*>(QStringLiteral("assistantMessageBubble"));
        QCOMPARE(assistant.size(), 1);
        QCOMPARE(assistant.first()->text(), QStringLiteral("最终回复"));
    }

    void conversationHistoryCoalescesStreamingScrollRequests()
    {
        ConversationHistoryWindow history;
        auto* timer = history.findChild<QTimer*>(
            QStringLiteral("conversationHistoryScrollTimer"));
        QVERIFY(timer != nullptr);
        for (int index = 0; index < 100; ++index) {
            history.appendAssistantDelta(QStringLiteral("x"));
        }
        QVERIFY(timer->isActive());
        QTest::qWait(10);
        QVERIFY(!timer->isActive());
        QCOMPARE(history.findChildren<QLabel*>(
            QStringLiteral("assistantMessageBubble")).size(), 1);
    }

    void conversationCanBeRecoveredAfterDatabaseReopen()
    {
        QTemporaryDir temporaryDirectory;
        const QString path = QDir(temporaryDirectory.path()).filePath(QStringLiteral("recover.sqlite"));
        QString conversationId;
        {
            Database database;
            QVERIFY(database.open(path));
            SqliteConversationRepository conversations(&database);
            conversationId = conversations.createConversation(QStringLiteral("可恢复会话"));
            QVERIFY(conversations.appendMessage(
                conversationId, Message::create(MessageRole::User, QStringLiteral("重启前消息"))));
        }
        {
            Database database;
            QVERIFY(database.open(path));
            SqliteConversationRepository conversations(&database);
            const QVector<Conversation> active = conversations.listConversations(false);
            QCOMPARE(active.size(), 1);
            QCOMPARE(active.first().id, conversationId);
            const QVector<ConversationMessage> messages = conversations.recentMessages(conversationId, 20);
            QCOMPARE(messages.size(), 1);
            QCOMPARE(messages.first().message.content, QStringLiteral("重启前消息"));
        }
    }

    void conversationControllerManagesCreateSwitchArchive()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        SettingsRepository settings(directory.filePath(QStringLiteral("settings.ini")));
        QVERIFY(settings.load());
        Database database;
        QVERIFY(database.open(directory.filePath(QStringLiteral("chat.sqlite"))));
        SqliteConversationRepository repository(&database);
        ConversationController controller(&repository, &settings);
        QSignalSpy listSpy(&controller, &ConversationController::conversationsChanged);
        QSignalSpy currentSpy(&controller, &ConversationController::currentConversationChanged);
        AppError error;
        QVERIFY2(controller.initialize(&error), qPrintable(error.technicalMessage));
        QCOMPARE(controller.conversations().size(), 1);
        const QString firstId = controller.currentConversationId();
        QVERIFY(!firstId.isEmpty());
        QVERIFY(controller.createConversation(QStringLiteral("工作"), &error));
        QCOMPARE(controller.currentConversationTitle(), QStringLiteral("工作"));
        const QString secondId = controller.currentConversationId();
        QVERIFY(secondId != firstId);
        QVERIFY(controller.switchConversation(firstId, &error));
        QCOMPARE(controller.currentConversationId(), firstId);
        QVERIFY(controller.archiveConversation(firstId, &error));
        QCOMPARE(controller.currentConversationId(), secondId);
        QVERIFY(!controller.conversations().isEmpty());
        QCOMPARE(controller.archivedConversations().size(), 1);
        QCOMPARE(controller.archivedConversations().first().id, firstId);
        QVERIFY(listSpy.count() >= 1);
        QVERIFY(currentSpy.count() >= 3);
        QCOMPARE(settings.value(QStringLiteral("chat/current_conversation_id")).toString(), secondId);
    }

    void conversationControllerRestoresPersistedCurrentSession()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));
        const QString databasePath = directory.filePath(QStringLiteral("chat.sqlite"));
        QString selectedId;
        {
            SettingsRepository settings(settingsPath);
            QVERIFY(settings.load());
            Database database;
            QVERIFY(database.open(databasePath));
            SqliteConversationRepository repository(&database);
            ConversationController controller(&repository, &settings);
            QVERIFY(controller.initialize());
            QVERIFY(controller.createConversation(QStringLiteral("要恢复的会话")));
            selectedId = controller.currentConversationId();
        }
        {
            SettingsRepository settings(settingsPath);
            QVERIFY(settings.load());
            Database database;
            QVERIFY(database.open(databasePath));
            SqliteConversationRepository repository(&database);
            ConversationController controller(&repository, &settings);
            QVERIFY(controller.initialize());
            QCOMPARE(controller.currentConversationId(), selectedId);
            QCOMPARE(controller.currentConversationTitle(), QStringLiteral("要恢复的会话"));
        }
    }

    void conversationControllerDeletesConversationAndMessages()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));
        const QString databasePath = directory.filePath(QStringLiteral("chat.sqlite"));
        SettingsRepository settings(settingsPath);
        QVERIFY(settings.load());
        Database database;
        QVERIFY(database.open(databasePath));
        SqliteConversationRepository repository(&database);
        ConversationController controller(&repository, &settings);
        AppError error;
        QVERIFY2(controller.initialize(&error), qPrintable(error.technicalMessage));

        const QString firstId = controller.currentConversationId();
        QVERIFY(repository.appendMessage(firstId,
            Message::create(MessageRole::User, QStringLiteral("待删除消息"))));
        QVERIFY(!repository.recentMessages(firstId, 20).isEmpty());

        QVERIFY2(controller.deleteCurrentConversation(&error), qPrintable(error.technicalMessage));
        QVERIFY(controller.currentConversationId() != firstId);
        QCOMPARE(settings.value(QStringLiteral("chat/current_conversation_id")).toString(),
                 controller.currentConversationId());
        const auto deletedConversation = repository.getConversationResult(firstId);
        QVERIFY(!deletedConversation);
        QCOMPARE(deletedConversation.error().code, AppErrorCode::NotFound);
        QVERIFY(repository.recentMessages(firstId, 20).isEmpty());
        QSqlQuery messageCount(database.connection());
        messageCount.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM conversation_messages WHERE conversation_id=?"));
        messageCount.addBindValue(firstId);
        QVERIFY(messageCount.exec());
        QVERIFY(messageCount.next());
        QCOMPARE(messageCount.value(0).toInt(), 0);

        const QString secondId = controller.currentConversationId();
        QVERIFY(controller.createConversation(QStringLiteral("保留会话"), &error));
        const QString thirdId = controller.currentConversationId();
        QVERIFY(thirdId != secondId);
        QVERIFY2(controller.deleteConversation(secondId, &error), qPrintable(error.technicalMessage));
        const auto deletedNonCurrent = repository.getConversationResult(secondId);
        QVERIFY(!deletedNonCurrent);
        QCOMPARE(deletedNonCurrent.error().code, AppErrorCode::NotFound);
        QCOMPARE(controller.currentConversationId(), thirdId);
    }

    void conversationControllerRejectsChangesWhileChatIsRunning()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        SettingsRepository settings(directory.filePath(QStringLiteral("settings.ini")));
        QVERIFY(settings.load());
        Database database;
        QVERIFY(database.open(directory.filePath(QStringLiteral("chat.sqlite"))));
        SqliteConversationRepository repository(&database);
        ConversationController conversations(&repository, &settings);
        QVERIFY(conversations.initialize());
        MemoryOrchestrator memory(&repository);
        MockChatProvider provider(QStringLiteral("稍后完成"));
        ChatController chat(&provider, &memory);
        QVERIFY(chat.setPersonaConfig(testPersona()));
        conversations.setChatController(&chat);
        QSignalSpy finishSpy(&chat, &ChatController::replyFinished);

        QVERIFY(!chat.sendMessage(conversations.currentConversationId(),
                                  QStringLiteral("运行中的请求")).isEmpty());
        AppError error;
        QVERIFY(!conversations.createConversation(QStringLiteral("不应创建"), &error));
        QCOMPARE(error.code, AppErrorCode::Busy);
        QCOMPARE(conversations.conversations().size(), 1);
        QVERIFY(finishSpy.wait(1000));
        QVERIFY(conversations.createConversation(QStringLiteral("请求完成后可创建"), &error));
    }

    void mainWindowListsSwitchesAndArchivesConversations()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        SettingsRepository settings(directory.filePath(QStringLiteral("settings.ini")));
        QVERIFY(settings.load());
        Database database;
        QVERIFY(database.open(directory.filePath(QStringLiteral("chat.sqlite"))));
        SqliteConversationRepository repository(&database);
        ConversationController controller(&repository, &settings);
        QVERIFY(controller.initialize());
        const QString firstId = controller.currentConversationId();
        QVERIFY(repository.appendMessage(
            firstId, Message::create(MessageRole::User, QStringLiteral("第一会话历史"))));
        QVERIFY(repository.appendMessage(
            firstId, Message::create(MessageRole::Assistant, QStringLiteral("桌宠历史回复"))));
        QVERIFY(controller.createConversation(QStringLiteral("第二会话")));

        MainWindow window;
        window.setConversationController(&controller);
        auto* manager = window.conversationWindow();
        auto* selector = manager->findChild<QListWidget*>(QStringLiteral("conversationList"));
        auto* archive = manager->findChild<QPushButton*>(QStringLiteral("archiveConversationButton"));
        QVERIFY(selector != nullptr && manager != nullptr && archive != nullptr);
        QCOMPARE(selector->count(), 2);
        QVERIFY(manager->parentWidget() == nullptr);
        QVERIFY(manager->historyWindow() == nullptr);

        window.showPetShell();
        auto* openManager = window.findChild<QPushButton*>(QStringLiteral("conversationManagerButton"));
        QVERIFY(openManager != nullptr);
        openManager->click();
        QVERIFY(manager->isVisible());
        QVERIFY(manager->styleSheet().contains(QStringLiteral("#dcecff")));
        QVERIFY(manager->testAttribute(Qt::WA_TranslucentBackground));
        QVERIFY(manager->styleSheet().contains(QStringLiteral("border-radius:24px")));

        int firstIndex = -1;
        int secondIndex = -1;
        for (int index = 0; index < selector->count(); ++index) {
            if (selector->item(index)->data(Qt::UserRole).toString() == firstId) firstIndex = index;
            else secondIndex = index;
        }
        QVERIFY(firstIndex >= 0 && secondIndex >= 0);
        selector->setCurrentRow(firstIndex);
        QTest::mouseClick(selector->viewport(), Qt::LeftButton, Qt::NoModifier,
                          selector->visualItemRect(selector->item(firstIndex)).center());
        QCOMPARE(window.conversationId(), firstId);
        QTRY_VERIFY_WITH_TIMEOUT(manager->historyWindow() != nullptr, 500);
        auto* firstHistory = manager->historyWindow();
        QVERIFY(firstHistory != nullptr && firstHistory->isVisible());
        QVERIFY(firstHistory->parentWidget() == nullptr);
        QCOMPARE(firstHistory->conversationId(), firstId);
        QVERIFY(firstHistory->styleSheet().contains(QStringLiteral("#fffaf0")));
        QVERIFY(firstHistory->testAttribute(Qt::WA_TranslucentBackground));
        QVERIFY(firstHistory->styleSheet().contains(QStringLiteral("border-radius:24px")));
        auto* title = firstHistory->findChild<QLabel*>(QStringLiteral("conversationHistoryTitle"));
        QVERIFY(title != nullptr);
        QCOMPARE(title->text(), QStringLiteral("默认会话"));
        const auto userBubbles = firstHistory->findChildren<QLabel*>(QStringLiteral("userMessageBubble"));
        const auto assistantBubbles = firstHistory->findChildren<QLabel*>(
            QStringLiteral("assistantMessageBubble"));
        QCOMPARE(userBubbles.size(), 1);
        QCOMPARE(assistantBubbles.size(), 1);
        QCOMPARE(firstHistory->findChildren<QLabel*>(QStringLiteral("petAvatar")).size(), 1);
        QCOMPARE(selector->verticalScrollBarPolicy(), Qt::ScrollBarAsNeeded);
        auto* historyScroll = firstHistory->findChild<QScrollArea*>(
            QStringLiteral("conversationHistoryScrollArea"));
        QVERIFY(historyScroll != nullptr);
        QCOMPARE(historyScroll->verticalScrollBarPolicy(), Qt::ScrollBarAsNeeded);
        QVERIFY(userBubbles.first()->styleSheet().contains(QStringLiteral("#a88cf5")));
        QVERIFY(assistantBubbles.first()->styleSheet().contains(QStringLiteral("#79adf3")));

        selector->setCurrentRow(secondIndex);
        QTest::mouseClick(selector->viewport(), Qt::LeftButton, Qt::NoModifier,
                          selector->visualItemRect(selector->item(secondIndex)).center());
        QCOMPARE(manager->historyWindow(), firstHistory);
        QCOMPARE(firstHistory->conversationId(), controller.currentConversationId());
        manager->hide();
        QTRY_VERIFY_WITH_TIMEOUT(!firstHistory->isVisible(), 500);
        manager->show();

        selector->setCurrentRow(firstIndex);
        QTest::mouseClick(selector->viewport(), Qt::LeftButton, Qt::NoModifier,
                          selector->visualItemRect(selector->item(firstIndex)).center());

        QTest::mouseClick(archive, Qt::LeftButton);
        QCOMPARE(selector->count(), 1);
        QVERIFY(window.conversationId() != firstId);
        const auto archived = repository.getConversationResult(firstId);
        QVERIFY(archived);
        QVERIFY(archived.value().isArchived());
    }

    void settingsControllerAppliesAndPersistsRuntimeConfiguration()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString modelPath = directory.filePath(QStringLiteral("models.json"));
        const QString appPath = directory.filePath(QStringLiteral("app.json"));
        QVERIFY(QFile::copy(QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("config/model-providers.json")), modelPath));
        QVERIFY(QFile::copy(QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("config/app-settings.json")), appPath));
        ModelConfigRepository models(modelPath);
        AppConfigRepository app(appPath);
        Database database;
        QVERIFY(database.open(directory.filePath(QStringLiteral("chat.sqlite"))));
        SqliteConversationRepository conversations(&database);
        MemoryOrchestrator memory(&conversations);
        HttpClient http;
        SecretStore secrets;
        ChatProviderFactory factory(&http, &secrets);
        ProviderManager providers(&factory);
        ModelProviderConfig initial;
        QVERIFY2(models.loadActive(&initial), "active model should load");
        QString error;
        QVERIFY2(providers.switchProvider(initial, &error), qPrintable(error));
        ChatController chat(&providers, &memory);
        PersonaConfig persona;
        QHash<QString, QString> messages;
        MemoryLimits limits;
        QVERIFY2(app.load(&persona, &messages, &error, &limits), qPrintable(error));
        QVERIFY(chat.setPersonaConfig(persona));
        ErrorCenter errors;
        SettingsController settings(&models, &app, &factory, &providers, &chat,
                                     &memory, &secrets, &errors);
        ModelProviderConfig changed = initial;
        changed.displayName = QStringLiteral("Mock 已应用");
        changed.profileId = QStringLiteral("settings-test");
        changed.providerType = QStringLiteral("mock");
        changed.mockReply = QStringLiteral("测试回复");
        changed.baseUrl.clear(); changed.model.clear();
        changed.credentialService.clear(); changed.credentialAccount.clear();
        PersonaConfig changedPersona = persona;
        changedPersona.name = QStringLiteral("设置后的桌宠");
        MemoryLimits changedLimits = limits;
        changedLimits.recentMessageLimit = limits.recentMessageLimit + 1;
        AppError applyError;
        QVERIFY2(settings.apply(changed, changedPersona, changedLimits, {}, &applyError),
                 qPrintable(applyError.technicalMessage));
        QCOMPARE(providers.activeConfiguration().profileId, QStringLiteral("settings-test"));
        QCOMPARE(chat.personaConfig().name, QStringLiteral("设置后的桌宠"));
        QCOMPARE(memory.limits().recentMessageLimit, changedLimits.recentMessageLimit);
        ModelProviderConfig persisted;
        QVERIFY2(models.loadActive(&persisted, &error), qPrintable(error));
        QCOMPARE(persisted.profileId, QStringLiteral("settings-test"));
        PersonaConfig persistedPersona;
        QHash<QString, QString> persistedMessages;
        MemoryLimits persistedLimits;
        QVERIFY2(app.load(&persistedPersona, &persistedMessages, &error, &persistedLimits), qPrintable(error));
        QCOMPARE(persistedPersona.name, QStringLiteral("设置后的桌宠"));
        QCOMPARE(persistedLimits.recentMessageLimit, changedLimits.recentMessageLimit);
        QCOMPARE(persistedMessages, messages);
        SettingsDialog dialog(&settings);
        auto* profileList = dialog.findChild<QComboBox*>(QStringLiteral("settingsModelProfile"));
        auto* modelUrl = dialog.findChild<QLineEdit*>(QStringLiteral("settingsModelUrl"));
        auto* applyButton = dialog.findChild<QPushButton*>(QStringLiteral("settingsApplyButton"));
        QVERIFY(profileList != nullptr && modelUrl != nullptr && applyButton != nullptr);
        auto* personaGroup = dialog.findChild<QGroupBox*>(QStringLiteral("settingsPersonaGroup"));
        auto* userAddress = dialog.findChild<QLineEdit*>(QStringLiteral("settingsUserAddress"));
        QVERIFY(personaGroup != nullptr && userAddress != nullptr);
        QStringList personaLabels;
        for (const QLabel* label : personaGroup->findChildren<QLabel*>()) {
            personaLabels.append(label->text());
        }
        QVERIFY(!personaLabels.contains(QStringLiteral("名称")));
        QVERIFY(!personaLabels.contains(QStringLiteral("语气")));
        QVERIFY(personaLabels.contains(QStringLiteral("对你的称呼")));
        auto* relevantLimit = dialog.findChild<QSpinBox*>(
            QStringLiteral("settingsRelevantHistoryLimit"));
        auto* longTermLimit = dialog.findChild<QSpinBox*>(
            QStringLiteral("settingsLongTermMemoryLimit"));
        auto* contextLimit = dialog.findChild<QSpinBox*>(
            QStringLiteral("settingsContextTokenLimit"));
        QVERIFY(relevantLimit != nullptr && longTermLimit != nullptr && contextLimit != nullptr);
        QCOMPARE(relevantLimit->maximum(), MemoryLimits::MaximumRetrievedItems);
        QCOMPARE(longTermLimit->maximum(), MemoryLimits::MaximumRetrievedItems);
        QCOMPARE(contextLimit->maximum(), MemoryLimits::MaximumContextTokens);
        QCOMPARE(profileList->currentData().toString(), QStringLiteral("settings-test"));
        QCOMPARE(profileList->currentText(), QStringLiteral("Mock 已应用"));
        const QString hiddenPersonaName = chat.personaConfig().name;
        const QString hiddenPersonaTone = chat.personaConfig().tone;
        userAddress->setText(QStringLiteral("指挥官"));
        dialog.show();
        QTest::mouseClick(applyButton, Qt::LeftButton);
        QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
        QCOMPARE(chat.personaConfig().name, hiddenPersonaName);
        QCOMPARE(chat.personaConfig().tone, hiddenPersonaTone);
        QCOMPARE(chat.personaConfig().userAddress, QStringLiteral("指挥官"));
        QVERIFY(chat.personaConfig().systemInstruction().contains(QStringLiteral("指挥官")));
        QVERIFY2(app.load(&persistedPersona, &persistedMessages, &error, &persistedLimits),
                 qPrintable(error));
        QCOMPARE(persistedPersona.name, hiddenPersonaName);
        QCOMPARE(persistedPersona.tone, hiddenPersonaTone);
        QCOMPARE(persistedPersona.userAddress, QStringLiteral("指挥官"));
    }

    void settingsControllerTestsCandidateWithoutSwitchingProvider()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString modelPath = directory.filePath(QStringLiteral("models.json"));
        const QString appPath = directory.filePath(QStringLiteral("app.json"));
        QVERIFY(QFile::copy(QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("config/model-providers.json")), modelPath));
        QVERIFY(QFile::copy(QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("config/app-settings.json")), appPath));
        ModelConfigRepository models(modelPath);
        AppConfigRepository app(appPath);
        Database database;
        QVERIFY(database.open(directory.filePath(QStringLiteral("chat.sqlite"))));
        SqliteConversationRepository conversations(&database);
        MemoryOrchestrator memory(&conversations);
        HttpClient http;
        SecretStore secrets;
        ChatProviderFactory factory(&http, &secrets);
        ProviderManager providers(&factory);
        ModelProviderConfig initial;
        QVERIFY(models.loadActive(&initial));
        QVERIFY(providers.switchProvider(initial));
        ChatController chat(&providers, &memory);
        PersonaConfig persona;
        QHash<QString, QString> messages;
        MemoryLimits limits;
        QVERIFY(app.load(&persona, &messages, nullptr, &limits));
        QVERIFY(chat.setPersonaConfig(persona));
        ErrorCenter errors;
        SettingsController settings(&models, &app, &factory, &providers, &chat,
                                     &memory, &secrets, &errors);
        QSignalSpy spy(&settings, &SettingsController::connectionTestFinished);
        ModelProviderConfig candidate = initial;
        candidate.displayName = QStringLiteral("尚未应用的候选模型");
        candidate.mockReply = QStringLiteral("pong");
        QVERIFY(settings.testConnection(candidate, {}));
        QVERIFY(spy.wait(1000));
        QVERIFY(spy.first().at(0).toBool());
        QCOMPARE(providers.activeConfiguration().displayName, initial.displayName);
        const QString conversationId = conversations.createConversation(QStringLiteral("忙碌检查"));
        QSignalSpy failedSpy(&chat, &ChatController::requestFailed);
        QVERIFY(!chat.sendMessage(conversationId, QStringLiteral("尚未完成")).isEmpty());
        AppError busyError;
        QVERIFY(!settings.apply(initial, persona, limits, {}, &busyError));
        QCOMPARE(busyError.code, AppErrorCode::Busy);
        settings.cancelActiveChat();
        QVERIFY(failedSpy.wait(1000));

        ModelProviderConfig remote;
        remote.profileId = QStringLiteral("remote-without-key");
        remote.providerType = QStringLiteral("openai-compatible");
        remote.displayName = QStringLiteral("Remote Without Key");
        remote.baseUrl = QStringLiteral("https://example.invalid/v1");
        remote.model = QStringLiteral("test-model");
        remote.credentialService = QStringLiteral("zhu_screen_pet");
        remote.credentialAccount = QStringLiteral("missing-key-%1")
            .arg(QUuid::createUuid().toString(QUuid::Id128));
        remote.timeoutMs = 1000;
        remote.maxRetries = 0;
        remote.retryBaseDelayMs = 50;
        AppError secretError;
        QVERIFY(!settings.apply(remote, persona, limits, {}, &secretError));
        QCOMPARE(secretError.code, AppErrorCode::ConfigInvalid);
        QCOMPARE(secretError.operation, QStringLiteral("settings.validate_secret"));
        QCOMPARE(providers.activeConfiguration().profileId, initial.profileId);
    }

    void settingsControllerRollsBackWhenSecondConfigCannotBeSaved()
    {
#ifdef Q_OS_WIN
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString modelPath = directory.filePath(QStringLiteral("models.json"));
        const QString appPath = directory.filePath(QStringLiteral("app.json"));
        QVERIFY(QFile::copy(QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("config/model-providers.json")), modelPath));
        QVERIFY(QFile::copy(QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("config/app-settings.json")), appPath));
        ModelConfigRepository models(modelPath);
        AppConfigRepository app(appPath);
        Database database;
        QVERIFY(database.open(directory.filePath(QStringLiteral("chat.sqlite"))));
        SqliteConversationRepository conversations(&database);
        MemoryOrchestrator memory(&conversations);
        HttpClient http;
        SecretStore secrets;
        ChatProviderFactory factory(&http, &secrets);
        ProviderManager providers(&factory);
        ModelProviderConfig initial;
        QVERIFY(models.loadActive(&initial));
        QVERIFY(providers.switchProvider(initial));
        ChatController chat(&providers, &memory);
        PersonaConfig persona;
        QHash<QString, QString> messages;
        MemoryLimits limits;
        QVERIFY(app.load(&persona, &messages, nullptr, &limits));
        QVERIFY(chat.setPersonaConfig(persona));
        ErrorCenter errors;
        SettingsController settings(&models, &app, &factory, &providers, &chat,
                                     &memory, &secrets, &errors);
        QByteArray modelBefore;
        QByteArray appBefore;
        QVERIFY(models.snapshot(&modelBefore));
        QVERIFY(app.snapshot(&appBefore));
        const HANDLE lock = CreateFileW(
            reinterpret_cast<LPCWSTR>(appPath.utf16()), GENERIC_READ,
            FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        QVERIFY(lock != INVALID_HANDLE_VALUE);
        ModelProviderConfig changed = initial;
        changed.profileId = QStringLiteral("must-be-rolled-back");
        changed.displayName = QStringLiteral("不应保留");
        AppError applyError;
        QVERIFY(!settings.apply(changed, persona, limits, {}, &applyError));
        CloseHandle(lock);
        QCOMPARE(applyError.code, AppErrorCode::Io);
        QCOMPARE(providers.activeConfiguration().profileId, initial.profileId);
        QByteArray modelAfter;
        QByteArray appAfter;
        QVERIFY(models.snapshot(&modelAfter));
        QVERIFY(app.snapshot(&appAfter));
        QCOMPARE(modelAfter, modelBefore);
        QCOMPARE(appAfter, appBefore);
#else
        QSKIP("Windows file sharing is used to make the second save fail deterministically");
#endif
    }
};

} // namespace zhu_screen_pet

QTEST_MAIN(zhu_screen_pet::SmokeTest)
#include "smoke_test.moc"
