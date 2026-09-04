#pragma once

#include <QPoint>
#include <QRect>
#include <QSize>
#include <QVector>

namespace zhu_screen_pet {

/** 附属窗口相对锚点窗口的首选方向。 */
enum class AttachmentSide
{
    Left,
    Right,
    Above,
    Below
};

/** 附属窗口沿锚点边缘的对齐方式。 */
enum class AttachmentAlignment
{
    Start,
    Center,
    End
};

/** 一次附属窗口定位请求。 */
struct WindowPlacementRequest
{
    QRect availableGeometry;
    QRect anchorGeometry;
    QSize windowSize;
    AttachmentSide preferredSide = AttachmentSide::Right;
    AttachmentAlignment alignment = AttachmentAlignment::Center;
    int gap = 8;
};

/** 定位结果；actualSide 可能因屏幕边界与 preferredSide 不同。 */
struct WindowPlacementResult
{
    QPoint position;
    AttachmentSide actualSide = AttachmentSide::Right;
    bool flipped = false;
};

/** 从同一锚点向左或向右依次展开的一组顶层窗口。 */
struct HorizontalWindowChainRequest
{
    QRect availableGeometry;
    QRect anchorGeometry;
    QVector<QSize> windowSizes;
    AttachmentSide preferredSide = AttachmentSide::Right;
    int gap = 12;
};

/** 窗口位置顺序与请求尺寸一致；窗口链空间不足时整体换边。 */
struct HorizontalWindowChainResult
{
    QVector<QPoint> positions;
    AttachmentSide actualSide = AttachmentSide::Right;
    bool flipped = false;
};

/** 纯几何窗口定位服务，不依赖具体 QWidget，便于多屏和边缘场景测试。 */
class WindowPlacement final
{
public:
    static WindowPlacementResult adjacent(const WindowPlacementRequest& request);
    static HorizontalWindowChainResult horizontalChain(
        const HorizontalWindowChainRequest& request);
    static QPoint clamp(const QRect& availableGeometry, const QSize& windowSize,
                        const QPoint& desiredPosition);
    static AttachmentSide opposite(AttachmentSide side);
};

} // namespace zhu_screen_pet
