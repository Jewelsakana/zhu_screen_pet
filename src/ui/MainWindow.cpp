#include "ui/MainWindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QGuiApplication>
#include <QHideEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QPixmap>
#include <QScreen>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QWidget>

#include "app/ChatController.h"
#include "app/ConversationController.h"
#include "app/ErrorCenter.h"
#include "app/SettingsController.h"
#include "infrastructure/DesktopWindowPolicy.h"
#include "infrastructure/ImageCompressor.h"
#include "infrastructure/ScreenCapture.h"
#include "infrastructure/WindowAttachmentManager.h"
#include "infrastructure/WindowPlacement.h"
#include "ui/ActionPanel.h"
#include "ui/ChatInputPanel.h"
#include "ui/ConversationWindow.h"
#include "ui/ConversationHistoryWindow.h"
#include "ui/ErrorBannerWindow.h"
#include "ui/HoverRevealController.h"
#include "ui/ReplyBubbleWindow.h"
#include "ui/SettingsDialog.h"

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
    resize(360, 420);
    setMinimumSize(280, 320);
    auto* surface = new QWidget(this);
    surface->setObjectName(QStringLiteral("petSurface"));
    surface->setStyleSheet(QStringLiteral(
        "QWidget#petSurface{background:rgba(104,129,210,42);border:2px solid rgba(255,255,255,70);"
        "border-radius:72px;} QLabel{color:white;background:transparent;}"));
    auto* layout = new QVBoxLayout(surface);
    layout->setContentsMargins(24, 24, 24, 20);
    petVisual_ = new QLabel(QStringLiteral("ʕ •ᴥ• ʔ\n\n小 屏"), surface);
    petVisual_->setObjectName(QStringLiteral("petVisual"));
    petVisual_->setAlignment(Qt::AlignCenter);
    petVisual_->setAttribute(Qt::WA_TransparentForMouseEvents);
    QFont petFont = petVisual_->font();
    petFont.setPointSize(25);
    petFont.setBold(true);
    petVisual_->setFont(petFont);
    stateLabel_ = new QLabel(QStringLiteral("空闲"), surface);
    stateLabel_->setObjectName(QStringLiteral("stateLabel"));
    stateLabel_->setAlignment(Qt::AlignCenter);
    stateLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(petVisual_, 1);
    layout->addWidget(stateLabel_);
    setCentralWidget(surface);
    screenCapture_ = new ScreenCapture(this);
    connect(screenCapture_, &ScreenCapture::captureFailed, this, [this](const QString& message) {
        Q_UNUSED(message);
    });
    createOverlayWindows();
}

