#pragma once

#include <QDialog>
#include <QVector>

#include "memory/ConversationTypes.h"

class QComboBox;
class QLineEdit;
class QTableWidget;

namespace zhu_screen_pet {

class SettingsController;

/** 查看、筛选、编辑和删除本地短期/长期记忆。 */
class MemoryManagementDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit MemoryManagementDialog(SettingsController* controller, QWidget* parent = nullptr);

private:
    void reload();
    void editSelected();
    void deleteSelected();
    void clearCurrentKind();
    qint64 selectedId() const;

    SettingsController* controller_ = nullptr;
    QLineEdit* search_ = nullptr;
    QComboBox* kind_ = nullptr;
    QComboBox* category_ = nullptr;
    QTableWidget* table_ = nullptr;
    QVector<MemoryItem> items_;
};

} // namespace zhu_screen_pet
