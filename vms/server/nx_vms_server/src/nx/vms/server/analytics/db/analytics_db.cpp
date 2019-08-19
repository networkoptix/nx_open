#include "analytics_db.h"

#include <cmath>

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/sql_functions.h>
#include <nx/sql/filter.h>
#include <nx/sql/sql_cursor.h>
#include <nx/utils/log/log.h>

#include <analytics/db/config.h>

#include <nx/vms/server/root_fs.h>

#include "cursor.h"
#include "cleaner.h"
#include "object_searcher.h"
#include "serializers.h"
#include "time_period_fetcher.h"

namespace nx::analytics::db {

static constexpr char kSaveEventQueryAggregationKey[] = "c119fb61-b7d3-42c5-b833-456437eaa7c7";

//-------------------------------------------------------------------------------------------------

EventsStorage::EventsStorage(QnMediaServerModule* mediaServerModule):
    m_mediaServerModule(mediaServerModule),
    m_objectCache(kTrackAggregationPeriod, kMaxCachedObjectLifeTime),
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

    // Flushing all cached data.
    // Since update queries are queued all scheduled requests will be completed before flush.
    std::promise<ResultCode> done;
    flush([&done](auto resultCode) { done.set_value(resultCode); });
    done.get_future().wait();
}

bool EventsStorage::initialize(const Settings& settings)
{
    auto dbConnectionOptions = settings.dbConnectionOptions;
    dbConnectionOptions.dbName = closeDirPath(settings.path) + dbConnectionOptions.dbName;
    NX_DEBUG(this, "Opening analytics event storage from [%1]", dbConnectionOptions.dbName);

    QnMutexLocker lock(&m_dbControllerMutex);
    {
        QnMutexLocker cursorLock(&m_cursorsMutex);
        m_closingDbController = true;
        for (auto& cursor: m_openedCursors)
            cursor->close();
        m_openedCursors.clear();
    }
    m_analyticsArchiveDirectory.reset();
    m_dbController.reset();

    m_closingDbController = false;

    m_dbController = std::make_unique<DbController>(dbConnectionOptions);
    if (!ensureDbDirIsWritable(settings.path)
        || !m_dbController->initialize()
        || !readMaximumEventTimestamp()
        || !loadDictionaries())
    {
        m_dbController.reset();
        NX_WARNING(this, "Failed to open analytics DB at %1", settings.path);
        return false;
    }

    m_analyticsArchiveDirectory = std::make_unique<AnalyticsArchiveDirectory>(
        m_mediaServerModule,
        settings.path + "/archive/");

    return true;
}

void EventsStorage::save(common::metadata::ConstDetectionMetadataPacketPtr packet)
{
    using namespace std::chrono;

    QElapsedTimer t;
    t.restart();

    NX_VERBOSE(this, "Saving packet %1", *packet);

    QnMutexLocker lock(&m_mutex);

    m_maxRecordedTimestamp = std::max<milliseconds>(
        m_maxRecordedTimestamp,
        duration_cast<milliseconds>(microseconds(packet->timestampUsec)));

    savePacketDataToCache(lock, packet);
    auto detectionDataSaver = takeDataToSave(lock, /*flush*/ false);
    lock.unlock();

    QnMutexLocker dbLock(&m_dbControllerMutex);

    if (!m_dbController)
    {
        NX_DEBUG(this, "Attempt to write to non-initialized analytics DB");
        return;
    }

    if (detectionDataSaver.empty())
    {
        NX_VERBOSE(this, "Saving packet (1) took %1ms", t.elapsed());
        return;
    }

    m_dbController->queryExecutor().executeUpdate(
        [packet = packet, detectionDataSaver = std::move(detectionDataSaver)](
            nx::sql::QueryContext* queryContext) mutable
        {
            detectionDataSaver.save(queryContext);
            return nx::sql::DBResult::ok;
        },
        [this](sql::DBResult resultCode) { logDataSaveResult(resultCode); },
        kSaveEventQueryAggregationKey);

    NX_VERBOSE(this, "Saving packet (2) took %1ms", t.elapsed());
}

