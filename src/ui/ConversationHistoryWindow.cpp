#include "ui/ConversationHistoryWindow.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>

#include "infrastructure/DesktopWindowPolicy.h"

namespace zhu_screen_pet {

ConversationHistoryWindow::ConversationHistoryWindow(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setObjectName(QStringLiteral("conversationHistoryWindow"));
    setAttribute(Qt::WA_StyledBackground, true);
    setWindowTitle(QStringLiteral("会话历史"));
    DesktopWindowPolicy::apply(this, {true, true, false, true, true, false});
    resize(760, 620);
    setMinimumSize(520, 420);
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
    root->addLayout(header);
    root->addWidget(scrollArea_, 1);

    connect(close, &QPushButton::clicked, this, &QWidget::hide);
    // 只能由会话列表主动打开，不能随父级窗口首次显示。
    hide();
}

QString ConversationHistoryWindow::conversationId() const
{
    return conversationId_;
}

void ConversationHistoryWindow::setPetAvatarPath(const QString& path)
{
    petAvatarPath_ = path.trimmed();
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
    bubble->setMaximumWidth(520);
    bubble->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    if (role == MessageRole::Assistant) {
        auto* avatar = new QLabel(row);
        avatar->setObjectName(QStringLiteral("petAvatar"));
        avatar->setAlignment(Qt::AlignCenter);
        avatar->setFixedSize(38, 38);
        avatar->setStyleSheet(QStringLiteral(
            "background:#6ea8f7;color:white;border-radius:19px;font-weight:600;"));
        const QPixmap pixmap(petAvatarPath_);
        if (!pixmap.isNull()) {
            avatar->setPixmap(pixmap.scaled(38, 38, Qt::KeepAspectRatio,
                                             Qt::SmoothTransformation));
            avatar->setStyleSheet(QStringLiteral("background:transparent;border-radius:19px;"));
        } else {
            avatar->setText(QStringLiteral("宠"));
        }
        bubble->setObjectName(QStringLiteral("assistantMessageBubble"));
        bubble->setStyleSheet(QStringLiteral(
            "background:#79adf3;color:#172d52;border-radius:15px;padding:10px 13px;"));
        layout->addWidget(avatar, 0, Qt::AlignTop);
        layout->addWidget(bubble, 0, Qt::AlignTop);
        layout->addStretch();
    } else if (role == MessageRole::User) {
        bubble->setObjectName(QStringLiteral("userMessageBubble"));
        bubble->setStyleSheet(QStringLiteral(
            "background:#a88cf5;color:#24184f;border-radius:15px;padding:10px 13px;"));
        layout->addStretch();
        layout->addWidget(bubble, 0, Qt::AlignTop);
    } else {
        bubble->setObjectName(QStringLiteral("systemMessageBubble"));
        bubble->setStyleSheet(QStringLiteral(
            "background:#eef2f8;color:#667085;border-radius:12px;padding:8px 12px;"));
        layout->addStretch();
        layout->addWidget(bubble, 0, Qt::AlignTop);
        layout->addStretch();
    }
    messageLayout_->addWidget(row);
    return bubble;
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
