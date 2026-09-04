#include "ui/ActionPanel.h"

#include <QPushButton>
#include <QVBoxLayout>

#include "infrastructure/DesktopWindowPolicy.h"

namespace zhu_screen_pet {

ActionPanel::ActionPanel(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setObjectName(QStringLiteral("actionPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    DesktopWindowPolicy::apply(this, {true, true, true, false, false, false});
    setStyleSheet(QStringLiteral(
        "QWidget#actionPanel{background:transparent;border:none;}"
        "QPushButton{color:#32466f;background:#e7f0ff;border:1px solid #c5d7f2;"
        "border-radius:16px;padding:10px 14px;text-align:left;}"
        "QPushButton:hover{background:#cfe0ff;color:#213b70;}"));
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
    connect(close, &QPushButton::clicked, this, &ActionPanel::closeRequested);
    connect(minimize, &QPushButton::clicked, this, &ActionPanel::minimizeRequested);
    connect(settings, &QPushButton::clicked, this, &ActionPanel::settingsRequested);
    connect(conversations, &QPushButton::clicked, this, &ActionPanel::conversationsRequested);
    adjustSize();
}

} // namespace zhu_screen_pet
