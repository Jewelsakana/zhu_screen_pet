#include <QtTest/QtTest>

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QUuid>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "app/AppConfigRepository.h"
#include "app/ChatController.h"
#include "app/ErrorCenter.h"
#include "app/SettingsController.h"
#include "infrastructure/Database.h"
#include "infrastructure/HttpClient.h"
#include "infrastructure/SecretStore.h"
#include "memory/MemoryOrchestrator.h"
#include "memory/SqliteConversationRepository.h"
#include "model/ChatProviderFactory.h"
#include "model/ModelConfigRepository.h"
#include "model/ProviderManager.h"
#include "ui/MainWindow.h"
#include "ui/SettingsDialog.h"

namespace zhu_screen_pet {

class SettingsTransactionTest final : public QObject
{
    Q_OBJECT

private slots:
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
        QVERIFY2(app.load(&persistedPersona, &persistedMessages, &error, &persistedLimits),
                 qPrintable(error));
        QCOMPARE(persistedPersona.name, QStringLiteral("设置后的桌宠"));
        QCOMPARE(persistedLimits.recentMessageLimit, changedLimits.recentMessageLimit);
        QCOMPARE(persistedMessages, messages);

        UiConfig resizedUi = settings.uiConfig();
        resizedUi.windowScalePercent = 125;
        QSignalSpy uiSpy(&settings, &SettingsController::uiConfigurationChanged);
        QVERIFY2(settings.updateUiConfig(resizedUi, &applyError),
                 qPrintable(applyError.technicalMessage));
        QCOMPARE(uiSpy.count(), 1);
        QCOMPARE(settings.uiConfig().windowScalePercent, 125);
        UiConfig persistedUi;
        QVERIFY2(app.load(&persistedPersona, &persistedMessages, &error,
                          &persistedLimits, &persistedUi), qPrintable(error));
        QCOMPARE(persistedUi.windowScalePercent, 125);

        MainWindow toolbarWindow;
        toolbarWindow.setSettingsController(&settings);
        auto* captureToggle = toolbarWindow.findChild<QPushButton*>(
            QStringLiteral("screenCaptureToggleButton"));
        auto* captureOnChatToggle = toolbarWindow.findChild<QPushButton*>(
            QStringLiteral("captureOnChatToggleButton"));
        QVERIFY(captureToggle != nullptr && captureOnChatToggle != nullptr);
        QVERIFY(!captureToggle->isChecked());
        QVERIFY(!captureOnChatToggle->isEnabled());
        QTest::mouseClick(captureToggle, Qt::LeftButton);
        QCOMPARE(settings.uiConfig().screenCaptureEnabled, true);
        QVERIFY(captureToggle->isChecked());
        QVERIFY(captureOnChatToggle->isEnabled());
        QVERIFY2(app.load(&persistedPersona, &persistedMessages, &error,
                          &persistedLimits, &persistedUi), qPrintable(error));
        QVERIFY(persistedUi.screenCaptureEnabled);
        QTest::mouseClick(captureToggle, Qt::LeftButton);
        QCOMPARE(settings.uiConfig().screenCaptureEnabled, false);
        QVERIFY(!captureOnChatToggle->isEnabled());

        SettingsDialog dialog(&settings);
        auto* profileList = dialog.findChild<QComboBox*>(QStringLiteral("settingsModelProfile"));
        auto* modelUrl = dialog.findChild<QLineEdit*>(QStringLiteral("settingsModelUrl"));
        auto* applyButton = dialog.findChild<QPushButton*>(QStringLiteral("settingsApplyButton"));
        QVERIFY(profileList != nullptr && modelUrl != nullptr && applyButton != nullptr);
        auto* personaGroup = dialog.findChild<QGroupBox*>(QStringLiteral("settingsPersonaGroup"));
        auto* userAddress = dialog.findChild<QLineEdit*>(QStringLiteral("settingsUserAddress"));
        auto* windowScale = dialog.findChild<QSpinBox*>(QStringLiteral("settingsMainWindowScale"));
        QVERIFY(personaGroup != nullptr && userAddress != nullptr && windowScale != nullptr);
        QCOMPARE(windowScale->value(), 125);
        QStringList personaLabels;
        for (const QLabel* label : personaGroup->findChildren<QLabel*>()) {
            personaLabels.append(label->text());
        }
        QVERIFY(!personaLabels.contains(QStringLiteral("名称")));
        QVERIFY(!personaLabels.contains(QStringLiteral("语气")));
        QVERIFY(personaLabels.contains(QStringLiteral("对你的称呼")));
        auto* relevantLimit = dialog.findChild<QSpinBox*>(QStringLiteral("settingsRelevantHistoryLimit"));
        auto* longTermLimit = dialog.findChild<QSpinBox*>(QStringLiteral("settingsLongTermMemoryLimit"));
        auto* contextLimit = dialog.findChild<QSpinBox*>(QStringLiteral("settingsContextTokenLimit"));
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

QTEST_MAIN(zhu_screen_pet::SettingsTransactionTest)
#include "settings_transaction_test.moc"
