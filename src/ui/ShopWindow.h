#pragma once

#include <QDialog>

class QButtonGroup;
class QLabel;
class QPushButton;
class QResizeEvent;
class QTabWidget;
class QVBoxLayout;

namespace zhu_screen_pet {

class PetEconomyController;
enum class ShopItemKind;

/** 两类商品的独立商店窗口，每页按四列显示商品。 */
class ShopWindow final : public QDialog
{
    Q_OBJECT

public:
    explicit ShopWindow(QWidget* parent = nullptr);
    void setController(PetEconomyController* controller);
    void setUiScalePercent(int percent);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QWidget* createCategoryPage(ShopItemKind kind);
    void populateCategory(QWidget* page, ShopItemKind kind);
    void reflowCategory(QWidget* page);
    int responsiveColumnCount(QWidget* page) const;
    void updateCardMinimumSizes(int percent);
    void purchaseSelected();
    void refreshBalance(qint64 balance);

    PetEconomyController* controller_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    QButtonGroup* itemButtons_ = nullptr;
    QLabel* balance_ = nullptr;
    QPushButton* purchase_ = nullptr;
    QWidget* giftPage_ = nullptr;
    QWidget* foodPage_ = nullptr;
    int preferredCardWidth_ = 120;
    int preferredCardHeight_ = 122;
    int uiScalePercent_ = 100;
    bool reflowPending_ = false;
};

} // namespace zhu_screen_pet
