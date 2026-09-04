#pragma once

#include <QPoint>
#include <QRect>
#include <QSize>

class QScreen;
class QWidget;

namespace zhu_screen_pet {

class SettingsRepository;

/** 负责窗口位置持久化及屏幕工作区边界约束。 */
class WindowManager final
{
public:
    /** 创建窗口管理器；settings 为空时只执行边界计算，不持久化位置。 */
    explicit WindowManager(SettingsRepository* settings = nullptr);

    /** 从配置恢复窗口位置，并将其限制在可见工作区内。 */
    void restore(QWidget* window) const;
    /** 使用独立键恢复指定窗口；适用于桌宠、会话面板等多个顶层窗口。 */
    void restore(QWidget* window, const QString& windowId,
                 const QPoint& fallback = QPoint(100, 100)) const;
    /** 保存窗口当前位置到配置。 */
    bool save(const QWidget* window, QString* errorMessage = nullptr);
    /** 使用独立键保存指定窗口的位置。 */
    bool save(const QWidget* window, const QString& windowId,
              QString* errorMessage = nullptr);
    /** 将窗口当前位置限制在当前显示器工作区内。 */
    void clamp(QWidget* window) const;

    /** 根据工作区、窗口尺寸和目标位置计算合法位置。 */
    static QPoint clampPosition(const QRect& availableGeometry,
                                const QSize& windowSize,
                                const QPoint& desiredPosition);

private:
    SettingsRepository* settings_ = nullptr;
};

} // namespace zhu_screen_pet
