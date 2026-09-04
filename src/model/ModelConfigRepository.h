#pragma once

#include <QJsonObject>
#include <QByteArray>
#include <QStringList>

#include "model/ModelProviderConfig.h"

namespace zhu_screen_pet {

/** 从独立 JSON 文件读取和原子保存模型配置档案。 */
class ModelConfigRepository final
{
public:
    /** 绑定磁盘上的模型配置文件；仓库不提供厂商默认配置。 */
    explicit ModelConfigRepository(QString filePath);

    QString filePath() const;
    QStringList profileIds(QString* errorMessage = nullptr) const;
    bool loadProfile(const QString& profileId, ModelProviderConfig* config,
                     QString* errorMessage = nullptr) const;
    bool loadActive(ModelProviderConfig* config, QString* errorMessage = nullptr) const;
    bool saveProfile(const ModelProviderConfig& config, bool setActive = true,
                     QString* errorMessage = nullptr);
    bool setActiveProfile(const QString& profileId, QString* errorMessage = nullptr);
    QString activeProfileId(QString* errorMessage = nullptr) const;
    /** 获取/恢复完整文件快照，供跨配置保存失败时执行事务补偿。 */
    bool snapshot(QByteArray* data, QString* errorMessage = nullptr) const;
    bool restore(const QByteArray& data, QString* errorMessage = nullptr) const;

private:
    bool readRoot(QJsonObject* root, QString* errorMessage) const;
    bool writeRoot(const QJsonObject& root, QString* errorMessage) const;
    bool configFromJson(const QJsonObject& object, ModelProviderConfig* config,
                        QString* errorMessage) const;
    QJsonObject configToJson(const ModelProviderConfig& config) const;

    QString filePath_;
};

} // namespace zhu_screen_pet
