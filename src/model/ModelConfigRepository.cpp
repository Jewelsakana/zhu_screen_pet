#include "model/ModelConfigRepository.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>

#include <utility>

namespace zhu_screen_pet {

ModelConfigRepository::ModelConfigRepository(QString filePath)
    : filePath_(std::move(filePath))
{
}

QString ModelConfigRepository::filePath() const
{
    return filePath_;
}

QStringList ModelConfigRepository::profileIds(QString* errorMessage) const
{
    QJsonObject root;
    if (!readRoot(&root, errorMessage)) return {};
    QStringList result;
    for (const QJsonValue& value : root.value(QStringLiteral("profiles")).toArray()) {
        if (!value.isObject()) continue;
        const QString id = value.toObject().value(QStringLiteral("id")).toString().trimmed();
        if (!id.isEmpty() && !result.contains(id)) result.append(id);
    }
    return result;
}

bool ModelConfigRepository::loadProfile(const QString& profileId, ModelProviderConfig* config,
                                        QString* errorMessage) const
{
    if (config == nullptr) {
        if (errorMessage) *errorMessage = QStringLiteral("model configuration output is null");
        return false;
    }
    QJsonObject root;
    if (!readRoot(&root, errorMessage)) return false;
    const QString wanted = profileId.trimmed();
    for (const QJsonValue& value : root.value(QStringLiteral("profiles")).toArray()) {
        if (!value.isObject()) continue;
        const QJsonObject object = value.toObject();
        if (object.value(QStringLiteral("id")).toString().trimmed() == wanted) {
            return configFromJson(object, config, errorMessage);
        }
    }
    if (errorMessage) *errorMessage = QStringLiteral("model profile does not exist: %1").arg(wanted);
    return false;
}

bool ModelConfigRepository::loadActive(ModelProviderConfig* config, QString* errorMessage) const
{
    const QString id = activeProfileId(errorMessage);
    return !id.isEmpty() && loadProfile(id, config, errorMessage);
}

bool ModelConfigRepository::saveProfile(const ModelProviderConfig& source, bool setActive,
                                        QString* errorMessage)
{
    const ModelProviderConfig config = source.normalized();
    if (!config.validate(errorMessage)) return false;

    QJsonObject root;
    if (QFile::exists(filePath_)) {
        if (!readRoot(&root, errorMessage)) return false;
    } else {
        root.insert(QStringLiteral("version"), 1);
        root.insert(QStringLiteral("profiles"), QJsonArray{});
    }

    QJsonArray profiles = root.value(QStringLiteral("profiles")).toArray();
    bool replaced = false;
    for (int i = 0; i < profiles.size(); ++i) {
        const QJsonObject current = profiles.at(i).toObject();
        if (current.value(QStringLiteral("id")).toString() == config.profileId) {
            profiles.replace(i, configToJson(config));
            replaced = true;
            break;
        }
    }
    if (!replaced) profiles.append(configToJson(config));
    root.insert(QStringLiteral("profiles"), profiles);
    if (setActive) root.insert(QStringLiteral("active_profile"), config.profileId);
    return writeRoot(root, errorMessage);
}

bool ModelConfigRepository::setActiveProfile(const QString& profileId, QString* errorMessage)
{
    QJsonObject root;
    if (!readRoot(&root, errorMessage)) return false;
    const QString wanted = profileId.trimmed();
    bool found = false;
    for (const QJsonValue& value : root.value(QStringLiteral("profiles")).toArray()) {
        if (value.toObject().value(QStringLiteral("id")).toString().trimmed() == wanted) {
            found = true;
            break;
        }
    }
    if (!found) {
        if (errorMessage) *errorMessage = QStringLiteral("model profile does not exist: %1").arg(wanted);
        return false;
    }
    root.insert(QStringLiteral("active_profile"), wanted);
    return writeRoot(root, errorMessage);
}

QString ModelConfigRepository::activeProfileId(QString* errorMessage) const
{
    QJsonObject root;
    if (!readRoot(&root, errorMessage)) return {};
    const QString id = root.value(QStringLiteral("active_profile")).toString().trimmed();
    if (id.isEmpty() && errorMessage) *errorMessage = QStringLiteral("active_profile is missing");
    return id;
}

bool ModelConfigRepository::snapshot(QByteArray* data, QString* errorMessage) const
{
    if (data == nullptr) {
        if (errorMessage) *errorMessage = QStringLiteral("model configuration snapshot output is null");
        return false;
    }
    QFile file(filePath_);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) *errorMessage = QStringLiteral("cannot read model configuration snapshot: %1")
            .arg(file.errorString());
        return false;
    }
    *data = file.readAll();
    return true;
}

