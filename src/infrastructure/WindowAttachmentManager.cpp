#include "infrastructure/WindowAttachmentManager.h"

#include <QEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QWidget>

#include <algorithm>
#include <cstddef>

namespace zhu_screen_pet {

WindowAttachmentManager::WindowAttachmentManager(QObject* parent)
    : QObject(parent)
{
    // 主界面当前有 6 个常驻附属窗口。预留少量余量，避免初始化
    // 期间在同步 Windows 窗口事件中扩容容器。
    attachments_.reserve(8);
}

void WindowAttachmentManager::beginUpdate()
{
    ++updateDepth_;
}

void WindowAttachmentManager::endUpdate()
{
    if (updateDepth_ <= 0) return;
    --updateDepth_;
    if (updateDepth_ == 0 && repositionPending_) {
        repositionPending_ = false;
        reposition();
    }
}

void WindowAttachmentManager::setAnchor(QWidget* anchor)
{
    if (anchor_ == anchor) return;
    if (anchor_ != nullptr) anchor_->removeEventFilter(this);
    anchor_ = anchor;
    if (anchor_ != nullptr) anchor_->installEventFilter(this);
    if (updateDepth_ > 0) {
        repositionPending_ = true;
        return;
    }
    reposition();
}

void WindowAttachmentManager::attach(QWidget* window, const WindowAttachmentOptions& options)
{
    if (window == nullptr || window == anchor_) return;
    for (Attachment& attachment : attachments_) {
        if (attachment.window == window) {
            attachment.options = options;
            if (updateDepth_ > 0) {
                repositionPending_ = true;
                return;
            }
            reposition();
            return;
        }
    }
    attachments_.push_back({window, options});
    if (updateDepth_ > 0) {
        repositionPending_ = true;
        return;
    }
    reposition();
}

void WindowAttachmentManager::detach(QWidget* window)
{
    attachments_.erase(std::remove_if(attachments_.begin(), attachments_.end(),
        [window](const Attachment& attachment) {
            return attachment.window == nullptr || attachment.window == window;
        }), attachments_.end());
}

void WindowAttachmentManager::clear()
{
    attachments_.clear();
}

void WindowAttachmentManager::reposition()
{
    if (updateDepth_ > 0) {
        repositionPending_ = true;
        return;
    }
    if (repositioning_ || anchor_ == nullptr) return;
    repositioning_ = true;
    const QRect anchorGeometry = anchor_->isWindow()
        ? anchor_->frameGeometry()
        : QRect(anchor_->mapToGlobal(QPoint(0, 0)), anchor_->size());
    QScreen* screen = QGuiApplication::screenAt(anchorGeometry.center());
    if (screen == nullptr) screen = QGuiApplication::primaryScreen();
    if (screen != nullptr) {
        for (std::size_t remaining = attachments_.size(); remaining > 0; --remaining) {
            const std::size_t index = remaining - 1;
            Attachment& attachment = attachments_[index];
            if (attachment.window == nullptr) {
                attachments_.erase(attachments_.begin()
                                   + static_cast<std::ptrdiff_t>(index));
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
