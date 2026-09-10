#pragma once

#include <QObject>

namespace zhu_screen_pet {

/** Windows 全局输入观察器：只报告按键/点击事件，不采集按键内容、坐标或窗口信息。 */
class InputActivityMonitor final : public QObject
{
    Q_OBJECT

public:
    explicit InputActivityMonitor(QObject* parent = nullptr);
    ~InputActivityMonitor() override;

    bool start(QString* errorMessage = nullptr);
    void stop();
    bool isRunning() const;
    bool isSupported() const;

signals:
    void keyboardInputDetected();
    void mouseInputDetected();

private:
    void* keyboardHook_ = nullptr;
    void* mouseHook_ = nullptr;
};

} // namespace zhu_screen_pet
