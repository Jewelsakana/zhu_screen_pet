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
#include <QFontMetrics>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScreen>
#include <QSqlDatabase>
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
#include "infrastructure/ScreenFingerprint.h"
#include "infrastructure/Logger.h"
#include "infrastructure/SecretStore.h"
#include "model/MockChatProvider.h"
#include "model/ChatProviderFactory.h"
#include "model/ProviderManager.h"
#include "memory/SqliteConversationRepository.h"
#include "memory/SqliteMemoryRepository.h"
#include "memory/SqliteObservationRepository.h"
#include "memory/MemoryOrchestrator.h"
#include "app/ChatController.h"
#include "app/ConversationController.h"
#include "app/SettingsController.h"
#include "app/ScreenObservationCoordinator.h"
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

class ControllableChatProvider final : public ChatProvider
{
public:
    QString startChat(const std::vector<Message>& messages,
                      const ChatOptions& options) override
    {
        Q_UNUSED(messages);
        Q_UNUSED(options);
        activeRequestId_ = QUuid::createUuid().toString(QUuid::Id128);
        ++requestCount_;
        emit chatStarted(activeRequestId_);
        return activeRequestId_;
    }

    void cancel(const QString& requestId) override
    {
        if (requestId != activeRequestId_) return;
        finish(ChatResult::failure(
            {ModelErrorCode::Cancelled, QStringLiteral("cancelled"), 0}));
    }

    void finish(const ChatResult& result)
    {
        if (activeRequestId_.isEmpty()) return;
        const QString requestId = activeRequestId_;
        activeRequestId_.clear();
        emit chatFinished(requestId, result);
    }

