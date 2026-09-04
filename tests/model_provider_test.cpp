#include <QtTest/QtTest>
#include <QHostAddress>
#include <QSignalSpy>
#include <QSslSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include "infrastructure/HttpClient.h"
#include "infrastructure/SecretStore.h"
#include "model/ChatProviderFactory.h"
#include "model/MockChatProvider.h"
#include "model/OpenAICompatibleProvider.h"
#include "model/ProviderManager.h"

namespace zhu_screen_pet {

class ModelProviderTest final : public QObject
{
    Q_OBJECT

private:
    static ChatResult resultFromSpy(const QSignalSpy& spy)
    {
        return qvariant_cast<ChatResult>(spy.at(0).at(1));
    }

private slots:
    void initTestCase()
    {
        qRegisterMetaType<ChatResult>("ChatResult");
        qRegisterMetaType<HttpResponse>("HttpResponse");
    }

    void qtRuntimeSupportsTls()
    {
        QVERIFY2(QSslSocket::supportsSsl(),
                 qPrintable(QStringLiteral("Qt SSL initialization failed; build=%1, runtime=%2")
                     .arg(QSslSocket::sslLibraryBuildVersionString(),
                          QSslSocket::sslLibraryVersionString())));
        QVERIFY(!QSslSocket::sslLibraryVersionString().isEmpty());
    }

    void mockProviderReturnsConfiguredResponse()
    {
        MockChatProvider provider(QStringLiteral("hello from mock"));
        QSignalSpy spy(&provider, &ChatProvider::chatFinished);
        const std::vector<Message> messages = {
            Message::create(MessageRole::User, QStringLiteral("hello"))
        };
        provider.startChat(messages, ChatOptions{});
        QVERIFY(spy.wait(1000));
        const ChatResult result = resultFromSpy(spy);

        QVERIFY(result.succeeded);
        QCOMPARE(result.content, QStringLiteral("hello from mock"));
        QCOMPARE(provider.requestCount(), 1);
    }

    void mockProviderRejectsEmptyMessages()
    {
        MockChatProvider provider(QStringLiteral("mock reply"));
        QSignalSpy spy(&provider, &ChatProvider::chatFinished);
        provider.startChat({}, ChatOptions{});
        QVERIFY(spy.wait(1000));
        const ChatResult result = resultFromSpy(spy);

        QVERIFY(!result.succeeded);
        QCOMPARE(result.error.code, ModelErrorCode::InvalidRequest);
        QCOMPARE(provider.requestCount(), 0);
    }

    void mockProviderHonorsCancellation()
    {
        MockChatProvider provider(QStringLiteral("mock reply"));
        QSignalSpy spy(&provider, &ChatProvider::chatFinished);
        const QString requestId = provider.startChat(
            {Message::create(MessageRole::User, QStringLiteral("hello"))},
            ChatOptions{});
        provider.cancel(requestId);
        QVERIFY(spy.wait(1000));
        const ChatResult result = resultFromSpy(spy);

        QVERIFY(!result.succeeded);
        QCOMPARE(result.error.code, ModelErrorCode::Cancelled);
        QCOMPARE(provider.requestCount(), 0);
    }

    void mockProviderCanSimulateError()
    {
        MockChatProvider provider(QStringLiteral("mock reply"));
        provider.setError({ModelErrorCode::Timeout, QStringLiteral("simulated timeout"), 0});
        QSignalSpy spy(&provider, &ChatProvider::chatFinished);
        provider.startChat(
            {Message::create(MessageRole::User, QStringLiteral("hello"))},
            ChatOptions{});
        QVERIFY(spy.wait(1000));
        const ChatResult result = resultFromSpy(spy);

        QVERIFY(!result.succeeded);
        QCOMPARE(result.error.code, ModelErrorCode::Timeout);
        QCOMPARE(result.error.codeName(), QStringLiteral("timeout"));
        QCOMPARE(provider.requestCount(), 1);

        provider.clearError();
        spy.clear();
        provider.startChat(
            {Message::create(MessageRole::User, QStringLiteral("hello again"))},
            ChatOptions{});
        QVERIFY(spy.wait(1000));
        QVERIFY(resultFromSpy(spy).succeeded);
    }

