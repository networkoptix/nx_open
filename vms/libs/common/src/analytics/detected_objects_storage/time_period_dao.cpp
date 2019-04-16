#include "time_period_dao.h"

#include "device_dao.h"

namespace nx::analytics::storage {

static constexpr auto kMaxTimePeriodLength = std::chrono::minutes(10);
static constexpr auto kTimePeriodPreemptiveLength = std::chrono::minutes(1);

//-------------------------------------------------------------------------------------------------

namespace detail {

std::chrono::milliseconds TimePeriod::length() const
{
    return endTime - startTime;
}

bool TimePeriod::addPacketToPeriod(
    std::chrono::milliseconds timestamp,
    std::chrono::milliseconds duration)
{
    if (length() > kMaxTimePeriodLength ||
        startTime > timestamp || //< Packet from the past?
        (timestamp > endTime && timestamp - endTime > kMinTimePeriodAggregationPeriod))
    {
        return false;
    }

    if (endTime < timestamp + duration)
        endTime = timestamp + duration;

    return true;
}

} // namespace detail

//-------------------------------------------------------------------------------------------------

TimePeriodDao::TimePeriodDao(DeviceDao* deviceDao):
    m_deviceDao(deviceDao)
{
}

long long TimePeriodDao::insertOrUpdateTimePeriod(
    nx::sql::QueryContext* queryContext,
    const common::metadata::DetectionMetadataPacket& packet)
{
    using namespace std::chrono;

    const auto packetTimestamp = duration_cast<milliseconds>(microseconds(packet.timestampUsec));
    const auto packetDuration = duration_cast<milliseconds>(microseconds(packet.durationUsec));
    const auto deviceId = m_deviceDao->deviceIdFromGuid(packet.deviceId);

    auto currentPeriod = insertOrFetchCurrentTimePeriod(
        queryContext, deviceId, packetTimestamp, packetDuration);

    if (!currentPeriod->addPacketToPeriod(packetTimestamp, packetDuration))
    {
        closeCurrentTimePeriod(queryContext, deviceId);

        // Adding new period.
        currentPeriod = insertOrFetchCurrentTimePeriod(
            queryContext, deviceId, packetTimestamp, packetDuration);
    }
    else if (currentPeriod->lastSavedEndTime < currentPeriod->endTime)
    {
        currentPeriod->lastSavedEndTime = currentPeriod->endTime + kTimePeriodPreemptiveLength;

        saveTimePeriodEnd(
            queryContext,
            currentPeriod->id,
            currentPeriod->lastSavedEndTime);
    }

    return currentPeriod->id;
}

detail::TimePeriod* TimePeriodDao::insertOrFetchCurrentTimePeriod(
    nx::sql::QueryContext* queryContext,
    long long deviceId,
    std::chrono::milliseconds packetTimestamp,
    std::chrono::milliseconds packetDuration)
{
    {
        QnMutexLocker lock(&m_mutex);

        auto periodIter = m_deviceIdToCurrentTimePeriod.find(deviceId);
        if (periodIter != m_deviceIdToCurrentTimePeriod.end())
            return &periodIter->second;
    }

    detail::TimePeriod timePeriod;

    timePeriod.deviceId = deviceId;
    timePeriod.startTime = packetTimestamp;
    timePeriod.endTime = packetTimestamp + packetDuration;

    auto query = queryContext->connection()->createQuery();
    query->prepare(R"sql(
        INSERT INTO time_period_full(device_id, period_start_ms, period_end_ms)
        VALUES (:deviceId, :periodStartMs, :periodEndMs)
    )sql");
    query->bindValue(":deviceId", deviceId);
    query->bindValue(":periodStartMs", (long long)packetTimestamp.count());
    // Filling period end so that this period is used in queries.
    query->bindValue(":periodEndMs", (long long)(timePeriod.endTime + kTimePeriodPreemptiveLength).count());

    query->exec();

    timePeriod.id = query->lastInsertId().toLongLong();
    timePeriod.lastSavedEndTime = timePeriod.endTime + kTimePeriodPreemptiveLength;

    QnMutexLocker lock(&m_mutex);

    auto periodIter = m_deviceIdToCurrentTimePeriod.emplace(deviceId, timePeriod).first;
    m_idToTimePeriod[periodIter->second.id] = periodIter;

    NX_VERBOSE(this, "Added new time period %1 (device %2, start time %3)",
        timePeriod.id, m_deviceDao->deviceGuidFromId(timePeriod.deviceId), timePeriod.startTime);

    return &periodIter->second;
}

void TimePeriodDao::closeCurrentTimePeriod(
    nx::sql::QueryContext* queryContext,
    long long deviceId)
{
    auto periodIter = m_deviceIdToCurrentTimePeriod.find(deviceId);
    NX_ASSERT(periodIter != m_deviceIdToCurrentTimePeriod.end());
    if (periodIter == m_deviceIdToCurrentTimePeriod.end())
        return;

    saveTimePeriodEnd(
        queryContext,
        periodIter->second.id,
        periodIter->second.endTime);

    NX_VERBOSE(this, "Closed time period %1 (device %2)",
        periodIter->second.id, m_deviceDao->deviceGuidFromId(deviceId));

    QnMutexLocker lock(&m_mutex);

    m_idToTimePeriod.erase(periodIter->second.id);
    m_deviceIdToCurrentTimePeriod.erase(periodIter);
}

void TimePeriodDao::saveTimePeriodEnd(
    nx::sql::QueryContext* queryContext,
    long long id,
    std::chrono::milliseconds endTime)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(
        "UPDATE time_period_full SET period_end_ms=:periodEndMs WHERE id=:id");

    query->bindValue(":id", id);
    query->bindValue(":periodEndMs", (long long)endTime.count());
    query->exec();

    NX_VERBOSE(this, "Updated end of time period %1 to %2", id, endTime);
}

void TimePeriodDao::fillCurrentTimePeriodsCache(
    nx::sql::QueryContext* queryContext)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(R"sql(
        SELECT id, device_id, period_start_ms, period_end_ms
        FROM time_period_full
        GROUP BY device_id
        HAVING max(rowid)
    )sql");

    while (query->next())
    {
        detail::TimePeriod timePeriod;

        timePeriod.id = query->value("id").toLongLong();
        timePeriod.deviceId = query->value("device_id").toLongLong();
        timePeriod.startTime = std::chrono::milliseconds(query->value("period_start_ms").toLongLong());
        timePeriod.endTime = std::chrono::milliseconds(query->value("period_end_ms").toLongLong());

        QnMutexLocker lock(&m_mutex);

        auto it = m_deviceIdToCurrentTimePeriod.emplace(timePeriod.deviceId, timePeriod).first;
        m_idToTimePeriod[timePeriod.id] = it;
    }
}

std::optional<detail::TimePeriod> TimePeriodDao::getTimePeriodById(long long id) const
{
    QnMutexLocker lock(&m_mutex);

    if (auto it = m_idToTimePeriod.find(id); it != m_idToTimePeriod.end())
        return it->second->second;
    return std::nullopt;
}

} // namespace nx::analytics::storage
