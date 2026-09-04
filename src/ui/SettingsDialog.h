#pragma once

#include <QDialog>
#include "core/AppError.h"
#include "model/ModelProviderConfig.h"

class QComboBox;
class QCheckBox;
class QLineEdit;
class QSpinBox;
class QLabel;
class QPushButton;

namespace zhu_screen_pet {

class SettingsController;
class ScreenCapture;

/** 设置窗口：编辑模型、用户称呼和记忆限制；核心人设由配置文件管理。 */
class SettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(SettingsController* controller, QWidget* parent = nullptr,
                            ScreenCapture* screenCapture = nullptr);

private slots:
    void loadSelectedProfile(int index);
    void testConnection();
    void applySettings();
    void onTestFinished(bool succeeded, const AppError& error);

private:
    void populate();
    void showError(const AppError& error);

    SettingsController* controller_ = nullptr;
    QComboBox* profile_ = nullptr;
    QComboBox* providerType_ = nullptr;
    QLineEdit* profileId_ = nullptr;
    QLineEdit* displayName_ = nullptr;
    QLineEdit* baseUrl_ = nullptr;
    QLineEdit* model_ = nullptr;
    QLineEdit* apiKey_ = nullptr;
    QSpinBox* timeoutMs_ = nullptr;
    QSpinBox* maxRetries_ = nullptr;
    QSpinBox* retryDelayMs_ = nullptr;
    QLineEdit* userAddress_ = nullptr;
    QSpinBox* maxReplyTokens_ = nullptr;
    QSpinBox* proactiveLevel_ = nullptr;
    QSpinBox* recentLimit_ = nullptr;
    QSpinBox* relevantLimit_ = nullptr;
    QSpinBox* longTermLimit_ = nullptr;
    QSpinBox* contextTokens_ = nullptr;
    QSpinBox* bubbleDurationSeconds_ = nullptr;
    QCheckBox* screenCaptureEnabled_ = nullptr;
    QSpinBox* screenCaptureIntervalSeconds_ = nullptr;
    QCheckBox* captureOnChat_ = nullptr;
    QComboBox* captureImageFormat_ = nullptr;
    QSpinBox* captureMaxWidth_ = nullptr;
    QSpinBox* captureQuality_ = nullptr;
    QPushButton* captureTestButton_ = nullptr;
    QLabel* status_ = nullptr;
    ModelProviderConfig editingModel_;
    ScreenCapture* screenCapture_ = nullptr;
};

} // namespace zhu_screen_pet
