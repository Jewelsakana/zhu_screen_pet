#pragma once

#include <QObject>

class QTimer;

namespace zhu_screen_pet {

class InputActivityMonitor;
class SettingsRepository;

/** 在内存中累计匿名输入次数，并按周期持久化到本地设置。 */
class InputActivityController final : public QObject
{
    Q_OBJECT

public:
    static constexpr int SaveIntervalMs = 10 * 60 * 1000;
    static constexpr qint64 MaximumCount = 100000;

    InputActivityController(InputActivityMonitor* monitor,
                            SettingsRepository* settings,
                            QObject* parent = nullptr);

    bool initialize(QString* errorMessage = nullptr);
    bool start(QString* errorMessage = nullptr);
    bool shutdown(QString* errorMessage = nullptr);
    qint64 inputCount() const;
    bool trySpend(qint64 amount, QString* errorMessage = nullptr);
    bool refund(qint64 amount, QString* errorMessage = nullptr);

signals:
    void countChanged(qint64 inputCount);
    void persistenceFailed(const QString& detail);

private:
    void recordInput();
    bool save(QString* errorMessage = nullptr);

    InputActivityMonitor* monitor_ = nullptr;
    SettingsRepository* settings_ = nullptr;
    QTimer* saveTimer_ = nullptr;
    QTimer* publishTimer_ = nullptr;
    qint64 inputCount_ = 0;
    bool initialized_ = false;
    bool running_ = false;
};

} // namespace zhu_screen_pet
