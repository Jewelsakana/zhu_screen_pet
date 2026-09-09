#include "ui/ConversationHistoryWindow.h"

#include <QGuiApplication>
#include <QApplication>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

#include "infrastructure/DesktopWindowPolicy.h"
#include "infrastructure/WindowPlacement.h"
#include "ui/UiScaleMetrics.h"

namespace zhu_screen_pet {

ConversationHistoryWindow::ConversationHistoryWindow(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setObjectName(QStringLiteral("conversationHistoryWindow"));
    setAttribute(Qt::WA_StyledBackground, true);
    setWindowTitle(QStringLiteral("会话历史"));
    DesktopWindowPolicy::apply(this, {true, true, false, true, true, false});
    const QRect available = QGuiApplication::primaryScreen()
        ? QGuiApplication::primaryScreen()->availableGeometry()
        : QRect(0, 0, 1920, 1080);
    resize(WindowPlacement::scaleForScreen(QSize(620, 540), available));
    setMinimumSize(WindowPlacement::scaleForScreen(QSize(460, 390), available));
    setStyleSheet(QStringLiteral(
        "QWidget#conversationHistoryWindow{background:#fffaf0;border:1px solid #b8c9e8;border-radius:24px;}"
        "QLabel#conversationHistoryTitle{color:#26375d;font-size:18px;font-weight:600;}"
        "QScrollArea{background:#fffaf0;border:none;border-radius:16px;}"
        "QWidget#historyMessageContainer{background:#fffaf0;border-radius:16px;}"
        "QPushButton{background:#e5efff;color:#36558f;border:none;border-radius:10px;padding:7px 12px;}"
        "QPushButton:hover{background:#ccdeff;}"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(18, 14, 18, 18);
    auto* header = new QHBoxLayout();
    title_ = new QLabel(QStringLiteral("会话历史"), this);
    title_->setObjectName(QStringLiteral("conversationHistoryTitle"));
    auto* close = new QPushButton(QStringLiteral("关闭"), this);
    close->setObjectName(QStringLiteral("conversationHistoryClose"));
    header->addWidget(title_);
    header->addStretch();
    header->addWidget(close);

    scrollArea_ = new QScrollArea(this);
    scrollArea_->setObjectName(QStringLiteral("conversationHistoryScrollArea"));
    scrollArea_->setWidgetResizable(true);
    scrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    messageContainer_ = new QWidget(scrollArea_);
    messageContainer_->setObjectName(QStringLiteral("historyMessageContainer"));
    messageLayout_ = new QVBoxLayout(messageContainer_);
    messageLayout_->setContentsMargins(16, 18, 16, 18);
    messageLayout_->setSpacing(16);
    messageLayout_->setAlignment(Qt::AlignTop);
    scrollArea_->setWidget(messageContainer_);
    scrollTimer_ = new QTimer(this);
    scrollTimer_->setObjectName(QStringLiteral("conversationHistoryScrollTimer"));
    scrollTimer_->setSingleShot(true);
    connect(scrollTimer_, &QTimer::timeout, this, [this]() {
        scrollArea_->verticalScrollBar()->setValue(scrollArea_->verticalScrollBar()->maximum());
    });
    connect(scrollArea_->verticalScrollBar(), &QScrollBar::valueChanged, this,
            [this](int value) {
                if (!isVisible() || !hasOlderMessages_ || olderRequestPending_
                    || value != scrollArea_->verticalScrollBar()->minimum()) return;
                olderRequestPending_ = true;
                emit olderMessagesRequested();
            });
    root->addLayout(header);
    root->addWidget(scrollArea_, 1);

    connect(close, &QPushButton::clicked, this, &QWidget::hide);
    setUiScalePercent(100);
    // 只能由会话列表主动打开，不能随父级窗口首次显示。
    hide();
}

void ConversationHistoryWindow::replaceMessagesPreservingPosition(
    const QVector<ConversationMessage>& messages, bool hasOlderMessages)
{
    QScrollBar* scrollBar = scrollArea_->verticalScrollBar();
    const int oldMaximum = scrollBar->maximum();
    const int oldValue = scrollBar->value();
    if (scrollTimer_->isActive()) scrollTimer_->stop();
    clearMessages();
    for (const ConversationMessage& message : messages) {
        addMessageBubble(message.message.role, message.message.content);
    }
    hasOlderMessages_ = hasOlderMessages;
    olderRequestPending_ = false;
    QTimer::singleShot(0, this, [this, oldMaximum, oldValue]() {
        QScrollBar* current = scrollArea_->verticalScrollBar();
        current->setValue(oldValue + qMax(0, current->maximum() - oldMaximum));
    });
}

void ConversationHistoryWindow::setHasOlderMessages(bool value)
{
    hasOlderMessages_ = value;
    olderRequestPending_ = false;
}

QString ConversationHistoryWindow::conversationId() const
{
    return conversationId_;
}

void ConversationHistoryWindow::setPetAvatarPath(const QString& path)
{
    petAvatarPath_ = path.trimmed();
}

void ConversationHistoryWindow::setUiScalePercent(int percent)
{
    uiScalePercent_ = percent;
    const UiScaleMetrics metrics(percent);
    const int frameRadius = metrics.scaled(24, 10);
    const int contentRadius = metrics.scaled(16, 7);
    const int buttonRadius = metrics.scaled(10, 6);
    const int buttonPaddingY = metrics.scaled(7, 5);
    const int buttonPaddingX = metrics.scaled(12, 8);
    setStyleSheet(QStringLiteral(
        "QWidget#conversationHistoryWindow{background:#fffaf0;border:1px solid #b8c9e8;"
        "border-radius:%1px;}"
        "QLabel#conversationHistoryTitle{color:#26375d;font-weight:600;}"
        "QScrollArea{background:#fffaf0;border:none;border-radius:%2px;}"
        "QWidget#historyMessageContainer{background:#fffaf0;border-radius:%2px;}"
        "QPushButton{background:#e5efff;color:#36558f;border:none;border-radius:%3px;"
        "padding:%4px %5px;}"
        "QPushButton:hover{background:#ccdeff;}")
        .arg(frameRadius).arg(contentRadius).arg(buttonRadius)
        .arg(buttonPaddingY).arg(buttonPaddingX));
    if (auto* root = qobject_cast<QVBoxLayout*>(layout())) {
        root->setContentsMargins(metrics.scaled(18), metrics.scaled(14),
                                 metrics.scaled(18), metrics.scaled(18));
        root->setSpacing(metrics.scaled(6, 4));
    }
    messageLayout_->setContentsMargins(metrics.scaled(16), metrics.scaled(18),
                                        metrics.scaled(16), metrics.scaled(18));
    messageLayout_->setSpacing(metrics.scaled(16, 6));
    const QFont readableFont = metrics.readableFont(QApplication::font());
    QFont titleFont = readableFont;
    titleFont.setBold(true);
    titleFont.setPointSizeF(qMax(readableFont.pointSizeF(),
                                 11.0 * qMax(metrics.factor(), 0.75)));
    title_->setFont(titleFont);
    const int textHeight = QFontMetrics(readableFont).height();
    for (QPushButton* button : findChildren<QPushButton*>()) {
        button->setFont(readableFont);
        button->setMinimumWidth(0);
        button->setMinimumHeight(qMax(metrics.scaled(32),
                                      textHeight + buttonPaddingY * 2 + 2));
        button->setMinimumWidth(button->sizeHint().width());
    }
    const int avatarSize = metrics.scaled(38, 24);
    for (QLabel* avatar : findChildren<QLabel*>(QStringLiteral("petAvatar"))) {
        avatar->setFixedSize(avatarSize, avatarSize);
        avatar->setFont(readableFont);
        const QPixmap pixmap(petAvatarPath_);
        if (!pixmap.isNull()) {
            avatar->setPixmap(pixmap.scaled(avatarSize, avatarSize, Qt::KeepAspectRatio,
                                             Qt::SmoothTransformation));
        }
    }
    for (QWidget* row : findChildren<QWidget*>(QStringLiteral("historyMessageRow"))) {
        if (auto* rowLayout = qobject_cast<QHBoxLayout*>(row->layout())) {
            rowLayout->setSpacing(metrics.scaled(10, 5));
        }
    }
    for (const QString& name : {QStringLiteral("assistantMessageBubble"),
                                QStringLiteral("userMessageBubble"),
                                QStringLiteral("systemMessageBubble")}) {
        for (QLabel* bubble : findChildren<QLabel*>(name)) {
            bubble->setFont(readableFont);
            applyMessageBubbleStyle(bubble);
        }
    }
    const QSize desiredMinimum = metrics.scaledForWindow(this, QSize(460, 390));
    const QSize desiredSize = metrics.scaledForWindow(this, QSize(620, 540));
    setMinimumSize(QSize(0, 0));
    const QSize contentMinimum = minimumSizeHint();
    setMinimumSize(desiredMinimum.expandedTo(contentMinimum));
    resize(desiredSize.expandedTo(minimumSize()));
}

void ConversationHistoryWindow::setConversation(
    const QString& id, const QString& title, const QVector<ConversationMessage>& messages)
{
    conversationId_ = id;
    title_->setText(title.isEmpty() ? QStringLiteral("会话历史") : title);
    setWindowTitle(title_->text());
    clearMessages();
    for (const ConversationMessage& message : messages) {
        appendMessage(message.message.role, message.message.content);
    }
}

void ConversationHistoryWindow::appendMessage(MessageRole role, const QString& content)
{
    if (content.isEmpty()) return;
    addMessageBubble(role, content);
    scrollToBottom();
}

void ConversationHistoryWindow::beginAssistantReply()
{
    if (streamingBubble_ != nullptr) return;
    streamingBubble_ = addMessageBubble(MessageRole::Assistant, QString{});
    scrollToBottom();
}

void ConversationHistoryWindow::appendAssistantDelta(const QString& delta)
{
    beginAssistantReply();
    streamingBubble_->setText(streamingBubble_->text() + delta);
    scrollToBottom();
}

void ConversationHistoryWindow::finishAssistantReply(const QString& content)
{
    if (streamingBubble_ == nullptr) {
        appendMessage(MessageRole::Assistant, content);
        return;
    }
    if (!content.isEmpty()) streamingBubble_->setText(content);
    streamingBubble_ = nullptr;
    scrollToBottom();
}

QLabel* ConversationHistoryWindow::addMessageBubble(MessageRole role, const QString& content)
{
    auto* row = new QWidget(messageContainer_);
    row->setObjectName(QStringLiteral("historyMessageRow"));
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto* bubble = new QLabel(content, row);
    bubble->setTextFormat(Qt::PlainText);
    bubble->setWordWrap(true);
    bubble->setTextInteractionFlags(Qt::TextSelectableByMouse);
    const UiScaleMetrics metrics(uiScalePercent_);
    const int avatarSize = metrics.scaled(38, 24);
    const QFont readableFont = metrics.readableFont(QApplication::font());
    bubble->setFont(readableFont);
    bubble->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    if (role == MessageRole::Assistant) {
        auto* avatar = new QLabel(row);
        avatar->setObjectName(QStringLiteral("petAvatar"));
        avatar->setAlignment(Qt::AlignCenter);
        avatar->setFixedSize(avatarSize, avatarSize);
        avatar->setFont(readableFont);
        avatar->setStyleSheet(QStringLiteral(
            "background:#6ea8f7;color:white;border-radius:%1px;font-weight:600;")
            .arg(avatarSize / 2));
        const QPixmap pixmap(petAvatarPath_);
        if (!pixmap.isNull()) {
            avatar->setPixmap(pixmap.scaled(avatarSize, avatarSize, Qt::KeepAspectRatio,
                                             Qt::SmoothTransformation));
            avatar->setStyleSheet(QStringLiteral("background:transparent;border-radius:%1px;")
                                  .arg(avatarSize / 2));
        } else {
            avatar->setText(QStringLiteral("宠"));
        }
        bubble->setObjectName(QStringLiteral("assistantMessageBubble"));
        layout->addWidget(avatar, 0, Qt::AlignTop);
        layout->addWidget(bubble, 0, Qt::AlignTop);
        layout->addStretch();
    } else if (role == MessageRole::User) {
        bubble->setObjectName(QStringLiteral("userMessageBubble"));
        layout->addStretch();
        layout->addWidget(bubble, 0, Qt::AlignTop);
    } else {
        bubble->setObjectName(QStringLiteral("systemMessageBubble"));
        layout->addStretch();
        layout->addWidget(bubble, 0, Qt::AlignTop);
        layout->addStretch();
    }
    applyMessageBubbleStyle(bubble);
    messageLayout_->addWidget(row);
    return bubble;
}

void ConversationHistoryWindow::applyMessageBubbleStyle(QLabel* bubble) const
{
    if (bubble == nullptr) return;
    const UiScaleMetrics metrics(uiScalePercent_);
    const int maximumWidth = metrics.scaled(520, 180);
    bubble->setMaximumWidth(maximumWidth);
    if (bubble->objectName() == QStringLiteral("assistantMessageBubble")) {
        bubble->setStyleSheet(QStringLiteral(
            "background:#79adf3;color:#172d52;border-radius:%1px;padding:%2px %3px;")
            .arg(metrics.scaled(15, 7)).arg(metrics.scaled(10, 6))
            .arg(metrics.scaled(13, 8)));
    } else if (bubble->objectName() == QStringLiteral("userMessageBubble")) {
        bubble->setStyleSheet(QStringLiteral(
            "background:#a88cf5;color:#24184f;border-radius:%1px;padding:%2px %3px;")
            .arg(metrics.scaled(15, 7)).arg(metrics.scaled(10, 6))
            .arg(metrics.scaled(13, 8)));
    } else {
        bubble->setStyleSheet(QStringLiteral(
            "background:#eef2f8;color:#667085;border-radius:%1px;padding:%2px %3px;")
            .arg(metrics.scaled(12, 6)).arg(metrics.scaled(8, 5))
            .arg(metrics.scaled(12, 8)));
    }
}

void ConversationHistoryWindow::clearMessages()
{
    streamingBubble_ = nullptr;
    while (QLayoutItem* item = messageLayout_->takeAt(0)) {
        delete item->widget();
        delete item;
    }
}

void ConversationHistoryWindow::scrollToBottom()
{
    if (!scrollTimer_->isActive()) scrollTimer_->start(0);
}

} // namespace zhu_screen_pet
