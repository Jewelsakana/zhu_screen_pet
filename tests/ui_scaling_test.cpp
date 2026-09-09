#include <QtTest/QtTest>

#include <QApplication>
#include <QComboBox>
#include <QFontMetrics>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QWheelEvent>

#include "app/UiConfig.h"
#include "memory/ConversationTypes.h"
#include "model/ChatProvider.h"
#include "model/Message.h"
#include "app/SettingsController.h"
#include "ui/ActionPanel.h"
#include "ui/ChatInputPanel.h"
#include "ui/ConversationHistoryWindow.h"
#include "ui/ConversationWindow.h"
#include "ui/ErrorBannerWindow.h"
#include "ui/MainWindow.h"
#include "ui/ReplyBubbleWindow.h"
#include "ui/SettingsDialog.h"
#include "ui/UiScaleMetrics.h"

namespace zhu_screen_pet {

class UiScalingTest final : public QObject
{
    Q_OBJECT

private slots:
    void sharedMetricsEnforceScaleAndReadabilityLimits()
    {
        const UiScaleMetrics metrics(25);
        QCOMPARE(metrics.percent(), 25);
        QCOMPARE(metrics.scaled(10, 6), 6);
        const QSize constrained = metrics.scaledForScreen(
            QSize(400, 300), QRect(0, 0, 80, 60));
        QVERIFY(constrained.width() > 0 && constrained.width() <= 80);
        QVERIFY(constrained.height() > 0 && constrained.height() <= 60);

        QFont pointFont;
        pointFont.setPointSizeF(20.0);
        QCOMPARE(metrics.readableFont(pointFont).pointSizeF(), 8.5);
        QFont pixelFont;
        pixelFont.setPixelSize(20);
        QCOMPARE(metrics.readableFont(pixelFont).pixelSize(), 12);
    }

    void settingsWheelDoesNotChangeNumericOrChoiceValues()
    {
        SettingsDialog dialog(nullptr);
        auto* scale = dialog.findChild<QSpinBox*>(QStringLiteral("settingsMainWindowScale"));
        auto* format = dialog.findChild<QComboBox*>(QStringLiteral("settingsCaptureImageFormat"));
        QVERIFY(scale != nullptr && format != nullptr);
        scale->setValue(100);
        format->setCurrentIndex(0);

        const QPoint localScale = scale->rect().center();
        QWheelEvent scaleWheel(QPointF(localScale), QPointF(scale->mapToGlobal(localScale)),
                               QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                               Qt::NoScrollPhase, false);
        QApplication::sendEvent(scale, &scaleWheel);
        QCOMPARE(scale->value(), 100);

        const QPoint localFormat = format->rect().center();
        QWheelEvent formatWheel(QPointF(localFormat), QPointF(format->mapToGlobal(localFormat)),
                                QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier,
                                Qt::NoScrollPhase, false);
        QApplication::sendEvent(format, &formatWheel);
        QCOMPARE(format->currentIndex(), 0);
    }

    void mainWindowScaleUpdatesAttachedWindowSizes()
    {
        SettingsController settings(nullptr, nullptr, nullptr, nullptr, nullptr,
                                    nullptr, nullptr, nullptr);
        MainWindow window;
        auto* actions = window.findChild<ActionPanel*>(QStringLiteral("actionPanel"));
        auto* input = window.findChild<ChatInputPanel*>(QStringLiteral("chatInputPanel"));
        auto* bubble = window.findChild<ReplyBubbleWindow*>(QStringLiteral("replyBubble"));
        auto* error = window.findChild<ErrorBannerWindow*>(QStringLiteral("errorBanner"));
        ConversationWindow* conversations = window.conversationWindow();
        QVERIFY(actions != nullptr && input != nullptr && bubble != nullptr
                && error != nullptr && conversations != nullptr);
        const QSize mainSize = window.size();
        const QSize actionSize = actions->size();
        const QSize inputSize = input->size();
        const QSize bubbleSize = bubble->size();
        const QSize errorSize = error->size();
        const QSize conversationSize = conversations->size();

        UiConfig ui;
        ui.windowScalePercent = 150;
        settings.setInitialUiConfig(ui);
        window.setSettingsController(&settings);

        QVERIFY(window.width() > mainSize.width());
        QVERIFY(window.height() > mainSize.height());
        QVERIFY(actions->width() > actionSize.width());
        QVERIFY(actions->height() > actionSize.height());
        QVERIFY(input->width() > inputSize.width());
        QVERIFY(bubble->width() > bubbleSize.width());
        QVERIFY(error->width() > errorSize.width());
        QVERIFY(conversations->width() > conversationSize.width());
        QVERIFY(conversations->height() > conversationSize.height());

        ConversationHistoryWindow history;
        const QSize historySize = history.size();
        history.setUiScalePercent(150);
        QVERIFY(history.width() > historySize.width());
        QVERIFY(history.height() > historySize.height());
    }

