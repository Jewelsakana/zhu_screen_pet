#include "ui/MainWindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QGuiApplication>
#include <QHideEvent>
#include <QFileInfo>
#include <QLabel>
#include <QMouseEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QPixmap>
#include <QResizeEvent>
#include <QScreen>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QtMath>

#include "app/ChatController.h"
#include "app/AffectionController.h"
#include "app/ConversationController.h"
#include "app/ErrorCenter.h"
#include "app/SettingsController.h"
#include "app/ScreenObservationCoordinator.h"
#include "app/PetLifecycleController.h"
#include "app/InputActivityController.h"
#include "app/PetEconomyController.h"
#include "app/SatietyController.h"
#include "infrastructure/DesktopWindowPolicy.h"
#include "infrastructure/ScreenCapture.h"
#include "infrastructure/WindowAttachmentManager.h"
#include "infrastructure/WindowPlacement.h"
#include "ui/ActionPanel.h"
#include "ui/BackpackWindow.h"
#include "ui/ChatInputPanel.h"
#include "ui/CaptureUiController.h"
#include "ui/ConversationWindow.h"
#include "ui/ConversationHistoryWindow.h"
#include "ui/ErrorBannerWindow.h"
#include "ui/HoverRevealController.h"
#include "ui/InputActivityPanel.h"
#include "ui/PetWindowResizeController.h"
#include "ui/PetAnimationPlayer.h"
#include "ui/LevelProgressWidget.h"
#include "ui/ReplyBubbleWindow.h"
#include "ui/SettingsDialog.h"
#include "ui/ShopWindow.h"
#include "ui/UiScaleMetrics.h"

namespace zhu_screen_pet {

namespace {
QString resolveConfiguredAssetPath(const QString& configuredPath)
{
    const QString path = configuredPath.trimmed();
    if (path.isEmpty() || QDir::isAbsolutePath(path)) return path;
    return QDir(QCoreApplication::applicationDirPath()).filePath(path);
}

}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setObjectName(QStringLiteral("petWindow"));
    setWindowTitle(QStringLiteral("小珠看着你"));
    // 主窗口保留任务栏入口；只有附属气泡和悬浮面板使用 Tool 窗口。
    DesktopWindowPolicy::apply(this, {true, true, true, true, false, false});
    const QRect available = QGuiApplication::primaryScreen()
        ? QGuiApplication::primaryScreen()->availableGeometry()
        : QRect(0, 0, 1920, 1080);
    resize(WindowPlacement::scaleForScreen(QSize(300, 350), available));
    setMinimumSize(WindowPlacement::scaleForScreen(QSize(240, 280), available));
    auto* surface = new QWidget(this);
    surface->setObjectName(QStringLiteral("petSurface"));
    surface->setStyleSheet(QStringLiteral(
        "QWidget#petSurface{background:rgba(104,129,210,42);border:2px solid rgba(255,255,255,70);"
        "border-radius:72px;} QLabel{color:white;background:transparent;}"));
    auto* layout = new QVBoxLayout(surface);
    layout->setContentsMargins(24, 24, 24, 20);
    auto* progressRow = new QWidget(surface);
    auto* progressLayout = new QHBoxLayout(progressRow);
    progressLayout->setContentsMargins(0, 0, 0, 0);
    progressLayout->setSpacing(8);
    affectionProgress_ = new LevelProgressWidget(progressRow);
    affectionProgress_->setObjectName(QStringLiteral("affectionProgress"));
    affectionProgress_->setMaximumLevel(AffectionController::MaximumLevel);
    affectionProgress_->setColors(QColor(QStringLiteral("#f5a9c6")),
                                  QColor(QStringLiteral("#d9364f")));
    affectionProgress_->setShowMaximumLabel(true);
    affectionProgress_->setFixedSize(66, 66);
    satietyProgress_ = new LevelProgressWidget(progressRow);
    satietyProgress_->setObjectName(QStringLiteral("satietyProgress"));
    satietyProgress_->setMaximumLevel(SatietyController::MaximumValue);
    satietyProgress_->setColors(QColor(QStringLiteral("#f2c94c")),
                                QColor(QStringLiteral("#7a5b00")));
    satietyProgress_->setShowLevelPrefix(false);
    satietyProgress_->setFixedSize(66, 66);
    levelProgress_ = new LevelProgressWidget(progressRow);
    levelProgress_->setFixedSize(66, 66);
    progressLayout->addWidget(affectionProgress_);
    progressLayout->addWidget(satietyProgress_);
    progressLayout->addWidget(levelProgress_);
    layout->addWidget(progressRow, 0, Qt::AlignHCenter);
    petVisual_ = new QLabel(QStringLiteral("ʕ •ᴥ• ʔ\n\n小 屏"), surface);
    petVisual_->setObjectName(QStringLiteral("petVisual"));
    petVisual_->setAlignment(Qt::AlignCenter);
    petVisual_->setAttribute(Qt::WA_TransparentForMouseEvents);
    QFont petFont = petVisual_->font();
    petFont.setPointSize(25);
    petFont.setBold(true);
    petVisual_->setFont(petFont);
    // 断开图片尺寸对 QLabel 最小尺寸的约束，让头像可随窗口自由缩放。
    petVisual_->setMinimumSize(1, 1);
    animationPlayer_ = new PetAnimationPlayer(petVisual_, this);
    stateLabel_ = new QLabel(QStringLiteral("空闲"), surface);
    stateLabel_->setObjectName(QStringLiteral("stateLabel"));
    stateLabel_->setAlignment(Qt::AlignCenter);
    stateLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(petVisual_, 1);
    layout->addWidget(stateLabel_);
    levelUpTimer_ = new QTimer(this);
    levelUpTimer_->setSingleShot(true);
    connect(levelUpTimer_, &QTimer::timeout, this, [this]() {
        showingLevelUp_ = false;
        animationPlayer_->stopLevelUp();
        stateLabel_->setStyleSheet(QString{});
        refreshStateLabel();
    });
    setCentralWidget(surface);
    setMouseTracking(true);
    surface->setMouseTracking(true);
    surface->installEventFilter(this);
    resizeController_ = new PetWindowResizeController(this, this);
    connect(resizeController_, &PetWindowResizeController::scalePreviewRequested,
            this, [this](int percent) {
                uiConfig_.windowScalePercent = percent;
                applyWindowScale();
            });
    connect(resizeController_, &PetWindowResizeController::scaleCommitRequested,
            this, [this](int) { persistInteractiveScale(); });
    connect(resizeController_, &PetWindowResizeController::windowGeometryChanged,
            this, [this]() {
                if (attachments_ != nullptr) attachments_->reposition();
                repositionConversationChain();
            });
    screenObservation_ = new ScreenObservationCoordinator(this);
    connect(screenObservation_, &ScreenObservationCoordinator::operationFailed,
            this, [this](const AppError& error) {
                if (errorCenter_ != nullptr) errorCenter_->report(error);
                else onOperationFailed(error);
            });
    connect(screenObservation_, &ScreenObservationCoordinator::scheduledImageReady,
            this, &MainWindow::onScreenCaptured);
    captureUiController_ = new CaptureUiController(screenObservation_, this);
    connect(captureUiController_, &CaptureUiController::operationFailed,
            this, [this](const AppError& error) {
                if (errorCenter_ != nullptr) errorCenter_->report(error);
                else onOperationFailed(error);
            });
    createOverlayWindows();
}

