#include "ui/MemoryManagementDialog.h"

#include <algorithm>

#include <QComboBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "app/SettingsController.h"

namespace zhu_screen_pet {

MemoryManagementDialog::MemoryManagementDialog(SettingsController* controller, QWidget* parent)
    : QDialog(parent), controller_(controller)
{
    setWindowTitle(QStringLiteral("记忆管理")); resize(980, 560);
    auto* root = new QVBoxLayout(this); auto* filters = new QHBoxLayout();
    kind_ = new QComboBox(this); kind_->addItem(QStringLiteral("长期记忆"), QStringLiteral("long_term"));
    kind_->addItem(QStringLiteral("短期记忆"), QStringLiteral("short_term")); kind_->addItem(QStringLiteral("全部记忆"), QString{});
    category_ = new QComboBox(this); category_->addItem(QStringLiteral("全部分类"), QString{});
    const QStringList categories = {QStringLiteral("conversation_summary"),QStringLiteral("identity"),QStringLiteral("preference"),QStringLiteral("relationship"),QStringLiteral("goal"),QStringLiteral("plan"),QStringLiteral("constraint"),QStringLiteral("habit"),QStringLiteral("other")};
    for (const QString& category : categories) category_->addItem(category, category);
    search_ = new QLineEdit(this); search_->setPlaceholderText(QStringLiteral("搜索记忆内容"));
    auto* refresh = new QPushButton(QStringLiteral("刷新"), this);
    filters->addWidget(new QLabel(QStringLiteral("类型"),this)); filters->addWidget(kind_);
    filters->addWidget(new QLabel(QStringLiteral("分类"),this)); filters->addWidget(category_);
    filters->addWidget(search_,1); filters->addWidget(refresh); root->addLayout(filters);

    table_ = new QTableWidget(this); table_->setColumnCount(8);
    table_->setHorizontalHeaderLabels({QStringLiteral("ID"),QStringLiteral("类型"),QStringLiteral("分类"),QStringLiteral("内容"),QStringLiteral("置信度"),QStringLiteral("重要性"),QStringLiteral("来源"),QStringLiteral("更新时间")});
    table_->setSelectionBehavior(QAbstractItemView::SelectRows); table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers); table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setSectionResizeMode(3,QHeaderView::Stretch); root->addWidget(table_,1);
    auto* buttons = new QHBoxLayout(); auto* edit = new QPushButton(QStringLiteral("编辑"),this);
    auto* remove = new QPushButton(QStringLiteral("删除"),this); auto* clear = new QPushButton(QStringLiteral("清空当前类型"),this);
    auto* close = new QPushButton(QStringLiteral("关闭"),this); buttons->addWidget(edit); buttons->addWidget(remove); buttons->addWidget(clear); buttons->addStretch(); buttons->addWidget(close); root->addLayout(buttons);
    connect(refresh,&QPushButton::clicked,this,&MemoryManagementDialog::reload);
    connect(search_,&QLineEdit::returnPressed,this,&MemoryManagementDialog::reload);
    connect(kind_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](){reload();});
    connect(category_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](){reload();});
    connect(edit,&QPushButton::clicked,this,&MemoryManagementDialog::editSelected);
    connect(remove,&QPushButton::clicked,this,&MemoryManagementDialog::deleteSelected);
    connect(clear,&QPushButton::clicked,this,&MemoryManagementDialog::clearCurrentKind);
    connect(close,&QPushButton::clicked,this,&QDialog::accept);
    connect(table_,&QTableWidget::cellDoubleClicked,this,[this](){editSelected();});
    reload();
}

void MemoryManagementDialog::reload()
{
    if (controller_ == nullptr) return; AppError error;
    items_ = controller_->memories(kind_->currentData().toString(), search_->text(), &error);
    if (error.code != AppErrorCode::None) { QMessageBox::warning(this,QStringLiteral("记忆管理"),error.message); return; }
    table_->setRowCount(0); const QString categoryFilter=category_->currentData().toString();
    for (const MemoryItem& item : items_) {
        if (!categoryFilter.isEmpty() && item.category != categoryFilter) continue;
        const int row=table_->rowCount(); table_->insertRow(row);
        const QString source=!item.sourceReference.isEmpty()?QStringLiteral("%1: %2").arg(item.sourceKind,item.sourceReference):item.sourceEventId;
        const QStringList cells={QString::number(item.id),item.kind,item.category,item.content,QString::number(item.confidence,'f',2),QString::number(item.importance,'f',2),source,(item.updatedAt.isValid()?item.updatedAt:item.createdAt).toLocalTime().toString(Qt::ISODate)};
        for(int column=0;column<cells.size();++column){auto* cell=new QTableWidgetItem(cells.at(column));cell->setData(Qt::UserRole,item.id);table_->setItem(row,column,cell);}
    }
    table_->resizeColumnsToContents(); table_->horizontalHeader()->setSectionResizeMode(3,QHeaderView::Stretch);
}

qint64 MemoryManagementDialog::selectedId() const
{ const auto selected=table_->selectedItems(); return selected.isEmpty()?0:selected.first()->data(Qt::UserRole).toLongLong(); }

void MemoryManagementDialog::editSelected()
{
    const qint64 id=selectedId(); if(id<=0)return; auto it=std::find_if(items_.begin(),items_.end(),[id](const MemoryItem&i){return i.id==id;}); if(it==items_.end())return;
    bool ok=false; const QString content=QInputDialog::getMultiLineText(this,QStringLiteral("编辑记忆"),QStringLiteral("内容"),it->content,&ok).trimmed();
    if(!ok||content.isEmpty())return; MemoryItem updated=*it;updated.content=content;AppError error;
    if(!controller_->updateMemory(updated,&error))QMessageBox::warning(this,QStringLiteral("记忆管理"),error.message);else reload();
}

void MemoryManagementDialog::deleteSelected()
{
    const qint64 id=selectedId();if(id<=0)return;if(QMessageBox::question(this,QStringLiteral("删除记忆"),QStringLiteral("确定永久删除选中的记忆吗？"))!=QMessageBox::Yes)return;
    AppError error;if(!controller_->deleteMemory(id,&error))QMessageBox::warning(this,QStringLiteral("记忆管理"),error.message);else reload();
}

void MemoryManagementDialog::clearCurrentKind()
{
    const QString kind=kind_->currentData().toString();if(kind.isEmpty()){QMessageBox::information(this,QStringLiteral("记忆管理"),QStringLiteral("请选择长期记忆或短期记忆后再清空。"));return;}
    if(QMessageBox::warning(this,QStringLiteral("清空记忆"),QStringLiteral("确定永久清空当前类型的全部记忆吗？"),QMessageBox::Yes|QMessageBox::No)!=QMessageBox::Yes)return;
    AppError error;if(!controller_->clearMemories(kind,&error))QMessageBox::warning(this,QStringLiteral("记忆管理"),error.message);else reload();
}

} // namespace zhu_screen_pet
