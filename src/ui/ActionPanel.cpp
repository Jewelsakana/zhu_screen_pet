#include "ui/ActionPanel.h"

#include <QApplication>
#include <QFontMetrics>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QtMath>

#include "infrastructure/DesktopWindowPolicy.h"
#include "ui/UiScaleMetrics.h"

namespace zhu_screen_pet {

ActionPanel::ActionPanel(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setObjectName(QStringLiteral("actionPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    DesktopWindowPolicy::apply(this, {true, true, true, false, false, false});
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(7);
    auto add = [this, layout](const QString& text, const QString& name) {
        auto* button = new QPushButton(text, this);
        button->setObjectName(name);
        button->setMinimumHeight(38);
        layout->addWidget(button);
        return button;
    };
    auto* close = add(QStringLiteral("✕  关闭"), QStringLiteral("petCloseButton"));
    auto* minimize = add(QStringLiteral("—  最小化"), QStringLiteral("petMinimizeButton"));
    auto* settings = add(QStringLiteral("⚙  设置"), QStringLiteral("settingsButton"));
    auto* conversations = add(QStringLiteral("☰  会话管理"), QStringLiteral("conversationManagerButton"));
    screenCapture_ = add(QStringLiteral("截图：已关闭"),
                         QStringLiteral("screenCaptureToggleButton"));
    screenCapture_->setCheckable(true);
    auto* shop = add(QStringLiteral("🛍  商店"), QStringLiteral("shopButton"));
    auto* backpack = add(QStringLiteral("🎒  背包"), QStringLiteral("backpackButton"));
    captureOnChat_ = add(QStringLiteral("随消息附图：不可用"),
                         QStringLiteral("captureOnChatToggleButton"));
    captureOnChat_->setCheckable(true);
    captureOnChat_->setEnabled(false);
    connect(close, &QPushButton::clicked, this, &ActionPanel::closeRequested);
    connect(minimize, &QPushButton::clicked, this, &ActionPanel::minimizeRequested);
    connect(settings, &QPushButton::clicked, this, &ActionPanel::settingsRequested);
    connect(conversations, &QPushButton::clicked, this, &ActionPanel::conversationsRequested);
    connect(screenCapture_, &QPushButton::toggled, this, &ActionPanel::screenCaptureToggled);
    connect(shop, &QPushButton::clicked, this, &ActionPanel::shopRequested);
    connect(backpack, &QPushButton::clicked, this, &ActionPanel::backpackRequested);
    connect(captureOnChat_, &QPushButton::toggled, this, &ActionPanel::captureOnChatToggled);
    setUiScalePercent(100);
}

void ActionPanel::setUiScalePercent(int percent)
{
    uiScalePercent_ = percent;
    const UiScaleMetrics metrics(percent);
    const int radius = metrics.scaled(16, 7);
    const int verticalPadding = metrics.scaled(10, 5);
    const int horizontalPadding = metrics.scaled(14, 8);
    setStyleSheet(QStringLiteral(
        "QWidget#actionPanel{background:transparent;border:none;}"
        "QPushButton{color:#32466f;background:#e7f0ff;border:1px solid #c5d7f2;"
        "border-radius:%1px;padding:%2px %3px;text-align:left;}"
        "QPushButton:checked{background:#9fc5ff;color:#17345f;border-color:#79adf3;}"
        "QPushButton:disabled{background:#edf0f5;color:#8b93a1;border-color:#d8dde6;}"
        "QPushButton:hover{background:#cfe0ff;color:#213b70;}")
        .arg(radius).arg(verticalPadding).arg(horizontalPadding));
    if (auto* box = qobject_cast<QVBoxLayout*>(layout())) {
        box->setContentsMargins(metrics.scaled(8), metrics.scaled(8),
                                metrics.scaled(8), metrics.scaled(8));
        box->setSpacing(metrics.scaled(7));
    }
    const QFont buttonFont = metrics.readableFont(QApplication::font());
    for (QPushButton* button : findChildren<QPushButton*>()) {
        button->setFont(buttonFont);
        const int readableHeight = QFontMetrics(buttonFont).height()
            + verticalPadding * 2 + 2;
        button->setMinimumHeight(qMax(metrics.scaled(38), readableHeight));
    }
    setMinimumSize(QSize(0, 0));
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    adjustSize();
    setFixedSize(sizeHint());
}

void ActionPanel::setCaptureOptions(bool screenCaptureEnabled, bool captureOnChat)
{
    const QSignalBlocker screenBlocker(screenCapture_);
    const QSignalBlocker chatBlocker(captureOnChat_);
    screenCapture_->setChecked(screenCaptureEnabled);
    screenCapture_->setText(screenCaptureEnabled
        ? QStringLiteral("截图：已开启") : QStringLiteral("截图：已关闭"));
    captureOnChat_->setEnabled(screenCaptureEnabled);
    captureOnChat_->setChecked(screenCaptureEnabled && captureOnChat);
    captureOnChat_->setText(!screenCaptureEnabled
        ? QStringLiteral("随消息附图：不可用")
        : (captureOnChat ? QStringLiteral("随消息附图：已开启")
                         : QStringLiteral("随消息附图：已关闭")));
    setUiScalePercent(uiScalePercent_);
}

} // namespace zhu_screen_pet
