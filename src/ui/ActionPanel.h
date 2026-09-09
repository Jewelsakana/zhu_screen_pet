#pragma once

#include <QWidget>

class QPushButton;

namespace zhu_screen_pet {

/** 桌宠右侧悬浮操作栏。 */
class ActionPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit ActionPanel(QWidget* parent = nullptr);
    void setUiScalePercent(int percent);
    void setCaptureOptions(bool screenCaptureEnabled, bool captureOnChat);

signals:
    void closeRequested();
    void minimizeRequested();
    void settingsRequested();
    void conversationsRequested();
    void screenCaptureToggled(bool enabled);
    void captureOnChatToggled(bool enabled);

private:
    QPushButton* screenCapture_ = nullptr;
    QPushButton* captureOnChat_ = nullptr;
    int uiScalePercent_ = 100;
};

} // namespace zhu_screen_pet
