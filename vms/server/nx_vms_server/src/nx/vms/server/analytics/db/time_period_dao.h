#pragma once

#include <map>
#include <optional>

#include <nx/sql/query_context.h>
#include <nx/utils/thread/mutex.h>

#include <analytics/db/analytics_events_storage_types.h>

namespace nx::analytics::db {

class DeviceDao;

namespace detail {

class TimePeriod
{
public:
    long long id = -1;
    long long deviceId = -1;
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
    long long insertOrUpdateTimePeriod(
        nx::sql::QueryContext* queryContext,
        const common::metadata::DetectionMetadataPacket& packet);

    void fillCurrentTimePeriodsCache(nx::sql::QueryContext* queryContext);

    /**
     * NOTE: Can be safely called from any thread.
     */
    std::optional<detail::TimePeriod> getTimePeriodById(long long id) const;

private:
    using DeviceIdToCurrentTimePeriod = std::map<long long, detail::TimePeriod>;
    using IdToTimePeriod = std::map<long long, DeviceIdToCurrentTimePeriod::iterator>;

    DeviceDao* m_deviceDao = nullptr;
    DeviceIdToCurrentTimePeriod m_deviceIdToCurrentTimePeriod;
    IdToTimePeriod m_idToTimePeriod;
    mutable QnMutex m_mutex;

    detail::TimePeriod* insertOrFetchCurrentTimePeriod(
        nx::sql::QueryContext* queryContext,
        long long deviceId,
        std::chrono::milliseconds packetTimestamp,
        std::chrono::milliseconds packetDuration);

    void closeCurrentTimePeriod(
        nx::sql::QueryContext* queryContext,
        long long deviceId);

    void saveTimePeriodEnd(
        nx::sql::QueryContext* queryContext,
        long long id,
        std::chrono::milliseconds endTime);
};

} // namespace nx::analytics::db
