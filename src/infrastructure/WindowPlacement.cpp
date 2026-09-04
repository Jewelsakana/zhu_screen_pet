#include "infrastructure/WindowPlacement.h"

#include <QtGlobal>

namespace zhu_screen_pet {
namespace {

QPoint candidatePosition(const WindowPlacementRequest& request, AttachmentSide side)
{
    const QRect anchor = request.anchorGeometry;
    const QSize size = request.windowSize;
    int x = anchor.left();
    int y = anchor.top();
    if (side == AttachmentSide::Left) x = anchor.left() - request.gap - size.width();
    if (side == AttachmentSide::Right) x = anchor.right() + 1 + request.gap;
    if (side == AttachmentSide::Above) y = anchor.top() - request.gap - size.height();
    if (side == AttachmentSide::Below) y = anchor.bottom() + 1 + request.gap;

    const bool verticalSide = side == AttachmentSide::Left || side == AttachmentSide::Right;
    if (verticalSide) {
        if (request.alignment == AttachmentAlignment::Center) {
            y = anchor.center().y() - size.height() / 2;
        } else if (request.alignment == AttachmentAlignment::End) {
            y = anchor.bottom() - size.height() + 1;
        }
    } else {
        if (request.alignment == AttachmentAlignment::Center) {
            x = anchor.center().x() - size.width() / 2;
        } else if (request.alignment == AttachmentAlignment::End) {
            x = anchor.right() - size.width() + 1;
        }
    }
    return {x, y};
}

int visibleArea(const QRect& available, const QPoint& position, const QSize& size)
{
    return available.intersected(QRect(position, size)).width()
        * available.intersected(QRect(position, size)).height();
}

int chainWidth(const QVector<QSize>& sizes, int gap)
{
    int width = 0;
    for (const QSize& size : sizes) width += qMax(0, size.width());
    if (sizes.size() > 1) width += gap * (sizes.size() - 1);
    return width;
}

bool chainFitsHorizontally(const QRect& available, const QRect& anchor,
                           const QVector<QSize>& sizes, int gap, AttachmentSide side)
{
    const int width = chainWidth(sizes, gap);
    if (side == AttachmentSide::Right) {
        return anchor.right() + 1 + gap + width - 1 <= available.right();
    }
    return anchor.left() - gap - width >= available.left();
}

} // namespace

QPoint WindowPlacement::clamp(const QRect& availableGeometry, const QSize& windowSize,
                              const QPoint& desiredPosition)
{
    const int maxX = availableGeometry.right() - windowSize.width() + 1;
    const int maxY = availableGeometry.bottom() - windowSize.height() + 1;
    return {qBound(availableGeometry.left(), desiredPosition.x(),
                   qMax(availableGeometry.left(), maxX)),
            qBound(availableGeometry.top(), desiredPosition.y(),
                   qMax(availableGeometry.top(), maxY))};
}

AttachmentSide WindowPlacement::opposite(AttachmentSide side)
{
    switch (side) {
    case AttachmentSide::Left: return AttachmentSide::Right;
    case AttachmentSide::Right: return AttachmentSide::Left;
    case AttachmentSide::Above: return AttachmentSide::Below;
    case AttachmentSide::Below: return AttachmentSide::Above;
    }
    return AttachmentSide::Right;
}

WindowPlacementResult WindowPlacement::adjacent(const WindowPlacementRequest& request)
{
    WindowPlacementResult result;
    result.actualSide = request.preferredSide;
    const QSize size(qMax(0, request.windowSize.width()), qMax(0, request.windowSize.height()));
    WindowPlacementRequest normalized = request;
    normalized.windowSize = size;
    normalized.gap = qMax(0, request.gap);
    const QPoint preferred = candidatePosition(normalized, request.preferredSide);
    const QRect preferredRect(preferred, size);
    const AttachmentSide otherSide = opposite(request.preferredSide);
    const QPoint other = candidatePosition(normalized, otherSide);
    const QRect otherRect(other, size);
    if (request.availableGeometry.contains(preferredRect)) {
        result.position = preferred;
    } else if (request.availableGeometry.contains(otherRect)) {
        result.position = other;
        result.actualSide = otherSide;
        result.flipped = true;
    } else {
        const bool useOther = visibleArea(request.availableGeometry, other, size)
            > visibleArea(request.availableGeometry, preferred, size);
        result.position = useOther ? other : preferred;
        result.actualSide = useOther ? otherSide : request.preferredSide;
        result.flipped = useOther;
        result.position = clamp(request.availableGeometry, size, result.position);
    }
    return result;
}

HorizontalWindowChainResult WindowPlacement::horizontalChain(
    const HorizontalWindowChainRequest& request)
{
    HorizontalWindowChainResult result;
    const int gap = qMax(0, request.gap);
    const AttachmentSide preferred = request.preferredSide == AttachmentSide::Left
        ? AttachmentSide::Left : AttachmentSide::Right;
    const AttachmentSide oppositeSide = opposite(preferred);
    const bool preferredFits = chainFitsHorizontally(
        request.availableGeometry, request.anchorGeometry, request.windowSizes, gap, preferred);
    const bool oppositeFits = chainFitsHorizontally(
        request.availableGeometry, request.anchorGeometry, request.windowSizes, gap, oppositeSide);
    if (preferredFits) {
        result.actualSide = preferred;
    } else if (oppositeFits) {
        result.actualSide = oppositeSide;
        result.flipped = true;
    } else {
        const int rightSpace = request.availableGeometry.right() - request.anchorGeometry.right();
        const int leftSpace = request.anchorGeometry.left() - request.availableGeometry.left();
        result.actualSide = rightSpace >= leftSpace ? AttachmentSide::Right : AttachmentSide::Left;
        result.flipped = result.actualSide != preferred;
    }

    const int totalWidth = chainWidth(request.windowSizes, gap);
    int blockLeft = result.actualSide == AttachmentSide::Right
        ? request.anchorGeometry.right() + 1 + gap
        : request.anchorGeometry.left() - gap - totalWidth;
    // 左右空间都不足时只钳制整条链一次，保持窗口之间的相对位置，避免每个
    // 窗口分别钳制到同一屏幕边缘而互相覆盖。
    if (totalWidth <= request.availableGeometry.width()) {
        blockLeft = qBound(request.availableGeometry.left(), blockLeft,
                           request.availableGeometry.right() - totalWidth + 1);
    }
    int cursorX = result.actualSide == AttachmentSide::Right
        ? blockLeft : blockLeft + totalWidth;
    for (const QSize& sourceSize : request.windowSizes) {
        const QSize size(qMax(0, sourceSize.width()), qMax(0, sourceSize.height()));
        int x = cursorX;
        if (result.actualSide == AttachmentSide::Left) x -= size.width();
        int y = request.anchorGeometry.center().y() - size.height() / 2;
        y = qBound(request.availableGeometry.top(), y,
                   qMax(request.availableGeometry.top(),
                        request.availableGeometry.bottom() - size.height() + 1));
        result.positions.append({x, y});
        cursorX = result.actualSide == AttachmentSide::Right
            ? x + size.width() + gap : x - gap;
    }
    return result;
}

} // namespace zhu_screen_pet
