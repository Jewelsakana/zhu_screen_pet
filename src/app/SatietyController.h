#pragma once

#include <QElapsedTimer>
#include <QObject>

class QTimer;

namespace zhu_screen_pet {

class SettingsRepository;

/** 管理仅在应用运行期间下降的饱食度，每 30 分钟降低 5 点。 */
class SatietyController final : public QObject
{
    Q_OBJECT

public:
    static constexpr int MinimumValue = 0;
    static constexpr int MaximumValue = 100;
    static constexpr int DecayAmount = 5;
    static constexpr int DecayIntervalSeconds = 30 * 60;
    static constexpr int RefreshIntervalMs = 60 * 1000;
    static constexpr int SaveIntervalMs = 10 * 60 * 1000;

    explicit SatietyController(SettingsRepository* settings,
                               QObject* parent = nullptr);

    bool initialize(QString* errorMessage = nullptr);
    void start();
    bool shutdown(QString* errorMessage = nullptr);
    bool increase(int amount, QString* errorMessage = nullptr);
    int value() const;

signals:
    void valueChanged(int value);
    void persistenceFailed(const QString& detail);

private:
    void reconcileElapsed();
    void applyElapsedSeconds(qint64 seconds);
    bool save(QString* errorMessage = nullptr);

    SettingsRepository* settings_ = nullptr;
    QTimer* refreshTimer_ = nullptr;
    QTimer* saveTimer_ = nullptr;
    QElapsedTimer activeElapsed_;
    int value_ = MaximumValue;
    qint64 elapsedSeconds_ = 0;
    bool initialized_ = false;
    bool running_ = false;
};

} // namespace zhu_screen_pet
