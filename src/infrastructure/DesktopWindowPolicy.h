#pragma once

#include <QString>

class QWidget;

namespace zhu_screen_pet {

/** 可组合的桌面窗口策略；桌宠、气泡和工具栏可按需要选择不同组合。 */
struct DesktopWindowOptions
{
    bool frameless = true;
    bool translucentBackground = true;
    bool alwaysOnTop = true;
    bool showInTaskbar = false;
    bool acceptFocus = true;
    bool mouseInputTransparent = false;
};

/** 封装 Qt/Windows 顶层窗口标志，避免 UI 组件直接散落原生窗口调用。 */
class DesktopWindowPolicy final
{
public:
    /** 应用完整策略；应在窗口第一次 show() 前调用。 */
    static bool apply(QWidget* window, const DesktopWindowOptions& options,
                      QString* errorMessage = nullptr);
    /** 运行时切换鼠标穿透；透明区域需要点击桌面时使用。 */
    static bool setMouseInputTransparent(QWidget* window, bool enabled,
                                         QString* errorMessage = nullptr);
};

} // namespace zhu_screen_pet