MainWindow::~MainWindow()
{
    screenObservation_->shutdown();
    if (conversationWindow_ != nullptr) {
        conversationWindow_->removeEventFilter(this);
        if (ConversationHistoryWindow* history = conversationWindow_->historyWindow()) {
            history->removeEventFilter(this);
        }
    }
    delete conversationWindow_;
    conversationWindow_ = nullptr;
}

void MainWindow::createOverlayWindows()
{
    actionPanel_ = new ActionPanel(this);
    inputPanel_ = new ChatInputPanel(this);
    inputActivityPanel_ = new InputActivityPanel(this);
    replyBubble_ = new ReplyBubbleWindow(this);
    errorBanner_ = new ErrorBannerWindow(this);
    // 会话列表是无父级独立顶层窗口，不进入桌宠主窗口的 QWidget 子树。
    conversationWindow_ = new ConversationWindow();
    actionHotZone_ = createHotZone(QStringLiteral("actionRevealHotZone"), QSize(44, 220));
    inputHotZone_ = createHotZone(QStringLiteral("inputRevealHotZone"), QSize(420, 44));
    for (QWidget* window : {static_cast<QWidget*>(this), static_cast<QWidget*>(actionPanel_),
                            static_cast<QWidget*>(inputPanel_), static_cast<QWidget*>(replyBubble_),
                            static_cast<QWidget*>(inputActivityPanel_),
                            static_cast<QWidget*>(errorBanner_), static_cast<QWidget*>(conversationWindow_),
                            actionHotZone_, inputHotZone_}) {
        captureUiController_->registerWindow(window);
    }
    attachments_ = new WindowAttachmentManager(this);
    attachments_->setAnchor(this);
    connect(attachments_, &WindowAttachmentManager::attachmentPositioned,
            replyBubble_, [this](QWidget* window, AttachmentSide actualSide) {
                if (window == replyBubble_) replyBubble_->setAttachmentSide(actualSide);
                if (window == inputPanel_) repositionInputActivityPanel();
            });
    attachments_->attach(actionPanel_, {AttachmentSide::Right, AttachmentAlignment::Center, 12});
    attachments_->attach(actionHotZone_, {AttachmentSide::Right, AttachmentAlignment::Center, 3});
    attachments_->attach(inputPanel_, {AttachmentSide::Below, AttachmentAlignment::Center, 12});
    attachments_->attach(inputHotZone_, {AttachmentSide::Below, AttachmentAlignment::Center, 3});
    attachments_->attach(replyBubble_, {AttachmentSide::Left, AttachmentAlignment::Center, 14});
    attachments_->attach(errorBanner_, {AttachmentSide::Above, AttachmentAlignment::Center, 12});
    actionReveal_ = new HoverRevealController(this);
    inputReveal_ = new HoverRevealController(this);
    actionReveal_->bind(actionPanel_, actionHotZone_);
    inputReveal_->bind(inputPanel_, inputHotZone_);
    // MainWindow 真正显示前不允许热区自行唤醒顶层附属窗口。
    actionReveal_->setActive(false);
    inputReveal_->setActive(false);
    inputReveal_->setCanHidePredicate([this]() { return inputPanel_->canAutoHide(); });
    actionReveal_->setCanHidePredicate([this]() {
        return conversationWindow_ == nullptr || !conversationWindow_->isVisible();
    });
    actionPanel_->installEventFilter(this);
    conversationWindow_->installEventFilter(this);
    applyUiConfig(uiConfig_);

    connect(actionPanel_, &ActionPanel::closeRequested,
            this, &MainWindow::applicationExitRequested);
    connect(actionPanel_, &ActionPanel::minimizeRequested, this, &MainWindow::hidePetShell);
    connect(actionPanel_, &ActionPanel::settingsRequested, this, &MainWindow::openSettings);
    connect(actionPanel_, &ActionPanel::conversationsRequested,
            this, &MainWindow::openConversationWindow);
    connect(actionPanel_, &ActionPanel::screenCaptureToggled,
            this, &MainWindow::toggleScreenCapture);
    connect(actionPanel_, &ActionPanel::captureOnChatToggled,
            this, &MainWindow::toggleCaptureOnChat);
    connect(actionPanel_, &ActionPanel::shopRequested,
            this, &MainWindow::openShopWindow);
    connect(actionPanel_, &ActionPanel::backpackRequested,
            this, &MainWindow::openBackpackWindow);
    connect(inputPanel_, &ChatInputPanel::sendRequested, this, &MainWindow::sendCurrentMessage);
    connect(inputPanel_, &ChatInputPanel::cancelRequested, this, &MainWindow::cancelCurrentRequest);
    connect(inputPanel_, &ChatInputPanel::retryRequested, this, &MainWindow::retryLastMessage);
    connect(errorBanner_, &ErrorBannerWindow::retryRequested, this, &MainWindow::retryLastMessage);
    connect(errorBanner_, &ErrorBannerWindow::settingsRequested, this, &MainWindow::openSettings);
    connect(conversationWindow_, &ConversationWindow::historyWindowShown, this, [this]() {
        if (ConversationHistoryWindow* history = conversationWindow_->historyWindow()) {
            history->installEventFilter(this);
            captureUiController_->registerWindow(history);
        }
        repositionConversationChain();
    });
}

