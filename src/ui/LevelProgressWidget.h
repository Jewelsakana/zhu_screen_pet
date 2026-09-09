#pragma once

#include <QWidget>

namespace zhu_screen_pet {

/** 在主界面绘制圆环进度和两位等级文本。 */
class LevelProgressWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit LevelProgressWidget(QWidget* parent = nullptr);
    void setProgress(int level, int percent);
    int level() const;
    int progressPercent() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize sizeHint() const override;

private:
    int level_ = 0;
    int percent_ = 0;
};

} // namespace zhu_screen_pet
