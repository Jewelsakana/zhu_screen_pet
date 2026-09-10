#include "ui/BackpackWindow.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "app/PetEconomyController.h"
#include "app/PetEconomyTypes.h"
#include "ui/UiScaleMetrics.h"

namespace zhu_screen_pet {

BackpackWindow::BackpackWindow(QWidget* parent)
    : QDialog(parent), itemButtons_(new QButtonGroup(this))
{
    setObjectName(QStringLiteral("backpackWindow"));
    setWindowTitle(QStringLiteral("背包"));
    itemButtons_->setExclusive(true);
    auto* root = new QVBoxLayout(this);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    content_ = new QWidget(scroll);
    grid_ = new QGridLayout(content_);
    scroll->setWidget(content_);
    root->addWidget(scroll, 1);
    give_ = new QPushButton(QStringLiteral("赠与"), this);
    give_->setObjectName(QStringLiteral("backpackGiveButton"));
    root->addWidget(give_, 0, Qt::AlignRight);
    connect(give_, &QPushButton::clicked, this, &BackpackWindow::giveSelected);
    setUiScalePercent(100);
    refreshItems();
}

void BackpackWindow::setController(PetEconomyController* controller)
{
    if (controller_ == controller) return;
    if (controller_ != nullptr) disconnect(controller_, nullptr, this, nullptr);
    controller_ = controller;
    if (controller_ != nullptr) {
        connect(controller_, &PetEconomyController::inventoryChanged,
                this, &BackpackWindow::refreshItems);
    }
    refreshItems();
}

void BackpackWindow::refreshItems()
{
    while (QLayoutItem* child = grid_->takeAt(0)) {
        if (QWidget* widget = child->widget()) {
            // 空背包占位项是 QLabel，不能把转换失败得到的 nullptr 传给
            // QButtonGroup::removeButton()，Qt 6.8.3 会在内部解引用它。
            if (auto* button = qobject_cast<QAbstractButton*>(widget)) {
                itemButtons_->removeButton(button);
            }
            widget->deleteLater();
        }
        delete child;
    }
    const QVector<InventoryEntry> entries = controller_ == nullptr
        ? QVector<InventoryEntry>{} : controller_->inventory();
    if (entries.isEmpty()) {
        auto* empty = new QLabel(QStringLiteral("背包是空的"), content_);
        empty->setAlignment(Qt::AlignCenter);
        grid_->addWidget(empty, 0, 0, 1, 4);
        give_->setEnabled(false);
        return;
    }
    give_->setEnabled(true);
    for (int index = 0; index < entries.size(); ++index) {
        const InventoryEntry& entry = entries.at(index);
        auto* card = new QPushButton(
            QStringLiteral("%1\n%2\n数量：%3").arg(entry.item.emoji, entry.item.name,
                                                    QString::number(entry.quantity)), content_);
        card->setCheckable(true);
        card->setProperty("itemId", entry.item.id);
        card->setMinimumSize(120, 105);
        itemButtons_->addButton(card);
        grid_->addWidget(card, index / 4, index % 4);
    }
    grid_->setRowStretch((entries.size() + 3) / 4, 1);
}

void BackpackWindow::giveSelected()
{
    QAbstractButton* selected = itemButtons_->checkedButton();
    if (controller_ == nullptr || selected == nullptr) {
        QMessageBox::information(this, QStringLiteral("赠与"), QStringLiteral("请先选择物品"));
        return;
    }
    QString message;
    if (!controller_->give(selected->property("itemId").toString(), &message)) {
        QMessageBox::warning(this, QStringLiteral("赠与失败"), message);
    }
}

void BackpackWindow::setUiScalePercent(int percent)
{
    const UiScaleMetrics metrics(percent);
    resize(metrics.scaled(620), metrics.scaled(480));
    setStyleSheet(QStringLiteral(
        "QDialog{background:#fffaf0;color:#26375d;}"
        "QScrollArea{border:1px solid #dfd3bd;border-radius:10px;background:transparent;}"
        "QPushButton{background:#fffdf8;color:#26375d;border:1px solid #eadfca;"
        "border-radius:12px;padding:8px;}"
        "QPushButton:checked{background:#fff1b8;border:2px solid #e0b330;}"
        "QPushButton#backpackGiveButton{background:#f5a9c6;color:#7a1736;min-width:100px;}"));
}

} // namespace zhu_screen_pet
