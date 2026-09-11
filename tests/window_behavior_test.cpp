#include <QtTest/QtTest>
#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QCursor>
#include <QDialog>
#include <QElapsedTimer>
#include <QFile>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTimer>

#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "infrastructure/DesktopWindowPolicy.h"
#include "infrastructure/SettingsRepository.h"
#include "infrastructure/WindowAttachmentManager.h"
#include "infrastructure/WindowManager.h"
#include "infrastructure/WindowPlacement.h"
#include "app/ScreenObservationCoordinator.h"
#include "ui/ActionPanel.h"
#include "ui/ChatInputPanel.h"
#include "ui/CaptureUiController.h"
#include "ui/ConversationWindow.h"
#include "ui/ErrorBannerWindow.h"
#include "ui/HoverRevealController.h"
#include "ui/LevelProgressWidget.h"
#include "ui/MainWindow.h"
#include "ui/PetWindowResizeController.h"
#include "ui/ReplyBubbleWindow.h"
#include "ui/SettingsDialog.h"

namespace zhu_screen_pet {

class WindowBehaviorTest final : public QObject
{
    Q_OBJECT

private slots:
    void captureUiControllerAllowsOwnWindowsAndReportsStatus()
    {
        ScreenObservationCoordinator observation;
        CaptureUiController controller(&observation);
        QWidget window(nullptr, Qt::Window);
        window.setObjectName(QStringLiteral("capturePolicyWindow"));
        controller.registerWindow(&window);
        QSignalSpy statusSpy(
            &controller, &CaptureUiController::ownWindowExclusionStatusChanged);
        UiConfig config;
        config.excludeOwnWindowsFromCapture = false;
        const UiConfig applied = controller.applyConfiguration(config);
        QVERIFY(!applied.excludeOwnWindowsFromCapture);
        QVERIFY(controller.ownWindowExclusionAvailable());
        QVERIFY(!controller.ownWindowsExcluded());
        QCOMPARE(statusSpy.count(), 1);

        SettingsDialog settings(nullptr, nullptr, observation.screenCapture(), &controller);
        auto* includeOwn = settings.findChild<QCheckBox*>(
            QStringLiteral("settingsIncludeOwnWindowsInCapture"));
        auto* status = settings.findChild<QLabel*>(
            QStringLiteral("settingsCaptureExclusionStatus"));
        QVERIFY(includeOwn != nullptr && status != nullptr);
        includeOwn->setChecked(true);
        QCOMPARE(status->text(), QStringLiteral("已允许自身窗口进入截图"));
    }

