#include "infrastructure/TrayController.h"

#include <QAction>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QWidget>

namespace zhu_screen_pet {

TrayController::TrayController(QObject* parent)
    : QObject(parent), trayIcon_(new QSystemTrayIcon(this))
{
}

void TrayController::initialize(QWidget* window, const QIcon& icon)
{
    trayIcon_->setIcon(icon);
    trayIcon_->setToolTip(QStringLiteral("小珠看着你"));

    auto* menu = new QMenu(window);
    auto* showAction = menu->addAction(QStringLiteral("显示窗口"));
    auto* settingsAction = menu->addAction(QStringLiteral("设置"));
    auto* conversationsAction = menu->addAction(QStringLiteral("会话管理"));
    menu->addSeparator();
    auto* quitAction = menu->addAction(QStringLiteral("退出"));
    trayIcon_->setContextMenu(menu);

    connect(showAction, &QAction::triggered, this, &TrayController::showRequested);
    connect(settingsAction, &QAction::triggered, this, &TrayController::settingsRequested);
    connect(conversationsAction, &QAction::triggered,
            this, &TrayController::conversationsRequested);
    connect(quitAction, &QAction::triggered, this, &TrayController::quitRequested);
    connect(trayIcon_, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger
                    || reason == QSystemTrayIcon::DoubleClick) {
                    emit showRequested();
                }
            });
}

void TrayController::show() { trayIcon_->show(); }
void TrayController::hide() { trayIcon_->hide(); }

} // namespace zhu_screen_pet
