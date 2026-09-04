#include "infrastructure/WindowAttachmentManager.h"

#include <QEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QWidget>

namespace zhu_screen_pet {

WindowAttachmentManager::WindowAttachmentManager(QObject* parent)
    : QObject(parent)
{
}

void WindowAttachmentManager::setAnchor(QWidget* anchor)
{
    if (anchor_ == anchor) return;
    if (anchor_ != nullptr) anchor_->removeEventFilter(this);
    anchor_ = anchor;
    if (anchor_ != nullptr) anchor_->installEventFilter(this);
    reposition();
}

void WindowAttachmentManager::attach(QWidget* window, const WindowAttachmentOptions& options)
{
    if (window == nullptr || window == anchor_) return;
    for (Attachment& attachment : attachments_) {
        if (attachment.window == window) {
            attachment.options = options;
            reposition();
            return;
        }
    }
    attachments_.append({window, options});
    reposition();
}

void WindowAttachmentManager::detach(QWidget* window)
{
    for (int index = attachments_.size() - 1; index >= 0; --index) {
        if (attachments_.at(index).window == nullptr || attachments_.at(index).window == window) {
            attachments_.removeAt(index);
        }
    }
}

void WindowAttachmentManager::clear()
{
    attachments_.clear();
}

void WindowAttachmentManager::reposition()
{
    if (repositioning_ || anchor_ == nullptr) return;
    repositioning_ = true;
    const QRect anchorGeometry = anchor_->isWindow()
        ? anchor_->frameGeometry()
        : QRect(anchor_->mapToGlobal(QPoint(0, 0)), anchor_->size());
    QScreen* screen = QGuiApplication::screenAt(anchorGeometry.center());
    if (screen == nullptr) screen = QGuiApplication::primaryScreen();
    if (screen != nullptr) {
        for (int index = attachments_.size() - 1; index >= 0; --index) {
            Attachment& attachment = attachments_[index];
            if (attachment.window == nullptr) {
                attachments_.removeAt(index);
                continue;
            }
            const WindowPlacementResult placement = WindowPlacement::adjacent({
                screen->availableGeometry(), anchorGeometry, attachment.window->size(),
                attachment.options.side, attachment.options.alignment, attachment.options.gap});
            attachment.window->move(placement.position);
            emit attachmentPositioned(attachment.window, placement.actualSide);
        }
    }
    repositioning_ = false;
}

bool WindowAttachmentManager::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == anchor_ && (event->type() == QEvent::Move
                               || event->type() == QEvent::Resize
                               || event->type() == QEvent::Show
                               || event->type() == QEvent::WindowStateChange
                               || event->type() == QEvent::ScreenChangeInternal)) {
        reposition();
    }
    return QObject::eventFilter(watched, event);
}

} // namespace zhu_screen_pet