    int requestCount() const { return requestCount_; }

private:
    QString activeRequestId_;
    int requestCount_ = 0;
};

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

    void databaseInitializesSchema()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString databasePath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("zhu_screen_pet.sqlite"));
        Database database;
        QString errorMessage;
        QVERIFY2(database.open(databasePath, &errorMessage), qPrintable(errorMessage));
        QVERIFY(database.isOpen());
        QCOMPARE(database.schemaVersion(), 5);
    }

    void databaseMigratesVersionThreeMemorySourcesToObservationForeignKey()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("schema-v3.sqlite"));
        const QString connectionName = QStringLiteral("schema_v3_%1")
            .arg(QUuid::createUuid().toString(QUuid::Id128));
        {
            QSqlDatabase legacy = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
            legacy.setDatabaseName(path);
            QVERIFY(legacy.open());
            QSqlQuery query(legacy);
            QVERIFY(query.exec(QStringLiteral("CREATE TABLE schema_version(version INTEGER NOT NULL)")));
            QVERIFY(query.exec(QStringLiteral("INSERT INTO schema_version VALUES(3)")));
            QVERIFY(query.exec(QStringLiteral(
                "CREATE TABLE conversations(id TEXT PRIMARY KEY,title TEXT NOT NULL,"
                "created_at TEXT NOT NULL,updated_at TEXT NOT NULL,archived_at TEXT)")));
            QVERIFY(query.exec(QStringLiteral(
                "CREATE TABLE conversation_messages(id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "conversation_id TEXT NOT NULL,role TEXT NOT NULL,content TEXT NOT NULL,"
                "token_count INTEGER NOT NULL DEFAULT 0,created_at TEXT NOT NULL,summarized_at TEXT)")));
            QVERIFY(query.exec(QStringLiteral(
                "CREATE TABLE memories(id INTEGER PRIMARY KEY AUTOINCREMENT,kind TEXT NOT NULL,"
                "content TEXT NOT NULL,source_event_id TEXT,created_at TEXT NOT NULL,expires_at TEXT)")));
            QVERIFY(query.exec(QStringLiteral(
                "CREATE TABLE database_capabilities(name TEXT PRIMARY KEY,enabled INTEGER NOT NULL,detail TEXT)")));
            QVERIFY(query.exec(QStringLiteral(
                "INSERT INTO conversations VALUES('legacy-chat','旧会话',"
                "'2026-01-01T00:00:00.000Z','2026-01-01T00:00:03.000Z',NULL)")));
            QVERIFY(query.exec(QStringLiteral(
                "INSERT INTO conversation_messages(conversation_id,role,content,created_at) VALUES"
                "('legacy-chat','user','用户发起了一次截图分析','2026-01-01T00:00:00.000Z'),"
                "('legacy-chat','assistant','旧屏幕观察摘要','2026-01-01T00:00:01.000Z'),"
                "('legacy-chat','user','保留的普通消息','2026-01-01T00:00:02.000Z')")));
            QVERIFY(query.exec(QStringLiteral(
                "INSERT INTO memories(kind,content,source_event_id,created_at) "
                "VALUES('long_term','保留的旧记忆','legacy-orphan','2026-01-01T00:00:00.000Z')")));
            query.finish();
            legacy.close();
        }
        QSqlDatabase::removeDatabase(connectionName);

        Database database;
        QString error;
        QVERIFY2(database.open(path, &error), qPrintable(error));
        QCOMPARE(database.schemaVersion(), 5);
        QSqlQuery memory(database.connection());
        QVERIFY(memory.exec(QStringLiteral(
            "SELECT content,source_event_id FROM memories ORDER BY id LIMIT 1")));
        QVERIFY(memory.next());
        QCOMPARE(memory.value(0).toString(), QStringLiteral("保留的旧记忆"));
        QVERIFY(memory.value(1).isNull());
        memory.finish();
        QSqlQuery messages(database.connection());
        QVERIFY(messages.exec(QStringLiteral(
            "SELECT role,content FROM conversation_messages ORDER BY id")));
        QVERIFY(messages.next());
        QCOMPARE(messages.value(0).toString(), QStringLiteral("user"));
        QCOMPARE(messages.value(1).toString(), QStringLiteral("保留的普通消息"));
        QVERIFY(!messages.next());
        QSqlQuery observation(database.connection());
        QVERIFY(observation.exec(QStringLiteral(
            "SELECT summary FROM observation_events WHERE id='legacy-1'")));
        QVERIFY(observation.next());
        QCOMPARE(observation.value(0).toString(), QStringLiteral("旧屏幕观察摘要"));

        bool sourceForeignKeyFound = false;
        QSqlQuery foreignKeys(database.connection());
        QVERIFY(foreignKeys.exec(QStringLiteral("PRAGMA foreign_key_list(memories)")));
        while (foreignKeys.next()) {
            if (foreignKeys.value(2).toString() == QStringLiteral("observation_events")
                && foreignKeys.value(3).toString() == QStringLiteral("source_event_id")
                && foreignKeys.value(4).toString() == QStringLiteral("id")
                && foreignKeys.value(6).toString() == QStringLiteral("SET NULL")) {
                sourceForeignKeyFound = true;
            }
        }
        QVERIFY(sourceForeignKeyFound);
        QSqlQuery check(database.connection());
        QVERIFY(check.exec(QStringLiteral("PRAGMA foreign_key_check")));
        QVERIFY(!check.next());
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

    void mainWindowSendsMemoryOnlyCapturedImage()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        Database database;
        QVERIFY(database.open(temporaryDirectory.filePath(QStringLiteral("vision-ui.sqlite"))));
        SqliteConversationRepository conversations(&database);
        const QString conversationId = conversations.createConversation(QStringLiteral("截图界面"));
        SqliteObservationRepository observations(&database);
        MemoryOrchestrator memory(&conversations, nullptr, &observations);
        MockChatProvider provider(QStringLiteral("我看到了当前屏幕"));
        ChatController controller(&provider, &memory);
        QVERIFY(controller.setPersonaConfig(testPersona()));
        MainWindow window;
        window.setCaptureDirectory(temporaryDirectory.path());
        window.setChatController(&controller);
        window.setConversation(conversationId, {});
        ScreenCapture* capture = window.findChild<ScreenCapture*>();
        QVERIFY(capture != nullptr);
        ImageCompressionOptions compression;
        capture->configure(true, 30000, temporaryDirectory.path(), compression);
        auto* observation = window.findChild<ScreenObservationCoordinator*>();
        QVERIFY(observation != nullptr);
        UiConfig captureConfig;
        captureConfig.screenCaptureEnabled = true;
        captureConfig.automaticScreenAnalysisEnabled = true;
        observation->applyConfiguration(captureConfig);

        CapturedImage image;
        image.data = QByteArrayLiteral("compressed-image");
        image.fingerprint = QByteArrayLiteral("ui-fingerprint");
        image.capturedAt = QDateTime::currentDateTimeUtc();
        image.format = QStringLiteral("jpeg");
        image.trigger = CaptureTrigger::Scheduled;
        emit capture->captured(image);

        QTRY_COMPARE_WITH_TIMEOUT(provider.requestCount(), 1, 1000);
        QTRY_COMPARE_WITH_TIMEOUT(controller.pendingRequestCount(), 0, 1000);
        QVERIFY(image.filePath.isEmpty());
        QVERIFY(QDir(temporaryDirectory.path()).entryList(
            {QStringLiteral("capture_*")}, QDir::Files).isEmpty());
        const QVector<ConversationMessage> history = conversations.recentMessages(conversationId, 10);
        QVERIFY(history.isEmpty());
        const auto latest = observations.latestValidResult(
            conversationId, image.capturedAt.addSecs(1));
        QVERIFY(latest);
        QVERIFY(latest.value().has_value());
        QCOMPARE(latest.value()->summary, QStringLiteral("我看到了当前屏幕"));
        QVERIFY(provider.lastMessages().back().hasImage());
        QVERIFY(!provider.lastOptions().stream);
        QVERIFY(provider.lastOptions().disableThinking);
    }

    void mainWindowSkipsUnchangedScreenshotAndAdvancesFingerprintBeforeFailure()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        Database database;
        QVERIFY(database.open(temporaryDirectory.filePath(QStringLiteral("vision-dedup.sqlite"))));
        SqliteConversationRepository conversations(&database);
        const QString conversationId = conversations.createConversation(QStringLiteral("截图去重"));
        SqliteObservationRepository observations(&database);
        MemoryOrchestrator memory(&conversations, nullptr, &observations);
        ControllableChatProvider provider;
        ChatController controller(&provider, &memory);
        QVERIFY(controller.setPersonaConfig(testPersona()));
        MainWindow window;
        window.setCaptureDirectory(temporaryDirectory.path());
        window.setChatController(&controller);
        window.setConversation(conversationId, {});
        ScreenCapture* capture = window.findChild<ScreenCapture*>();
        QVERIFY(capture != nullptr);
        ImageCompressionOptions compression;
        capture->configure(true, 30000, temporaryDirectory.path(), compression);
        auto* observation = window.findChild<ScreenObservationCoordinator*>();
        QVERIFY(observation != nullptr);
        UiConfig captureConfig;
        captureConfig.screenCaptureEnabled = true;
        captureConfig.automaticScreenAnalysisEnabled = true;
        observation->applyConfiguration(captureConfig);

        constexpr int fingerprintBytes =
            (ScreenFingerprint::Width * ScreenFingerprint::Height + 7) / 8;
        const QByteArray baseline(fingerprintBytes, '\0');
        const auto makeCapture = [&](const QString&, const QByteArray& fingerprint) {
            CapturedImage image;
            image.data = QByteArrayLiteral("compressed-image");
            image.fingerprint = fingerprint;
            image.capturedAt = QDateTime::currentDateTimeUtc();
            image.format = QStringLiteral("jpeg");
            image.trigger = CaptureTrigger::Scheduled;
            return image;
        };

        const CapturedImage first = makeCapture(QStringLiteral("capture_dedup_first.jpeg"), baseline);
        QVERIFY(first.filePath.isEmpty());
        emit capture->captured(first);
        QCOMPARE(provider.requestCount(), 1);
        provider.finish(ChatResult::success(QStringLiteral("首次分析")));
        QTRY_COMPARE_WITH_TIMEOUT(controller.pendingRequestCount(), 0, 1000);

        const CapturedImage unchanged = makeCapture(
            QStringLiteral("capture_dedup_unchanged.jpeg"), baseline);
        QVERIFY(unchanged.filePath.isEmpty());
        emit capture->captured(unchanged);
        QCOMPARE(provider.requestCount(), 1);
        QVERIFY(conversations.recentMessages(conversationId, 10).isEmpty());

        QByteArray changed = baseline;
        for (int bit = 0; bit < 70; ++bit) {
            changed[bit / 8] = static_cast<char>(
                static_cast<unsigned char>(changed.at(bit / 8))
                | static_cast<unsigned char>(1U << (bit % 8)));
        }
        const CapturedImage failed = makeCapture(
            QStringLiteral("capture_dedup_failed.jpeg"), changed);
        emit capture->captured(failed);
        QCOMPARE(provider.requestCount(), 2);
        provider.finish(ChatResult::failure(
            {ModelErrorCode::Network, QStringLiteral("network failure"), 0}));
        QTRY_COMPARE_WITH_TIMEOUT(controller.pendingRequestCount(), 0, 1000);
        QTRY_VERIFY_WITH_TIMEOUT(!QFileInfo::exists(failed.filePath), 1000);

        const CapturedImage sameAfterFailure = makeCapture(
            QStringLiteral("capture_dedup_after_failure.jpeg"), changed);
        emit capture->captured(sameAfterFailure);
        QCOMPARE(provider.requestCount(), 2);
        QVERIFY(!QFileInfo::exists(sameAfterFailure.filePath));
    }

    void mainWindowBlocksConcurrentScreenshotRequestsAndCleansFailures()
    {
        QTemporaryDir temporaryDirectory;
        Database database;
        QVERIFY(database.open(temporaryDirectory.filePath(QStringLiteral("vision-block.sqlite"))));
        SqliteConversationRepository conversations(&database);
        const QString conversationId = conversations.createConversation(QStringLiteral("截图阻断"));
        SqliteObservationRepository observations(&database);
        MemoryOrchestrator memory(&conversations, nullptr, &observations);
        ControllableChatProvider provider;
        ChatController controller(&provider, &memory);
        QVERIFY(controller.setPersonaConfig(testPersona()));
        MainWindow window;
        window.setCaptureDirectory(temporaryDirectory.path());
        window.setChatController(&controller);
        window.setConversation(conversationId, {});
        ScreenCapture* capture = window.findChild<ScreenCapture*>();
        QVERIFY(capture != nullptr);
        ImageCompressionOptions compression;
        capture->configure(true, 30000, temporaryDirectory.path(), compression);
        auto* observation = window.findChild<ScreenObservationCoordinator*>();
        QVERIFY(observation != nullptr);
        UiConfig captureConfig;
        captureConfig.screenCaptureEnabled = true;
        captureConfig.automaticScreenAnalysisEnabled = true;
        observation->applyConfiguration(captureConfig);
        const auto makeCapture = [&](const QString& name) {
            CapturedImage image;
            image.data = QByteArrayLiteral("compressed-image");
            image.fingerprint = QByteArrayLiteral("blocking-fingerprint-") + name.toUtf8();
            image.capturedAt = QDateTime::currentDateTimeUtc();
            image.format = QStringLiteral("jpeg");
            image.trigger = CaptureTrigger::Scheduled;
            return image;
        };

        const CapturedImage first = makeCapture(QStringLiteral("capture_first.jpeg"));
        QVERIFY(first.filePath.isEmpty());
        emit capture->captured(first);
        QCOMPARE(provider.requestCount(), 1);

        const CapturedImage blocked = makeCapture(QStringLiteral("capture_blocked.jpeg"));
        QVERIFY(blocked.filePath.isEmpty());
        emit capture->captured(blocked);
        QCOMPARE(provider.requestCount(), 1);

        provider.finish(ChatResult::success(QStringLiteral("第一次分析完成")));
        QTRY_COMPARE_WITH_TIMEOUT(controller.pendingRequestCount(), 0, 1000);

        const CapturedImage failed = makeCapture(QStringLiteral("capture_failed.jpeg"));
        QVERIFY(failed.filePath.isEmpty());
        emit capture->captured(failed);
        QCOMPARE(provider.requestCount(), 2);
        AppError failure;
        failure.code = AppErrorCode::Network;
        failure.message = QStringLiteral("network failure");
        failure.retryable = true;
        provider.finish(ChatResult::failure(failure));
        QTRY_COMPARE_WITH_TIMEOUT(controller.pendingRequestCount(), 0, 1000);
        QTRY_COMPARE_WITH_TIMEOUT(controller.pendingRequestCount(), 0, 1000);
        QVERIFY(controller.retryLast().isEmpty());
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

    void conversationControllerLoadsOlderMessagesInTwoHundredMessagePages()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Database database;
        QVERIFY(database.open(directory.filePath(QStringLiteral("paged-chat.sqlite"))));
        SqliteConversationRepository repository(&database);
        const QString id = repository.createConversation(QStringLiteral("长会话"));
        for (int index = 0; index < 450; ++index) {
            QVERIFY(repository.appendMessage(id, Message::create(
                index % 2 == 0 ? MessageRole::User : MessageRole::Assistant,
                QStringLiteral("message-%1").arg(index, 3, 10, QLatin1Char('0')))));
        }
        ConversationController controller(&repository);
        QVERIFY(controller.switchConversation(id));
        QCOMPARE(controller.currentConversationMessages().size(), 200);
        QCOMPARE(controller.currentConversationMessages().front().message.content,
                 QStringLiteral("message-250"));
        QVERIFY(controller.hasOlderMessages());
        QSignalSpy loadedSpy(&controller, &ConversationController::olderMessagesLoaded);

        QVERIFY(controller.loadOlderMessages());
        QCOMPARE(controller.currentConversationMessages().size(), 400);
        QCOMPARE(controller.currentConversationMessages().front().message.content,
                 QStringLiteral("message-050"));
        QVERIFY(controller.hasOlderMessages());
        QCOMPARE(loadedSpy.count(), 1);
        QCOMPARE(loadedSpy.at(0).at(2).toInt(), 200);

        QVERIFY(controller.loadOlderMessages());
        QCOMPARE(controller.currentConversationMessages().size(), 450);
        QCOMPARE(controller.currentConversationMessages().front().message.content,
                 QStringLiteral("message-000"));
        QVERIFY(!controller.hasOlderMessages());
        QCOMPARE(loadedSpy.count(), 2);
        QCOMPARE(loadedSpy.at(1).at(2).toInt(), 50);
        QVERIFY(controller.loadOlderMessages());
        QCOMPARE(loadedSpy.count(), 2);
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

};

} // namespace zhu_screen_pet

QTEST_MAIN(zhu_screen_pet::SmokeTest)
#include "smoke_test.moc"
