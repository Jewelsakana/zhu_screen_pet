#include "ui/SettingsDialog.h"

#include <QComboBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "app/SettingsController.h"
#include "infrastructure/ScreenCapture.h"
#include "app/ErrorCenter.h"

namespace zhu_screen_pet {

namespace {
QSpinBox* spin(QWidget* parent, int min, int max)
{
    auto* result = new QSpinBox(parent);
    result->setRange(min, max);
    return result;
}
}

SettingsDialog::SettingsDialog(SettingsController* controller, QWidget* parent,
                               ScreenCapture* screenCapture)
    : QDialog(parent), controller_(controller), screenCapture_(screenCapture)
{
    setWindowTitle(QStringLiteral("设置"));
    setObjectName(QStringLiteral("settingsDialog"));
    resize(560, 780);
    setStyleSheet(QStringLiteral(
        "QDialog#settingsDialog{background:#fffaf0;color:#26375d;}"
        "QGroupBox{background:#fffdf8;border:1px solid #dfd3bd;border-radius:12px;"
        "margin-top:12px;padding:12px 8px 8px 8px;color:#32466f;font-weight:600;}"
        "QGroupBox::title{subcontrol-origin:margin;left:12px;padding:0 5px;}"
        "QLineEdit,QComboBox,QSpinBox{background:white;color:#26375d;border:1px solid #c9d9f1;"
        "border-radius:8px;padding:6px;}"
        "QPushButton{background:#e5efff;color:#36558f;border:1px solid #c9d9f1;"
        "border-radius:9px;padding:7px 12px;} QPushButton:hover{background:#ccdeff;}"
        "QPushButton#settingsApplyButton{background:#79adf3;color:#17345f;border:none;}"));
    auto* root = new QVBoxLayout(this);
    auto* modelBox = new QGroupBox(QStringLiteral("模型"), this);
    auto* modelForm = new QFormLayout(modelBox);
    profile_ = new QComboBox(modelBox);
    profile_->setObjectName(QStringLiteral("settingsModelProfile"));
    providerType_ = new QComboBox(modelBox);
    providerType_->addItems({QStringLiteral("mock"), QStringLiteral("openai-compatible"),
                             QStringLiteral("deepseek")});
    profileId_ = new QLineEdit(modelBox);
    displayName_ = new QLineEdit(modelBox);
    baseUrl_ = new QLineEdit(modelBox);
    baseUrl_->setObjectName(QStringLiteral("settingsModelUrl"));
    model_ = new QLineEdit(modelBox);
    model_->setObjectName(QStringLiteral("settingsModelName"));
    apiKey_ = new QLineEdit(modelBox);
    apiKey_->setObjectName(QStringLiteral("settingsApiKey"));
    apiKey_->setEchoMode(QLineEdit::Password);
    apiKey_->setPlaceholderText(QStringLiteral("留空表示保持 Credential Manager 中的密钥"));
    timeoutMs_ = spin(modelBox, 100, 600000);
    maxRetries_ = spin(modelBox, 0, 10);
    retryDelayMs_ = spin(modelBox, 50, 30000);
    modelForm->addRow(QStringLiteral("配置档案"), profile_);
    modelForm->addRow(QStringLiteral("Provider 类型"), providerType_);
    modelForm->addRow(QStringLiteral("档案 ID"), profileId_);
    modelForm->addRow(QStringLiteral("名称"), displayName_);
    modelForm->addRow(QStringLiteral("模型 URL"), baseUrl_);
    modelForm->addRow(QStringLiteral("模型名称"), model_);
    modelForm->addRow(QStringLiteral("API Key"), apiKey_);
    modelForm->addRow(QStringLiteral("超时（毫秒）"), timeoutMs_);
    modelForm->addRow(QStringLiteral("最大重试次数"), maxRetries_);
    modelForm->addRow(QStringLiteral("重试基础延迟（毫秒）"), retryDelayMs_);

    auto* personaBox = new QGroupBox(QStringLiteral("人格"), this);
    personaBox->setObjectName(QStringLiteral("settingsPersonaGroup"));
    auto* personaForm = new QFormLayout(personaBox);
    userAddress_ = new QLineEdit(personaBox);
    userAddress_->setObjectName(QStringLiteral("settingsUserAddress"));
    userAddress_->setPlaceholderText(QStringLiteral("例如：主人大人、小主人或你的名字"));
    maxReplyTokens_ = spin(personaBox, 1, 32768);
    proactiveLevel_ = spin(personaBox, 0, 3);
    personaForm->addRow(QStringLiteral("对你的称呼"), userAddress_);
    personaForm->addRow(QStringLiteral("最大回复 Token"), maxReplyTokens_);
    personaForm->addRow(QStringLiteral("主动程度（0-3）"), proactiveLevel_);

    auto* memoryBox = new QGroupBox(QStringLiteral("记忆限制"), this);
    auto* memoryForm = new QFormLayout(memoryBox);
    recentLimit_ = spin(memoryBox, MemoryLimits::MinimumRecentMessages,
                        MemoryLimits::MaximumRecentMessages);
    relevantLimit_ = spin(memoryBox, MemoryLimits::MinimumRetrievedItems,
                          MemoryLimits::MaximumRetrievedItems);
    longTermLimit_ = spin(memoryBox, MemoryLimits::MinimumRetrievedItems,
                          MemoryLimits::MaximumRetrievedItems);
    contextTokens_ = spin(memoryBox, MemoryLimits::MinimumContextTokens,
                          MemoryLimits::MaximumContextTokens);
    recentLimit_->setObjectName(QStringLiteral("settingsRecentMessageLimit"));
    relevantLimit_->setObjectName(QStringLiteral("settingsRelevantHistoryLimit"));
    longTermLimit_->setObjectName(QStringLiteral("settingsLongTermMemoryLimit"));
    contextTokens_->setObjectName(QStringLiteral("settingsContextTokenLimit"));
    memoryForm->addRow(QStringLiteral("最近消息条数"), recentLimit_);
    memoryForm->addRow(QStringLiteral("相关历史条数"), relevantLimit_);
    memoryForm->addRow(QStringLiteral("长期记忆条数"), longTermLimit_);
    memoryForm->addRow(QStringLiteral("总上下文 Token"), contextTokens_);

    auto* uiBox = new QGroupBox(QStringLiteral("界面"), this);
    auto* uiForm = new QFormLayout(uiBox);
    bubbleDurationSeconds_ = spin(uiBox, 1, 300);
    bubbleDurationSeconds_->setSuffix(QStringLiteral(" 秒"));
    uiForm->addRow(QStringLiteral("回复气泡显示时间"), bubbleDurationSeconds_);

    auto* captureBox = new QGroupBox(QStringLiteral("屏幕截图"), this);
    auto* captureForm = new QFormLayout(captureBox);
    screenCaptureEnabled_ = new QCheckBox(QStringLiteral("启用屏幕截图"), captureBox);
    screenCaptureEnabled_->setObjectName(QStringLiteral("settingsScreenCaptureEnabled"));
    screenCaptureIntervalSeconds_ = spin(captureBox, 1, 600);
    screenCaptureIntervalSeconds_->setSuffix(QStringLiteral(" 秒"));
    screenCaptureIntervalSeconds_->setObjectName(QStringLiteral("settingsScreenCaptureInterval"));
    captureOnChat_ = new QCheckBox(QStringLiteral("发送对话时额外截图一次"), captureBox);
    captureOnChat_->setObjectName(QStringLiteral("settingsCaptureOnChat"));
    captureImageFormat_ = new QComboBox(captureBox);
    captureImageFormat_->addItem(QStringLiteral("JPEG"), QStringLiteral("jpeg"));
    captureImageFormat_->addItem(QStringLiteral("WebP（不可用时回退 JPEG）"), QStringLiteral("webp"));
    captureImageFormat_->setObjectName(QStringLiteral("settingsCaptureImageFormat"));
    captureMaxWidth_ = spin(captureBox, 320, 8192);
    captureMaxWidth_->setSuffix(QStringLiteral(" px"));
    captureMaxWidth_->setObjectName(QStringLiteral("settingsCaptureMaxWidth"));
    captureQuality_ = spin(captureBox, 1, 100);
    captureQuality_->setSuffix(QStringLiteral("%"));
    captureQuality_->setObjectName(QStringLiteral("settingsCaptureQuality"));
    captureForm->addRow(screenCaptureEnabled_);
    captureForm->addRow(QStringLiteral("自动截图间隔"), screenCaptureIntervalSeconds_);
    captureForm->addRow(captureOnChat_);
    captureForm->addRow(QStringLiteral("图像格式"), captureImageFormat_);
    captureForm->addRow(QStringLiteral("最大图像宽度"), captureMaxWidth_);
    captureForm->addRow(QStringLiteral("压缩质量"), captureQuality_);
    captureTestButton_ = new QPushButton(QStringLiteral("立即截图测试"), captureBox);
    captureTestButton_->setObjectName(QStringLiteral("settingsCaptureTestButton"));
    captureTestButton_->setEnabled(screenCapture_ != nullptr);
    captureForm->addRow(captureTestButton_);
    connect(captureTestButton_, &QPushButton::clicked, this, [this]() {
        if (screenCapture_ == nullptr) return;
        QString error;
        if (screenCapture_->captureNow(&error)) {
            status_->setText(QStringLiteral("截图成功，已保存到 captures 目录"));
        } else {
            status_->setText(QStringLiteral("截图失败：%1").arg(error));
        }
    });

    auto* buttons = new QHBoxLayout();
    auto* test = new QPushButton(QStringLiteral("测试连接"), this);
    auto* apply = new QPushButton(QStringLiteral("应用"), this);
    apply->setObjectName(QStringLiteral("settingsApplyButton"));
    auto* close = new QPushButton(QStringLiteral("关闭"), this);
    status_ = new QLabel(this);
    status_->setObjectName(QStringLiteral("settingsStatus"));
    buttons->addWidget(test);
    buttons->addWidget(status_, 1);
    buttons->addWidget(apply);
    buttons->addWidget(close);
    root->addWidget(modelBox);
    root->addWidget(personaBox);
    root->addWidget(memoryBox);
    root->addWidget(uiBox);
    root->addWidget(captureBox);
    root->addLayout(buttons);
    connect(profile_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::loadSelectedProfile);
    connect(test, &QPushButton::clicked, this, &SettingsDialog::testConnection);
    connect(apply, &QPushButton::clicked, this, &SettingsDialog::applySettings);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    if (controller_ != nullptr) {
        connect(controller_, &SettingsController::connectionTestFinished,
                this, &SettingsDialog::onTestFinished);
        populate();
    }
}

void SettingsDialog::populate()
{
    QString error;
    profile_->clear();
    if (controller_ == nullptr) return;
    for (const QString& id : controller_->modelProfileIds(&error)) {
        ModelProviderConfig config;
        if (controller_->loadModelProfile(id, &config, &error)) {
            profile_->addItem(config.displayName, id);
        }
    }
    const ModelProviderConfig active = controller_->activeModel();
    const int index = profile_->findData(active.profileId);
    profile_->setCurrentIndex(index >= 0 ? index : 0);
    loadSelectedProfile(profile_->currentIndex());
    const PersonaConfig persona = controller_->persona();
    userAddress_->setText(persona.userAddress);
    maxReplyTokens_->setValue(persona.maxReplyTokens);
    proactiveLevel_->setValue(persona.proactiveLevel);
    const MemoryLimits limits = controller_->memoryLimits();
    recentLimit_->setValue(limits.recentMessageLimit);
    relevantLimit_->setValue(limits.relevantHistoryLimit);
    longTermLimit_->setValue(limits.longTermMemoryLimit);
    contextTokens_->setValue(limits.maxContextTokens);
    bubbleDurationSeconds_->setValue(controller_->uiConfig().replyBubbleDurationMs / 1000);
    const UiConfig ui = controller_->uiConfig();
    screenCaptureEnabled_->setChecked(ui.screenCaptureEnabled);
    screenCaptureIntervalSeconds_->setValue(ui.screenCaptureIntervalMs / 1000);
    captureOnChat_->setChecked(ui.captureOnChat);
    captureImageFormat_->setCurrentIndex(captureImageFormat_->findData(ui.captureImageFormat));
    captureMaxWidth_->setValue(ui.captureMaxWidth);
    captureQuality_->setValue(ui.captureQuality);
}

void SettingsDialog::loadSelectedProfile(int index)
{
    if (controller_ == nullptr || index < 0) return;
    ModelProviderConfig config;
    QString error;
    if (!controller_->loadModelProfile(profile_->itemData(index).toString(), &config, &error)) return;
    editingModel_ = config;
    providerType_->setCurrentText(config.providerType);
    profileId_->setText(config.profileId);
    displayName_->setText(config.displayName);
    baseUrl_->setText(config.baseUrl);
    model_->setText(config.model);
    timeoutMs_->setValue(config.timeoutMs);
    maxRetries_->setValue(config.maxRetries);
    retryDelayMs_->setValue(config.retryBaseDelayMs);
    apiKey_->clear();
}

void SettingsDialog::testConnection()
{
    if (controller_ == nullptr) return;
    ModelProviderConfig model = editingModel_;
    model.profileId = profileId_->text(); model.providerType = providerType_->currentText();
    model.displayName = displayName_->text(); model.baseUrl = baseUrl_->text(); model.model = model_->text();
    if (model.providerType != QStringLiteral("mock")) {
        if (model.credentialService.isEmpty()) model.credentialService = QStringLiteral("zhu_screen_pet");
        if (model.profileId != editingModel_.profileId || model.credentialAccount.isEmpty()) {
            model.credentialAccount = profileId_->text() + QStringLiteral("-api-key");
        }
    }
    model.timeoutMs = timeoutMs_->value(); model.maxRetries = maxRetries_->value();
    model.retryBaseDelayMs = retryDelayMs_->value();
    status_->setText(QStringLiteral("测试中…"));
    AppError error;
    if (!controller_->testConnection(model, apiKey_->text(), &error)) showError(error);
}

void SettingsDialog::applySettings()
{
    if (controller_ == nullptr) return;
    ModelProviderConfig model = editingModel_;
    model.profileId = profileId_->text(); model.providerType = providerType_->currentText();
    model.displayName = displayName_->text(); model.baseUrl = baseUrl_->text(); model.model = model_->text();
    if (model.providerType != QStringLiteral("mock")) {
        if (model.credentialService.isEmpty()) model.credentialService = QStringLiteral("zhu_screen_pet");
        if (model.profileId != editingModel_.profileId || model.credentialAccount.isEmpty()) {
            model.credentialAccount = profileId_->text() + QStringLiteral("-api-key");
        }
    }
    model.timeoutMs = timeoutMs_->value(); model.maxRetries = maxRetries_->value();
    model.retryBaseDelayMs = retryDelayMs_->value();
    PersonaConfig persona = controller_->persona();
    persona.userAddress = userAddress_->text();
    persona.maxReplyTokens = maxReplyTokens_->value(); persona.proactiveLevel = proactiveLevel_->value();
    MemoryLimits limits;
    limits.recentMessageLimit = recentLimit_->value(); limits.relevantHistoryLimit = relevantLimit_->value();
    limits.longTermMemoryLimit = longTermLimit_->value(); limits.maxContextTokens = contextTokens_->value();
    UiConfig ui = controller_->uiConfig();
    ui.replyBubbleDurationMs = bubbleDurationSeconds_->value() * 1000;
    ui.screenCaptureEnabled = screenCaptureEnabled_->isChecked();
    ui.screenCaptureIntervalMs = screenCaptureIntervalSeconds_->value() * 1000;
    ui.captureOnChat = captureOnChat_->isChecked();
    ui.captureImageFormat = captureImageFormat_->currentData().toString();
    ui.captureMaxWidth = captureMaxWidth_->value();
    ui.captureQuality = captureQuality_->value();
    AppError error;
    if (!controller_->apply(model, persona, limits, ui, apiKey_->text(), &error)) {
        if (error.code == AppErrorCode::Busy) {
            const auto answer = QMessageBox::question(
                this, QStringLiteral("正在生成回复"),
                QStringLiteral("当前模型请求尚未结束。是否取消这次回复？会话和已有消息会保留，取消完成后可再次点击应用。"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer == QMessageBox::Yes) {
                controller_->cancelActiveChat();
                status_->setText(QStringLiteral("正在取消当前回复，请稍后再次应用"));
                return;
            }
        }
        showError(error); return;
    }
    accept();
}

void SettingsDialog::onTestFinished(bool succeeded, const AppError& error)
{
    status_->setText(succeeded ? QStringLiteral("连接成功") : error.message);
}

void SettingsDialog::showError(const AppError& error)
{
    status_->setText(error.message);
}

} // namespace zhu_screen_pet