MainWindow::~MainWindow()
{
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
    replyBubble_ = new ReplyBubbleWindow(this);
    errorBanner_ = new ErrorBannerWindow(this);
    // 会话列表是无父级独立顶层窗口，不进入桌宠主窗口的 QWidget 子树。
    conversationWindow_ = new ConversationWindow();
    actionHotZone_ = createHotZone(QStringLiteral("actionRevealHotZone"), QSize(44, 220));
    inputHotZone_ = createHotZone(QStringLiteral("inputRevealHotZone"), QSize(420, 44));
    attachments_ = new WindowAttachmentManager(this);
    attachments_->setAnchor(this);
    connect(attachments_, &WindowAttachmentManager::attachmentPositioned,
            replyBubble_, [this](QWidget* window, AttachmentSide actualSide) {
                if (window == replyBubble_) replyBubble_->setAttachmentSide(actualSide);
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
    connect(inputPanel_, &ChatInputPanel::sendRequested, this, &MainWindow::sendCurrentMessage);
    connect(inputPanel_, &ChatInputPanel::cancelRequested, this, &MainWindow::cancelCurrentRequest);
    connect(inputPanel_, &ChatInputPanel::retryRequested, this, &MainWindow::retryLastMessage);
    connect(errorBanner_, &ErrorBannerWindow::retryRequested, this, &MainWindow::retryLastMessage);
    connect(errorBanner_, &ErrorBannerWindow::settingsRequested, this, &MainWindow::openSettings);
    connect(conversationWindow_, &ConversationWindow::historyWindowShown, this, [this]() {
        if (ConversationHistoryWindow* history = conversationWindow_->historyWindow()) {
            history->installEventFilter(this);
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
        screen->availableGeometry(), anchorGeometry, sizes, AttachmentSide::Right, 12});
    if (placement.positions.isEmpty()) return;
    repositioningConversationChain_ = true;
    conversationWindow_->move(placement.positions.at(0));
    if (history != nullptr && history->isVisible() && placement.positions.size() > 1) {
        history->move(placement.positions.at(1));
    }
    repositioningConversationChain_ = false;
}

void MainWindow::setChatController(ChatController* controller)
{
    if (chatController_ == controller) return;
    if (chatController_ != nullptr) disconnect(chatController_, nullptr, this, nullptr);
    chatController_ = controller;
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
    if (settingsController_ != nullptr) {
        applyUiConfig(settingsController_->uiConfig());
        connect(settingsController_, &SettingsController::uiConfigurationChanged,
                this, &MainWindow::applyUiConfig);
    }
}

void MainWindow::setCaptureDirectory(const QString& directory)
{
    captureDirectory_ = directory;
}

void MainWindow::setModelErrorMessages(const QHash<QString, QString>& messages)
{
    errorPresenter_ = ModelErrorPresenter(messages);
}

void MainWindow::setConversation(const QString& conversationId,
                                 const QVector<ConversationMessage>& messages)
{
    conversationId_ = conversationId;
    QString title;
    if (conversationController_ != nullptr
        && conversationController_->currentConversationId() == conversationId) {
        title = conversationController_->currentConversationTitle();
    }
    conversationWindow_->setConversation(conversationId, title, messages);
    lastAssistantReply_.clear();
    for (const ConversationMessage& message : messages) {
        if (message.message.role == MessageRole::Assistant) lastAssistantReply_ = message.message.content;
    }
}

QString MainWindow::conversationId() const { return conversationId_; }
ConversationWindow* MainWindow::conversationWindow() const { return conversationWindow_; }

void MainWindow::applyUiConfig(const UiConfig& config)
{
    uiConfig_ = config.normalized();
    replyBubble_->setDisplayDuration(uiConfig_.replyBubbleDurationMs);
    actionReveal_->setTimings(uiConfig_.hoverHideDelayMs, uiConfig_.fadeDurationMs);
    inputReveal_->setTimings(uiConfig_.hoverHideDelayMs, uiConfig_.fadeDurationMs);
    const QString avatarPath = resolveConfiguredAssetPath(uiConfig_.petAvatarPath);
    const QPixmap avatar(avatarPath);
    if (!avatar.isNull()) {
        petVisual_->setPixmap(avatar.scaled(280, 300, Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation));
        petVisual_->setText(QString{});
    } else {
        petVisual_->setPixmap(QPixmap{});
        petVisual_->setText(QStringLiteral("ʕ •ᴥ• ʔ\n\n小 屏"));
    }
    if (conversationWindow_ != nullptr) {
        conversationWindow_->setConversationAvatarPath(
            resolveConfiguredAssetPath(uiConfig_.conversationAvatarPath));
    }
    ImageCompressionOptions compression;
    compression.format = uiConfig_.captureImageFormat;
    compression.maxWidth = uiConfig_.captureMaxWidth;
    compression.quality = uiConfig_.captureQuality;
    screenCapture_->configure(uiConfig_.screenCaptureEnabled,
                              uiConfig_.screenCaptureIntervalMs,
                              captureDirectory_, compression);
    if (uiConfig_.screenCaptureEnabled) screenCapture_->start();
    else screenCapture_->stop();
}

void MainWindow::sendCurrentMessage()
{
    if (chatController_ == nullptr || !currentRequestId_.isEmpty()) return;
    const QString text = inputPanel_->text();
    if (text.isEmpty() || conversationId_.isEmpty()) return;
    if (uiConfig_.screenCaptureEnabled && uiConfig_.captureOnChat) {
        screenCapture_->captureNow();
    }
    ChatOptions options;
    options.stream = true;
    const QString requestId = chatController_->sendMessage(conversationId_, text, options);
    if (requestId.isEmpty()) return;
    currentRequestId_ = requestId;
    streamingReplyStarted_ = false;
    conversationWindow_->appendMessage(MessageRole::User, text);
    inputPanel_->clear();
    inputPanel_->setBusy(true);
    inputPanel_->setRetryEnabled(false);
}

void MainWindow::retryLastMessage()
{
    if (chatController_ == nullptr || !currentRequestId_.isEmpty()) return;
    const QString requestId = chatController_->retryLast();
    if (requestId.isEmpty()) return;
    currentRequestId_ = requestId;
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
    if (!streamingReplyStarted_) replyBubble_->beginReply();
    replyBubble_->finishReply(content);
    conversationWindow_->finishAssistantReply(content);
    attachments_->reposition();
    lastAssistantReply_ = content;
    currentRequestId_.clear();
    streamingReplyStarted_ = false;
    inputPanel_->setBusy(false);
    inputPanel_->setRetryEnabled(false);
    inputPanel_->focusInput();
    if (conversationController_ != nullptr) conversationController_->switchConversation(conversationId_);
}

void MainWindow::onRequestFailed(const QString& requestId, const ModelError& error)
{
    if (!isCurrentRequest(requestId)) return;
    currentRequestId_.clear();
    streamingReplyStarted_ = false;
    inputPanel_->setBusy(false);
    inputPanel_->setRetryEnabled(error.retryable);
    if (errorCenter_ == nullptr) {
        errorBanner_->showError(errorPresenter_.message(error), error.retryable);
        attachments_->reposition();
    }
    if (conversationController_ != nullptr) conversationController_->switchConversation(conversationId_);
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
    conversationWindow_->setConversation(conversationId, title, messages);
}

void MainWindow::openSettings()
{
    if (settingsController_ == nullptr) return;
    SettingsDialog dialog(settingsController_, this, screenCapture_);
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

void MainWindow::hidePetShell()
{
    actionReveal_->setActive(false);
    inputReveal_->setActive(false);
    replyBubble_->hide();
    errorBanner_->hide();
    conversationWindow_->hideAllHistoryWindows();
    conversationWindow_->hide();
    hide();
}

void MainWindow::showPetShell()
{
    show();
    raise();
    errorBanner_->restoreIfActive();
    attachments_->reposition();
    actionReveal_->showTemporarily();
    inputReveal_->showTemporarily();
}

void MainWindow::updatePetState(PetState state)
{
    switch (state) {
    case PetState::Idle: stateLabel_->setText(QStringLiteral("空闲")); break;
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
    if (event->button() == Qt::LeftButton) {
        dragging_ = true;
        dragOffset_ = event->globalPos() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent* event)
{
    if (dragging_ && (event->buttons() & Qt::LeftButton)) {
        const QPoint desired = event->globalPos() - dragOffset_;
        QScreen* screen = QGuiApplication::screenAt(event->globalPos());
        if (screen == nullptr) screen = QGuiApplication::screenAt(frameGeometry().center());
        if (screen == nullptr) screen = QGuiApplication::primaryScreen();
        move(screen == nullptr ? desired : WindowPlacement::clamp(
            screen->availableGeometry(), frameGeometry().size(), desired));
        event->accept();
        return;
    }
    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent* event)
{
    dragging_ = false;
    QMainWindow::mouseReleaseEvent(event);
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    actionReveal_->setActive(true);
    inputReveal_->setActive(true);
    attachments_->reposition();
    actionReveal_->showTemporarily();
    inputReveal_->showTemporarily();
}

void MainWindow::hideEvent(QHideEvent* event)
{
    actionReveal_->setActive(false);
    inputReveal_->setActive(false);
    replyBubble_->hide();
    errorBanner_->hide();
    QMainWindow::hideEvent(event);
}

} // namespace zhu_screen_pet
