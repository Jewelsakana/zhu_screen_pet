#pragma once

#include <QWizard>

#include "model/ModelProviderConfig.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;

namespace zhu_screen_pet {

class SettingsController;

/** 首次启动向导：收集运行前最必要的称呼、模型、隐私权限和系统设置。 */
class FirstRunWizard final : public QWizard
{
    Q_OBJECT

public:
    explicit FirstRunWizard(SettingsController* controller, QWidget* parent = nullptr);

protected:
    void accept() override;

private:
    void populateProfiles();
    void loadSelectedProfile(int index);
    void showError(const QString& message);

    SettingsController* controller_ = nullptr;
    QLineEdit* userAddress_ = nullptr;
    QComboBox* profile_ = nullptr;
    QLineEdit* baseUrl_ = nullptr;
    QLineEdit* modelName_ = nullptr;
    QLineEdit* apiKey_ = nullptr;
    QCheckBox* screenCaptureEnabled_ = nullptr;
    QCheckBox* autoStartEnabled_ = nullptr;
    QLabel* status_ = nullptr;
    ModelProviderConfig editingModel_;
};

} // namespace zhu_screen_pet
