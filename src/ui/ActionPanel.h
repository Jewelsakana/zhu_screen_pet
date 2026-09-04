#pragma once

#include <QWidget>

namespace zhu_screen_pet {

/** 桌宠右侧悬浮操作栏。 */
class ActionPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit ActionPanel(QWidget* parent = nullptr);

signals:
    void closeRequested();
    void minimizeRequested();
    void settingsRequested();
    void conversationsRequested();
};

} // namespace zhu_screen_pet
