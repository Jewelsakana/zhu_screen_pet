#include "ui/ChatInputPanel.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QPushButton>
#include <QTextEdit>

#include "infrastructure/DesktopWindowPolicy.h"

namespace zhu_screen_pet {

ChatInputPanel::ChatInputPanel(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setObjectName(QStringLiteral("chatInputPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    DesktopWindowPolicy::apply(this, {true, true, true, false, true, false});
    setStyleSheet(QStringLiteral(
        "QWidget#chatInputPanel{background:transparent;border:none;}"
        "QTextEdit{color:#26375d;background:#fffdf8;border:1px solid #eadfca;border-radius:17px;padding:9px;}"
        "QPushButton{color:#17345f;background:#79adf3;border:none;border-radius:17px;padding:8px 13px;}"
        "QPushButton#retryButton{background:#a88cf5;color:#24184f;}"
        "QPushButton:disabled{background:#e5e7eb;color:#9ca3af;}"));
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 8, 8);
    input_ = new QTextEdit(this);
    input_->setObjectName(QStringLiteral("chatInput"));
    input_->setPlaceholderText(QStringLiteral("和桌宠说点什么…  Enter 发送"));
    input_->setAcceptRichText(false);
    input_->setFixedHeight(54);
    input_->installEventFilter(this);
    retry_ = new QPushButton(QStringLiteral("重试"), this);
    retry_->setObjectName(QStringLiteral("retryButton"));
    cancel_ = new QPushButton(QStringLiteral("取消"), this);
    cancel_->setObjectName(QStringLiteral("cancelButton"));
    send_ = new QPushButton(QStringLiteral("发送"), this);
    send_->setObjectName(QStringLiteral("sendButton"));
    retry_->setEnabled(false);
    retry_->setMinimumHeight(38);
    cancel_->setMinimumHeight(38);
    send_->setMinimumHeight(38);
    cancel_->setVisible(false);
    layout->addWidget(input_, 1);
    layout->addWidget(retry_);
    layout->addWidget(cancel_);
    layout->addWidget(send_);
    setFixedWidth(520);
    connect(send_, &QPushButton::clicked, this, &ChatInputPanel::sendRequested);
    connect(cancel_, &QPushButton::clicked, this, &ChatInputPanel::cancelRequested);
    connect(retry_, &QPushButton::clicked, this, &ChatInputPanel::retryRequested);
    adjustSize();
}

QString ChatInputPanel::text() const { return input_->toPlainText().trimmed(); }
void ChatInputPanel::clear() { input_->clear(); }
void ChatInputPanel::focusInput() { input_->setFocus(); }

void ChatInputPanel::setBusy(bool busy)
{
    send_->setEnabled(!busy);
    cancel_->setVisible(busy);
    input_->setEnabled(!busy);
}

void ChatInputPanel::setRetryEnabled(bool enabled) { retry_->setEnabled(enabled); }

bool ChatInputPanel::canAutoHide() const
{
    return input_->toPlainText().trimmed().isEmpty() && !input_->hasFocus() && !composing_;
}

bool ChatInputPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == input_ && event->type() == QEvent::InputMethod) {
        composing_ = !static_cast<QInputMethodEvent*>(event)->preeditString().isEmpty();
    }
    if (watched == input_ && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)
            && !(key->modifiers() & Qt::ShiftModifier) && !composing_) {
            emit sendRequested();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace zhu_screen_pet
