#include "movable_analytics_db.h"

namespace nx::analytics::db {

MovableAnalyticsDb::MovableAnalyticsDb(ActualAnalyticsDbFactoryFunc func):
    m_factoryFunc(std::move(func))
{
}

bool MovableAnalyticsDb::initialize(const Settings& settings)
{
    auto otherDb = std::shared_ptr<AbstractEventsStorage>(m_factoryFunc());
    bool result = otherDb->initialize(settings);
    if (!result)
    {
        NX_INFO(this, "Failed to initialize analytics DB at %1", settings.path);
        otherDb.reset();
    }

    // NOTE: Switching to the new DB object anyway. That's a functional requirement.
    // So, DB becomes non-operational in case of a failure to re-initialize it.

    {
        QnMutexLocker locker(&m_mutex);
        std::swap(m_db, otherDb);
    }

    // Waiting for the old DB to become unused.
    // NOTE: Using loop with a delay to make things simpler here.
    // Anyway, closing / opening a DB is a time-consuming operation.
    // Extra millisecond sleep does not make it worse.
    while (otherDb.use_count() > 1)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    return result;
}

bool MovableAnalyticsDb::initialized() const
{
    return m_db != nullptr;
}

void MovableAnalyticsDb::save(common::metadata::ConstObjectMetadataPacketPtr packet)
{
    auto db = getDb();
    if (!db)
    {
        NX_DEBUG(this, "Attempt to write to non-initialized analytics DB");
        return;
    }

    return db->save(std::move(packet));
}

std::vector<ObjectPosition> MovableAnalyticsDb::lookupTrackDetailsSync(const ObjectTrack& track)
{
    auto db = getDb();
    if (!db)
    {
        NX_DEBUG(this, "Attempt to lookup to non-initialized analytics DB");
        return std::vector<ObjectPosition>();
    }

    return db->lookupTrackDetailsSync(track);
}

void MovableAnalyticsDb::lookup(
    Filter filter,
    LookupCompletionHandler completionHandler)
{
    auto db = getDb();
    if (!db)
    {
        NX_DEBUG(this, "Attempt to look up tracks in non-initialized analytics DB");
        return completionHandler(ResultCode::ok, LookupResult());
    }

    return db->lookup(
        std::move(filter),
        std::move(completionHandler));
}

void MovableAnalyticsDb::lookupTimePeriods(
    Filter filter,
    TimePeriodsLookupOptions options,
    TimePeriodsLookupCompletionHandler completionHandler)
{
    auto db = getDb();
    if (!db)
    {
        NX_DEBUG(this, "Attempt to look up time periods in non-initialized analytics DB");
        return completionHandler(ResultCode::error, QnTimePeriodList());
    }

    return db->lookupTimePeriods(
        std::move(filter),
        std::move(options),
        std::move(completionHandler));
}

void MovableAnalyticsDb::markDataAsDeprecated(
    QnUuid deviceId,
    std::chrono::milliseconds oldestDataToKeepTimestamp)
{
    auto db = getDb();
    if (!db)
    {
        NX_DEBUG(this, "Attempt to remove from non-initialized analytics DB");
        return;
    }

    return db->markDataAsDeprecated(
        deviceId,
        oldestDataToKeepTimestamp);
}

void MovableAnalyticsDb::flush(StoreCompletionHandler completionHandler)
{
    auto db = getDb();
    if (!db)
    {
        NX_DEBUG(this, "Attempt to flush non-initialized analytics DB");
        return completionHandler(ResultCode::error);
    }

    return db->flush(std::move(completionHandler));
}

std::optional<nx::sql::QueryStatistics> MovableAnalyticsDb::statistics() const
{
    if (const auto db = getDb())
        return db->statistics();
    return std::nullopt;
}

bool MovableAnalyticsDb::readMinimumEventTimestamp(std::chrono::milliseconds* outResult)
{
    auto db = getDb();
    if (!db)
    {
        NX_DEBUG(this, "Attempt to read min timestamp from non-initialized analytics DB");
        return false;
    }

    return db->readMinimumEventTimestamp(outResult);
}

std::shared_ptr<AbstractEventsStorage> MovableAnalyticsDb::getDb()
{
    QnMutexLocker locker(&m_mutex);
    return m_db;
}

std::shared_ptr<const AbstractEventsStorage> MovableAnalyticsDb::getDb() const
{
    QnMutexLocker locker(&m_mutex);
    return m_db;
}

} // namespace nx::analytics::db
