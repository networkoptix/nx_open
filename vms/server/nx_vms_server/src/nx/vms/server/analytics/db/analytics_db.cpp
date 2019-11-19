#include "analytics_db.h"

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/sql_functions.h>
#include <nx/sql/filter.h>
#include <nx/sql/sql_cursor.h>
#include <nx/utils/log/log.h>

#include <analytics/db/config.h>

#include <nx/vms/server/root_fs.h>

#include "cleaner.h"
#include "object_track_searcher.h"
#include "serializers.h"
#include "time_period_fetcher.h"

#include <cmath>

namespace nx::analytics::db {

static constexpr char kSaveEventQueryAggregationKey[] = "c119fb61-b7d3-42c5-b833-456437eaa7c7";

//-------------------------------------------------------------------------------------------------

EventsStorage::EventsStorage(QnMediaServerModule* mediaServerModule):
    m_mediaServerModule(mediaServerModule),
    m_trackAggregator(
        kTrackSearchResolutionX,
        kTrackSearchResolutionY,
        kTrackAggregationPeriod)
{
    NX_CRITICAL(std::pow(10, kCoordinateDecimalDigits) < kCoordinatesPrecision);
}

EventsStorage::~EventsStorage()
{
    if (!m_dbController)
        return;

    {
        QnMutexLocker lock(&m_mutex);
        m_stopped = true;
        closeAllCursors(lock);
    }

    m_asyncOperationGuard.reset();

    // Flushing all cached data.
    // Since update queries are queued all scheduled requests will be completed before flush.
    std::promise<ResultCode> done;
    flush([&done](auto resultCode) { done.set_value(resultCode); });
    done.get_future().wait();
}

bool EventsStorage::makePath(const QString& path)
{
    if (!m_mediaServerModule) //< #TODO #akulikov Hack for tests. Refactor it.
        return true;

    if (!m_mediaServerModule->rootFileSystem()->makeDirectory(path))
    {
        NX_WARNING(this, "Failed to create directory %1", path);
        return false;
    }

    NX_DEBUG(this, "Directory %1 exists or has been created successfully", path);
    return true;
}

bool EventsStorage::changeOwner(const std::vector<PathAndMode>& pathAndModeList)
{
    if (!m_mediaServerModule) //< #TODO #akulikov Hack for tests. Refactor it.
        return true;

    for (const auto& entry: pathAndModeList)
    {
        const ChownMode mode = std::get<ChownMode>(entry);
        const QString path = std::get<QString>(entry);
        const QString modeString = mode == ChownMode::recursive ? "recursive" : "non-recursive";
        const auto rootFs = m_mediaServerModule->rootFileSystem();

        if (!rootFs->isPathExists(path))
        {
            NX_DEBUG(
                this,
                "Requested to change owner for a path %1, but it doesn't exist. Skipping.", path);
            continue;
        }

        if (!rootFs->changeOwner(path, mode == ChownMode::recursive))
        {
            NX_WARNING(
                this, "Failed to change access rights. Mode: %1. Path: %2", modeString, path);
            return false;
        }

        NX_DEBUG(
            this, "Suceeded to change access rights. Mode: %1. Path: %2", modeString, path);
    }

    return true;
}

std::vector<EventsStorage::PathAndMode> EventsStorage::enumerateSqlFiles(const QString& dbFileName)
{
    const auto fileInfo = QFileInfo(dbFileName);
    const auto dirPath = fileInfo.canonicalPath();
    NX_ASSERT(!dirPath.isEmpty(), "Invalid path");

    const auto baseName = closeDirPath(dirPath) + fileInfo.baseName();
    const QStringList possibleExtensions = {".sqlite", ".sqlite-shm", ".sqlite-wal"};
    std::vector<PathAndMode> result;

    for (const auto& ext: possibleExtensions)
        result.push_back(std::make_tuple(baseName + ext, ChownMode::nonRecursive));

    return result;
}

bool EventsStorage::initialize(const Settings& settings)
{
    if (m_dbController)
    {
        NX_ASSERT(false, "Reinitializing is not supported by this class");
        return false;
    }

    m_objectTrackCache = std::make_unique<ObjectTrackCache>(
        kTrackAggregationPeriod,
        settings.maxCachedObjectLifeTime);

    auto dbConnectionOptions = settings.dbConnectionOptions;
    dbConnectionOptions.dbName = closeDirPath(settings.path) + dbConnectionOptions.dbName;
    NX_DEBUG(this, "Opening analytics event storage from [%1]", dbConnectionOptions.dbName);

    m_dbController = std::make_unique<DbController>(dbConnectionOptions);
    const auto archivePath = closeDirPath(settings.path) + "archive/";

    if (!makePath(archivePath)
        || !changeOwner({
            {settings.path, ChownMode::nonRecursive},
            {archivePath, ChownMode::nonRecursive}})
        || !changeOwner(enumerateSqlFiles(dbConnectionOptions.dbName)))
    {
        m_dbController.reset();
        NX_WARNING(this, "Failed to initialize analytics DB directories at %1", settings.path);
        return false;
    }

    NX_DEBUG(this, "Initializing analytics SQLite DB");

    if (!m_dbController->initialize()
        || !readMaximumEventTimestamp()
        || !loadDictionaries())
    {
        m_dbController.reset();
        NX_WARNING(this, "Failed to open analytics DB at %1", settings.path);
        return false;
    }

    NX_DEBUG(this, "Initializing archive directory at %1", archivePath);

    m_analyticsArchiveDirectory = std::make_unique<AnalyticsArchiveDirectory>(
        m_mediaServerModule,
        archivePath);

    NX_DEBUG(this, "Analytics DB initialized");

    return true;
}

bool EventsStorage::initialized() const
{
    return m_dbController != nullptr;
}

void EventsStorage::save(common::metadata::ConstObjectMetadataPacketPtr packet)
{
    using namespace std::chrono;

    NX_VERBOSE(this, "Saving packet %1", *packet);

    QElapsedTimer t;
    t.restart();

    QnMutexLocker lock(&m_mutex);

    NX_VERBOSE(this, "Saving packet (1). %1ms", t.elapsed());

    m_maxRecordedTimestamp = std::max<milliseconds>(
        m_maxRecordedTimestamp,
        duration_cast<milliseconds>(microseconds(packet->timestampUs)));

    savePacketDataToCache(lock, packet);
    ObjectTrackDataSaver detectionDataSaver = takeDataToSave(lock, /*flush*/ false);
    lock.unlock();

    if (detectionDataSaver.empty())
    {
        NX_VERBOSE(this, "Saving packet (2) took %1ms", t.elapsed());
        return;
    }

    NX_VERBOSE(this, "Saving packet (3). %1ms", t.elapsed());

    m_dbController->queryExecutor().executeUpdate(
        [packet = packet, detectionDataSaver = std::move(detectionDataSaver)](
            nx::sql::QueryContext* queryContext) mutable
        {
            detectionDataSaver.save(queryContext);
            return nx::sql::DBResult::ok;
        },
        [this](sql::DBResult resultCode) { logDataSaveResult(resultCode); },
        kSaveEventQueryAggregationKey);

    NX_VERBOSE(this, "Saving packet (4) took %1ms", t.elapsed());
}

std::optional<nx::sql::QueryStatistics> EventsStorage::statistics() const
{
    if (m_dbController)
        return m_dbController->statisticsCollector().getQueryStatistics();
    return std::nullopt;
}

void EventsStorage::lookup(
    Filter filter,
    LookupCompletionHandler completionHandler)
{
    NX_DEBUG(this, "Selecting tracks. Filter %1", filter);

    auto result = std::make_shared<std::vector<ObjectTrack>>();
    m_dbController->queryExecutor().executeSelect(
        [this, filter = std::move(filter), result](nx::sql::QueryContext* queryContext)
        {
            ObjectTrackSearcher objectSearcher(
                m_deviceDao,
                m_objectTypeDao,
                &m_attributesDao,
                m_analyticsArchiveDirectory.get(),
                std::move(filter));
            *result = objectSearcher.lookup(queryContext);
            return nx::sql::DBResult::ok;
        },
        [this, result, completionHandler = std::move(completionHandler)](
            sql::DBResult resultCode)
        {
            NX_DEBUG(this, "%1 objects selected. Result code %2", result->size(), resultCode);

            completionHandler(
                dbResultToResultCode(resultCode),
                std::move(*result));
        });
}

void EventsStorage::lookupTimePeriods(
    Filter filter,
    TimePeriodsLookupOptions options,
    TimePeriodsLookupCompletionHandler completionHandler)
{
    NX_DEBUG(this, "Selecting time periods. Filter %1, detail level %2",
        filter, options.detailLevel);

    auto result = std::make_shared<QnTimePeriodList>();
    m_dbController->queryExecutor().executeSelect(
        [this, filter = std::move(filter), options = std::move(options), result](
            nx::sql::QueryContext* queryContext)
        {
            TimePeriodFetcher timePeriodFetcher(
                m_deviceDao,
                m_objectTypeDao,
                &m_attributesDao,
                m_analyticsArchiveDirectory.get(),
                m_maxRecordedTimestamp);
            return timePeriodFetcher.selectTimePeriods(
                queryContext, filter, options, result.get());
        },
        [this, result, completionHandler = std::move(completionHandler)](
            sql::DBResult resultCode)
        {
            NX_DEBUG(this, "%1 time periods selected. Result code %2", result->size(), resultCode);

            completionHandler(
                dbResultToResultCode(resultCode),
                std::move(*result));
        });
}

void EventsStorage::markDataAsDeprecated(
    QnUuid deviceId,
    std::chrono::milliseconds oldestDataToKeepTimestamp)
{
    NX_VERBOSE(this, "Cleaning data of device %1 up to timestamp %2",
        deviceId, oldestDataToKeepTimestamp.count());

    QnMutexLocker lock(&m_mutex);
    scheduleDataCleanup(lock, deviceId, oldestDataToKeepTimestamp);
}

void EventsStorage::flush(StoreCompletionHandler completionHandler)
{
    m_dbController->queryExecutor().executeUpdate(
        [this](nx::sql::QueryContext* queryContext)
        {
            NX_DEBUG(this, "Flushing unsaved data");

            QnMutexLocker lock(&m_mutex);
            ObjectTrackDataSaver detectionDataSaver = takeDataToSave(lock, /*flush*/ true);
            lock.unlock();

            if (!detectionDataSaver.empty())
                detectionDataSaver.save(queryContext);

            // Since sqlite supports only one update thread this will be executed after every
            // packet has been saved.

            return sql::DBResult::ok;
        },
        [completionHandler = std::move(completionHandler)](sql::DBResult resultCode)
        {
            completionHandler(dbResultToResultCode(resultCode));
        });
}

bool EventsStorage::readMaximumEventTimestamp()
{
    NX_DEBUG(this, "Loading max event timestamp");

    try
    {
        m_maxRecordedTimestamp = m_dbController->queryExecutor().executeSelectQuerySync(
            [](nx::sql::QueryContext* queryContext)
            {
                auto query = queryContext->connection()->createQuery();
                query->prepare("SELECT max(track_end_ms) FROM track");
                query->exec();
                if (query->next())
                    return std::chrono::milliseconds(query->value(0).toLongLong());
                return std::chrono::milliseconds::zero();
            });
    }
    catch (const std::exception& e)
    {
        NX_WARNING(this, "Failed to read maximum event timestamp from the DB. %1", e.what());
        return false;
    }

    NX_DEBUG(this, "Loaded max event timestamp %1", m_maxRecordedTimestamp);

    return true;
}

bool EventsStorage::readMinimumEventTimestamp(std::chrono::milliseconds* outResult)
{
    // TODO: The mutex is locked here for the duration of DB query which is a long lock.
    // Long locks should not happen.

    try
    {
        *outResult = m_dbController->queryExecutor().executeSelectQuerySync(
            [](nx::sql::QueryContext* queryContext)
            {
                auto query = queryContext->connection()->createQuery();
                query->prepare("SELECT min(track_start_ms) FROM track");
                query->exec();
                if (query->next())
                    return std::chrono::milliseconds(query->value(0).toLongLong());
                return std::chrono::milliseconds::zero();
            });
    }
    catch (const std::exception& e)
    {
        NX_WARNING(this, "Failed to read minimum event timestamp from the DB. %1", e.what());
        return false;
    }

    return true;
}

bool EventsStorage::loadDictionaries()
{
    NX_DEBUG(this, "Loading dictionaries");

    try
    {
        m_dbController->queryExecutor().executeSelectQuerySync(
            [this](nx::sql::QueryContext* queryContext)
            {
                m_objectTypeDao.loadObjectTypeDictionary(queryContext);
                m_deviceDao.loadDeviceDictionary(queryContext);
                // TODO: #ak Remove when executeSelectQuerySync supports void functors.
                return true;
            });
    }
    catch (const std::exception& e)
    {
        NX_WARNING(this, "Failed to read maximum event timestamp from the DB. %1", e.what());
        return false;
    }

    NX_DEBUG(this, "Dictionaries loaded");

    return true;
}

void EventsStorage::closeAllCursors(const QnMutexLockerBase&)
{
    for (auto& cursor: m_openedCursors)
        cursor->close();
    m_openedCursors.clear();
}

void EventsStorage::savePacketDataToCache(
    const QnMutexLockerBase& /*lock*/,
    const common::metadata::ConstObjectMetadataPacketPtr& packet)
{
    using namespace std::chrono;

    m_objectTrackCache->add(packet);

    for (const auto& objectMetadata: packet->objectMetadataList)
    {
        m_trackAggregator.add(
            objectMetadata.trackId,
            duration_cast<milliseconds>(microseconds(packet->timestampUs)),
            objectMetadata.boundingBox);
    }
}

ObjectTrackDataSaver EventsStorage::takeDataToSave(
    const QnMutexLockerBase& /*lock*/,
    bool flushData)
{
    ObjectTrackDataSaver detectionDataSaver(
        &m_attributesDao,
        &m_deviceDao,
        &m_objectTypeDao,
        &m_trackGroupDao,
        m_objectTrackCache.get(),
        m_analyticsArchiveDirectory.get());

    detectionDataSaver.load(&m_trackAggregator, flushData);

    m_objectTrackCache->removeExpiredData();

    return detectionDataSaver;
}

void EventsStorage::scheduleDataCleanup(
    const QnMutexLockerBase&,
    QnUuid deviceId,
    std::chrono::milliseconds oldestDataToKeepTimestamp)
{
    m_dbController->queryExecutor().executeUpdate(
        [this, deviceId, oldestDataToKeepTimestamp](nx::sql::QueryContext* queryContext)
        {
            // Device id NULL means clean for all devices.
            const auto internalId = m_deviceDao.deviceIdFromGuid(deviceId);
            if (internalId == -1 && !deviceId.isNull())
                return nx::sql::DBResult::ok; //< Not found in the database.

            Cleaner cleaner(
                &m_attributesDao,
                internalId,
                oldestDataToKeepTimestamp);

            if (cleaner.clean(queryContext) == Cleaner::Result::incomplete)
            {
                NX_VERBOSE(this, "Could not clean all data in one run. Scheduling another one");

                QnMutexLocker lock(&m_mutex);
                if (!m_stopped)
                    scheduleDataCleanup(lock, deviceId, oldestDataToKeepTimestamp);
            }

            return nx::sql::DBResult::ok;
        },
        [this, deviceId, oldestDataToKeepTimestamp](nx::sql::DBResult result)
        {
            logCleanupResult(result, deviceId, oldestDataToKeepTimestamp);
        });
}

void EventsStorage::logCleanupResult(
    sql::DBResult resultCode,
    const QnUuid& deviceId,
    std::chrono::milliseconds oldestDataToKeepTimestamp)
{
    if (resultCode == sql::DBResult::ok)
    {
        NX_VERBOSE(this, "Cleaned data of device %1 up to timestamp %2",
            deviceId, oldestDataToKeepTimestamp);
    }
    else
    {
        NX_DEBUG(this, "Error (%1) while cleaning up data of device %2 up to timestamp %3",
            toString(resultCode), deviceId, oldestDataToKeepTimestamp);
    }
};

void EventsStorage::logDataSaveResult(sql::DBResult resultCode)
{
    if (resultCode != sql::DBResult::ok)
    {
        NX_DEBUG(this, "Error saving detection metadata packet. %1", resultCode);
    }
    else
    {
        NX_VERBOSE(this, "Detection metadata packet has been saved successfully");
    }
}

//-------------------------------------------------------------------------------------------------

MovableAnalyticsDb::MovableAnalyticsDb(QnMediaServerModule* mediaServerModule):
    m_mediaServerModule(mediaServerModule)
{
}

MovableAnalyticsDb::~MovableAnalyticsDb()
{
}

bool MovableAnalyticsDb::initialize(const Settings& settings)
{
    auto otherDb = std::make_shared<EventsStorage>(m_mediaServerModule);
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

std::shared_ptr<EventsStorage> MovableAnalyticsDb::getDb()
{
    QnMutexLocker locker(&m_mutex);
    return m_db;
}

std::shared_ptr<const EventsStorage> MovableAnalyticsDb::getDb() const
{
    QnMutexLocker locker(&m_mutex);
    return m_db;
}

//-------------------------------------------------------------------------------------------------

EventsStorageFactory::EventsStorageFactory():
    base_type([this](auto&&... args)
        { return defaultFactoryFunction(std::forward<decltype(args)>(args)...); })
{
}

EventsStorageFactory& EventsStorageFactory::instance()
{
    static EventsStorageFactory staticInstance;
    return staticInstance;
}

std::unique_ptr<AbstractEventsStorage> EventsStorageFactory::defaultFactoryFunction(
    QnMediaServerModule* mediaServerModule)
{
    return std::make_unique<MovableAnalyticsDb>(mediaServerModule);
}

} // namespace nx::analytics::db