    void actionPanelRemainsReadableAndRoundedAtMinimumScale()
    {
        ActionPanel panel;
        panel.setUiScalePercent(UiConfig::MinimumWindowScalePercent);
        panel.show();
        QTest::qWait(20);
        QVERIFY(panel.styleSheet().contains(QStringLiteral("border-radius:8px")));
        const QList<QPushButton*> buttons = panel.findChildren<QPushButton*>();
        QCOMPARE(buttons.size(), 6);
        for (QPushButton* button : buttons) {
            const QFontMetrics metrics(button->font());
            QVERIFY2(button->contentsRect().width() >= metrics.horizontalAdvance(button->text()),
                     qPrintable(button->objectName() + QStringLiteral(" text is clipped")));
            QVERIFY2(button->contentsRect().height() >= metrics.height(),
                     qPrintable(button->objectName() + QStringLiteral(" height is clipped")));
            QVERIFY(button->width() >= button->sizeHint().width());
            QVERIFY(button->height() >= button->sizeHint().height());
        }
    }

    void actionPanelCaptureButtonsExposeSharedConfigurationState()
    {
        ActionPanel panel;
        auto* capture = panel.findChild<QPushButton*>(QStringLiteral("screenCaptureToggleButton"));
        auto* onChat = panel.findChild<QPushButton*>(QStringLiteral("captureOnChatToggleButton"));
        QVERIFY(capture != nullptr && onChat != nullptr);
        QVERIFY(!capture->isChecked());
        QVERIFY(!onChat->isEnabled());

        QSignalSpy captureSpy(&panel, &ActionPanel::screenCaptureToggled);
        QSignalSpy chatSpy(&panel, &ActionPanel::captureOnChatToggled);
        panel.setCaptureOptions(true, false);
        QVERIFY(capture->isChecked());
        QVERIFY(onChat->isEnabled());
        QCOMPARE(captureSpy.count(), 0);
        QTest::mouseClick(onChat, Qt::LeftButton);
        QCOMPARE(chatSpy.count(), 1);
        QCOMPARE(chatSpy.takeFirst().first().toBool(), true);
        panel.setCaptureOptions(false, true);
        QVERIFY(!capture->isChecked());
        QVERIFY(!onChat->isChecked());
        QVERIFY(!onChat->isEnabled());
    }

    void attachedChatAndConversationControlsRemainReadableAtMinimumScale()
    {
        const int scale = UiConfig::MinimumWindowScalePercent;
        ChatInputPanel input;
        input.setUiScalePercent(scale);
        input.show();
        QTest::qWait(20);
        QVERIFY(input.styleSheet().contains(QStringLiteral("border-radius:9px")));
        auto* editor = input.findChild<QTextEdit*>(QStringLiteral("chatInput"));
        QVERIFY(editor != nullptr);
        QVERIFY(editor->contentsRect().height() >= QFontMetrics(editor->font()).height());
        for (const QString& name : {QStringLiteral("retryButton"),
                                    QStringLiteral("cancelButton"),
                                    QStringLiteral("sendButton")}) {
            auto* button = input.findChild<QPushButton*>(name);
            QVERIFY(button != nullptr);
            const QFontMetrics metrics(button->font());
            QVERIFY(button->contentsRect().width() >= metrics.horizontalAdvance(button->text()));
            QVERIFY(button->contentsRect().height() >= metrics.height());
        }

        ConversationWindow conversations;
        auto* list = conversations.findChild<QListWidget*>(QStringLiteral("conversationList"));
        QVERIFY(list != nullptr);
        auto* item = new QListWidgetItem(QStringLiteral("需要完整显示的会话标题"), list);
        conversations.setUiScalePercent(scale);
        QVERIFY(conversations.styleSheet().contains(QStringLiteral("border-radius:12px")));
        QVERIFY(conversations.styleSheet().contains(QStringLiteral("border-radius:7px")));
        QVERIFY(item->sizeHint().height() >= QFontMetrics(list->font()).height());
        for (QPushButton* button : conversations.findChildren<QPushButton*>()) {
            const QFontMetrics metrics(button->font());
            QVERIFY(button->contentsRect().width() >= metrics.horizontalAdvance(button->text()));
            QVERIFY(button->contentsRect().height() >= metrics.height());
        }

        ConversationHistoryWindow history;
        ConversationMessage assistant;
        assistant.message = Message::create(MessageRole::Assistant,
                                             QStringLiteral("缩放后仍应清晰显示的历史回复"));
        ConversationMessage user;
        user.message = Message::create(MessageRole::User,
                                        QStringLiteral("缩放后仍应清晰显示的用户消息"));
        history.setConversation(QStringLiteral("scale-test"), QStringLiteral("缩放测试"),
                                {assistant, user});
        history.setUiScalePercent(scale);
        QVERIFY(history.styleSheet().contains(QStringLiteral("border-radius:12px")));
        const auto assistantBubbles = history.findChildren<QLabel*>(
            QStringLiteral("assistantMessageBubble"));
        const auto userBubbles = history.findChildren<QLabel*>(QStringLiteral("userMessageBubble"));
        QCOMPARE(assistantBubbles.size(), 1);
        QCOMPARE(userBubbles.size(), 1);
        for (QLabel* bubble : {assistantBubbles.first(), userBubbles.first()}) {
            QVERIFY(bubble->styleSheet().contains(QStringLiteral("border-radius:8px")));
            QVERIFY(bubble->contentsRect().height() >= QFontMetrics(bubble->font()).height());
        }
        auto* close = history.findChild<QPushButton*>(QStringLiteral("conversationHistoryClose"));
        QVERIFY(close != nullptr);
        QVERIFY(close->contentsRect().width()
                >= QFontMetrics(close->font()).horizontalAdvance(close->text()));
    }
};

} // namespace zhu_screen_pet

QTEST_MAIN(zhu_screen_pet::UiScalingTest)
#include "ui_scaling_test.moc"
