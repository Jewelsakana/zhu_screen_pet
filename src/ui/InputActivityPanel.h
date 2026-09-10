#pragma once

#include <QWidget>

class QLabel;
class QPaintEvent;

namespace zhu_screen_pet {

/** 显示匿名键盘和鼠标累计次数的只读浮动面板。 */
class InputActivityPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit InputActivityPanel(QWidget* parent = nullptr);
    void setCount(qint64 inputCount);
    void setUiScalePercent(int percent);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QLabel* inputCount_ = nullptr;
    qreal cornerRadius_ = 14.0;
};

} // namespace zhu_screen_pet
