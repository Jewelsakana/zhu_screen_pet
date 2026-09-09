#include "ui/ChatInputPanel.h"

#include <QEvent>
#include <QApplication>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QPushButton>
#include <QTextEdit>
#include <QtMath>

#include "infrastructure/DesktopWindowPolicy.h"
#include "ui/UiScaleMetrics.h"

namespace zhu_screen_pet {

ChatInputPanel::ChatInputPanel(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setObjectName(QStringLiteral("chatInputPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    DesktopWindowPolicy::apply(this, {true, true, true, false, true, false});
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 8, 8);
    input_ = new QTextEdit(this);
    input_->setObjectName(QStringLiteral("chatInput"));
    input_->setPlaceholderText(QStringLiteral("和小珠说点什么…  Enter 发送"));
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
    setUiScalePercent(100);
}

QString ChatInputPanel::text() const { return input_->toPlainText().trimmed(); }
void ChatInputPanel::clear() { input_->clear(); }
void ChatInputPanel::focusInput() { input_->setFocus(); }

void ChatInputPanel::setBusy(bool busy)
{
    send_->setEnabled(!busy);
    cancel_->setVisible(busy);
    input_->setEnabled(!busy);
    setUiScalePercent(uiScalePercent_);
}

void ChatInputPanel::setRetryEnabled(bool enabled) { retry_->setEnabled(enabled); }

void ChatInputPanel::setUiScalePercent(int percent)
{
    uiScalePercent_ = percent;
    const UiScaleMetrics metrics(percent);
    const int fieldRadius = metrics.scaled(17, 7);
    const int fieldPadding = metrics.scaled(9, 5);
    const int buttonRadius = metrics.scaled(17, 7);
    const int buttonVerticalPadding = metrics.scaled(8, 5);
    const int buttonHorizontalPadding = metrics.scaled(13, 8);
    setStyleSheet(QStringLiteral(
        "QWidget#chatInputPanel{background:transparent;border:none;}"
        "QTextEdit{color:#26375d;background:#fffdf8;border:1px solid #eadfca;"
        "border-radius:%1px;padding:%2px;}"
        "QPushButton{color:#17345f;background:#79adf3;border:none;border-radius:%3px;"
        "padding:%4px %5px;}"
        "QPushButton#retryButton{background:#a88cf5;color:#24184f;}"
        "QPushButton:disabled{background:#e5e7eb;color:#9ca3af;}")
        .arg(fieldRadius).arg(fieldPadding).arg(buttonRadius)
        .arg(buttonVerticalPadding).arg(buttonHorizontalPadding));
    if (auto* box = qobject_cast<QHBoxLayout*>(layout())) {
        box->setContentsMargins(metrics.scaled(12), metrics.scaled(8),
                                metrics.scaled(8), metrics.scaled(8));
        box->setSpacing(metrics.scaled(6, 4));
    }
    const QFont readableFont = metrics.readableFont(QApplication::font());
    input_->setFont(readableFont);
    const int textHeight = QFontMetrics(readableFont).height();
    input_->setFixedHeight(qMax(metrics.scaled(54), textHeight + fieldPadding * 2 + 8));
    for (QPushButton* button : {retry_, cancel_, send_}) {
        button->setFont(readableFont);
        button->setMinimumWidth(0);
        button->setMinimumHeight(qMax(metrics.scaled(38),
            textHeight + buttonVerticalPadding * 2 + 2));
        button->setMinimumWidth(button->sizeHint().width());
    }
    input_->setMinimumWidth(metrics.scaled(260, 120));
    setMinimumSize(QSize(0, 0));
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    adjustSize();
    const int contentWidth = minimumSizeHint().width();
    setFixedSize(qMax(metrics.scaled(520), contentWidth), sizeHint().height());
}

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
