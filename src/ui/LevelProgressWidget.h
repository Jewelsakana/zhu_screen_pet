#pragma once

#include <QWidget>
#include <QColor>

namespace zhu_screen_pet {

/** 在主界面绘制圆环进度和两位等级文本。 */
class LevelProgressWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit LevelProgressWidget(QWidget* parent = nullptr);
    void setProgress(int level, int percent);
    void setMaximumLevel(int maximumLevel);
    void setColors(const QColor& progressColor, const QColor& levelColor);
    void setShowMaximumLabel(bool enabled);
    void setShowLevelPrefix(bool enabled);
    int level() const;
    int progressPercent() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize sizeHint() const override;

private:
    int level_ = 0;
    int percent_ = 0;
    int maximumLevel_ = 100;
    QColor progressColor_ = QColor(QStringLiteral("#35b66a"));
    QColor levelColor_ = QColor(QStringLiteral("#20242c"));
    bool showMaximumLabel_ = false;
    bool showLevelPrefix_ = true;
};

} // namespace zhu_screen_pet