QWidget* MainWindow::createHotZone(const QString& objectName, const QSize& size)
{
    auto* zone = new QWidget(this, Qt::Window);
    zone->setObjectName(objectName);
    DesktopWindowPolicy::apply(zone, {true, true, true, false, false, false});
    zone->setFixedSize(size);
    zone->setStyleSheet(QStringLiteral(
        "background:rgba(255,255,255,2);border:none;"));
    return zone;
}

void MainWindow::repositionConversationChain()
{
    if (repositioningConversationChain_ || conversationWindow_ == nullptr
        || !conversationWindow_->isVisible()) return;
    auto* button = actionPanel_->findChild<QPushButton*>(
        QStringLiteral("conversationManagerButton"));
    if (button == nullptr) return;
    const QRect anchorGeometry(button->mapToGlobal(QPoint(0, 0)), button->size());
    QScreen* screen = QGuiApplication::screenAt(anchorGeometry.center());
    if (screen == nullptr) screen = QGuiApplication::primaryScreen();
    if (screen == nullptr) return;

    QVector<QSize> sizes{conversationWindow_->size()};
    ConversationHistoryWindow* history = conversationWindow_->historyWindow();
    if (history != nullptr && history->isVisible()) sizes.append(history->size());
    const HorizontalWindowChainResult placement = WindowPlacement::horizontalChain({
        screen->availableGeometry(), anchorGeometry, sizes, AttachmentSide::Right,
        qMax(1, qRound(12 * qreal(uiConfig_.windowScalePercent) / 100.0))});
    if (placement.positions.isEmpty()) return;
    repositioningConversationChain_ = true;
    conversationWindow_->move(placement.positions.at(0));
    if (history != nullptr && history->isVisible() && placement.positions.size() > 1) {
        history->move(placement.positions.at(1));
    }
    repositioningConversationChain_ = false;
}

void MainWindow::repositionInputActivityPanel()
{
    if (inputActivityPanel_ == nullptr || inputPanel_ == nullptr) return;
    const UiScaleMetrics metrics(uiConfig_.windowScalePercent);
    const QPoint desired(inputPanel_->x() + metrics.scaled(12),
                         inputPanel_->y() - inputActivityPanel_->height()
                             + metrics.scaled(5));
    QScreen* targetScreen = QGuiApplication::screenAt(frameGeometry().center());
    if (targetScreen == nullptr) targetScreen = QGuiApplication::primaryScreen();
    inputActivityPanel_->move(targetScreen == nullptr ? desired
        : WindowPlacement::clamp(targetScreen->availableGeometry(),
                                 inputActivityPanel_->size(), desired));
}

void MainWindow::setChatController(ChatController* controller)
{
    if (chatController_ == controller) return;
    if (chatController_ != nullptr) disconnect(chatController_, nullptr, this, nullptr);
    chatController_ = controller;
    screenObservation_->setObservationReady(chatController_ != nullptr
                                             && !conversationId_.isEmpty());
    if (chatController_ != nullptr) {
        connect(chatController_, &ChatController::requestStarted, this, &MainWindow::onRequestStarted);
        connect(chatController_, &ChatController::replyDelta, this, &MainWindow::onReplyDelta);
        connect(chatController_, &ChatController::replyFinished, this, &MainWindow::onReplyFinished);
        connect(chatController_, &ChatController::requestFailed, this, &MainWindow::onRequestFailed);
        connect(chatController_, &ChatController::operationFailed, this, &MainWindow::onOperationFailed);
        connect(chatController_, &ChatController::stateChanged, this, &MainWindow::updatePetState);
        updatePetState(chatController_->state());
    }
}

