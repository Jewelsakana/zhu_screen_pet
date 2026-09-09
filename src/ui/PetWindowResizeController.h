#pragma once

#include <QObject>
#include <QPoint>
#include <QRect>

class QMouseEvent;
class QWidget;

namespace zhu_screen_pet {

/** 管理桌宠主窗口的拖动、边缘命中和固定比例交互缩放。 */
class PetWindowResizeController final : public QObject
{
    Q_OBJECT

public:
    explicit PetWindowResizeController(QWidget* window, QObject* parent = nullptr);

    void setScalePercent(int percent);
    int scalePercent() const;
    bool isResizing() const;
    bool handlePress(QMouseEvent* event);
    bool handleMove(QMouseEvent* event);
    bool handleRelease(QMouseEvent* event);

signals:
    void scalePreviewRequested(int percent);
    void scaleCommitRequested(int percent);
    void windowGeometryChanged();

private:
    Qt::Edges edgesAt(const QPoint& globalPosition) const;
    void updateCursor(Qt::Edges edges);
    void resizeFromPointer(const QPoint& globalPosition);

    QWidget* window_ = nullptr;
    int scalePercent_ = 100;
    bool dragging_ = false;
    bool resizing_ = false;
    Qt::Edges resizeEdges_;
    QPoint dragOffset_;
    QPoint resizeStartGlobal_;
    QRect resizeStartGeometry_;
    int resizeStartScalePercent_ = 100;
};

} // namespace zhu_screen_pet
