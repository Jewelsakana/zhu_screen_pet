#pragma once

#include <QObject>

class QIcon;
class QWidget;
class QSystemTrayIcon;

namespace zhu_screen_pet {

/** 系统托盘控制器，提供显示窗口和退出等基础操作。 */
class TrayController final : public QObject
{
    Q_OBJECT

public:
    /** 创建托盘控制器。 */
    explicit TrayController(QObject* parent = nullptr);

    /** 绑定窗口和图标，并创建托盘菜单。 */
    void initialize(QWidget* window, const QIcon& icon);
    /** 显示托盘图标。 */
    void show();
    /** 隐藏托盘图标。 */
    void hide();

signals:
    /** 用户请求显示主窗口时发出。 */
    void showRequested();
    /** 用户从托盘直接请求打开设置。 */
    void settingsRequested();
    /** 用户从托盘直接请求打开会话管理。 */
    void conversationsRequested();
    /** 用户请求退出应用时发出。 */
    void quitRequested();

private:
    QSystemTrayIcon* trayIcon_ = nullptr;
};

} // namespace zhu_screen_pet
