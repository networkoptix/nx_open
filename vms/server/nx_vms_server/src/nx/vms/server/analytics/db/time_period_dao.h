#pragma once

#include <map>
#include <optional>

#include <nx/sql/query_context.h>
#include <nx/utils/thread/mutex.h>

#include <analytics/db/analytics_db_types.h>

namespace nx::analytics::db {

class DeviceDao;

namespace detail {

class TimePeriod
{
public:
    int64_t id = -1;
    int64_t deviceId = -1;
    std::chrono::milliseconds startTime = std::chrono::milliseconds::zero();
    std::chrono::milliseconds endTime = std::chrono::milliseconds::zero();
    std::chrono::milliseconds lastSavedEndTime = std::chrono::milliseconds::zero();

    std::chrono::milliseconds length() const;

    bool addPacketToPeriod(
        std::chrono::milliseconds timestamp,
        std::chrono::milliseconds duration);
};

} // namespace detail

//-------------------------------------------------------------------------------------------------

/**
 * All DB updates are expected to be called from one thread at a time
 * (sqlite does not support concurrent updates).
 */
class TimePeriodDao
{
public:
    TimePeriodDao(DeviceDao* deviceDao);

    /**
     * @return Id of the current time period.
     */
    int64_t insertOrUpdateTimePeriod(
        nx::sql::QueryContext* queryContext,
        const common::metadata::DetectionMetadataPacket& packet);

    void fillCurrentTimePeriodsCache(nx::sql::QueryContext* queryContext);

    /**
     * NOTE: Can be safely called from any thread.
     */
    std::optional<detail::TimePeriod> getTimePeriodById(int64_t id) const;

private:
    using DeviceIdToCurrentTimePeriod = std::map<int64_t, detail::TimePeriod>;
    using IdToTimePeriod = std::map<int64_t, DeviceIdToCurrentTimePeriod::iterator>;

    DeviceDao* m_deviceDao = nullptr;
    DeviceIdToCurrentTimePeriod m_deviceIdToCurrentTimePeriod;
    IdToTimePeriod m_idToTimePeriod;
    mutable QnMutex m_mutex;

    detail::TimePeriod* insertOrFetchCurrentTimePeriod(
        nx::sql::QueryContext* queryContext,
        int64_t deviceId,
        std::chrono::milliseconds packetTimestamp,
        std::chrono::milliseconds packetDuration);

    void closeCurrentTimePeriod(
        nx::sql::QueryContext* queryContext,
        int64_t deviceId);

    void saveTimePeriodEnd(
        nx::sql::QueryContext* queryContext,
        int64_t id,
        std::chrono::milliseconds endTime);
};

} // namespace nx::analytics::db