    void openAiCompatibleProviderBuildsAndParsesRequest()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));

        QByteArray receivedRequest;
        connect(&server, &QTcpServer::newConnection, this, [&]() {
            QTcpSocket* socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket]() {
                receivedRequest += socket->readAll();
                if (!receivedRequest.contains("\r\n\r\n")) {
                    return;
                }
                const QByteArray responseBody =
                    QByteArrayLiteral("{\"choices\":[{\"message\":{\"content\":\"local reply\"}}]}");
                const QByteArray response = QByteArrayLiteral(
                    "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: ")
                    + QByteArray::number(responseBody.size())
                    + QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + responseBody;
                socket->write(response);
                socket->flush();
                QTimer::singleShot(50, socket, &QTcpSocket::disconnectFromHost);
            });
        });

        HttpClient httpClient;
        ProviderConfig config;
        config.baseUrl = QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());
        config.model = QStringLiteral("test-model");
        config.apiKey = QStringLiteral("test-key");
        config.timeoutMs = 2000;
        OpenAICompatibleProvider provider(config, &httpClient);
        QSignalSpy spy(&provider, &ChatProvider::chatFinished);
        QSignalSpy transportSpy(&httpClient, &HttpClient::requestFinished);

        provider.startChat({Message::create(MessageRole::User, QStringLiteral("hello"))},
                            ChatOptions{});
        if (!spy.wait(3000)) {
            qDebug().noquote() << "transport responses:" << transportSpy.count();
            for (int i = 0; i < transportSpy.count(); ++i) {
                const HttpResponse response = qvariant_cast<HttpResponse>(transportSpy.at(i).at(1));
                qDebug().noquote() << "response" << i << response.statusCode
                                   << response.networkError << response.errorString
                                   << response.body;
            }
            qDebug().noquote() << "received request:" << receivedRequest;
            QVERIFY2(false, "provider did not emit chatFinished");
        }

        const ChatResult result = resultFromSpy(spy);
        QVERIFY(result.succeeded);
        QCOMPARE(result.content, QStringLiteral("local reply"));
        QVERIFY(receivedRequest.contains("Authorization: Bearer test-key"));
        QVERIFY(receivedRequest.contains("\"model\":\"test-model\""));
        QVERIFY(receivedRequest.contains("\"content\":\"hello\""));
    }

    void openAiCompatibleProviderClassifiesHttpErrorsBeforeTransportErrors()
    {
        struct HttpErrorCase
        {
            int status;
            ModelErrorCode expectedCode;
            bool retryable;
            int expectedAttempts;
        };
        const QVector<HttpErrorCase> cases{
            {400, ModelErrorCode::InvalidRequest, false, 1},
            {401, ModelErrorCode::Authentication, false, 1},
            {403, ModelErrorCode::Authentication, false, 1},
            {404, ModelErrorCode::InvalidRequest, false, 1},
            {408, ModelErrorCode::Timeout, true, 3},
            {429, ModelErrorCode::RateLimit, true, 3},
            {500, ModelErrorCode::Network, true, 3},
        };

        for (const HttpErrorCase& testCase : cases) {
            QTcpServer server;
            QVERIFY(server.listen(QHostAddress::LocalHost));
            int connectionCount = 0;
            connect(&server, &QTcpServer::newConnection, this, [&]() {
                while (server.hasPendingConnections()) {
                    QTcpSocket* socket = server.nextPendingConnection();
                    ++connectionCount;
                    connect(socket, &QTcpSocket::readyRead, socket,
                            [socket, status = testCase.status, responded = false]() mutable {
                        if (responded) return;
                        responded = true;
                        socket->readAll();
                        const QByteArray body = QByteArrayLiteral(
                            "{\"error\":{\"message\":\"test http failure\"}}");
                        const QByteArray response = QByteArrayLiteral("HTTP/1.1 ")
                            + QByteArray::number(status)
                            + QByteArrayLiteral(" Error\r\nContent-Type: application/json\r\nContent-Length: ")
                            + QByteArray::number(body.size())
                            + QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + body;
                        socket->write(response);
                        socket->flush();
                        QTimer::singleShot(20, socket, &QTcpSocket::disconnectFromHost);
                    });
                }
            });

            HttpClient httpClient;
            ProviderConfig config;
            config.baseUrl = QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());
            config.model = QStringLiteral("test-model");
            config.apiKey = QStringLiteral("test-key");
            config.timeoutMs = 2000;
            config.maxRetries = 2;
            config.retryBaseDelayMs = 1;
            OpenAICompatibleProvider provider(config, &httpClient);
            QSignalSpy finishSpy(&provider, &ChatProvider::chatFinished);

            provider.startChat({Message::create(MessageRole::User, QStringLiteral("hello"))},
                               ChatOptions{});
            QVERIFY2(finishSpy.wait(3000), qPrintable(
                QStringLiteral("HTTP %1 did not finish").arg(testCase.status)));
            const ChatResult result = resultFromSpy(finishSpy);
            QVERIFY(!result.succeeded);
            QCOMPARE(result.error.httpStatus, testCase.status);
            QCOMPARE(result.error.code, testCase.expectedCode);
            QCOMPARE(result.error.retryable, testCase.retryable);
            QCOMPARE(provider.lastAttemptCount(), testCase.expectedAttempts);
            QCOMPARE(connectionCount, testCase.expectedAttempts);
        }
    }

    void deepSeekProviderUsesDefaults()
    {
        ProviderConfig config;
        config.baseUrl = QStringLiteral("https://configured.example/v1");
        config.model = QStringLiteral("configured-model");
        config.credentialAccount = QStringLiteral("configured-credential");
        DeepSeekAICompatibleProvider provider(config);
        QCOMPARE(provider.providerName(), QStringLiteral("DeepSeek"));
        QCOMPARE(provider.baseUrl(), QStringLiteral("https://configured.example/v1"));
    }

    void providerManagerSwitchesMockProfilesWhenIdle()
    {
        HttpClient httpClient;
        SecretStore secretStore;
        ChatProviderFactory factory(&httpClient, &secretStore);
        ProviderManager manager(&factory);
        QSignalSpy finishSpy(&manager, &ChatProvider::chatFinished);

        ModelProviderConfig first;
        first.profileId = QStringLiteral("first");
        first.providerType = QStringLiteral("mock");
        first.displayName = QStringLiteral("First Mock");
        first.mockReply = QStringLiteral("第一模型");
        first.timeoutMs = 30000;
        first.maxRetries = 3;
        first.retryBaseDelayMs = 1000;
        QString errorMessage;
        QVERIFY2(manager.switchProvider(first, &errorMessage), qPrintable(errorMessage));
        const QString requestId = manager.startChat(
            {Message::create(MessageRole::User, QStringLiteral("hello"))}, ChatOptions{});
        QVERIFY(!requestId.isEmpty());

        ModelProviderConfig second;
        second.profileId = QStringLiteral("second");
        second.providerType = QStringLiteral("mock");
        second.displayName = QStringLiteral("Second Mock");
        second.mockReply = QStringLiteral("第二模型");
        second.timeoutMs = 30000;
        second.maxRetries = 3;
        second.retryBaseDelayMs = 1000;
        QVERIFY(!manager.switchProvider(second, &errorMessage));
        QVERIFY(errorMessage.contains(QStringLiteral("running")));
        QVERIFY(finishSpy.wait(1000));
        QCOMPARE(resultFromSpy(finishSpy).content, QStringLiteral("第一模型"));
        QCOMPARE(manager.activeRequestCount(), 0);

        QVERIFY2(manager.switchProvider(second, &errorMessage), qPrintable(errorMessage));
        QCOMPARE(manager.activeConfiguration().profileId, QStringLiteral("second"));
        QCOMPARE(manager.activeProviderName(), QStringLiteral("Second Mock"));
        finishSpy.clear();
        manager.startChat({Message::create(MessageRole::User, QStringLiteral("hello again"))},
                          ChatOptions{});
        QVERIFY(finishSpy.wait(1000));
        QCOMPARE(resultFromSpy(finishSpy).content, QStringLiteral("第二模型"));
    }

    void mockProviderEmitsStreamingDelta()
    {
        MockChatProvider provider(QStringLiteral("streamed mock"));
        QSignalSpy deltaSpy(&provider, &ChatProvider::chatDelta);
        QSignalSpy finishSpy(&provider, &ChatProvider::chatFinished);
        ChatOptions options;
        options.stream = true;

        provider.startChat({Message::create(MessageRole::User, QStringLiteral("hello"))}, options);
        QVERIFY(finishSpy.wait(1000));
        QCOMPARE(deltaSpy.count(), 1);
        QCOMPARE(deltaSpy.at(0).at(1).toString(), QStringLiteral("streamed mock"));
        QVERIFY(resultFromSpy(finishSpy).succeeded);
    }

    void openAiCompatibleProviderParsesSseDeltas()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));

        connect(&server, &QTcpServer::newConnection, this, [&]() {
            QTcpSocket* socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [socket, responded = false]() mutable {
                if (responded) {
                    return;
                }
                responded = true;
                socket->readAll();
                const QByteArray body = QByteArrayLiteral(
                    "data: {\"choices\":[{\"delta\":{\"content\":\"hello\"}}]}\n\n"
                    "data: {\"choices\":[{\"delta\":{\"content\":\" world\"}}]}\n\n"
                    "data: [DONE]\n\n");
                const QByteArray header = QByteArrayLiteral(
                    "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nContent-Length: ")
                    + QByteArray::number(body.size())
                    + QByteArrayLiteral("\r\nConnection: close\r\n\r\n");
                socket->write(header);
                socket->write(body.left(17));
                socket->flush();
                QTimer::singleShot(20, socket, [socket, body]() {
                    socket->write(body.mid(17));
                    socket->flush();
                    QTimer::singleShot(50, socket, &QTcpSocket::disconnectFromHost);
                });
            });
        });

        HttpClient httpClient;
        ProviderConfig config;
        config.baseUrl = QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());
        config.model = QStringLiteral("test-model");
        config.apiKey = QStringLiteral("test-key");
        config.timeoutMs = 2000;
        OpenAICompatibleProvider provider(config, &httpClient);
        QSignalSpy deltaSpy(&provider, &ChatProvider::chatDelta);
        QSignalSpy finishSpy(&provider, &ChatProvider::chatFinished);
        ChatOptions options;
        options.stream = true;

        provider.startChat({Message::create(MessageRole::User, QStringLiteral("hello"))}, options);
        QVERIFY(finishSpy.wait(3000));
        QCOMPARE(deltaSpy.count(), 2);
        QCOMPARE(deltaSpy.at(0).at(1).toString(), QStringLiteral("hello"));
        QCOMPARE(deltaSpy.at(1).at(1).toString(), QStringLiteral(" world"));
        const ChatResult result = resultFromSpy(finishSpy);
        QVERIFY(result.succeeded);
        QCOMPARE(result.content, QStringLiteral("hello world"));
    }

    void streamingFailureAfterDeltaIsNotRetriedOrDuplicated()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        int connectionCount = 0;
        connect(&server, &QTcpServer::newConnection, this, [&]() {
            ++connectionCount;
            QTcpSocket* socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [socket, responded = false]() mutable {
                if (responded) return;
                responded = true;
                socket->readAll();
                const QByteArray partial = QByteArrayLiteral(
                    "data: {\"choices\":[{\"delta\":{\"content\":\"partial\"}}]}\n\n");
                const QByteArray header = QByteArrayLiteral(
                    "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nContent-Length: 999\r\n"
                    "Connection: close\r\n\r\n");
                socket->write(header + partial);
                socket->flush();
                QTimer::singleShot(20, socket, &QTcpSocket::disconnectFromHost);
            });
        });

        HttpClient httpClient;
        ProviderConfig config;
        config.baseUrl = QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());
        config.model = QStringLiteral("test-model");
        config.apiKey = QStringLiteral("test-key");
        config.timeoutMs = 1000;
        config.maxRetries = 3;
        config.retryBaseDelayMs = 10;
        OpenAICompatibleProvider provider(config, &httpClient);
        QSignalSpy deltaSpy(&provider, &ChatProvider::chatDelta);
        QSignalSpy finishSpy(&provider, &ChatProvider::chatFinished);
        ChatOptions options;
        options.stream = true;
        provider.startChat({Message::create(MessageRole::User, QStringLiteral("hello"))}, options);
        QVERIFY(finishSpy.wait(3000));
        QCOMPARE(deltaSpy.count(), 1);
        QCOMPARE(deltaSpy.at(0).at(1).toString(), QStringLiteral("partial"));
        Q_UNUSED(connectionCount);
        QCOMPARE(provider.lastAttemptCount(), 1);
        QVERIFY(!resultFromSpy(finishSpy).succeeded);
    }

};

} // namespace zhu_screen_pet

QTEST_MAIN(zhu_screen_pet::ModelProviderTest)
#include "model_provider_test.moc"
