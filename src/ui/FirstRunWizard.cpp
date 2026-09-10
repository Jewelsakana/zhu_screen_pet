#include "ui/FirstRunWizard.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QWizardPage>

#include "app/SettingsController.h"

namespace zhu_screen_pet {

namespace {
QWizardPage* pageWithTitle(QWizard* wizard, const QString& title, const QString& subtitle)
{
    auto* page = new QWizardPage(wizard);
    page->setTitle(title);
    page->setSubTitle(subtitle);
    return page;
}
}

FirstRunWizard::FirstRunWizard(SettingsController* controller, QWidget* parent)
    : QWizard(parent), controller_(controller)
{
    setObjectName(QStringLiteral("firstRunWizard"));
    setWindowTitle(QStringLiteral("欢迎使用小珠看着你"));
    setWizardStyle(QWizard::ModernStyle);
    setOption(QWizard::NoBackButtonOnStartPage);
    resize(560, 460);

    auto* welcome = pageWithTitle(
        this, QStringLiteral("你好呀，我是小珠"),
        QStringLiteral("用几步完成初始配置。以后仍可在设置中修改这些选项。"));
    auto* welcomeLayout = new QVBoxLayout(welcome);
    auto* welcomeText = new QLabel(
        QStringLiteral("默认的 Mock 模型可以离线使用，不需要 API Key。\n"
                       "如果选择远程模型，请准备好对应服务的 API Key。"), welcome);
    welcomeText->setWordWrap(true);
    welcomeLayout->addWidget(welcomeText);
    welcomeLayout->addStretch();
    addPage(welcome);

    auto* personaPage = pageWithTitle(
        this, QStringLiteral("怎么称呼你？"),
        QStringLiteral("这个称呼会用于启动问候和日常对话。"));
    auto* personaForm = new QFormLayout(personaPage);
    userAddress_ = new QLineEdit(personaPage);
    userAddress_->setObjectName(QStringLiteral("onboardingUserAddress"));
    userAddress_->setPlaceholderText(QStringLiteral("例如：主人大人、小主人或你的名字"));
    personaForm->addRow(QStringLiteral("对你的称呼"), userAddress_);
    addPage(personaPage);

    auto* modelPage = pageWithTitle(
        this, QStringLiteral("选择模型"),
        QStringLiteral("可先使用离线 Mock，之后再到设置中切换远程模型。"));
    auto* modelForm = new QFormLayout(modelPage);
    profile_ = new QComboBox(modelPage);
    profile_->setObjectName(QStringLiteral("onboardingModelProfile"));
    baseUrl_ = new QLineEdit(modelPage);
    baseUrl_->setObjectName(QStringLiteral("onboardingModelUrl"));
    modelName_ = new QLineEdit(modelPage);
    modelName_->setObjectName(QStringLiteral("onboardingModelName"));
    apiKey_ = new QLineEdit(modelPage);
    apiKey_->setObjectName(QStringLiteral("onboardingApiKey"));
    apiKey_->setEchoMode(QLineEdit::Password);
    apiKey_->setPlaceholderText(QStringLiteral("远程模型必填；不会保存到配置文件或日志"));
    modelForm->addRow(QStringLiteral("配置档案"), profile_);
    modelForm->addRow(QStringLiteral("模型 URL"), baseUrl_);
    modelForm->addRow(QStringLiteral("模型名称"), modelName_);
    modelForm->addRow(QStringLiteral("API Key"), apiKey_);
    addPage(modelPage);

    auto* privacyPage = pageWithTitle(
        this, QStringLiteral("屏幕截图权限"),
        QStringLiteral("截图权限默认关闭，只有你明确启用后才能捕获屏幕。"));
    auto* privacyLayout = new QVBoxLayout(privacyPage);
    screenCaptureEnabled_ = new QCheckBox(
        QStringLiteral("允许把屏幕截图发送给当前模型"), privacyPage);
    screenCaptureEnabled_->setObjectName(QStringLiteral("onboardingScreenCaptureEnabled"));
    auto* privacyWarning = new QLabel(
        QStringLiteral("注意：启用后，屏幕截图可能会发送给当前配置的远程模型，"
                       "你的隐私信息可能随截图发送到远端。请务必在显示隐私内容前关闭截屏。\n\n"
                       "向导只授予基础截图权限，不会自动开启定时截图或聊天附图。"),
        privacyPage);
    privacyWarning->setWordWrap(true);
    privacyWarning->setStyleSheet(QStringLiteral(
        "background:#fff3cd;color:#6b4d00;border:1px solid #e5c36b;"
        "border-radius:8px;padding:10px;"));
    privacyLayout->addWidget(screenCaptureEnabled_);
    privacyLayout->addWidget(privacyWarning);
    privacyLayout->addStretch();
    addPage(privacyPage);

    auto* systemPage = pageWithTitle(
        this, QStringLiteral("系统设置"),
        QStringLiteral("完成后会立即应用配置。"));
    auto* systemLayout = new QVBoxLayout(systemPage);
    autoStartEnabled_ = new QCheckBox(
        QStringLiteral("登录 Windows 后自动启动小珠看着你"), systemPage);
    autoStartEnabled_->setObjectName(QStringLiteral("onboardingAutoStartEnabled"));
    status_ = new QLabel(systemPage);
    status_->setObjectName(QStringLiteral("onboardingStatus"));
    status_->setWordWrap(true);
    status_->setStyleSheet(QStringLiteral("color:#b42318;"));
    systemLayout->addWidget(autoStartEnabled_);
    systemLayout->addStretch();
    systemLayout->addWidget(status_);
    addPage(systemPage);

    connect(profile_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &FirstRunWizard::loadSelectedProfile);

    if (controller_ != nullptr) {
        userAddress_->setText(controller_->persona().userAddress);
        screenCaptureEnabled_->setChecked(controller_->uiConfig().screenCaptureEnabled);
        autoStartEnabled_->setEnabled(controller_->autoStartSupported());
        autoStartEnabled_->setChecked(controller_->autoStartEnabled());
        if (!controller_->autoStartSupported()) {
            autoStartEnabled_->setToolTip(QStringLiteral("当前系统不支持此功能"));
        }
        populateProfiles();
    } else {
        autoStartEnabled_->setEnabled(false);
        showError(QStringLiteral("设置服务暂不可用"));
    }
}

void FirstRunWizard::populateProfiles()
{
    QString error;
    profile_->clear();
    for (const QString& id : controller_->modelProfileIds(&error)) {
        ModelProviderConfig config;
        if (controller_->loadModelProfile(id, &config, &error)) {
            profile_->addItem(config.displayName, id);
        }
    }
    const int activeIndex = profile_->findData(controller_->activeModel().profileId);
    profile_->setCurrentIndex(activeIndex >= 0 ? activeIndex : 0);
    loadSelectedProfile(profile_->currentIndex());
    if (profile_->count() == 0) {
        showError(error.isEmpty() ? QStringLiteral("没有可用的模型配置") : error);
    }
}

void FirstRunWizard::loadSelectedProfile(int index)
{
    if (controller_ == nullptr || index < 0) return;
    QString error;
    if (!controller_->loadModelProfile(profile_->itemData(index).toString(),
                                       &editingModel_, &error)) {
        showError(error);
        return;
    }
    const bool isMock = editingModel_.providerType == QStringLiteral("mock");
    baseUrl_->setText(editingModel_.baseUrl);
    modelName_->setText(editingModel_.model);
    baseUrl_->setEnabled(!isMock);
    modelName_->setEnabled(!isMock);
    apiKey_->setEnabled(!isMock);
    apiKey_->clear();
}

void FirstRunWizard::accept()
{
    if (controller_ == nullptr || profile_->currentIndex() < 0) {
        showError(QStringLiteral("设置服务或模型配置不可用"));
        return;
    }
    if (userAddress_->text().trimmed().isEmpty()) {
        setCurrentId(1);
        QMessageBox::warning(this, QStringLiteral("缺少称呼"),
                             QStringLiteral("请填写希望小珠如何称呼你。"));
        return;
    }

    ModelProviderConfig model = editingModel_;
    model.baseUrl = baseUrl_->text();
    model.model = modelName_->text();
    PersonaConfig persona = controller_->persona();
    persona.userAddress = userAddress_->text();
    UiConfig ui = controller_->uiConfig();
    ui.screenCaptureEnabled = screenCaptureEnabled_->isChecked();
    ui.automaticScreenAnalysisEnabled = false;
    ui.captureOnChat = false;

    const bool previousAutoStart = controller_->autoStartEnabled();
    const bool requestedAutoStart = autoStartEnabled_->isChecked();
    AppError error;
    if (controller_->autoStartSupported()
        && previousAutoStart != requestedAutoStart
        && !controller_->setAutoStartEnabled(requestedAutoStart, &error)) {
        showError(error.message);
        return;
    }
    if (!controller_->apply(model, persona, controller_->memoryLimits(), ui,
                            apiKey_->text(), &error)) {
        if (controller_->autoStartSupported()
            && previousAutoStart != requestedAutoStart) {
            AppError rollbackError;
            if (!controller_->setAutoStartEnabled(previousAutoStart, &rollbackError)) {
                showError(QStringLiteral("%1；开机自启回滚也失败：%2")
                              .arg(error.message, rollbackError.message));
                return;
            }
        }
        showError(error.message);
        return;
    }
    QWizard::accept();
}

void FirstRunWizard::showError(const QString& message)
{
    if (status_ != nullptr) status_->setText(message);
}

} // namespace zhu_screen_pet
