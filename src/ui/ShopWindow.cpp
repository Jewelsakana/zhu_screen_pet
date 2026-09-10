#include "ui/ShopWindow.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QEvent>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "app/PetEconomyController.h"
#include "app/PetEconomyTypes.h"
#include "ui/UiScaleMetrics.h"

namespace zhu_screen_pet {

ShopWindow::ShopWindow(QWidget* parent)
    : QDialog(parent), itemButtons_(new QButtonGroup(this))
{
    setObjectName(QStringLiteral("shopWindow"));
    setWindowTitle(QStringLiteral("商店"));
    resize(620, 480);
    itemButtons_->setExclusive(true);
    auto* root = new QVBoxLayout(this);
    tabs_ = new QTabWidget(this);
    giftPage_ = createCategoryPage(ShopItemKind::Gift);
    foodPage_ = createCategoryPage(ShopItemKind::Food);
    tabs_->addTab(giftPage_, QStringLiteral("礼物商店"));
    tabs_->addTab(foodPage_, QStringLiteral("食物商店"));
    connect(tabs_, &QTabWidget::currentChanged, this, [this]() {
        if (QAbstractButton* selected = itemButtons_->checkedButton()) {
            itemButtons_->setExclusive(false);
            selected->setChecked(false);
            itemButtons_->setExclusive(true);
        }
    });
    root->addWidget(tabs_, 1);
    auto* footer = new QHBoxLayout();
    balance_ = new QLabel(QStringLiteral("输入余额：0"), this);
    balance_->setObjectName(QStringLiteral("shopBalance"));
    purchase_ = new QPushButton(QStringLiteral("购买"), this);
    purchase_->setObjectName(QStringLiteral("shopPurchaseButton"));
    footer->addWidget(balance_);
    footer->addStretch();
    footer->addWidget(purchase_);
    root->addLayout(footer);
    connect(purchase_, &QPushButton::clicked, this, &ShopWindow::purchaseSelected);
    setUiScalePercent(100);
}

void ShopWindow::setController(PetEconomyController* controller)
{
    if (controller_ == controller) return;
    if (controller_ != nullptr) disconnect(controller_, nullptr, this, nullptr);
    controller_ = controller;
    if (controller_ != nullptr) {
        connect(controller_, &PetEconomyController::balanceChanged,
                this, &ShopWindow::refreshBalance);
        refreshBalance(controller_->balance());
    }
    populateCategory(giftPage_, ShopItemKind::Gift);
    populateCategory(foodPage_, ShopItemKind::Food);
}

QWidget* ShopWindow::createCategoryPage(ShopItemKind kind)
{
    Q_UNUSED(kind);
    auto* page = new QWidget(tabs_);
    auto* pageLayout = new QVBoxLayout(page);
    auto* scroll = new QScrollArea(page);
    scroll->setObjectName(QStringLiteral("shopCategoryScroll"));
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->installEventFilter(this);
    auto* content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("shopCategoryContent"));
    auto* grid = new QGridLayout(content);
    grid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    scroll->setWidget(content);
    pageLayout->addWidget(scroll);
    return page;
}

void ShopWindow::populateCategory(QWidget* page, ShopItemKind kind)
{
    if (page == nullptr) return;
    auto* content = page->findChild<QWidget*>(QStringLiteral("shopCategoryContent"));
    auto* grid = content == nullptr ? nullptr : qobject_cast<QGridLayout*>(content->layout());
    if (grid == nullptr) return;
    while (QLayoutItem* child = grid->takeAt(0)) {
        if (QWidget* widget = child->widget()) {
            // 空分类占位项是 QLabel；只把实际商品按钮交给 QButtonGroup。
            if (auto* button = qobject_cast<QAbstractButton*>(widget)) {
                itemButtons_->removeButton(button);
            }
            widget->deleteLater();
        }
        delete child;
    }
    const QVector<ShopItemDefinition> items = controller_ == nullptr
        ? QVector<ShopItemDefinition>{} : controller_->items(kind);
    if (items.isEmpty()) {
        auto* empty = new QLabel(QStringLiteral("暂无商品，请稍后在配置中添加"), content);
        empty->setAlignment(Qt::AlignCenter);
        grid->addWidget(empty, 0, 0, 1, 4);
        return;
    }
    for (int index = 0; index < items.size(); ++index) {
        const ShopItemDefinition& item = items.at(index);
        const int effect = item.kind == ShopItemKind::Gift
            ? item.affectionGain : item.satietyGain;
        const QString effectName = item.kind == ShopItemKind::Gift
            ? QStringLiteral("好感度") : QStringLiteral("饱食度");
        auto* card = new QPushButton(
            QStringLiteral("%1\n%2\n价格：%3\n%4：+%5")
                .arg(item.emoji, item.name, QString::number(item.price),
                     effectName, QString::number(effect)), content);
        card->setCheckable(true);
        card->setProperty("itemId", item.id);
        card->setMinimumSize(preferredCardWidth_, preferredCardHeight_);
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        itemButtons_->addButton(card);
        grid->addWidget(card, index / 4, index % 4);
    }
    updateCardMinimumSizes(uiScalePercent_);
    reflowCategory(page);
}

int ShopWindow::responsiveColumnCount(QWidget* page) const
{
    if (page == nullptr) return 1;
    auto* scroll = page->findChild<QScrollArea*>(QStringLiteral("shopCategoryScroll"));
    auto* content = page->findChild<QWidget*>(QStringLiteral("shopCategoryContent"));
    auto* grid = content == nullptr ? nullptr : qobject_cast<QGridLayout*>(content->layout());
    if (scroll == nullptr || grid == nullptr) return 1;
    const QMargins margins = grid->contentsMargins();
    const int spacing = qMax(0, grid->horizontalSpacing());
    const int available = qMax(1, scroll->viewport()->width()
        - margins.left() - margins.right());
    return qMax(1, (available + spacing) / (preferredCardWidth_ + spacing));
}