void EventsStorage::createLookupCursor(
    Filter filter,
    CreateCursorCompletionHandler completionHandler)
{
    using namespace nx::utils;

    NX_VERBOSE(this, "Requested cursor with filter %1", filter);

    QnMutexLocker lock(&m_dbControllerMutex);

    if (!m_dbController)
    {
        NX_DEBUG(this, "Attempt to stream data from non-initialized analytics DB");
        lock.unlock();
        completionHandler(ResultCode::error, nullptr);
        return;
    }

    auto objectSearcher = std::make_shared<ObjectSearcher>(
        m_deviceDao,
        m_objectTypeDao,
        &m_attributesDao,
        m_analyticsArchiveDirectory.get(),
        std::move(filter));

    m_dbController->queryExecutor().createCursor<DetectedObject>(
        [objectSearcher](auto&&... args)
            { objectSearcher->prepareCursorQuery(std::forward<decltype(args)>(args)...); },
        [objectSearcher](auto&&... args)
            { objectSearcher->loadCurrentRecord(std::forward<decltype(args)>(args)...); },
        [this, completionHandler = std::move(completionHandler)](
            sql::DBResult resultCode,
            QnUuid dbCursorId) mutable
        {
            reportCreateCursorCompletion(
                resultCode,
                dbCursorId,
                std::move(completionHandler));
        });
}

