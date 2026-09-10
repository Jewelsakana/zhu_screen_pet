#include "ui/SettingsDialog.h"
#include "ui/MemoryManagementDialog.h"
#include "ui/CaptureUiController.h"
#include "ui/UiScaleMetrics.h"

#include <QComboBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "app/SettingsController.h"
#include "infrastructure/DesktopWindowPolicy.h"
#include "infrastructure/ScreenCapture.h"
#include "infrastructure/WindowPlacement.h"
#include "app/ErrorCenter.h"

namespace zhu_screen_pet {

namespace {
class NoWheelSpinBox final : public QSpinBox
{
public:
    using QSpinBox::QSpinBox;

protected:
    void wheelEvent(QWheelEvent* event) override { event->ignore(); }
};

class NoWheelComboBox final : public QComboBox
{
public:
    using QComboBox::QComboBox;

protected:
    void wheelEvent(QWheelEvent* event) override { event->ignore(); }
};

QSpinBox* spin(QWidget* parent, int min, int max)
{
    auto* result = new NoWheelSpinBox(parent);
    result->setRange(min, max);
    return result;
}

QComboBox* combo(QWidget* parent)
{
    return new NoWheelComboBox(parent);
}

}

SettingsDialog::SettingsDialog(SettingsController* controller, QWidget* parent,
                               ScreenCapture* screenCapture,
                               CaptureUiController* captureUiController)
    : QDialog(parent), controller_(controller), screenCapture_(screenCapture),
      captureUiController_(captureUiController)
{
    setWindowTitle(QStringLiteral("设置"));
    setObjectName(QStringLiteral("settingsDialog"));
    if (captureUiController_ != nullptr) captureUiController_->registerWindow(this);
    const QRect available = QGuiApplication::primaryScreen()
        ? QGuiApplication::primaryScreen()->availableGeometry()
        : QRect(0, 0, 1920, 1080);
    const int initialScale = controller_ == nullptr
        ? 100 : controller_->uiConfig().windowScalePercent;
    const UiScaleMetrics metrics(initialScale);
    resize(metrics.scaledForScreen(QSize(480, 660), available));
    setMinimumSize(metrics.scaledForScreen(QSize(360, 420), available));
    setStyleSheet(QStringLiteral(
        "QDialog#settingsDialog{background:#fffaf0;color:#26375d;}"
        "QScrollArea{background:transparent;border:none;}"
        "QGroupBox{background:#fffdf8;border:1px solid #dfd3bd;border-radius:12px;"
        "margin-top:12px;padding:12px 8px 8px 8px;color:#32466f;font-weight:600;}"
        "QGroupBox::title{subcontrol-origin:margin;left:12px;padding:0 5px;}"
        "QLineEdit,QComboBox,QSpinBox{background:white;color:#26375d;border:1px solid #c9d9f1;"
        "border-radius:8px;padding:6px;}"
        "QPushButton{background:#e5efff;color:#36558f;border:1px solid #c9d9f1;"
        "border-radius:9px;padding:7px 12px;} QPushButton:hover{background:#ccdeff;}"
        "QPushButton#settingsApplyButton{background:#79adf3;color:#17345f;border:none;}"));
    auto* root = new QVBoxLayout(this);
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget(scrollArea);
    scrollArea->setWidget(content);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(8);
    auto* modelBox = new QGroupBox(QStringLiteral("模型"), content);
    auto* modelForm = new QFormLayout(modelBox);
    profile_ = combo(modelBox);
    profile_->setObjectName(QStringLiteral("settingsModelProfile"));
    providerType_ = combo(modelBox);
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
    timeoutMs_ = spin(modelBox, ModelProviderConfig::MinimumTimeoutMs,
                      ModelProviderConfig::MaximumTimeoutMs);
    maxRetries_ = spin(modelBox, ModelProviderConfig::MinimumRetries,
                       ModelProviderConfig::MaximumRetries);
    retryDelayMs_ = spin(modelBox, ModelProviderConfig::MinimumRetryBaseDelayMs,
                         ModelProviderConfig::MaximumRetryBaseDelayMs);
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

    auto* personaBox = new QGroupBox(QStringLiteral("人格"), content);
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

    auto* memoryBox = new QGroupBox(QStringLiteral("记忆限制"), content);
    auto* memoryForm = new QFormLayout(memoryBox);
    recentLimit_ = spin(memoryBox, MemoryLimits::MinimumRecentMessages,
                        MemoryLimits::MaximumRecentMessages);
    relevantLimit_ = spin(memoryBox, MemoryLimits::MinimumRetrievedItems,
                          MemoryLimits::MaximumRetrievedItems);
    longTermLimit_ = spin(memoryBox, MemoryLimits::MinimumRetrievedItems,
                          MemoryLimits::MaximumRetrievedItems);
    contextTokens_ = spin(memoryBox, MemoryLimits::MinimumContextTokens,
                           MemoryLimits::MaximumContextTokens);
    summaryMessageThreshold_ = spin(memoryBox, MemoryLimits::MinimumSummaryMessages,
                                    MemoryLimits::MaximumSummaryMessages);
    summaryTokenThreshold_ = spin(memoryBox, MemoryLimits::MinimumSummaryTokens,
                                  MemoryLimits::MaximumSummaryTokens);
    recentLimit_->setObjectName(QStringLiteral("settingsRecentMessageLimit"));
    relevantLimit_->setObjectName(QStringLiteral("settingsRelevantHistoryLimit"));
    longTermLimit_->setObjectName(QStringLiteral("settingsLongTermMemoryLimit"));
    contextTokens_->setObjectName(QStringLiteral("settingsContextTokenLimit"));
    summaryMessageThreshold_->setObjectName(QStringLiteral("settingsSummaryMessageThreshold"));
    summaryTokenThreshold_->setObjectName(QStringLiteral("settingsSummaryTokenThreshold"));
    memoryForm->addRow(QStringLiteral("最近消息条数"), recentLimit_);
    memoryForm->addRow(QStringLiteral("相关历史条数"), relevantLimit_);
    memoryForm->addRow(QStringLiteral("长期记忆条数"), longTermLimit_);
    memoryForm->addRow(QStringLiteral("总上下文 Token"), contextTokens_);
    memoryForm->addRow(QStringLiteral("自动摘要消息阈值"), summaryMessageThreshold_);
    memoryForm->addRow(QStringLiteral("自动摘要 Token 阈值"), summaryTokenThreshold_);
    auto* manageMemories = new QPushButton(QStringLiteral("查看和管理具体记忆"), memoryBox);
    manageMemories->setObjectName(QStringLiteral("settingsManageMemoriesButton"));
    memoryForm->addRow(manageMemories);
    connect(manageMemories, &QPushButton::clicked, this, [this]() {
        MemoryManagementDialog dialog(controller_, this); dialog.exec();
    });

    auto* uiBox = new QGroupBox(QStringLiteral("界面"), content);
    auto* uiForm = new QFormLayout(uiBox);
    bubbleDurationSeconds_ = spin(uiBox, 1, 300);
    bubbleDurationSeconds_->setSuffix(QStringLiteral(" 秒"));
    windowScalePercent_ = spin(uiBox, UiConfig::MinimumWindowScalePercent,
                               UiConfig::MaximumWindowScalePercent);
    windowScalePercent_->setSingleStep(5);
    windowScalePercent_->setSuffix(QStringLiteral("%"));
    windowScalePercent_->setObjectName(QStringLiteral("settingsMainWindowScale"));
    uiForm->addRow(QStringLiteral("回复气泡显示时间"), bubbleDurationSeconds_);
    uiForm->addRow(QStringLiteral("主窗口及附属窗口缩放"), windowScalePercent_);

    auto* systemBox = new QGroupBox(QStringLiteral("系统"), content);
    auto* systemForm = new QFormLayout(systemBox);
    autoStartEnabled_ = new QCheckBox(
        QStringLiteral("登录 Windows 后自动启动小珠看着你"), systemBox);
    autoStartEnabled_->setObjectName(QStringLiteral("settingsAutoStartEnabled"));
    autoStartEnabled_->setEnabled(controller_ != nullptr
                                  && controller_->autoStartSupported());
    if (!autoStartEnabled_->isEnabled()) {
        autoStartEnabled_->setToolTip(QStringLiteral("当前系统不支持此功能"));
    }
    systemForm->addRow(autoStartEnabled_);

    auto* captureBox = new QGroupBox(QStringLiteral("屏幕截图"), content);
    auto* captureForm = new QFormLayout(captureBox);
    screenCaptureEnabled_ = new QCheckBox(
        QStringLiteral("允许把屏幕截图发送给当前模型"), captureBox);
    screenCaptureEnabled_->setObjectName(QStringLiteral("settingsScreenCaptureEnabled"));
    auto* capturePermissionRow = new QWidget(captureBox);
    auto* capturePermissionLayout = new QHBoxLayout(capturePermissionRow);
    capturePermissionLayout->setContentsMargins(0, 0, 0, 0);
    capturePermissionLayout->setSpacing(6);
    auto* capturePrivacyHelp = new QPushButton(QStringLiteral("?"), capturePermissionRow);
    capturePrivacyHelp->setObjectName(QStringLiteral("settingsCapturePrivacyHelp"));
    capturePrivacyHelp->setAccessibleName(QStringLiteral("查看屏幕截图隐私提醒"));
    capturePrivacyHelp->setToolTip(QStringLiteral("查看屏幕截图隐私提醒"));
    capturePrivacyHelp->setFixedSize(24, 24);
    capturePermissionLayout->addWidget(screenCaptureEnabled_);
    capturePermissionLayout->addWidget(capturePrivacyHelp);
    capturePermissionLayout->addStretch();
    automaticScreenAnalysisEnabled_ = new QCheckBox(
        QStringLiteral("定时截图并自动发送给模型（可能产生费用）"), captureBox);
    automaticScreenAnalysisEnabled_->setObjectName(
        QStringLiteral("settingsAutomaticScreenAnalysisEnabled"));
    auto* automaticCaptureRow = new QWidget(captureBox);
    auto* automaticCaptureLayout = new QHBoxLayout(automaticCaptureRow);
    automaticCaptureLayout->setContentsMargins(0, 0, 0, 0);
    automaticCaptureLayout->setSpacing(6);
    auto* automaticCaptureHelp = new QPushButton(QStringLiteral("?"), automaticCaptureRow);
    automaticCaptureHelp->setObjectName(QStringLiteral("settingsAutomaticCaptureHelp"));
    automaticCaptureHelp->setAccessibleName(QStringLiteral("查看定时截图费用提醒"));
    automaticCaptureHelp->setToolTip(QStringLiteral(
        "如果截图间隔太短，可能会产生大量的费用且占据更多的对话上下文，"
        "推荐将截图间隔至少在60s以上。"));
    automaticCaptureHelp->setFixedSize(24, 24);
    automaticCaptureLayout->addWidget(automaticScreenAnalysisEnabled_);
    automaticCaptureLayout->addWidget(automaticCaptureHelp);
    automaticCaptureLayout->addStretch();
    screenCaptureIntervalSeconds_ = spin(
        captureBox, UiConfig::MinimumScreenCaptureIntervalMs / 1000,
        UiConfig::MaximumScreenCaptureIntervalMs / 1000);
    screenCaptureIntervalSeconds_->setSuffix(QStringLiteral(" 秒"));
    screenCaptureIntervalSeconds_->setObjectName(QStringLiteral("settingsScreenCaptureInterval"));
    captureOnChat_ = new QCheckBox(
        QStringLiteral("随本次用户消息附带截图（不额外发起请求）"), captureBox);
    captureOnChat_->setObjectName(QStringLiteral("settingsCaptureOnChat"));
    includeOwnWindowsInCapture_ = new QCheckBox(
        QStringLiteral("允许桌宠及其窗口出现在截图中"), captureBox);
    includeOwnWindowsInCapture_->setObjectName(
        QStringLiteral("settingsIncludeOwnWindowsInCapture"));
    captureExclusionStatus_ = new QLabel(captureBox);
    captureExclusionStatus_->setObjectName(QStringLiteral("settingsCaptureExclusionStatus"));
    captureExclusionStatus_->setWordWrap(true);
    captureImageFormat_ = combo(captureBox);
    captureImageFormat_->addItem(QStringLiteral("JPEG"), QStringLiteral("jpeg"));
    captureImageFormat_->addItem(QStringLiteral("WebP（不可用时回退 JPEG）"), QStringLiteral("webp"));
    captureImageFormat_->setObjectName(QStringLiteral("settingsCaptureImageFormat"));
    captureMaxWidth_ = spin(captureBox, 320, 8192);
    captureMaxWidth_->setSuffix(QStringLiteral(" px"));
    captureMaxWidth_->setObjectName(QStringLiteral("settingsCaptureMaxWidth"));
    captureQuality_ = spin(captureBox, 1, 100);
    captureQuality_->setSuffix(QStringLiteral("%"));
    captureQuality_->setObjectName(QStringLiteral("settingsCaptureQuality"));
    captureForm->addRow(capturePermissionRow);
    captureForm->addRow(automaticCaptureRow);
    captureForm->addRow(QStringLiteral("自动截图间隔"), screenCaptureIntervalSeconds_);
    captureForm->addRow(captureOnChat_);
    captureForm->addRow(includeOwnWindowsInCapture_);
    captureForm->addRow(QStringLiteral("自身窗口保护状态"), captureExclusionStatus_);
    captureForm->addRow(QStringLiteral("图像格式"), captureImageFormat_);
    captureForm->addRow(QStringLiteral("最大图像宽度"), captureMaxWidth_);
    captureForm->addRow(QStringLiteral("压缩质量"), captureQuality_);
    captureTestButton_ = new QPushButton(QStringLiteral("立即截图测试"), captureBox);
    captureTestButton_->setObjectName(QStringLiteral("settingsCaptureTestButton"));
    captureTestButton_->setEnabled(screenCapture_ != nullptr);
    captureForm->addRow(captureTestButton_);
    connect(capturePrivacyHelp, &QPushButton::clicked, this, [this]() {
        QMessageBox::information(
            this, QStringLiteral("屏幕截图隐私提醒"),
            QStringLiteral("注意：启用后，屏幕截图会发送给当前配置的远程模型。"
                           "你的隐私信息可能会被截图并发送到远端，"
                           "一定要注意在隐私页面关闭截屏。"),
            QMessageBox::Ok);
    });
    connect(automaticCaptureHelp, &QPushButton::clicked, this, [this]() {
        QMessageBox::information(
            this, QStringLiteral("定时截图费用提醒"),
            QStringLiteral("如果截图间隔太短，可能会产生大量的费用且占据更多的对话上下文，"
                           "推荐将截图间隔至少在60s以上。"),
            QMessageBox::Ok);
    });
    connect(captureTestButton_, &QPushButton::clicked, this, [this]() {
        if (screenCapture_ == nullptr) return;
        QString error;
        if (screenCapture_->captureNow(&error)) {
            status_->setText(QStringLiteral("截图成功，已保存到 captures 目录"));
        } else {
            status_->setText(QStringLiteral("截图失败：%1").arg(error));
        }
    });
    connect(screenCaptureEnabled_, &QCheckBox::toggled, this, [this](bool enabled) {
        automaticScreenAnalysisEnabled_->setEnabled(enabled);
        captureOnChat_->setEnabled(enabled);
        if (!enabled) {
            automaticScreenAnalysisEnabled_->setChecked(false);
            captureOnChat_->setChecked(false);
        }
    });
    connect(includeOwnWindowsInCapture_, &QCheckBox::toggled,
            this, [this]() { updateCaptureExclusionStatus(); });

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
    contentLayout->addWidget(modelBox);
    contentLayout->addWidget(personaBox);
    contentLayout->addWidget(memoryBox);
    contentLayout->addWidget(uiBox);
    contentLayout->addWidget(systemBox);
    contentLayout->addWidget(captureBox);
    root->addWidget(scrollArea, 1);
    root->addLayout(buttons);
    connect(profile_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::loadSelectedProfile);
    connect(test, &QPushButton::clicked, this, &SettingsDialog::testConnection);
    connect(apply, &QPushButton::clicked, this, &SettingsDialog::applySettings);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    if (controller_ != nullptr) {
        connect(controller_, &SettingsController::connectionTestFinished,
                this, &SettingsDialog::onTestFinished);
        connect(controller_, &SettingsController::uiConfigurationChanged,
                this, [this](const UiConfig& ui) {
                    const QSignalBlocker scaleBlocker(windowScalePercent_);
                    const QSignalBlocker captureBlocker(screenCaptureEnabled_);
                    const QSignalBlocker automaticBlocker(automaticScreenAnalysisEnabled_);
                    const QSignalBlocker chatBlocker(captureOnChat_);
                    windowScalePercent_->setValue(ui.windowScalePercent);
                    screenCaptureEnabled_->setChecked(ui.screenCaptureEnabled);
                    automaticScreenAnalysisEnabled_->setChecked(
                        ui.automaticScreenAnalysisEnabled);
                    automaticScreenAnalysisEnabled_->setEnabled(ui.screenCaptureEnabled);
                    captureOnChat_->setChecked(ui.captureOnChat);
                    captureOnChat_->setEnabled(ui.screenCaptureEnabled);
                    includeOwnWindowsInCapture_->setChecked(
                        !ui.excludeOwnWindowsFromCapture);
                    updateCaptureExclusionStatus();
                });
        populate();
    }
    if (captureUiController_ != nullptr) {
        connect(captureUiController_, &CaptureUiController::ownWindowExclusionStatusChanged,
                this, [this]() { updateCaptureExclusionStatus(); });
    }
    updateCaptureExclusionStatus();
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
    summaryMessageThreshold_->setValue(limits.summaryMessageThreshold);
    summaryTokenThreshold_->setValue(limits.summaryTokenThreshold);
    bubbleDurationSeconds_->setValue(controller_->uiConfig().replyBubbleDurationMs / 1000);
    const UiConfig ui = controller_->uiConfig();
    windowScalePercent_->setValue(ui.windowScalePercent);
    autoStartEnabled_->setChecked(controller_->autoStartEnabled());
    screenCaptureEnabled_->setChecked(ui.screenCaptureEnabled);
    automaticScreenAnalysisEnabled_->setChecked(ui.automaticScreenAnalysisEnabled);
    automaticScreenAnalysisEnabled_->setEnabled(ui.screenCaptureEnabled);
    captureOnChat_->setEnabled(ui.screenCaptureEnabled);
    screenCaptureIntervalSeconds_->setValue(ui.screenCaptureIntervalMs / 1000);
    captureOnChat_->setChecked(ui.captureOnChat);
    includeOwnWindowsInCapture_->setChecked(!ui.excludeOwnWindowsFromCapture);
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
    limits.summaryMessageThreshold = summaryMessageThreshold_->value();
    limits.summaryTokenThreshold = summaryTokenThreshold_->value();
    UiConfig ui = controller_->uiConfig();
    ui.replyBubbleDurationMs = bubbleDurationSeconds_->value() * 1000;
    ui.windowScalePercent = windowScalePercent_->value();
    ui.screenCaptureEnabled = screenCaptureEnabled_->isChecked();
    ui.automaticScreenAnalysisEnabled = automaticScreenAnalysisEnabled_->isChecked();
    ui.screenCaptureIntervalMs = screenCaptureIntervalSeconds_->value() * 1000;
    ui.captureOnChat = captureOnChat_->isChecked();
    ui.excludeOwnWindowsFromCapture = !includeOwnWindowsInCapture_->isChecked();
    ui.captureImageFormat = captureImageFormat_->currentData().toString();
    ui.captureMaxWidth = captureMaxWidth_->value();
    ui.captureQuality = captureQuality_->value();
    const UiConfig previousUi = controller_->uiConfig();
    const bool previousAutoStart = controller_->autoStartEnabled();
    const bool requestedAutoStart = autoStartEnabled_->isChecked();
    const bool enablesRemoteCapture = ui.screenCaptureEnabled
        && ((ui.automaticScreenAnalysisEnabled
             && !previousUi.automaticScreenAnalysisEnabled)
            || (ui.captureOnChat && !previousUi.captureOnChat));
    if (enablesRemoteCapture
        && QMessageBox::warning(
            this, QStringLiteral("确认发送屏幕内容"),
            QStringLiteral("启用后，屏幕截图会发送给当前配置的远端模型，可能包含隐私信息并产生模型费用。是否继续？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    AppError error;
    if (controller_->autoStartSupported()
        && previousAutoStart != requestedAutoStart
        && !controller_->setAutoStartEnabled(requestedAutoStart, &error)) {
        showError(error);
        return;
    }
    if (!controller_->apply(model, persona, limits, ui, apiKey_->text(), &error)) {
        if (controller_->autoStartSupported()
            && previousAutoStart != requestedAutoStart) {
            AppError rollbackError;
            if (!controller_->setAutoStartEnabled(previousAutoStart, &rollbackError)) {
                error.message += QStringLiteral("；开机自启回滚失败：%1")
                    .arg(rollbackError.message);
            }
        }
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

void SettingsDialog::updateCaptureExclusionStatus()
{
    if (captureExclusionStatus_ == nullptr) return;
    if (includeOwnWindowsInCapture_ != nullptr
        && includeOwnWindowsInCapture_->isChecked()) {
        captureExclusionStatus_->setText(QStringLiteral("已允许自身窗口进入截图"));
        return;
    }
    if (captureUiController_ == nullptr) {
        captureExclusionStatus_->setText(QStringLiteral("尚未检测自身窗口保护能力"));
    } else if (captureUiController_->ownWindowExclusionAvailable()) {
        captureExclusionStatus_->setText(QStringLiteral("可用：桌宠自身窗口不会进入截图"));
    } else {
        captureExclusionStatus_->setText(QStringLiteral(
            "不可用：定时截图将被关闭。%1")
            .arg(captureUiController_->ownWindowExclusionDetail()));
    }
}

} // namespace zhu_screen_pet
