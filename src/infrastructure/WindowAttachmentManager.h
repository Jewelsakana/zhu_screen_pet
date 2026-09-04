#pragma once

#include <QList>
#include <QObject>
#include <QPointer>

#include "infrastructure/WindowPlacement.h"

class QWidget;

namespace zhu_screen_pet {

/** 单个附属顶层窗口相对桌宠窗口的定位规则。 */
struct WindowAttachmentOptions
{
    AttachmentSide side = AttachmentSide::Right;
    AttachmentAlignment alignment = AttachmentAlignment::Center;
    int gap = 8;
};

/** 监听锚点窗口移动和缩放，并让气泡、工具栏、输入框等窗口自动跟随。 */
class WindowAttachmentManager final : public QObject
{
    Q_OBJECT

public:
    explicit WindowAttachmentManager(QObject* parent = nullptr);

    /** 设置作为定位中心的窗口；传入 nullptr 会停止自动跟随。 */
    void setAnchor(QWidget* anchor);
    /** 添加或更新一个附属窗口。窗口所有权仍属于调用方。 */
    void attach(QWidget* window, const WindowAttachmentOptions& options);
    void detach(QWidget* window);
    void clear();
    /** 立即按照当前屏幕工作区重新定位全部附属窗口。 */
    void reposition();

signals:
    /** 定位完成后报告窗口实际所在方向，供气泡尾巴等视觉元素同步换边。 */
    void attachmentPositioned(QWidget* window, AttachmentSide actualSide);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Attachment
    {
        QPointer<QWidget> window;
        WindowAttachmentOptions options;
    };

    QPointer<QWidget> anchor_;
    QList<Attachment> attachments_;
    bool repositioning_ = false;
};

} // namespace zhu_screen_pet