void EventsStorage::lookup(
    Filter filter,
    LookupCompletionHandler completionHandler)
{
    QnMutexLocker lock(&m_dbControllerMutex);

    NX_DEBUG(this, "Selecting objects. Filter %1", filter);

    if (!m_dbController)
    {
        NX_DEBUG(this, "Attempt to look up objects in non-initialized analytics DB");
        lock.unlock();
        completionHandler(ResultCode::error, LookupResult());
        return;
    }

    auto result = std::make_shared<std::vector<DetectedObject>>();
    m_dbController->queryExecutor().executeSelect(
        [this, filter = std::move(filter), result](nx::sql::QueryContext* queryContext)
        {
            ObjectSearcher objectSearcher(
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
    QnMutexLocker lock(&m_dbControllerMutex);

    NX_DEBUG(this, "Selecting time periods. Filter %1, detail level %2",
        filter, options.detailLevel);

    if (!m_dbController)
    {
        NX_DEBUG(this, "Attempt to look up time periods in non-initialized analytics DB");
        lock.unlock();
        completionHandler(ResultCode::error, QnTimePeriodList());
        return;
    }

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
    QnMutexLocker lock(&m_dbControllerMutex);

    if (!m_dbController)
    {
        NX_DEBUG(this, "Attempt to delete data from non-initialized analytics DB");
        return;
    }

    NX_VERBOSE(this, "Cleaning data of device %1 up to timestamp %2",
        deviceId, oldestDataToKeepTimestamp.count());

    scheduleDataCleanup(deviceId, oldestDataToKeepTimestamp);
}

void EventsStorage::flush(StoreCompletionHandler completionHandler)
{
    QnMutexLocker lock(&m_dbControllerMutex);

    if (!m_dbController)
    {
        NX_DEBUG(this, "Attempt to flush non-initialized analytics DB");
        lock.unlock();

        completionHandler(ResultCode::error);
        return;
    }

    m_dbController->queryExecutor().executeUpdate(
        [this](nx::sql::QueryContext* queryContext)
        {
            NX_DEBUG(this, "Flushing unsaved data");

            QnMutexLocker lock(&m_mutex);
            auto detectionDataSaver = takeDataToSave(lock, /*flush*/ true);
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

bool EventsStorage::ensureDbDirIsWritable(const QString& path)
{
    if (!m_mediaServerModule)
        return true;

    if (!m_mediaServerModule->rootFileSystem()->makeDirectory(path))
    {
        NX_WARNING(this, "Failed to create directory %1 on root FS", path);
        return false;
    }

    if (!m_mediaServerModule->rootFileSystem()->changeOwner(path))
    {
        NX_WARNING(this, "Failed to change owner of directory %1 on root FS", path);
        return false;
    }

    NX_DEBUG(this, "Successfully changed access rights for %1", path);

    return true;
}

bool EventsStorage::readMaximumEventTimestamp()
{
    try
    {
        m_maxRecordedTimestamp = m_dbController->queryExecutor().executeSelectQuerySync(
            [](nx::sql::QueryContext* queryContext)
            {
                auto query = queryContext->connection()->createQuery();
                query->prepare("SELECT max(track_end_ms) FROM object");
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

    return true;
}

bool EventsStorage::readMinimumEventTimestamp(std::chrono::milliseconds* outResult)
{
    try
    {
        *outResult = m_dbController->queryExecutor().executeSelectQuerySync(
            [](nx::sql::QueryContext* queryContext)
        {
            auto query = queryContext->connection()->createQuery();
            query->prepare("SELECT min(track_start_ms) FROM object");
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

    return true;
}

void EventsStorage::updateDictionariesIfNeeded(
    nx::sql::QueryContext* queryContext,
    const common::metadata::DetectionMetadataPacket& packet,
    const common::metadata::DetectedObject& detectedObject)
{
    m_deviceDao.insertOrFetch(queryContext, packet.deviceId);
    m_objectTypeDao.insertOrFetch(queryContext, detectedObject.objectTypeId);
}

void EventsStorage::insertEvent(
    sql::QueryContext* queryContext,
    const common::metadata::DetectionMetadataPacket& packet,
    const common::metadata::DetectedObject& detectedObject,
    int64_t attributesId,
    int64_t /*timePeriodId*/)
{
    sql::SqlQuery insertEventQuery(queryContext->connection());
    insertEventQuery.prepare(QString::fromLatin1(R"sql(
        INSERT INTO event(timestamp_usec_utc, duration_usec,
            device_id, object_type_id, object_id, attributes_id,
            box_top_left_x, box_top_left_y, box_bottom_right_x, box_bottom_right_y)
        VALUES(:timestampMs, :durationMs, :deviceId,
            :objectTypeId, :objectAppearanceId, :attributesId,
            :boxTopLeftX, :boxTopLeftY, :boxBottomRightX, :boxBottomRightY)
    )sql"));
    insertEventQuery.bindValue(":timestampMs", packet.timestampUsec / kUsecInMs);
    insertEventQuery.bindValue(":durationMs", packet.durationUsec / kUsecInMs);
    insertEventQuery.bindValue(":deviceId", m_deviceDao.deviceIdFromGuid(packet.deviceId));
    insertEventQuery.bindValue(
        ":objectTypeId",
        (long long) m_objectTypeDao.objectTypeIdFromName(detectedObject.objectTypeId));
    insertEventQuery.bindValue(
        ":objectAppearanceId",
        QnSql::serialized_field(detectedObject.objectId));

    insertEventQuery.bindValue(":attributesId", (long long) attributesId);

    const auto packedBoundingBox = packRect(detectedObject.boundingBox);

    insertEventQuery.bindValue(":boxTopLeftX", packedBoundingBox.topLeft().x());
    insertEventQuery.bindValue(":boxTopLeftY", packedBoundingBox.topLeft().y());
    insertEventQuery.bindValue(":boxBottomRightX", packedBoundingBox.bottomRight().x());
    insertEventQuery.bindValue(":boxBottomRightY", packedBoundingBox.bottomRight().y());

    insertEventQuery.exec();
}

void EventsStorage::savePacketDataToCache(
    const QnMutexLockerBase& /*lock*/,
    const common::metadata::ConstDetectionMetadataPacketPtr& packet)
{
    using namespace std::chrono;

    m_objectCache.add(packet);

    for (const auto& detectedObject: packet->objects)
    {
        m_trackAggregator.add(
            detectedObject.objectId,
            duration_cast<milliseconds>(microseconds(packet->timestampUsec)),
            detectedObject.boundingBox);
    }
}

DetectionDataSaver EventsStorage::takeDataToSave(
    const QnMutexLockerBase& /*lock*/,
    bool flushData)
{
    DetectionDataSaver detectionDataSaver(
        &m_attributesDao,
        &m_deviceDao,
        &m_objectTypeDao,
        &m_objectGroupDao,
        &m_objectCache,
        m_analyticsArchiveDirectory.get());

    detectionDataSaver.load(&m_trackAggregator, flushData);

    m_objectCache.removeExpiredData();

    return detectionDataSaver;
}

void EventsStorage::reportCreateCursorCompletion(
    sql::DBResult resultCode,
    QnUuid dbCursorId,
    CreateCursorCompletionHandler completionHandler)
{
    if (resultCode != sql::DBResult::ok)
        return completionHandler(ResultCode::error, nullptr);

    QnMutexLocker lock(&m_cursorsMutex);

    if (m_closingDbController)
        return completionHandler(ResultCode::ok, nullptr);

    auto dbCursor = std::make_unique<sql::Cursor<DetectedObject>>(
        &m_dbController->queryExecutor(),
        dbCursorId);

    auto cursor = std::make_unique<Cursor>(std::move(dbCursor));
    cursor->setOnBeforeCursorDestroyed(
        [this](Cursor* cursor)
        {
            QnMutexLocker lock(&m_cursorsMutex);
            m_openedCursors.remove(cursor);
        });

    m_openedCursors.push_back(cursor.get());
    lock.unlock();

    completionHandler(ResultCode::ok, std::move(cursor));
}

void EventsStorage::scheduleDataCleanup(
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
                scheduleDataCleanup(deviceId, oldestDataToKeepTimestamp);
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

QRect EventsStorage::packRect(const QRectF& rectf)
{
    return translate(rectf, QSize(kCoordinatesPrecision, kCoordinatesPrecision));
}

QRectF EventsStorage::unpackRect(const QRect& rect)
{
    return translate(rect, QSize(kCoordinatesPrecision, kCoordinatesPrecision));
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
    return std::make_unique<EventsStorage>(mediaServerModule);
}

} // namespace nx::analytics::db
