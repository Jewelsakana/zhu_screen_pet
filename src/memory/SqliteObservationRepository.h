#pragma once

#include "memory/ObservationRepository.h"

namespace zhu_screen_pet {

class Database;

/** SQLite 屏幕观察仓库。 */
class SqliteObservationRepository final : public ObservationRepository
{
public:
    explicit SqliteObservationRepository(Database* database);

    Result<QString> saveResult(const ObservationEvent& observation) override;
    Result<std::optional<ObservationEvent>> latestValidResult(
        const QString& conversationId, const QDateTime& at) const override;
    Result<void> removeExpiredResult(const QDateTime& at) override;

private:
    Database* database_ = nullptr;
};

} // namespace zhu_screen_pet
