#include "ui/ConversationWindow.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "app/ConversationController.h"
#include "infrastructure/DesktopWindowPolicy.h"
#include "ui/ConversationHistoryWindow.h"

namespace zhu_screen_pet {

ConversationWindow::ConversationWindow(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setObjectName(QStringLiteral("conversationWindow"));
    setAttribute(Qt::WA_StyledBackground, true);
    setWindowTitle(QStringLiteral("会话列表"));
    DesktopWindowPolicy::apply(this, {true, true, false, true, true, false});
    resize(380, 620);
    setMinimumSize(320, 440);
    setStyleSheet(QStringLiteral(
        "QWidget#conversationWindow{background:#dcecff;border:1px solid #9bb9ea;border-radius:24px;}"
        "QLabel#conversationListTitle{color:#26375d;font-size:20px;font-weight:600;}"
        "QListWidget{background:transparent;border:none;border-radius:16px;outline:none;padding:2px;}"
        "QListWidget::item{background:#fffaf0;color:#26375d;border:1px solid #eadfca;"
        "border-radius:14px;margin:6px 2px;padding:14px;}"
        "QListWidget::item:hover{background:#fff2d8;border-color:#adc6ee;}"
        "QListWidget::item:selected{background:#f7edff;color:#4b347b;border-color:#a88cf5;}"
        "QPushButton{background:#fffaf0;color:#36558f;border:1px solid #c9d9f1;"
        "border-radius:11px;padding:8px 12px;}"
        "QPushButton:hover{background:#fff1d7;}"
        "QPushButton#newConversationButton{background:#79adf3;color:#17345f;border:none;}"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 16);
    root->setSpacing(12);
    auto* header = new QHBoxLayout();
    auto* heading = new QLabel(QStringLiteral("会话列表"), this);
    heading->setObjectName(QStringLiteral("conversationListTitle"));
    auto* close = new QPushButton(QStringLiteral("关闭"), this);
    close->setObjectName(QStringLiteral("conversationWindowClose"));
    header->addWidget(heading);
    header->addStretch();
    header->addWidget(close);

    auto* newConversation = new QPushButton(QStringLiteral("＋ 新建会话"), this);
    newConversation->setObjectName(QStringLiteral("newConversationButton"));
    list_ = new QListWidget(this);
    list_->setObjectName(QStringLiteral("conversationList"));
    list_->setSpacing(2);
    list_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* archive = new QPushButton(QStringLiteral("归档"), this);
    archive->setObjectName(QStringLiteral("archiveConversationButton"));
    auto* remove = new QPushButton(QStringLiteral("删除"), this);
    remove->setObjectName(QStringLiteral("deleteConversationButton"));
    auto* archived = new QPushButton(QStringLiteral("管理归档"), this);
    archived->setObjectName(QStringLiteral("manageArchivedButton"));
    auto* actions = new QHBoxLayout();
    actions->addWidget(archive);
    actions->addWidget(remove);
    actions->addWidget(archived);

    root->addLayout(header);
    root->addWidget(newConversation);
    root->addWidget(list_, 1);
    root->addLayout(actions);

    connect(close, &QPushButton::clicked, this, &QWidget::hide);
    connect(list_, &QListWidget::itemClicked, this, &ConversationWindow::openConversationItem);
    connect(list_, &QListWidget::itemActivated, this, &ConversationWindow::openConversationItem);
    connect(newConversation, &QPushButton::clicked, this, &ConversationWindow::createConversation);
    connect(archive, &QPushButton::clicked, this, &ConversationWindow::archiveConversation);
    connect(remove, &QPushButton::clicked, this, &ConversationWindow::deleteConversation);
    connect(archived, &QPushButton::clicked, this, &ConversationWindow::manageArchived);
    hide();
}

ConversationWindow::~ConversationWindow()
{
    delete historyWindow_.data();
}

void ConversationWindow::setController(ConversationController* controller)
{
    if (controller_ == controller) return;
    if (controller_ != nullptr) disconnect(controller_, nullptr, this, nullptr);
    hideAllHistoryWindows();
    controller_ = controller;
    if (controller_ == nullptr) return;
    connect(controller_, &ConversationController::conversationsChanged,
            this, &ConversationWindow::refreshList);
    connect(controller_, &ConversationController::currentConversationChanged, this,
            [this](const QString& id, const QString& title,
                   const QVector<ConversationMessage>& messages) {
                setConversation(id, title, messages);
                refreshList(controller_->conversations());
            });
    refreshList(controller_->conversations());
    setConversation(controller_->currentConversationId(), controller_->currentConversationTitle(),
                    controller_->currentConversationMessages());
}

void ConversationWindow::setConversation(const QString& id, const QString& title,
                                         const QVector<ConversationMessage>& messages)
{
    currentId_ = id;
    currentTitle_ = title;
    currentMessages_ = messages;
    streamingAssistantContent_.clear();
    streamingAssistantActive_ = false;
    if (historyWindow_ != nullptr && historyWindow_->isVisible()) {
        historyWindow_->setConversation(id, title, messages);
    }
}

void ConversationWindow::appendMessage(MessageRole role, const QString& content)
{
    if (content.isEmpty()) return;
    ConversationMessage message;
    message.conversationId = currentId_;
    message.message = Message::create(role, content);
    currentMessages_.append(message);
    if (ConversationHistoryWindow* history = currentHistoryWindow();
        history != nullptr && history->isVisible()) history->appendMessage(role, content);
}

void ConversationWindow::beginAssistantReply()
{
    if (streamingAssistantActive_) return;
    streamingAssistantActive_ = true;
    streamingAssistantContent_.clear();
    if (ConversationHistoryWindow* history = currentHistoryWindow();
        history != nullptr && history->isVisible()) history->beginAssistantReply();
}

void ConversationWindow::appendAssistantDelta(const QString& delta)
{
    if (!streamingAssistantActive_) beginAssistantReply();
    streamingAssistantContent_ += delta;
    if (ConversationHistoryWindow* history = currentHistoryWindow();
        history != nullptr && history->isVisible()) history->appendAssistantDelta(delta);
}

void ConversationWindow::finishAssistantReply(const QString& content)
{
    const QString finalContent = content.isEmpty() ? streamingAssistantContent_ : content;
    if (ConversationHistoryWindow* history = currentHistoryWindow();
        history != nullptr && history->isVisible()) history->finishAssistantReply(finalContent);
    if (!finalContent.isEmpty()) {
        ConversationMessage message;
        message.conversationId = currentId_;
        message.message = Message::create(MessageRole::Assistant, finalContent);
        currentMessages_.append(message);
    }
    streamingAssistantContent_.clear();
    streamingAssistantActive_ = false;
}

void ConversationWindow::setConversationAvatarPath(const QString& path)
{
    conversationAvatarPath_ = path;
    if (historyWindow_ != nullptr) historyWindow_->setPetAvatarPath(path);
}

void ConversationWindow::hideAllHistoryWindows()
{
    if (historyWindow_ != nullptr) historyWindow_->hide();
}

ConversationHistoryWindow* ConversationWindow::historyWindow() const
{
    return historyWindow_.data();
}

void ConversationWindow::refreshList(const QVector<Conversation>& conversations)
{
    const QString selected = selectedConversationId();
    QSignalBlocker blocker(list_);
    list_->clear();
    int selectedRow = -1;
    for (int index = 0; index < conversations.size(); ++index) {
        const Conversation& conversation = conversations.at(index);
        auto* item = new QListWidgetItem(conversation.title, list_);
        item->setData(Qt::UserRole, conversation.id);
        item->setSizeHint(QSize(0, 66));
        if (conversation.id == selected || (selected.isEmpty() && conversation.id == currentId_)) {
            selectedRow = index;
        }
    }
    if (selectedRow >= 0) list_->setCurrentRow(selectedRow);
}

void ConversationWindow::openConversationItem(QListWidgetItem* item)
{
    if (controller_ == nullptr || item == nullptr) return;
    const QString id = item->data(Qt::UserRole).toString();
    if (id.isEmpty()) return;
    if (id != controller_->currentConversationId() && !controller_->switchConversation(id)) return;
    showCurrentHistory();
}

void ConversationWindow::showCurrentHistory()
{
    if (currentId_.isEmpty()) return;
    if (historyWindow_ == nullptr) {
        // 历史窗口是真正无父级的顶层窗口，由本列表窗口显式管理生命周期。
        historyWindow_ = new ConversationHistoryWindow();
        historyWindow_->setPetAvatarPath(conversationAvatarPath_);
        connect(historyWindow_, &QObject::destroyed, this, [this]() { historyWindow_ = nullptr; });
    }
    historyWindow_->setConversation(currentId_, currentTitle_, currentMessages_);
    if (streamingAssistantActive_) {
        historyWindow_->beginAssistantReply();
        if (!streamingAssistantContent_.isEmpty()) {
            historyWindow_->appendAssistantDelta(streamingAssistantContent_);
        }
    }
    historyWindow_->show();
    historyWindow_->raise();
    historyWindow_->activateWindow();
    emit historyWindowShown();
}

ConversationHistoryWindow* ConversationWindow::currentHistoryWindow() const
{
    if (historyWindow_ == nullptr || historyWindow_->conversationId() != currentId_) return nullptr;
    return historyWindow_.data();
}

QString ConversationWindow::selectedConversationId() const
{
    return list_ != nullptr && list_->currentItem() != nullptr
        ? list_->currentItem()->data(Qt::UserRole).toString() : QString{};
}

void ConversationWindow::createConversation()
{
    if (controller_ == nullptr) return;
    bool accepted = false;
    const QString title = QInputDialog::getText(this, QStringLiteral("新建会话"),
        QStringLiteral("会话标题："), QLineEdit::Normal, QStringLiteral("新会话"), &accepted).trimmed();
    if (accepted && controller_->createConversation(title)) showCurrentHistory();
}

void ConversationWindow::archiveConversation()
{
    if (controller_ == nullptr) return;
    const QString id = selectedConversationId();
    if (id.isEmpty()) return;
    if (historyWindow_ != nullptr && historyWindow_->conversationId() == id) historyWindow_->hide();
    controller_->archiveConversation(id);
}

void ConversationWindow::deleteConversation()
{
    if (controller_ == nullptr) return;
    const QString id = selectedConversationId();
    if (id.isEmpty()) return;
    if (QMessageBox::question(this, QStringLiteral("删除会话"),
        QStringLiteral("确定永久删除所选会话及全部消息吗？此操作无法撤销。"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    if (historyWindow_ != nullptr && historyWindow_->conversationId() == id) historyWindow_->hide();
    controller_->deleteConversation(id);
}

void ConversationWindow::manageArchived()
{
    if (controller_ == nullptr) return;
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("管理已归档会话"));
    dialog.setStyleSheet(QStringLiteral(
        "QDialog{background:#fffaf0;} QListWidget{background:#dcecff;border:1px solid #b8c9e8;}"
        "QPushButton{background:#e5efff;border:none;border-radius:9px;padding:7px 11px;}"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* list = new QListWidget(&dialog);
    for (const Conversation& conversation : controller_->archivedConversations()) {
        auto* item = new QListWidgetItem(conversation.title, list);
        item->setData(Qt::UserRole, conversation.id);
    }
    auto* remove = new QPushButton(QStringLiteral("永久删除"), &dialog);
    layout->addWidget(list);
    layout->addWidget(remove);
    connect(remove, &QPushButton::clicked, &dialog, [this, list]() {
        QListWidgetItem* item = list->currentItem();
        if (item == nullptr) return;
        if (QMessageBox::question(this, QStringLiteral("永久删除"),
            QStringLiteral("确定删除“%1”吗？").arg(item->text()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes
            && controller_->deleteConversation(item->data(Qt::UserRole).toString())) {
            delete list->takeItem(list->row(item));
        }
    });
    dialog.exec();
}

void ConversationWindow::hideEvent(QHideEvent* event)
{
    hideAllHistoryWindows();
    emit hidden();
    QWidget::hideEvent(event);
}

} // namespace zhu_screen_pet
