#include "ui/ErrorBannerWindow.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "infrastructure/DesktopWindowPolicy.h"

namespace zhu_screen_pet {

ErrorBannerWindow::ErrorBannerWindow(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setObjectName(QStringLiteral("errorBanner"));
    setAttribute(Qt::WA_StyledBackground, true);
    DesktopWindowPolicy::apply(this, {true, false, true, false, false, false});
    setStyleSheet(QStringLiteral(
        "QWidget#errorBanner{background:#fffaf0;border:1px solid #dc806b;}"
        "QLabel{color:#702f24;} QPushButton{border:none;color:#8c3c2c;"
        "background:#ffe3da;border-radius:9px;padding:5px 9px;}"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 10, 10, 10);
    message_ = new QLabel(this);
    message_->setObjectName(QStringLiteral("errorBannerMessage"));
    message_->setWordWrap(true);
    auto* actions = new QHBoxLayout();
    retry_ = new QPushButton(QStringLiteral("重试"), this);
    auto* settings = new QPushButton(QStringLiteral("打开设置"), this);
    auto* close = new QPushButton(QStringLiteral("关闭"), this);
    close->setObjectName(QStringLiteral("errorBannerClose"));
    actions->addWidget(retry_);
    actions->addWidget(settings);
    actions->addStretch();
    actions->addWidget(close);
    root->addWidget(message_);
    root->addLayout(actions);
    setFixedWidth(380);
    connect(retry_, &QPushButton::clicked, this, &ErrorBannerWindow::retryRequested);
    connect(settings, &QPushButton::clicked, this, &ErrorBannerWindow::settingsRequested);
    connect(close, &QPushButton::clicked, this, &ErrorBannerWindow::dismiss);
    // 没有活动错误时保持显式隐藏，避免父窗口首次 show() 将空横幅带出。
    hide();
}

void ErrorBannerWindow::showError(const QString& userMessage, bool retryable)
{
    message_->setText(userMessage);
    retry_->setVisible(retryable);
    hasActiveError_ = true;
    adjustSize();
    show();
    raise();
}

void ErrorBannerWindow::dismiss()
{
    hasActiveError_ = false;
    hide();
}

void ErrorBannerWindow::restoreIfActive()
{
    if (hasActiveError_) {
        show();
        raise();
    }
}

QString ErrorBannerWindow::message() const { return message_->text(); }
bool ErrorBannerWindow::hasActiveError() const { return hasActiveError_; }

} // namespace zhu_screen_pet