void MainWindow::setErrorCenter(ErrorCenter* errorCenter)
{
    if (errorCenter_ == errorCenter) return;
    if (errorCenter_ != nullptr) disconnect(errorCenter_, nullptr, this, nullptr);
    errorCenter_ = errorCenter;
    if (errorCenter_ != nullptr) {
        connect(errorCenter_, &ErrorCenter::errorReported, this, &MainWindow::onErrorReported);
    }
}

void MainWindow::setConversationController(ConversationController* controller)
{
    if (conversationController_ == controller) return;
    if (conversationController_ != nullptr) disconnect(conversationController_, nullptr, this, nullptr);
    conversationController_ = controller;
    conversationWindow_->setController(controller);
    if (conversationController_ != nullptr) {
        connect(conversationController_, &ConversationController::currentConversationChanged,
                this, &MainWindow::onCurrentConversationChanged);
        connect(conversationController_, &ConversationController::operationFailed,
                this, [this](const AppError& error) {
                    if (errorCenter_ == nullptr) onOperationFailed(error);
                });
        conversationId_ = conversationController_->currentConversationId();
    }
}

void MainWindow::setSettingsController(SettingsController* controller)
{
    if (settingsController_ == controller) return;
    if (settingsController_ != nullptr) disconnect(settingsController_, nullptr, this, nullptr);
    settingsController_ = controller;
    captureUiController_->setSettingsController(controller);
    if (settingsController_ != nullptr) {
        applyUiConfig(settingsController_->uiConfig());
        connect(settingsController_, &SettingsController::uiConfigurationChanged,
                this, &MainWindow::applyUiConfig);
        connect(settingsController_, &SettingsController::settingsApplied,
                this, [this]() { screenObservation_->resetFingerprint(); });
    }
}

void MainWindow::setCaptureDirectory(const QString& directory)
{
    screenObservation_->setCaptureDirectory(directory);
}

void MainWindow::setModelErrorMessages(const QHash<QString, QString>& messages)
{
    errorPresenter_ = ModelErrorPresenter(messages);
}

void MainWindow::setConversation(const QString& conversationId,
                                 const QVector<ConversationMessage>& messages)
{
    conversationId_ = conversationId;
    screenObservation_->setObservationScope(conversationId_);
    screenObservation_->setObservationReady(chatController_ != nullptr
                                             && !conversationId_.isEmpty());
    QString title;
    if (conversationController_ != nullptr
        && conversationController_->currentConversationId() == conversationId) {
        title = conversationController_->currentConversationTitle();
    }
    conversationWindow_->setConversation(conversationId, title, messages);
}

QString MainWindow::conversationId() const { return conversationId_; }
ConversationWindow* MainWindow::conversationWindow() const { return conversationWindow_; }

void MainWindow::applyUiConfig(const UiConfig& config)
{
    const UiConfig requested = config.normalized();
    uiConfig_ = captureUiController_->applyConfiguration(requested);
    if (requested.automaticScreenAnalysisEnabled
        && !uiConfig_.automaticScreenAnalysisEnabled
        && settingsController_ != nullptr && !captureCorrectionPending_) {
        captureCorrectionPending_ = true;
        const UiConfig corrected = uiConfig_;
        QTimer::singleShot(0, this, [this, corrected]() {
            AppError ignored;
            settingsController_->updateUiConfig(corrected, &ignored);
            captureCorrectionPending_ = false;
        });
    }
    applyWindowScale();
    actionPanel_->setCaptureOptions(uiConfig_.screenCaptureEnabled, uiConfig_.captureOnChat);
    attachments_->reposition();
    repositionConversationChain();
    replyBubble_->setDisplayDuration(uiConfig_.replyBubbleDurationMs);
    actionReveal_->setTimings(uiConfig_.hoverHideDelayMs, uiConfig_.fadeDurationMs);
    inputReveal_->setTimings(uiConfig_.hoverHideDelayMs, uiConfig_.fadeDurationMs);
    const QString avatarPath = resolveConfiguredAssetPath(uiConfig_.petAvatarPath);
    petAvatarPixmap_ = QPixmap(avatarPath);
    if (!petAvatarPixmap_.isNull()) {
        petVisual_->setText(QString{});
    } else {
        petAvatarPixmap_ = QPixmap{};
        petVisual_->setPixmap(QPixmap{});
        petVisual_->setText(QStringLiteral("ʕ •ᴥ• ʔ\n\n小 屏"));
    }
    animationPlayer_->setFallbackPixmap(petAvatarPixmap_);
    const QString animationRoot = avatarPath.isEmpty()
        ? QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("animations"))
        : QFileInfo(avatarPath).dir().filePath(QStringLiteral("animations"));
    animationPlayer_->setAnimationRoot(animationRoot);
    animationPlayer_->setState(petState_);
    updatePetAvatar();
    if (conversationWindow_ != nullptr) {
        conversationWindow_->setConversationAvatarPath(
            resolveConfiguredAssetPath(uiConfig_.conversationAvatarPath));
    }
}