    void resizeControllerEmitsPreviewAndCommitForCornerDrag()
    {
        QWidget window(nullptr, Qt::Window);
        window.resize(300, 350);
        window.show();
        QTest::qWait(20);
        PetWindowResizeController controller(&window);
        controller.setScalePercent(100);
        connect(&controller, &PetWindowResizeController::scalePreviewRequested,
                &window, [&window](int percent) {
                    window.resize(3 * percent, qRound(3.5 * percent));
                });
        QSignalSpy previewSpy(
            &controller, &PetWindowResizeController::scalePreviewRequested);
        QSignalSpy commitSpy(
            &controller, &PetWindowResizeController::scaleCommitRequested);
        const QPoint local(window.width() - 1, window.height() - 1);
        const QPoint global = window.mapToGlobal(local);
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(local), QPointF(global),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QVERIFY(controller.handlePress(&press));
        QVERIFY(controller.isResizing());
        QMouseEvent move(QEvent::MouseMove, QPointF(local + QPoint(60, 70)),
                         QPointF(global + QPoint(60, 70)), Qt::NoButton,
                         Qt::LeftButton, Qt::NoModifier);
        QVERIFY(controller.handleMove(&move));
        QVERIFY(previewSpy.count() >= 1);
        QMouseEvent release(QEvent::MouseButtonRelease,
                            QPointF(local + QPoint(60, 70)),
                            QPointF(global + QPoint(60, 70)), Qt::LeftButton,
                            Qt::NoButton, Qt::NoModifier);
        QVERIFY(controller.handleRelease(&release));
        QVERIFY(!controller.isResizing());
        QCOMPARE(commitSpy.count(), 1);
        QCOMPARE(commitSpy.first().first().toInt(), controller.scalePercent());
        QVERIFY(qAbs(window.width() * 350 - window.height() * 300) <= 300);
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

    void attachmentBatchDefersRepositionUntilAllWindowsAreRegistered()
    {
        QWidget anchor;
        QWidget first;
        QWidget second;
        anchor.resize(200, 200);
        first.resize(100, 80);
        second.resize(120, 90);
        anchor.move(300, 240);
        anchor.show();
        first.show();
        second.show();
        QTest::qWait(20);

        WindowAttachmentManager manager;
        QSignalSpy positionedSpy(&manager,
                                 &WindowAttachmentManager::attachmentPositioned);
        manager.beginUpdate();
        manager.setAnchor(&anchor);
        manager.attach(&first,
                       {AttachmentSide::Left, AttachmentAlignment::Center, 10});
        manager.attach(&second,
                       {AttachmentSide::Right, AttachmentAlignment::Center, 10});
        QCOMPARE(positionedSpy.count(), 0);

        manager.endUpdate();
        QCOMPARE(positionedSpy.count(), 2);
        QVERIFY(first.pos() != QPoint(0, 0));
        QVERIFY(second.pos() != QPoint(0, 0));
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
#ifdef Q_OS_WIN
        if (QGuiApplication::platformName() == QStringLiteral("windows")) {
            DWORD affinity = WDA_NONE;
            QVERIFY(GetWindowDisplayAffinity(
                reinterpret_cast<HWND>(overlay.winId()), &affinity));
            QVERIFY(affinity == WDA_EXCLUDEFROMCAPTURE || affinity == WDA_MONITOR);
        }
#endif
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
        QVERIFY(bubble.findChild<QWidget*>(QStringLiteral("replyBubbleCard")) != nullptr);
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

    void replyBubbleShrinksAfterLongContentAndUsesAvailableWidth()
    {
        ReplyBubbleWindow bubble;
        bubble.show();
        QTest::qWait(20);
        auto* content = bubble.findChild<QTextBrowser*>(
            QStringLiteral("replyBubbleContent"));
        QVERIFY(content != nullptr);

        bubble.beginReply();
        bubble.finishReply(QString(600, QChar(0x957F)));
        QCoreApplication::processEvents();
        const int longContentHeight = content->height();
        const int longWindowHeight = bubble.height();

        bubble.beginReply();
        bubble.finishReply(QStringLiteral("你好，我已经收到你的消息了。"));
        QCoreApplication::processEvents();
        QVERIFY(content->document()->textWidth() >= 250.0);
        QVERIFY(content->height() < longContentHeight);
        QVERIFY(bubble.height() < longWindowHeight);
        QVERIFY(content->alignment().testFlag(Qt::AlignLeft));
        auto* close = bubble.findChild<QPushButton*>(QStringLiteral("replyBubbleClose"));
        QVERIFY(close != nullptr);
        // 正文与关闭按钮从同一顶部区域开始，不再被按钮所在的空白行下推。
        QVERIFY(content->geometry().top() <= close->geometry().bottom());
        auto* card = bubble.findChild<QWidget*>(QStringLiteral("replyBubbleCard"));
        QVERIFY(card != nullptr);
        const QImage cardImage = card->grab().toImage().convertToFormat(
            QImage::Format_ARGB32_Premultiplied);
        QVERIFY(cardImage.pixelColor(0, 0).alpha() < 32);
        QVERIFY(cardImage.pixelColor(cardImage.width() / 2,
                                     cardImage.height() / 2).alpha() > 200);
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
        auto* level = window.findChild<LevelProgressWidget*>(
            QStringLiteral("levelProgress"));
        QVERIFY(level != nullptr);
        QCOMPARE(level->level(), 0);
        QCOMPARE(level->progressPercent(), 0);
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

    void shopAndBackpackUseSettingsStyleModalLifecycle()
    {
        MainWindow window;
        const auto verifyDialog = [&window](const QString& buttonName,
                                             const QString& dialogName) {
            auto* button = window.findChild<QPushButton*>(buttonName);
            QVERIFY(button != nullptr);
            bool found = false;
            bool modal = false;
            bool parentedToMainWindow = false;
            QTimer::singleShot(0, &window, [&]() {
                auto* dialog = window.findChild<QDialog*>(dialogName);
                found = dialog != nullptr;
                if (dialog != nullptr) {
                    modal = dialog->isModal();
                    parentedToMainWindow = dialog->parentWidget() == &window;
                    dialog->accept();
                }
            });
            button->click();
            QVERIFY(found);
            QVERIFY(modal);
            QVERIFY(parentedToMainWindow);
            QVERIFY(window.findChild<QDialog*>(dialogName) == nullptr);
        };

        verifyDialog(QStringLiteral("shopButton"), QStringLiteral("shopWindow"));
        verifyDialog(QStringLiteral("backpackButton"), QStringLiteral("backpackWindow"));
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

    void mainWindowCornerDragResizesWithFixedAspectRatio()
    {
        MainWindow window;
        window.show();
        QTest::qWait(20);
        const QSize before = window.size();
        auto* actions = window.findChild<ActionPanel*>(QStringLiteral("actionPanel"));
        QVERIFY(actions != nullptr);
        const QSize actionBefore = actions->size();
        QWidget* surface = window.centralWidget();
        QVERIFY(surface != nullptr);
        const QPoint localPress(surface->width() - 2, surface->height() - 2);
        const QPoint globalPress = surface->mapToGlobal(localPress);
        QTest::mousePress(surface, Qt::LeftButton, Qt::NoModifier, localPress);
        const QPoint delta(qMax(20, before.width() / 4), qMax(20, before.height() / 4));
        const QPoint targetGlobal = globalPress + delta;
        QMouseEvent moveEvent(QEvent::MouseMove, QPointF(localPress + delta),
                              QPointF(targetGlobal), Qt::NoButton, Qt::LeftButton,
                              Qt::NoModifier);
        QApplication::sendEvent(surface, &moveEvent);
        QTest::mouseRelease(surface, Qt::LeftButton, Qt::NoModifier,
                            QPoint(surface->width() - 2, surface->height() - 2));

        QVERIFY(window.width() > before.width());
        QVERIFY(window.height() > before.height());
        QVERIFY(actions->width() > actionBefore.width());
        QVERIFY(actions->height() > actionBefore.height());
        QVERIFY(qAbs(window.width() * 350 - window.height() * 300) <= 350);
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
        QVERIFY(bubble->findChild<QWidget*>(QStringLiteral("replyBubbleCard")) != nullptr);
        QVERIFY(!error->testAttribute(Qt::WA_TranslucentBackground));
        QVERIFY(error->styleSheet().contains(QStringLiteral("#fffaf0")));

        SettingsDialog settings(nullptr);
        QVERIFY(!settings.testAttribute(Qt::WA_TranslucentBackground));
        QVERIFY(settings.styleSheet().contains(QStringLiteral("#fffaf0")));
        auto* capturePrivacyHelp = settings.findChild<QPushButton*>(
            QStringLiteral("settingsCapturePrivacyHelp"));
        auto* automaticCaptureHelp = settings.findChild<QPushButton*>(
            QStringLiteral("settingsAutomaticCaptureHelp"));
        auto* captureInterval = settings.findChild<QSpinBox*>(
            QStringLiteral("settingsScreenCaptureInterval"));
        QVERIFY(capturePrivacyHelp != nullptr && automaticCaptureHelp != nullptr
                && captureInterval != nullptr);
        QCOMPARE(captureInterval->minimum(), 30);
        QVERIFY(automaticCaptureHelp->toolTip().contains(QStringLiteral("至少在60s以上")));
        QCOMPARE(capturePrivacyHelp->text(), QStringLiteral("?"));
        QCOMPARE(capturePrivacyHelp->accessibleName(),
                 QStringLiteral("查看屏幕截图隐私提醒"));
    }

};

} // namespace zhu_screen_pet

QTEST_MAIN(zhu_screen_pet::WindowBehaviorTest)
#include "window_behavior_test.moc"