bool ModelConfigRepository::restore(const QByteArray& data, QString* errorMessage) const
{
    QSaveFile file(filePath_);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        if (errorMessage) *errorMessage = QStringLiteral("cannot restore model configuration: %1")
            .arg(file.errorString());
        return false;
    }
    return true;
}

bool ModelConfigRepository::readRoot(QJsonObject* root, QString* errorMessage) const
{
    if (root == nullptr || filePath_.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("model configuration path is empty");
        return false;
    }
    QFile file(filePath_);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("cannot open model configuration: %1").arg(file.errorString());
        }
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("invalid model configuration JSON: %1")
                                .arg(parseError.errorString());
        }
        return false;
    }
    *root = document.object();
    if (root->value(QStringLiteral("version")).toInt() != 1
        || !root->value(QStringLiteral("profiles")).isArray()) {
        if (errorMessage) *errorMessage = QStringLiteral("unsupported model configuration schema");
        return false;
    }
    return true;
}

bool ModelConfigRepository::writeRoot(const QJsonObject& root, QString* errorMessage) const
{
    QSaveFile file(filePath_);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("cannot write model configuration: %1").arg(file.errorString());
        }
        return false;
    }
    if (file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0 || !file.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("cannot commit model configuration: %1").arg(file.errorString());
        }
        return false;
    }
    return true;
}

bool ModelConfigRepository::configFromJson(const QJsonObject& object,
                                           ModelProviderConfig* config,
                                           QString* errorMessage) const
{
    if (object.contains(QStringLiteral("api_key")) || object.contains(QStringLiteral("token"))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("API secrets must be stored in SecretStore, not in model JSON");
        }
        return false;
    }
    ModelProviderConfig result;
    result.profileId = object.value(QStringLiteral("id")).toString();
    result.providerType = object.value(QStringLiteral("provider_type")).toString();
    result.displayName = object.value(QStringLiteral("display_name")).toString();
    result.baseUrl = object.value(QStringLiteral("base_url")).toString();
    result.model = object.value(QStringLiteral("model")).toString();
    result.credentialService = object.value(QStringLiteral("credential_service")).toString();
    result.credentialAccount = object.value(QStringLiteral("credential_account")).toString();
    result.mockReply = object.value(QStringLiteral("mock_reply")).toString();
    if (object.contains(QStringLiteral("timeout_ms"))) {
        result.timeoutMs = object.value(QStringLiteral("timeout_ms")).toInt();
    }
    if (object.contains(QStringLiteral("max_retries"))) {
        result.maxRetries = object.value(QStringLiteral("max_retries")).toInt(-1);
    }
    if (object.contains(QStringLiteral("retry_base_delay_ms"))) {
        result.retryBaseDelayMs = object.value(QStringLiteral("retry_base_delay_ms")).toInt();
    }
    result = result.normalized();
    if (!result.validate(errorMessage)) return false;
    *config = result;
    return true;
}

QJsonObject ModelConfigRepository::configToJson(const ModelProviderConfig& config) const
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), config.profileId);
    object.insert(QStringLiteral("provider_type"), config.providerType);
    object.insert(QStringLiteral("display_name"), config.displayName);
    object.insert(QStringLiteral("base_url"), config.baseUrl);
    object.insert(QStringLiteral("model"), config.model);
    object.insert(QStringLiteral("credential_service"), config.credentialService);
    object.insert(QStringLiteral("credential_account"), config.credentialAccount);
    object.insert(QStringLiteral("mock_reply"), config.mockReply);
    object.insert(QStringLiteral("timeout_ms"), config.timeoutMs);
    object.insert(QStringLiteral("max_retries"), config.maxRetries);
    object.insert(QStringLiteral("retry_base_delay_ms"), config.retryBaseDelayMs);
    return object;
}

} // namespace zhu_screen_pet