void MainWindow::setPetLifecycleController(PetLifecycleController* controller)
{
    if (petLifecycleController_ == controller) return;
    if (petLifecycleController_ != nullptr) {
        disconnect(petLifecycleController_, nullptr, this, nullptr);
    }
    petLifecycleController_ = controller;
    if (petLifecycleController_ == nullptr) return;
    idleDescription_ = petLifecycleController_->currentIdleDescription();
    levelProgress_->setProgress(petLifecycleController_->level(),
                                petLifecycleController_->progressPercent());
    connect(petLifecycleController_, &PetLifecycleController::startupGreetingRequested,
            this, &MainWindow::showStartupGreeting);
    connect(petLifecycleController_, &PetLifecycleController::idleDescriptionChanged,
            this, [this](const QString& text) {
                idleDescription_ = text;
                refreshStateLabel();
            });
    connect(petLifecycleController_, &PetLifecycleController::progressChanged,
            levelProgress_, &LevelProgressWidget::setProgress);
    connect(petLifecycleController_, &PetLifecycleController::levelUp,
            this, &MainWindow::showLevelUp);
    refreshStateLabel();
}

void MainWindow::setAffectionController(AffectionController* controller)
{
    if (affectionController_ == controller) return;
    if (affectionController_ != nullptr) {
        disconnect(affectionController_, nullptr, this, nullptr);
    }
    affectionController_ = controller;
    if (affectionController_ == nullptr) return;
    affectionProgress_->setProgress(affectionController_->level(),
                                    affectionController_->progressPercent());
    connect(affectionController_, &AffectionController::progressChanged,
            affectionProgress_, &LevelProgressWidget::setProgress);
}

void MainWindow::setSatietyController(SatietyController* controller)
{
    if (satietyController_ == controller) return;
    if (satietyController_ != nullptr) disconnect(satietyController_, nullptr, this, nullptr);
    satietyController_ = controller;
    if (satietyController_ == nullptr) return;
    satietyProgress_->setProgress(satietyController_->value(), satietyController_->value());
    connect(satietyController_, &SatietyController::valueChanged,
            this, [this](int value) { satietyProgress_->setProgress(value, value); });
}

void MainWindow::setPetEconomyController(PetEconomyController* controller)
{
    if (petEconomyController_ == controller) return;
    if (petEconomyController_ != nullptr) {
        disconnect(petEconomyController_, nullptr, this, nullptr);
    }
    petEconomyController_ = controller;
    if (petEconomyController_ != nullptr) {
        connect(petEconomyController_, &PetEconomyController::itemGiven,
                this, &MainWindow::showItemThanks);
    }
}

void MainWindow::setInputActivityController(InputActivityController* controller)
{
    if (inputActivityController_ == controller) return;
    if (inputActivityController_ != nullptr) {
        disconnect(inputActivityController_, nullptr, this, nullptr);
    }
    inputActivityController_ = controller;
    if (inputActivityController_ == nullptr) return;
    inputActivityPanel_->setCount(inputActivityController_->inputCount());
    connect(inputActivityController_, &InputActivityController::countChanged,
            this, [this](qint64 inputCount) {
                inputActivityPanel_->setCount(inputCount);
                repositionInputActivityPanel();
            });
}

void MainWindow::toggleScreenCapture(bool enabled)
{
    AppError error;
    if (!captureUiController_->setScreenCaptureEnabled(enabled, &error)) {
        actionPanel_->setCaptureOptions(uiConfig_.screenCaptureEnabled, uiConfig_.captureOnChat);
    }
}