void ShopWindow::reflowCategory(QWidget* page)
{
    if (page == nullptr) return;
    auto* content = page->findChild<QWidget*>(QStringLiteral("shopCategoryContent"));
    auto* grid = content == nullptr ? nullptr : qobject_cast<QGridLayout*>(content->layout());
    if (grid == nullptr) return;

    QVector<QWidget*> widgets;
    while (QLayoutItem* item = grid->takeAt(0)) {
        if (item->widget() != nullptr) widgets.append(item->widget());
        delete item;
    }
    const int columns = responsiveColumnCount(page);
    for (int column = 0; column < qMax(columns, grid->columnCount()); ++column) {
        grid->setColumnStretch(column, column < columns ? 1 : 0);
    }
    for (int index = 0; index < widgets.size(); ++index) {
        QWidget* widget = widgets.at(index);
        if (qobject_cast<QAbstractButton*>(widget) == nullptr) {
            grid->addWidget(widget, 0, 0, 1, columns);
        } else {
            grid->addWidget(widget, index / columns, index % columns);
        }
    }
}

void ShopWindow::purchaseSelected()
{
    QAbstractButton* selected = itemButtons_->checkedButton();
    if (controller_ == nullptr || selected == nullptr) {
        QMessageBox::information(this, QStringLiteral("购买"), QStringLiteral("请先选择商品"));
        return;
    }
    QString message;
    if (!controller_->purchase(selected->property("itemId").toString(), &message)) {
        QMessageBox::warning(this, QStringLiteral("购买失败"), message);
        return;
    }
    refreshBalance(controller_->balance());
}

void ShopWindow::refreshBalance(qint64 balance)
{
    balance_->setText(QStringLiteral("输入余额：%1").arg(balance));
}

void ShopWindow::setUiScalePercent(int percent)
{
    uiScalePercent_ = percent;
    const UiScaleMetrics metrics(percent);
    preferredCardWidth_ = metrics.scaled(120, 60);
    preferredCardHeight_ = metrics.scaled(122, 72);
    resize(metrics.scaled(620), metrics.scaled(480));
    for (QWidget* page : {giftPage_, foodPage_}) {
        if (page == nullptr) continue;
        auto* content = page->findChild<QWidget*>(QStringLiteral("shopCategoryContent"));
        auto* grid = content == nullptr ? nullptr : qobject_cast<QGridLayout*>(content->layout());
        if (grid != nullptr) {
            grid->setContentsMargins(metrics.scaled(10), metrics.scaled(10),
                                     metrics.scaled(10), metrics.scaled(10));
            grid->setHorizontalSpacing(metrics.scaled(6));
            grid->setVerticalSpacing(metrics.scaled(6));
        }
    }
    setStyleSheet(QStringLiteral(
        "QDialog{background:#fffaf0;color:#26375d;}"
        "QTabWidget::pane{border:1px solid #dfd3bd;border-radius:10px;}"
        "QPushButton{background:#fffdf8;color:#26375d;border:1px solid #eadfca;"
        "border-radius:12px;padding:8px;}"
        "QPushButton:checked{background:#ffe4ee;border:2px solid #f29abc;}"
        "QPushButton#shopPurchaseButton{background:#79adf3;color:#17345f;min-width:100px;}"));
    updateCardMinimumSizes(percent);
    reflowCategory(giftPage_);
    reflowCategory(foodPage_);
}

void ShopWindow::updateCardMinimumSizes(int percent)
{
    const UiScaleMetrics metrics(percent);
    int readableWidth = metrics.scaled(120, 60);
    int readableHeight = metrics.scaled(122, 72);
    for (QAbstractButton* button : itemButtons_->buttons()) {
        const QFontMetrics fontMetrics(button->font());
        const QStringList lines = button->text().split(QLatin1Char('\n'));
        int widestLine = 0;
        for (const QString& line : lines) {
            widestLine = qMax(widestLine, fontMetrics.horizontalAdvance(line));
        }
        readableWidth = qMax(readableWidth,
                             widestLine + metrics.scaled(34, 24));
        readableHeight = qMax(readableHeight,
                              lines.size() * fontMetrics.lineSpacing()
                                  + metrics.scaled(30, 22));
    }
    preferredCardWidth_ = readableWidth;
    preferredCardHeight_ = readableHeight;
    for (QAbstractButton* button : itemButtons_->buttons()) {
        button->setMinimumSize(preferredCardWidth_, preferredCardHeight_);
    }
}

bool ShopWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Resize && !reflowPending_) {
        const auto isCategoryViewport = [this, watched](QWidget* page) {
            auto* scroll = page == nullptr ? nullptr
                : page->findChild<QScrollArea*>(QStringLiteral("shopCategoryScroll"));
            return scroll != nullptr && watched == scroll->viewport();
        };
        if (isCategoryViewport(giftPage_) || isCategoryViewport(foodPage_)) {
            reflowPending_ = true;
            QTimer::singleShot(0, this, [this]() {
                reflowPending_ = false;
                reflowCategory(giftPage_);
                reflowCategory(foodPage_);
            });
        }
    }
    return QDialog::eventFilter(watched, event);
}

void ShopWindow::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    reflowCategory(giftPage_);
    reflowCategory(foodPage_);
}

} // namespace zhu_screen_pet
