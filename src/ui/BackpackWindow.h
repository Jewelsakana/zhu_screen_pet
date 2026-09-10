#pragma once

#include <QDialog>

class QButtonGroup;
class QPushButton;
class QGridLayout;
class QWidget;

namespace zhu_screen_pet {

class PetEconomyController;

/** 独立背包窗口，只显示数量大于零的物品。 */
class BackpackWindow final : public QDialog
{
    Q_OBJECT

public:
    explicit BackpackWindow(QWidget* parent = nullptr);
    void setController(PetEconomyController* controller);
    void setUiScalePercent(int percent);

private:
    void refreshItems();
    void giveSelected();

    PetEconomyController* controller_ = nullptr;
    QButtonGroup* itemButtons_ = nullptr;
    QWidget* content_ = nullptr;
    QGridLayout* grid_ = nullptr;
    QPushButton* give_ = nullptr;
};

} // namespace zhu_screen_pet
