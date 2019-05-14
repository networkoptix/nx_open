#include "analytics_db.h"

#include <cmath>

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/sql_functions.h>
#include <nx/sql/filter.h>
#include <nx/sql/sql_cursor.h>
#include <nx/utils/log/log.h>

#include <analytics/db/config.h>

#include "analytics_db_cursor.h"
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
    m_timePeriodDao(&m_deviceDao),
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
    // TODO: Waiting for completion or cancelling posted queries.
}

bool EventsStorage::initialize(const Settings& settings)
{
    NX_DEBUG(this, "Opening analytics event storage from [%1]",
        settings.dbConnectionOptions.dbName);
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
    
    auto dbConnectionOptions = settings.dbConnectionOptions;
    dbConnectionOptions.dbName = settings.path + "/" + dbConnectionOptions.dbName;
    m_dbController = std::make_unique<DbController>(dbConnectionOptions);
    if (!m_dbController->initialize()
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
    using namespace std::placeholders;
    using namespace std::chrono;

    NX_VERBOSE(this, "Saving packet %1", *packet);

    {
        QnMutexLocker lock(&m_mutex);
        m_maxRecordedTimestamp = std::max<milliseconds>(
            m_maxRecordedTimestamp,
            duration_cast<milliseconds>(microseconds(packet->timestampUsec)));
    }

    if (!kUseTrackAggregation)
    {
        QnMutexLocker lock(&m_dbControllerMutex);

        if (!m_dbController)
        {
            NX_DEBUG(this, "Attempt to write to non-initialized analytics DB");
            return;
        }

        m_dbController->queryExecutor().executeUpdate(
            std::bind(&EventsStorage::savePacket, this, _1, std::move(packet)),
            [this](sql::DBResult resultCode) { logDataSaveResult(resultCode); },
            kSaveEventQueryAggregationKey);
    }
    else
    {
        QnMutexLocker lock(&m_mutex);
        savePacketDataToCache(lock, packet);
        auto detectionDataSaver = takeDataToSave(lock, /*flush*/ false);
        lock.unlock();

        QnMutexLocker dbLock(&m_dbControllerMutex);
        
        if (!m_dbController)
        {
            NX_DEBUG(this, "Attempt to write to non-initialized analytics DB");
            return;
        }

        // TODO: #ak Avoid executeUpdate for every packet. We need it only to save a time period.
        m_dbController->queryExecutor().executeUpdate(
            [this, packet = packet, detectionDataSaver = std::move(detectionDataSaver)](
                nx::sql::QueryContext* queryContext) mutable
            {
                m_timePeriodDao.insertOrUpdateTimePeriod(queryContext, *packet);
                if (!detectionDataSaver.empty())
                    detectionDataSaver.save(queryContext);
                return nx::sql::DBResult::ok;
            },
            [this](sql::DBResult resultCode) { logDataSaveResult(resultCode); },
            kSaveEventQueryAggregationKey);
    }
}

void EventsStorage::createLookupCursor(
    Filter filter,
    CreateCursorCompletionHandler completionHandler)
{
    using namespace std::placeholders;
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

    if (kUseTrackAggregation)
    {
        auto objectSearcher = std::make_shared<ObjectSearcher>(
            m_deviceDao,
            m_objectTypeDao,
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
    else
    {
        m_dbController->queryExecutor().createCursor<DetectedObject>(
            std::bind(&EventsStorage::prepareCursorQuery, this, filter, _1),
            std::bind(&EventsStorage::loadObject, this, _1, _2),
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
}

void EventsStorage::closeCursor(const std::shared_ptr<AbstractCursor>& cursor)
{
    QnMutexLocker lock(&m_cursorsMutex);
    m_openedCursors.remove(cursor);
}

void EventsStorage::lookup(
    Filter filter,
    LookupCompletionHandler completionHandler)
{
    QnMutexLocker lock(&m_dbControllerMutex);

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
            if (kUseTrackAggregation)
            {
                ObjectSearcher objectSearcher(
                    m_deviceDao,
                    m_objectTypeDao,
                    std::move(filter));
                *result = objectSearcher.lookup(queryContext);
                return nx::sql::DBResult::ok;
            }
            else
            {
                return selectObjects(queryContext, std::move(filter), result.get());
            }
        },
        [this, result, completionHandler = std::move(completionHandler)](
            sql::DBResult resultCode)
        {
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
            if (kUseTrackAggregation)
            {
                TimePeriodFetcher timePeriodFetcher(
                    m_deviceDao,
                    m_objectTypeDao,
                    m_timePeriodDao,
                    m_analyticsArchiveDirectory.get(),
                    m_maxRecordedTimestamp);
                return timePeriodFetcher.selectTimePeriods(
                    queryContext, filter, options, result.get());
            }
            else
            {
                return selectTimePeriods(queryContext, filter, options, result.get());
            }
        },
        [this, result, completionHandler = std::move(completionHandler)](
            sql::DBResult resultCode)
        {
            completionHandler(
                dbResultToResultCode(resultCode),
                std::move(*result));
        });
}

void EventsStorage::markDataAsDeprecated(
    QnUuid deviceId,
    std::chrono::milliseconds oldestDataToKeepTimestamp)
{
    using namespace std::placeholders;

    QnMutexLocker lock(&m_dbControllerMutex);

    if (!m_dbController)
    {
        NX_DEBUG(this, "Attempt to delete data from non-initialized analytics DB");
        return;
    }

    NX_VERBOSE(this, "Cleaning data of device %1 up to timestamp %2",
        deviceId, oldestDataToKeepTimestamp.count());

    if (kUseTrackAggregation)
    {
        scheduleDataCleanup(deviceId, oldestDataToKeepTimestamp);
    }
    else
    {
        m_dbController->queryExecutor().executeUpdate(
            std::bind(&EventsStorage::cleanupData, this, _1, deviceId, oldestDataToKeepTimestamp),
            [this, deviceId, oldestDataToKeepTimestamp](nx::sql::DBResult result)
            {
                logCleanupResult(result, deviceId, oldestDataToKeepTimestamp);
            });
    }
}

void EventsStorage::flush(StoreCompletionHandler completionHandler)
{
    QnMutexLocker lock(&m_dbControllerMutex);

    if (!m_dbController)
    {
        NX_DEBUG(this, "Attempt to flush non-initialized analytics DB");
        return;
    }

    m_dbController->queryExecutor().executeUpdate(
        [this](nx::sql::QueryContext* queryContext)
        {
            NX_DEBUG(this, "Flushing unsaved data");

            if (kUseTrackAggregation)
            {
                QnMutexLocker lock(&m_mutex);
                auto detectionDataSaver = takeDataToSave(lock, /*flush*/ true);
                lock.unlock();

                if (!detectionDataSaver.empty())
                    detectionDataSaver.save(queryContext);
            }

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
    try
    {
        m_maxRecordedTimestamp = m_dbController->queryExecutor().executeSelectQuerySync(
            [](nx::sql::QueryContext* queryContext)
            {
                auto query = queryContext->connection()->createQuery();
                if (kUseTrackAggregation)
                    query->prepare("SELECT max(timestamp_seconds_utc) * 1000 FROM object_search");
                else
                    query->prepare("SELECT max(timestamp_usec_utc) FROM event");
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

bool EventsStorage::loadDictionaries()
{
    try
    {
        m_dbController->queryExecutor().executeSelectQuerySync(
            [this](nx::sql::QueryContext* queryContext)
            {
                m_objectTypeDao.loadObjectTypeDictionary(queryContext);
                m_deviceDao.loadDeviceDictionary(queryContext);
                m_timePeriodDao.fillCurrentTimePeriodsCache(queryContext);
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

sql::DBResult EventsStorage::savePacket(
    sql::QueryContext* queryContext,
    common::metadata::ConstDetectionMetadataPacketPtr packet)
{
    for (const auto& detectedObject: packet->objects)
    {
        updateDictionariesIfNeeded(queryContext, *packet, detectedObject);

        const auto attributesId = m_attributesDao.insertOrFetchAttributes(
            queryContext,
            detectedObject.labels);

        const auto timePeriodId = m_timePeriodDao.insertOrUpdateTimePeriod(queryContext, *packet);

        insertEvent(
            queryContext,
            *packet,
            detectedObject,
            attributesId,
            timePeriodId);
    }

    return sql::DBResult::ok;
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
    long long attributesId,
    long long /*timePeriodId*/)
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
        m_objectTypeDao.objectTypeIdFromName(detectedObject.objectTypeId));
    insertEventQuery.bindValue(
        ":objectAppearanceId",
        QnSql::serialized_field(detectedObject.objectId));

    insertEventQuery.bindValue(":attributesId", attributesId);

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
        &m_objectCache,
        m_analyticsArchiveDirectory.get());

    detectionDataSaver.load(&m_trackAggregator, flushData);

    m_objectCache.removeExpiredData();

    return detectionDataSaver;
}

void EventsStorage::prepareCursorQuery(
    const Filter& filter,
    nx::sql::SqlQuery* query)
{
    QString eventsFilteredByFreeTextSubQuery;
    const auto sqlQueryFilter =
        prepareSqlFilterExpression(filter, &eventsFilteredByFreeTextSubQuery);

    std::string whereClause = "WHERE e.attributes_id = attrs.id";

    auto sqlQueryFilterStr = sqlQueryFilter.toString();
    if (!sqlQueryFilterStr.empty())
        whereClause += " AND " + sqlQueryFilterStr;

    // TODO: #ak Think over limit in the following query.
    query->prepare(lm(R"sql(
        SELECT timestamp_usec_utc, duration_usec, device_id,
            object_type_id, object_id, attrs.content as attributes,
            box_top_left_x, box_top_left_y, box_bottom_right_x, box_bottom_right_y
        FROM %1 as e, unique_attributes attrs
        %2
        ORDER BY timestamp_usec_utc %3, object_id %3
        LIMIT 30000;
    )sql").args(
        eventsFilteredByFreeTextSubQuery,
        whereClause,
        filter.sortOrder == Qt::SortOrder::AscendingOrder ? "ASC" : "DESC").toQString());
    sqlQueryFilter.bindFields(query);
}

nx::sql::DBResult EventsStorage::selectObjects(
    nx::sql::QueryContext* queryContext,
    const Filter& filter,
    std::vector<DetectedObject>* result)
{
    sql::SqlQuery selectEventsQuery(queryContext->connection());
    selectEventsQuery.setForwardOnly(true);
    prepareLookupQuery(filter, &selectEventsQuery);
    selectEventsQuery.exec();

    loadObjects(selectEventsQuery, filter, result);

    queryTrackInfo(queryContext, result);

    return nx::sql::DBResult::ok;
}

void EventsStorage::prepareLookupQuery(
    const Filter& filter,
    nx::sql::SqlQuery* query)
{
    QString eventsFilteredByFreeTextSubQuery;
    const auto sqlQueryFilter =
        prepareSqlFilterExpression(filter, &eventsFilteredByFreeTextSubQuery);
    auto sqlQueryFilterStr = sqlQueryFilter.toString();
    if (!sqlQueryFilterStr.empty())
        sqlQueryFilterStr = "WHERE " + sqlQueryFilterStr;

    int maxObjectsToSelect = kMaxObjectLookupResultSet;
    if (filter.maxObjectsToSelect > 0)
        maxObjectsToSelect = std::min<int>(filter.maxObjectsToSelect, maxObjectsToSelect);

    auto sqlLimitStr = lm("LIMIT %1").args(maxObjectsToSelect).toQString();

    query->prepare(lm(R"sql(
        WITH filtered_events AS
        (SELECT timestamp_usec_utc, duration_usec, device_id,
            object_type_id, object_id, attributes_id,
            box_top_left_x, box_top_left_y, box_bottom_right_x, box_bottom_right_y
         FROM %1
         %2
         ORDER BY timestamp_usec_utc DESC
         LIMIT %3)
        SELECT *, attrs.content as attributes
        FROM filtered_events e
            INNER JOIN
            (SELECT object_id AS id, MIN(timestamp_usec_utc) AS track_start_time
             FROM filtered_events
             GROUP BY object_id
             ORDER BY MIN(timestamp_usec_utc) DESC
             %4) object
            ON e.object_id = object.id,
            unique_attributes attrs
        WHERE e.attributes_id = attrs.id
        ORDER BY object.track_start_time %5, e.timestamp_usec_utc ASC
    )sql").args(
        eventsFilteredByFreeTextSubQuery,
        sqlQueryFilterStr,
        kMaxFilterEventsResultSize,
        sqlLimitStr,
        filter.sortOrder == Qt::SortOrder::AscendingOrder ? "ASC" : "DESC").toQString());

    sqlQueryFilter.bindFields(query);
}

nx::sql::Filter EventsStorage::prepareSqlFilterExpression(
    const Filter& filter,
    QString* eventsFilteredByFreeTextSubQuery)
{
    nx::sql::Filter sqlFilter;

    ObjectSearcher::addObjectFilterConditions(
        filter,
        m_deviceDao,
        m_objectTypeDao,
        {"object_id", "timestamp_usec_utc", "timestamp_usec_utc"},
        &sqlFilter);

    if (!filter.boundingBox.isNull())
        ObjectSearcher::addBoundingBoxToFilter(packRect(filter.boundingBox), &sqlFilter);

    if (!filter.freeText.isEmpty())
    {
        *eventsFilteredByFreeTextSubQuery = lm(R"sql(
            (SELECT rowid, * FROM event WHERE attributes_id IN
             (SELECT docid FROM attributes_text_index WHERE content MATCH '%1*'))
        )sql").args(filter.freeText);
    }
    else
    {
        *eventsFilteredByFreeTextSubQuery = "event";
    }

    return sqlFilter;
}

void EventsStorage::loadObjects(
    sql::SqlQuery& selectEventsQuery,
    const Filter& filter,
    std::vector<DetectedObject>* result)
{
    struct ObjectLoadingContext
    {
        std::size_t posInResultVector = -1;
        bool ignore = false;
    };

    std::map<QnUuid, ObjectLoadingContext> objectIdToPosition;

    for (int count = 0; selectEventsQuery.next(); ++count)
    {
        DetectedObject detectedObject;
        loadObject(&selectEventsQuery, &detectedObject);

        auto [objContextIter, objContextInserted] = objectIdToPosition.emplace(
            detectedObject.objectAppearanceId,
            ObjectLoadingContext{result->size(), false});
        if (objContextInserted)
        {
            if (filter.maxObjectsToSelect > 0 && (int) result->size() >= filter.maxObjectsToSelect)
            {
                objContextIter->second.ignore = true;
                continue;
            }
            result->push_back(std::move(detectedObject));
        }
        else
        {
            if (objContextIter->second.ignore)
                continue;

            DetectedObject& existingObject = result->at(
                objContextIter->second.posInResultVector);

            bool loadTrack = true;
            if (filter.maxTrackSize > 0
                && (int) existingObject.track.size() >= filter.maxTrackSize)
            {
                loadTrack = false;
            }

            if (filter.sortOrder == Qt::AscendingOrder || !loadTrack)
            {
                mergeObjects(std::move(detectedObject), &existingObject, loadTrack);
            }
            else
            {
                mergeObjects(std::move(existingObject), &detectedObject, loadTrack);
                existingObject = std::move(detectedObject);
            }
        }
    }
}

void EventsStorage::loadObject(
    sql::SqlQuery* selectEventsQuery,
    DetectedObject* object)
{
    object->objectAppearanceId = QnSql::deserialized_field<QnUuid>(
        selectEventsQuery->value("object_id"));
    object->objectTypeId =
        m_objectTypeDao.objectTypeFromId(selectEventsQuery->value("object_type_id").toLongLong());

    object->attributes = AttributesDao::deserialize(
        selectEventsQuery->value("attributes").toString());

    object->track.push_back(ObjectPosition());
    ObjectPosition& objectPosition = object->track.back();

    objectPosition.deviceId = m_deviceDao.deviceGuidFromId(
        selectEventsQuery->value("device_id").toLongLong());
    // NOTE: *_usec fields actually contain ms until the completion of META-225.
    objectPosition.timestampUsec =
        selectEventsQuery->value("timestamp_usec_utc").toLongLong() * kUsecInMs;
    objectPosition.durationUsec = selectEventsQuery->value("duration_usec").toLongLong() * kUsecInMs;

    const QRect packedBoundingBox(
        QPoint(
            selectEventsQuery->value("box_top_left_x").toInt(),
            selectEventsQuery->value("box_top_left_y").toInt()),
        QPoint(
            selectEventsQuery->value("box_bottom_right_x").toInt(),
            selectEventsQuery->value("box_bottom_right_y").toInt()));
    objectPosition.boundingBox = unpackRect(packedBoundingBox);
}

void EventsStorage::mergeObjects(
    DetectedObject from,
    DetectedObject* to,
    bool loadTrack)
{
    if (loadTrack)
    {
        to->track.insert(
            to->track.end(),
            std::make_move_iterator(from.track.begin()),
            std::make_move_iterator(from.track.end()));
    }

    // TODO: #ak moving attributes.
    for (auto& attribute: from.attributes)
    {
        auto existingAttributeIter = std::find_if(
            to->attributes.begin(), to->attributes.end(),
            [&attribute](const nx::common::metadata::Attribute& existingAttribute)
            {
                return existingAttribute.name == attribute.name;
            });
        if (existingAttributeIter == to->attributes.end())
        {
            to->attributes.push_back(std::move(attribute));
            continue;
        }
        else if (existingAttributeIter->value == attribute.value)
        {
            continue;
        }
        else
        {
            // TODO: #ak Not sure it is correct.
            *existingAttributeIter = std::move(attribute);
        }
    }
}

void EventsStorage::queryTrackInfo(
    nx::sql::QueryContext* queryContext,
    std::vector<DetectedObject>* result)
{
    sql::SqlQuery trackInfoQuery(queryContext->connection());
    trackInfoQuery.setForwardOnly(true);
    trackInfoQuery.prepare(QString::fromLatin1(R"sql(
        SELECT min(timestamp_usec_utc), max(timestamp_usec_utc)
        FROM event
        WHERE object_id = :objectAppearanceId
    )sql"));

    for (auto& object: *result)
    {
        trackInfoQuery.bindValue(
            ":objectAppearanceId",
            QnSql::serialized_field(object.objectAppearanceId));
        trackInfoQuery.exec();
        if (!trackInfoQuery.next())
            continue;
        object.firstAppearanceTimeUsec = trackInfoQuery.value(0).toLongLong() * kUsecInMs;
        object.lastAppearanceTimeUsec = trackInfoQuery.value(1).toLongLong() * kUsecInMs;
    }
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

    std::shared_ptr<AbstractCursor> cursor;
    if (kUseTrackAggregation)
        cursor = ObjectSearcher::createCursor(std::move(dbCursor));
    else
        cursor = std::make_shared<deprecated::Cursor>(std::move(dbCursor));

    m_openedCursors.push_back(cursor);
    lock.unlock();

    completionHandler(
        ResultCode::ok,
        cursor);
}

nx::sql::DBResult EventsStorage::selectTimePeriods(
    nx::sql::QueryContext* queryContext,
    const Filter& filter,
    const TimePeriodsLookupOptions& options,
    QnTimePeriodList* result)
{
    auto query = queryContext->connection()->createQuery();
    query->setForwardOnly(true);

    Filter localFilter = filter;
    localFilter.deviceIds.clear();
    localFilter.timePeriod.clear();

    if (localFilter.empty())
    {
        prepareSelectTimePeriodsUnfilteredQuery(
            query.get(), filter.deviceIds, filter.timePeriod, options);
    }
    else
    {
        prepareSelectTimePeriodsFilteredQuery(query.get(), filter, options);
    }

    query->exec();
    loadTimePeriods(query.get(), filter.timePeriod, options, result);

    return nx::sql::DBResult::ok;
}

void EventsStorage::prepareSelectTimePeriodsUnfilteredQuery(
    nx::sql::AbstractSqlQuery* query,
    const std::vector<QnUuid>& deviceGuids,
    const QnTimePeriod& timePeriod,
    const TimePeriodsLookupOptions& /*options*/)
{
    nx::sql::Filter sqlFilter;

    if (!deviceGuids.empty())
    {
        auto condition = std::make_unique<nx::sql::SqlFilterFieldAnyOf>(
            "device_id", ":deviceId");
        for (const auto& deviceGuid: deviceGuids)
            condition->addValue(m_deviceDao.deviceIdFromGuid(deviceGuid));
        sqlFilter.addCondition(std::move(condition));
    }

    if (!timePeriod.isEmpty())
    {
        auto localTimePeriod = timePeriod;
        if (localTimePeriod.durationMs == QnTimePeriod::kInfiniteDuration)
            localTimePeriod.setEndTime(m_maxRecordedTimestamp);
        else if (localTimePeriod.endTime() > m_maxRecordedTimestamp)
            localTimePeriod.durationMs = QnTimePeriod::kInfiniteDuration;

        ObjectSearcher::addTimePeriodToFilter<std::chrono::milliseconds>(
            localTimePeriod,
            {"period_end_ms", "period_start_ms"},
            &sqlFilter);
    }

    std::string whereClause;
    const auto sqlFilterStr = sqlFilter.toString();
    if (!sqlFilterStr.empty())
        whereClause = "WHERE " + sqlFilterStr;

    query->prepare(lm(R"sql(
        SELECT id, device_id, period_start_ms, period_end_ms - period_start_ms AS duration_ms
        FROM time_period_full
        %1
        ORDER BY period_start_ms ASC
    )sql").args(whereClause));
    sqlFilter.bindFields(query);
}

void EventsStorage::prepareSelectTimePeriodsFilteredQuery(
    nx::sql::AbstractSqlQuery* query,
    const Filter& filter,
    const TimePeriodsLookupOptions& /*options*/)
{
    QString eventsFilteredByFreeTextSubQuery;
    const auto sqlQueryFilter =
        prepareSqlFilterExpression(filter, &eventsFilteredByFreeTextSubQuery);
    auto sqlQueryFilterStr = sqlQueryFilter.toString();
    if (!sqlQueryFilterStr.empty())
        sqlQueryFilterStr = "WHERE " + sqlQueryFilterStr;

    query->prepare(lm(R"sql(
        SELECT -1 AS id, timestamp_usec_utc AS period_start_ms, duration_usec AS duration_ms
        FROM %1
        %2
        ORDER BY timestamp_usec_utc ASC
    )sql").args(eventsFilteredByFreeTextSubQuery, sqlQueryFilterStr));
    sqlQueryFilter.bindFields(query);
}

void EventsStorage::loadTimePeriods(
    nx::sql::AbstractSqlQuery* query,
    const QnTimePeriod& timePeriodFilter,
    const TimePeriodsLookupOptions& options,
    QnTimePeriodList* result)
{
    using namespace std::chrono;

    while (query->next())
    {
        QnTimePeriod timePeriod(
            milliseconds(query->value("period_start_ms").toLongLong()),
            milliseconds(query->value("duration_ms").toLongLong()));

        // Fixing time period end if needed.
        const auto id = query->value("id").toLongLong();
        if (auto detailTimePeriod = m_timePeriodDao.getTimePeriodById(id);
            detailTimePeriod)
        {
            timePeriod.setEndTime(detailTimePeriod->endTime);
        }

        // Truncating time period by the filter
        if (!timePeriodFilter.isEmpty())
        {
            if (timePeriod.startTime() < timePeriodFilter.startTime())
                timePeriod.setStartTime(timePeriodFilter.startTime());

            if (timePeriodFilter.durationMs != QnTimePeriod::kInfiniteDuration &&
                timePeriod.endTime() > timePeriodFilter.endTime())
            {
                timePeriod.setEndTime(timePeriodFilter.endTime());
            }
        }

        if (!result->empty() && result->back() == timePeriod)
            continue;

        result->push_back(timePeriod);
    }

    *result = QnTimePeriodList::aggregateTimePeriodsUnconstrained(
        *result, std::max<milliseconds>(options.detailLevel, kMinTimePeriodAggregationPeriod));
}

void EventsStorage::scheduleDataCleanup(
    QnUuid deviceId,
    std::chrono::milliseconds oldestDataToKeepTimestamp)
{
    m_dbController->queryExecutor().executeUpdate(
        [this, deviceId, oldestDataToKeepTimestamp](nx::sql::QueryContext* queryContext)
        {
            Cleaner cleaner(
                &m_attributesDao,
                m_deviceDao.deviceIdFromGuid(deviceId),
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

nx::sql::DBResult EventsStorage::cleanupData(
    nx::sql::QueryContext* queryContext,
    const QnUuid& deviceId,
    std::chrono::milliseconds oldestDataToKeepTimestamp)
{
    try
    {
        cleanupEvents(queryContext, deviceId, oldestDataToKeepTimestamp);
        cleanupEventProperties(queryContext);
    }
    catch (const std::exception& e)
    {
        NX_INFO(this, lm("Error cleaning up detected objects data. %1").args(e.what()));
    }

    return nx::sql::DBResult::ok;
}

void EventsStorage::cleanupEvents(
    nx::sql::QueryContext* queryContext,
    const QnUuid& deviceId,
    std::chrono::milliseconds oldestDataToKeepTimestamp)
{
    using namespace std::chrono;

    sql::SqlQuery deleteEventsQuery(queryContext->connection());
    deleteEventsQuery.prepare(R"sql(
        DELETE FROM event
        WHERE device_id=:deviceId AND timestamp_usec_utc < :timestampMs
    )sql");
    deleteEventsQuery.bindValue(":deviceId", m_deviceDao.deviceIdFromGuid(deviceId));
    deleteEventsQuery.bindValue(
        ":timestampMs",
        (qint64) milliseconds(oldestDataToKeepTimestamp).count());

    deleteEventsQuery.exec();
}

void EventsStorage::cleanupEventProperties(
    nx::sql::QueryContext* /*queryContext*/)
{
    // TODO: #ak META-222

#if 0
    sql::SqlQuery query(queryContext->connection());

    query.prepare(R"sql(
        DELETE FROM event_properties
        WHERE docid NOT IN (SELECT rowid FROM event)
    )sql");

    query.exec();
#endif
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
