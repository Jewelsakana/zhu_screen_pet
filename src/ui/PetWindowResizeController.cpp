#include "ui/PetWindowResizeController.h"

#include <QGuiApplication>
#include <QMouseEvent>
#include <QScreen>
#include <QWidget>
#include <QtMath>

#include "app/UiConfig.h"
#include "infrastructure/WindowPlacement.h"
#include "ui/UiScaleMetrics.h"

namespace zhu_screen_pet {

PetWindowResizeController::PetWindowResizeController(QWidget* window, QObject* parent)
    : QObject(parent), window_(window)
{
}

void PetWindowResizeController::setScalePercent(int percent)
{
    scalePercent_ = qBound(UiConfig::MinimumWindowScalePercent, percent,
                           UiConfig::MaximumWindowScalePercent);
}

int PetWindowResizeController::scalePercent() const { return scalePercent_; }
bool PetWindowResizeController::isResizing() const { return resizing_; }

bool PetWindowResizeController::handlePress(QMouseEvent* event)
{
    if (window_ == nullptr || event == nullptr || event->button() != Qt::LeftButton) return false;
    resizeEdges_ = edgesAt(event->globalPosition().toPoint());
    if (resizeEdges_ != Qt::Edges{}) {
        resizing_ = true;
        dragging_ = false;
        resizeStartGlobal_ = event->globalPosition().toPoint();
        resizeStartGeometry_ = window_->frameGeometry();
        resizeStartScalePercent_ = scalePercent_;
    } else {
        dragging_ = true;
        resizing_ = false;
        dragOffset_ = event->globalPosition().toPoint() - window_->frameGeometry().topLeft();
    }
    event->accept();
    return true;
}

bool PetWindowResizeController::handleMove(QMouseEvent* event)
{
    if (window_ == nullptr || event == nullptr) return false;
    const QPoint global = event->globalPosition().toPoint();
    if (resizing_ && (event->buttons() & Qt::LeftButton)) {
        resizeFromPointer(global);
        event->accept();
        return true;
    }
    if (dragging_ && (event->buttons() & Qt::LeftButton)) {
        const QPoint desired = global - dragOffset_;
        QScreen* targetScreen = QGuiApplication::screenAt(global);
        if (targetScreen == nullptr) targetScreen = QGuiApplication::screenAt(
            window_->frameGeometry().center());
        if (targetScreen == nullptr) targetScreen = QGuiApplication::primaryScreen();
        window_->move(targetScreen == nullptr ? desired : WindowPlacement::clamp(
            targetScreen->availableGeometry(), window_->frameGeometry().size(), desired));
        emit windowGeometryChanged();
        event->accept();
        return true;
    }
    updateCursor(edgesAt(global));
    return false;
}

bool PetWindowResizeController::handleRelease(QMouseEvent* event)
{
    if (event == nullptr || event->button() != Qt::LeftButton) return false;
    const bool wasResizing = resizing_;
    const bool handled = resizing_ || dragging_;
    resizing_ = false;
    dragging_ = false;
    resizeEdges_ = {};
    updateCursor(edgesAt(event->globalPosition().toPoint()));
    if (wasResizing) emit scaleCommitRequested(scalePercent_);
    if (handled) event->accept();
    return handled;
}

Qt::Edges PetWindowResizeController::edgesAt(const QPoint& globalPosition) const
{
    if (window_ == nullptr) return {};
    const QPoint local = window_->mapFromGlobal(globalPosition);
    const int margin = UiScaleMetrics(scalePercent_).scaled(10, 6);
    Qt::Edges edges;
    if (local.x() >= 0 && local.x() < margin) edges |= Qt::LeftEdge;
    if (local.x() < window_->width() && local.x() >= window_->width() - margin) {
        edges |= Qt::RightEdge;
    }
    if (local.y() >= 0 && local.y() < margin) edges |= Qt::TopEdge;
    if (local.y() < window_->height() && local.y() >= window_->height() - margin) {
        edges |= Qt::BottomEdge;
    }
    return edges;
}

void PetWindowResizeController::updateCursor(Qt::Edges edges)
{
    if (window_ == nullptr) return;
    Qt::CursorShape shape = Qt::ArrowCursor;
    const bool left = edges.testFlag(Qt::LeftEdge);
    const bool right = edges.testFlag(Qt::RightEdge);
    const bool top = edges.testFlag(Qt::TopEdge);
    const bool bottom = edges.testFlag(Qt::BottomEdge);
    if ((left && top) || (right && bottom)) shape = Qt::SizeFDiagCursor;
    else if ((right && top) || (left && bottom)) shape = Qt::SizeBDiagCursor;
    else if (left || right) shape = Qt::SizeHorCursor;
    else if (top || bottom) shape = Qt::SizeVerCursor;
    window_->setCursor(shape);
    if (QWidget* surface = window_->findChild<QWidget*>(
            QStringLiteral("petSurface"), Qt::FindDirectChildrenOnly)) surface->setCursor(shape);
}

void PetWindowResizeController::resizeFromPointer(const QPoint& globalPosition)
{
    QScreen* targetScreen = QGuiApplication::screenAt(resizeStartGeometry_.center());
    if (targetScreen == nullptr) targetScreen = QGuiApplication::primaryScreen();
    const QRect available = targetScreen == nullptr
        ? QRect(0, 0, 1920, 1080) : targetScreen->availableGeometry();
    const QSize baseSize = UiScaleMetrics(100).scaledForScreen(QSize(300, 350), available);
    const QPoint delta = globalPosition - resizeStartGlobal_;
    const bool horizontal = resizeEdges_.testFlag(Qt::LeftEdge)
        || resizeEdges_.testFlag(Qt::RightEdge);
    const bool vertical = resizeEdges_.testFlag(Qt::TopEdge)
        || resizeEdges_.testFlag(Qt::BottomEdge);
    const int widthDelta = resizeEdges_.testFlag(Qt::LeftEdge) ? -delta.x() : delta.x();
    const int heightDelta = resizeEdges_.testFlag(Qt::TopEdge) ? -delta.y() : delta.y();
    const int widthPercent = resizeStartScalePercent_
        + qRound(100.0 * widthDelta / qMax(1, baseSize.width()));
    const int heightPercent = resizeStartScalePercent_
        + qRound(100.0 * heightDelta / qMax(1, baseSize.height()));
    int newPercent = resizeStartScalePercent_;
    if (horizontal && vertical) {
        newPercent = qAbs(widthPercent - resizeStartScalePercent_)
                >= qAbs(heightPercent - resizeStartScalePercent_)
            ? widthPercent : heightPercent;
    } else if (horizontal) {
        newPercent = widthPercent;
    } else if (vertical) {
        newPercent = heightPercent;
    }
    newPercent = qBound(UiConfig::MinimumWindowScalePercent, newPercent,
                        UiConfig::MaximumWindowScalePercent);
    if (newPercent == scalePercent_) return;

    scalePercent_ = newPercent;
    emit scalePreviewRequested(newPercent);
    const QSize newSize = window_->frameGeometry().size();
    QPoint topLeft = resizeStartGeometry_.topLeft();
    if (resizeEdges_.testFlag(Qt::LeftEdge)) {
        topLeft.setX(resizeStartGeometry_.right() - newSize.width() + 1);
    } else if (!horizontal) {
        topLeft.setX(resizeStartGeometry_.center().x() - newSize.width() / 2);
    }
    if (resizeEdges_.testFlag(Qt::TopEdge)) {
        topLeft.setY(resizeStartGeometry_.bottom() - newSize.height() + 1);
    } else if (!vertical) {
        topLeft.setY(resizeStartGeometry_.center().y() - newSize.height() / 2);
    }
    window_->move(WindowPlacement::clamp(available, newSize, topLeft));
    emit windowGeometryChanged();
}

} // namespace zhu_screen_pet