void MainWindow::toggleCaptureOnChat(bool enabled)
{
    if (!uiConfig_.screenCaptureEnabled) {
        actionPanel_->setCaptureOptions(false, false);
        return;
    }
    if (enabled
        && QMessageBox::warning(
            this, QStringLiteral("确认发送屏幕内容"),
            QStringLiteral("启用后，屏幕截图会随用户消息发送给当前配置的远端模型，"
                           "可能包含隐私信息并产生模型费用。是否继续？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        actionPanel_->setCaptureOptions(true, uiConfig_.captureOnChat);
        return;
    }
    AppError error;
    if (!captureUiController_->setCaptureOnChatEnabled(enabled, &error)) {
        actionPanel_->setCaptureOptions(uiConfig_.screenCaptureEnabled, uiConfig_.captureOnChat);
    }
}

void MainWindow::applyWindowScale()
{
    const int percent = uiConfig_.windowScalePercent;
    resizeController_->setScalePercent(percent);
    const UiScaleMetrics metrics(percent);
    QScreen* targetScreen = QGuiApplication::screenAt(frameGeometry().center());
    if (targetScreen == nullptr) targetScreen = screen();
    if (targetScreen == nullptr) targetScreen = QGuiApplication::primaryScreen();
    const QRect available = targetScreen == nullptr
        ? QRect(0, 0, 1920, 1080) : targetScreen->availableGeometry();

    setMinimumSize(metrics.scaledForScreen(QSize(240, 280), available));
    resize(metrics.scaledForScreen(QSize(300, 350), available));
    if (auto* petLayout = qobject_cast<QVBoxLayout*>(centralWidget()->layout())) {
        petLayout->setContentsMargins(metrics.scaled(24), metrics.scaled(24),
                                      metrics.scaled(24), metrics.scaled(20));
    }
    QFont petFont = petVisual_->font();
    petFont.setPointSizeF(25.0 * metrics.factor());
    petVisual_->setFont(petFont);
    levelProgress_->setFixedSize(metrics.scaled(66), metrics.scaled(66));
    affectionProgress_->setFixedSize(metrics.scaled(66), metrics.scaled(66));
    satietyProgress_->setFixedSize(metrics.scaled(66), metrics.scaled(66));
    if (affectionProgress_->parentWidget() != nullptr) {
        if (auto* progressLayout = qobject_cast<QHBoxLayout*>(
                affectionProgress_->parentWidget()->layout())) {
            progressLayout->setSpacing(metrics.scaled(8));
        }
    }

    actionPanel_->setUiScalePercent(percent);
    inputPanel_->setUiScalePercent(percent);
    inputActivityPanel_->setUiScalePercent(percent);
    replyBubble_->setUiScalePercent(percent);
    errorBanner_->setUiScalePercent(percent);
    conversationWindow_->setUiScalePercent(percent);
    actionHotZone_->setFixedSize(metrics.scaled(44), metrics.scaled(220));
    inputHotZone_->setFixedSize(metrics.scaled(420), metrics.scaled(44));

    attachments_->attach(actionPanel_, {AttachmentSide::Right,
        AttachmentAlignment::Center, metrics.scaled(12)});
    attachments_->attach(actionHotZone_, {AttachmentSide::Right,
        AttachmentAlignment::Center, metrics.scaled(3)});
    attachments_->attach(inputPanel_, {AttachmentSide::Below,
        AttachmentAlignment::Center, metrics.scaled(12)});
    attachments_->attach(inputHotZone_, {AttachmentSide::Below,
        AttachmentAlignment::Center, metrics.scaled(3)});
    attachments_->attach(replyBubble_, {AttachmentSide::Left,
        AttachmentAlignment::Center, metrics.scaled(14)});
    attachments_->attach(errorBanner_, {AttachmentSide::Above,
        AttachmentAlignment::Center, metrics.scaled(12)});
    if (!resizeController_->isResizing()) {
        move(WindowPlacement::clamp(available, frameGeometry().size(), frameGeometry().topLeft()));
    }
    attachments_->reposition();
    repositionInputActivityPanel();
    repositionConversationChain();
}

void MainWindow::updatePetAvatar()
{
    if (petVisual_ == nullptr || animationPlayer_ == nullptr) return;
    QSize target = petVisual_->size();
    if (target.width() <= 0 || target.height() <= 0) target = size();
    if (target.width() <= 0 || target.height() <= 0) return;
    animationPlayer_->setTargetSize(target);
}

void MainWindow::sendCurrentMessage()
{
    if (chatController_ == nullptr || !currentRequestId_.isEmpty()) return;
    const QString text = inputPanel_->text();
    if (text.isEmpty() || conversationId_.isEmpty()) return;
    screenObservation_->setBusy(true);
    ChatOptions options;
    options.stream = true;
    QString requestId;
    if (uiConfig_.screenCaptureEnabled && uiConfig_.captureOnChat) {
        CapturedImage captured;
        AppError captureError;
        if (!screenObservation_->captureForChat(&captured, &captureError)) {
            screenObservation_->setBusy(false);
            return;
        }
        MessageImage attachment;
        attachment.data = captured.data;
        attachment.mimeType = captured.format == QStringLiteral("webp")
            ? QStringLiteral("image/webp") : QStringLiteral("image/jpeg");
        attachment.detail = QStringLiteral("original");
        attachment.size = captured.size;
        requestId = chatController_->sendUserMessageWithScreenshot(
            conversationId_, text, attachment, captured.fingerprint,
            captured.capturedAt, options);
    } else {
        requestId = chatController_->sendMessage(conversationId_, text, options);
    }
    if (requestId.isEmpty()) {
        screenObservation_->setBusy(false);
        return;
    }
    currentRequestId_ = requestId;
    // 用户主动附图仍属于普通聊天，可沿用失败重试语义。
    currentRequestIsScreenshot_ = false;
    streamingReplyStarted_ = false;
    conversationWindow_->appendMessage(MessageRole::User, text);
    inputPanel_->clear();
    inputPanel_->setBusy(true);
    inputPanel_->setRetryEnabled(false);
}

void MainWindow::retryLastMessage()
{
    if (chatController_ == nullptr || !currentRequestId_.isEmpty()) return;
    screenObservation_->setBusy(true);
    const QString requestId = chatController_->retryLast();
    if (requestId.isEmpty()) {
        screenObservation_->setBusy(false);
        return;
    }
    currentRequestId_ = requestId;
    currentRequestIsScreenshot_ = false;
    streamingReplyStarted_ = false;
    errorBanner_->dismiss();
    inputPanel_->setBusy(true);
    inputPanel_->setRetryEnabled(false);
}

void MainWindow::cancelCurrentRequest()
{
    if (chatController_ != nullptr && !currentRequestId_.isEmpty()) {
        chatController_->cancel(currentRequestId_);
    }
}

void MainWindow::onRequestStarted(const QString& requestId)
{
    if (isCurrentRequest(requestId)) updatePetState(PetState::Thinking);
}

void MainWindow::onReplyDelta(const QString& requestId, const QString& delta)
{
    if (!isCurrentRequest(requestId)) return;
    if (!streamingReplyStarted_) {
        replyBubble_->beginReply();
        conversationWindow_->beginAssistantReply();
        streamingReplyStarted_ = true;
        attachments_->reposition();
    }
    replyBubble_->appendDelta(delta);
    conversationWindow_->appendAssistantDelta(delta);
}

void MainWindow::onReplyFinished(const QString& requestId, const QString& content)
{
    if (!isCurrentRequest(requestId)) return;
    const bool screenshotRequest = currentRequestIsScreenshot_;
    if (!streamingReplyStarted_) replyBubble_->beginReply();
    replyBubble_->finishReply(content);
    if (!screenshotRequest) conversationWindow_->finishAssistantReply(content);
    attachments_->reposition();
    currentRequestId_.clear();
    currentRequestIsScreenshot_ = false;
    streamingReplyStarted_ = false;
    inputPanel_->setBusy(false);
    inputPanel_->setRetryEnabled(false);
    inputPanel_->focusInput();
    if (conversationController_ != nullptr) conversationController_->switchConversation(conversationId_);
    if (screenshotRequest) screenObservation_->finishScheduledRequest(true);
    else screenObservation_->setBusy(false);
}

void MainWindow::onRequestFailed(const QString& requestId, const ModelError& error)
{
    if (!isCurrentRequest(requestId)) return;
    const bool screenshotRequest = currentRequestIsScreenshot_;
    currentRequestId_.clear();
    currentRequestIsScreenshot_ = false;
    streamingReplyStarted_ = false;
    inputPanel_->setBusy(false);
    inputPanel_->setRetryEnabled(!screenshotRequest && error.retryable);
    if (errorCenter_ == nullptr) {
        errorBanner_->showError(errorPresenter_.message(error), error.retryable);
        attachments_->reposition();
    }
    if (conversationController_ != nullptr) conversationController_->switchConversation(conversationId_);
    if (screenshotRequest) screenObservation_->finishScheduledRequest(false);
    else screenObservation_->setBusy(false);
}

void MainWindow::onScreenCaptured(const CapturedImage& image)
{
    if (chatController_ == nullptr || conversationId_.isEmpty()
        || !currentRequestId_.isEmpty()) {
        screenObservation_->finishScheduledRequest();
        return;
    }
    MessageImage attachment;
    attachment.data = image.data;
    attachment.mimeType = image.format == QStringLiteral("webp")
        ? QStringLiteral("image/webp") : QStringLiteral("image/jpeg");
    attachment.detail = QStringLiteral("original");
    attachment.size = image.size;
    ChatOptions options;
    // DeepSeek 视觉接口当前按普通 JSON 返回；截图请求不走 SSE 流式解析。
    options.stream = false;
    options.disableThinking = true;
    const QString requestId = chatController_->sendScreenshotMessage(
        conversationId_, QStringLiteral("请分析当前屏幕内容，并用自然、简洁的方式回应。"),
        attachment, image.fingerprint, image.capturedAt, options,
        image.captureId, image.source, image.durationMs, image.appHint,
        settingsController_ == nullptr ? QString{} : settingsController_->activeModel().model);
    if (requestId.isEmpty()) {
        screenObservation_->finishScheduledRequest(false);
        return;
    }
    currentRequestId_ = requestId;
    currentRequestIsScreenshot_ = true;
    streamingReplyStarted_ = false;
    inputPanel_->setBusy(true);
    inputPanel_->setRetryEnabled(false);
}

void MainWindow::onOperationFailed(const AppError& error)
{
    if (errorCenter_ != nullptr) return;
    errorBanner_->showError(errorPresenter_.message(error), error.retryable);
    attachments_->reposition();
    updatePetState(PetState::Error);
}

void MainWindow::onErrorReported(const AppError& error, const QString& userMessage)
{
    errorBanner_->showError(userMessage, error.retryable);
    attachments_->reposition();
}

void MainWindow::onCurrentConversationChanged(
    const QString& conversationId, const QString& title,
    const QVector<ConversationMessage>& messages)
{
    conversationId_ = conversationId;
    screenObservation_->setObservationScope(conversationId_);
    screenObservation_->setObservationReady(chatController_ != nullptr
                                             && !conversationId_.isEmpty());
    conversationWindow_->setConversation(conversationId, title, messages);
}

void MainWindow::openSettings()
{
    if (settingsController_ == nullptr) return;
    SettingsDialog dialog(settingsController_, this, screenObservation_->screenCapture(),
                          captureUiController_);
    dialog.exec();
}

void MainWindow::openConversationWindow()
{
    // 从操作栏或托盘进入时，先显示作为窗口链锚点的操作栏。
    actionReveal_->reveal();
    conversationWindow_->show();
    repositionConversationChain();
    conversationWindow_->raise();
    conversationWindow_->activateWindow();
}

void MainWindow::openShopWindow()
{
    ShopWindow dialog(this);
    dialog.setController(petEconomyController_);
    dialog.setUiScalePercent(uiConfig_.windowScalePercent);
    captureUiController_->registerWindow(&dialog);
    dialog.exec();
}

void MainWindow::openBackpackWindow()
{
    BackpackWindow dialog(this);
    dialog.setController(petEconomyController_);
    dialog.setUiScalePercent(uiConfig_.windowScalePercent);
    captureUiController_->registerWindow(&dialog);
    dialog.exec();
}

void MainWindow::hidePetShell()
{
    actionReveal_->setActive(false);
    inputReveal_->setActive(false);
    replyBubble_->hide();
    errorBanner_->hide();
    conversationWindow_->hideAllHistoryWindows();
    conversationWindow_->hide();
    inputActivityPanel_->hide();
    hide();
}

void MainWindow::showPetShell()
{
    show();
    inputActivityPanel_->show();
    raise();
    errorBanner_->restoreIfActive();
    attachments_->reposition();
    repositionInputActivityPanel();
    actionReveal_->showTemporarily();
    inputReveal_->showTemporarily();
}

void MainWindow::updatePetState(PetState state)
{
    petState_ = state;
    if (!showingLevelUp_ && animationPlayer_ != nullptr) {
        animationPlayer_->setState(state);
    }
    refreshStateLabel();
}

void MainWindow::showStartupGreeting(const QString& text)
{
    if (text.trimmed().isEmpty()) return;
    replyBubble_->beginReply();
    replyBubble_->finishReply(text);
    attachments_->reposition();
}

void MainWindow::showLevelUp(int level)
{
    Q_UNUSED(level);
    showingLevelUp_ = true;
    animationPlayer_->playLevelUp();
    stateLabel_->setStyleSheet(QStringLiteral("color:#ff3b30;font-weight:700;"));
    stateLabel_->setText(QStringLiteral("升级了！！！"));
    levelUpTimer_->start(10 * 1000);
}

void MainWindow::showItemThanks(const QString& itemName)
{
    if (itemName.trimmed().isEmpty()) return;
    const QString userAddress = settingsController_ == nullptr
        ? QStringLiteral("你") : settingsController_->persona().userAddress;
    replyBubble_->beginReply();
    replyBubble_->finishReply(QStringLiteral("谢谢%1，这是%2吗？谢谢你哦~我很喜欢这个！")
                                  .arg(userAddress, itemName));
    attachments_->reposition();
}

void MainWindow::refreshStateLabel()
{
    if (showingLevelUp_) return;
    stateLabel_->setStyleSheet(QString{});
    switch (petState_) {
    case PetState::Idle:
        stateLabel_->setText(idleDescription_.isEmpty()
            ? QStringLiteral("摸鱼中~") : idleDescription_);
        break;
    case PetState::Thinking: stateLabel_->setText(QStringLiteral("思考中…")); break;
    case PetState::Speaking: stateLabel_->setText(QStringLiteral("回复中…")); break;
    case PetState::Error: stateLabel_->setText(QStringLiteral("遇到问题")); break;
    }
}

bool MainWindow::isCurrentRequest(const QString& requestId) const
{
    return !requestId.isEmpty() && requestId == currentRequestId_;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // Windows 任务栏的“关闭窗口”和 Alt+F4 都会进入这里；业务清理由应用编排器统一完成。
    event->accept();
    emit applicationExitRequested();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == centralWidget()) {
        if (event->type() == QEvent::MouseButtonPress
            && resizeController_->handlePress(static_cast<QMouseEvent*>(event))) return true;
        if (event->type() == QEvent::MouseMove
            && resizeController_->handleMove(static_cast<QMouseEvent*>(event))) return true;
        if (event->type() == QEvent::MouseButtonRelease
            && resizeController_->handleRelease(static_cast<QMouseEvent*>(event))) return true;
    }
    if ((watched == actionPanel_ || watched == conversationWindow_
         || watched == conversationWindow_->historyWindow())
        && (event->type() == QEvent::Move || event->type() == QEvent::Resize
            || event->type() == QEvent::Show)) {
        repositionConversationChain();
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::mousePressEvent(QMouseEvent* event)
{
    if (resizeController_->handlePress(event)) return;
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent* event)
{
    if (resizeController_->handleMove(event)) return;
    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent* event)
{
    if (resizeController_->handleRelease(event)) return;
    QMainWindow::mouseReleaseEvent(event);
}

void MainWindow::persistInteractiveScale()
{
    if (settingsController_ == nullptr
        || settingsController_->uiConfig().windowScalePercent == uiConfig_.windowScalePercent) {
        return;
    }
    const UiConfig previous = settingsController_->uiConfig();
    AppError error;
    if (!settingsController_->updateUiConfig(uiConfig_, &error)) applyUiConfig(previous);
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    // 布局重排尚未完成，延迟到事件循环后再缩放，避免拿到过期尺寸。
    QTimer::singleShot(0, this, [this]() { updatePetAvatar(); });
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    actionReveal_->setActive(true);
    inputReveal_->setActive(true);
    attachments_->reposition();
    inputActivityPanel_->show();
    repositionInputActivityPanel();
    actionReveal_->showTemporarily();
    inputReveal_->showTemporarily();
}

void MainWindow::hideEvent(QHideEvent* event)
{
    actionReveal_->setActive(false);
    inputReveal_->setActive(false);
    replyBubble_->hide();
    errorBanner_->hide();
    inputActivityPanel_->hide();
    QMainWindow::hideEvent(event);
}

} // namespace zhu_screen_pet
