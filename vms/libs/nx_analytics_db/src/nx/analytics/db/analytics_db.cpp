#include "analytics_db.h"

#include <cmath>

#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFileInfo>

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/sql_functions.h>
#include <nx/sql/filter.h>
#include <nx/sql/sql_cursor.h>
#include <nx/utils/log/log.h>

#include <analytics/db/config.h>

#include "abstract_object_type_dictionary.h"
#include "cleaner.h"
#include "object_track_searcher.h"
#include "movable_analytics_db.h"
#include "serializers.h"
#include "time_period_fetcher.h"

#include <common/common_module.h>
#include <utils/common/util.h>

namespace nx::analytics::db {

static constexpr char kSaveEventQueryAggregationKey[] = "c119fb61-b7d3-42c5-b833-456437eaa7c7";

QString toString(ChownMode mode)
{
    switch (mode)
    {
        case ChownMode::recursive: return "recursive";
        case ChownMode::nonRecursive: return "nonRecursive";
        case ChownMode::mountPoint: return "mountPoint";
    }

    NX_ASSERT(false);
    return "?";
}

//-------------------------------------------------------------------------------------------------

EventsStorage::EventsStorage(
    QnCommonModule* commonModule,
    AbstractIframeSearchHelper* iframeSearchHelper,
    AbstractObjectTypeDictionary* objectTypeDictionary)
    :
    m_commonModule(commonModule),
    m_iframeSearchHelper(iframeSearchHelper),
    m_attributesDao(objectTypeDictionary),
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
    if (!QDir().mkpath(path))
    {
        NX_WARNING(this, "Failed to create directory %1", path);
        return false;
    }

    NX_DEBUG(this, "Directory %1 exists or has been created successfully.", path);
    return true;
}

bool EventsStorage::makeWritable(
    const std::vector<PathAndMode>& /*pathAndModeList*/)
{
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

    NX_DEBUG(NX_SCOPE_TAG, "%1(%2) -> %3", __func__, dbFileName, result.size());
    return result;
}

bool EventsStorage::initialize(const Settings& settings)
{
    NX_INFO(this, "Initialize in %1", settings.path);
    if (m_dbController)
    {
        NX_ASSERT(false, "Reinitializing is not supported by this class.");
        return false;
    }

    m_objectTrackCache = std::make_unique<ObjectTrackCache>(
        kTrackAggregationPeriod,
        settings.maxCachedObjectLifeTime,
        m_iframeSearchHelper);

    auto dbConnectionOptions = settings.dbConnectionOptions;
    dbConnectionOptions.dbName = closeDirPath(settings.path) + dbConnectionOptions.dbName;
    NX_DEBUG(this, "Opening analytics event storage from [%1].", dbConnectionOptions.dbName);

    m_dbController = std::make_unique<DbController>(dbConnectionOptions);
    const auto archivePath = closeDirPath(settings.path) + "archive/";

    // Security huck: we have to set chmod 755, otherwise SQLite will not be able to manage DB on
    // this disk drive due to QT SQLite driver limitations.
    const auto diskMountPoint = QDir(settings.path).absoluteFilePath("..");
    if (!makeWritable({{diskMountPoint, ChownMode::mountPoint}})
        || !makePath(archivePath)
        || !makeWritable({
            {settings.path, ChownMode::nonRecursive},
            {archivePath, ChownMode::nonRecursive}})
        || !makeWritable(enumerateSqlFiles(dbConnectionOptions.dbName)))
    {
        m_dbController.reset();
        NX_WARNING(this, "Failed to initialize Analytics DB directories at %1", settings.path);
        return false;
    }

    NX_DEBUG(this, "Initializing analytics SQLite DB");

    if (!m_dbController->initialize()
        || !readMaximumEventTimestamp()
        || !loadDictionaries())
    {
        m_dbController.reset();
        NX_WARNING(this, "Failed to open Analytics DB at %1", settings.path);
        return false;
    }

    NX_DEBUG(this, "Initializing archive directory at %1", archivePath);

    m_analyticsArchiveDirectory = std::make_unique<AnalyticsArchiveDirectory>(
        m_commonModule,
        archivePath);

    NX_DEBUG(this, "Analytics DB initialized.");

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

    NX_VERBOSE(this, "Saving packet (1). %1 ms", t.elapsed());

    m_maxRecordedTimestamp = std::max<milliseconds>(
        m_maxRecordedTimestamp,
        duration_cast<milliseconds>(microseconds(packet->timestampUs)));

    savePacketDataToCache(lock, packet);
    ObjectTrackDataSaver detectionDataSaver = takeDataToSave(lock, /*flush*/ false);
    lock.unlock();

    if (detectionDataSaver.empty())
    {
        NX_VERBOSE(this, "Saving packet (2) took %1 ms", t.elapsed());
        return;
    }

    NX_VERBOSE(this, "Saving packet (3). %1 ms", t.elapsed());

    m_dbController->queryExecutor().executeUpdate(
        [packet = packet, detectionDataSaver = std::move(detectionDataSaver)](
            nx::sql::QueryContext* queryContext) mutable
        {
            detectionDataSaver.save(queryContext);
            return nx::sql::DBResult::ok;
        },
        [this](sql::DBResult resultCode) { logDataSaveResult(resultCode); },
        kSaveEventQueryAggregationKey);

    NX_VERBOSE(this, "Saving packet (4) took %1 ms", t.elapsed());
}

std::optional<nx::sql::QueryStatistics> EventsStorage::statistics() const
{
    if (m_dbController)
        return m_dbController->statisticsCollector().getQueryStatistics();
    return std::nullopt;
}

std::vector<ObjectPosition> EventsStorage::lookupTrackDetailsSync(const ObjectTrack& track)
{
    if (const auto& details = m_objectTrackCache->getTrackById(track.id))
    {
        NX_VERBOSE(this, "Return trackId %1 from the cache.", track.id);
        return details->objectPositionSequence;
    }

    return std::vector<ObjectPosition>();
}

void EventsStorage::lookup(
    Filter filter,
    LookupCompletionHandler completionHandler)
{
    NX_DEBUG(this, "Selecting tracks. Filter %1", filter);

    auto result = std::make_shared<std::vector<ObjectTrackEx>>();
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
            if (filter.needFullTrack && !filter.objectTrackId.isNull())
            {
                for (auto& track: *result)
                    track.objectPositionSequence = lookupTrackDetailsSync(track);
            }
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
                m_analyticsArchiveDirectory.get());
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
            NX_DEBUG(this, "Flushing unsaved data.");

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
    NX_DEBUG(this, "Loading max event timestamp.");

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
        NX_WARNING(this, "Failed to read maximum event timestamp from Analytics DB: %1", e.what());
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
        NX_WARNING(this, "Failed to read minimum event timestamp from Analytics DB: %1", e.what());
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
        NX_WARNING(this, "Failed to read maximum event timestamp from Analytics DB: %1", e.what());
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
                NX_VERBOSE(this, "Could not clean all data in one run. Scheduling another one.");

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
        NX_DEBUG(this, "Error saving detection metadata packet: %1", resultCode);
    }
    else
    {
        NX_VERBOSE(this, "Detection metadata packet has been saved successfully.");
    }
}

} // namespace nx::analytics::db
