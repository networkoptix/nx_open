#include "analytics_events_storage.h"

#include <cmath>

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/sql_functions.h>
#include <nx/sql/filter.h>
#include <nx/sql/sql_cursor.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace analytics {
namespace storage {

static constexpr char kSaveEventQueryAggregationKey[] = "c119fb61-b7d3-42c5-b833-456437eaa7c7";
static constexpr int kUsecPerMsec = 1000;

static constexpr auto kMaxTimePeriodLength = std::chrono::minutes(10);
static constexpr auto kTimePeriodPreemptiveLength = std::chrono::minutes(1);

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

EventsStorage::EventsStorage(const Settings& settings):
    m_settings(settings),
    m_dbController(settings.dbConnectionOptions)
{
    NX_CRITICAL(std::pow(10, kCoordinateDecimalDigits) < kCoordinatesPrecision);
}

bool EventsStorage::initialize()
{
    return m_dbController.initialize()
        && readMaximumEventTimestamp()
        && loadDictionaries();
}

void EventsStorage::save(
    common::metadata::ConstDetectionMetadataPacketPtr packet,
    StoreCompletionHandler completionHandler)
{
    using namespace std::placeholders;

    NX_VERBOSE(this, "Saving packet %1", *packet);

    {
        QnMutexLocker lock(&m_mutex);
        m_maxRecordedTimestamp = std::max(
            m_maxRecordedTimestamp,
            std::chrono::microseconds(packet->timestampUsec));
    }

    m_dbController.queryExecutor().executeUpdate(
        std::bind(&EventsStorage::savePacket, this, _1, std::move(packet)),
        [this, completionHandler = std::move(completionHandler)](
            sql::DBResult resultCode)
        {
            completionHandler(dbResultToResultCode(resultCode));
        },
        kSaveEventQueryAggregationKey);
}

void EventsStorage::createLookupCursor(
    Filter filter,
    CreateCursorCompletionHandler completionHandler)
{
    using namespace std::placeholders;
    using namespace nx::utils;

    NX_VERBOSE(this, "Requested cursor with filter %1", filter);

    m_dbController.queryExecutor().createCursor<DetectedObject>(
        std::bind(&EventsStorage::prepareCursorQuery, this, filter, _1),
        std::bind(&EventsStorage::loadObject, this, _1, _2),
        [this, completionHandler = std::move(completionHandler)](
            sql::DBResult resultCode,
            QnUuid dbCursorId)
        {
            if (resultCode != sql::DBResult::ok)
                return completionHandler(ResultCode::error, nullptr);

            completionHandler(
                ResultCode::ok,
                std::make_unique<Cursor>(std::make_unique<sql::Cursor<DetectedObject>>(
                    &m_dbController.queryExecutor(),
                    dbCursorId)));
        });
}

void EventsStorage::lookup(
    Filter filter,
    LookupCompletionHandler completionHandler)
{
    using namespace std::placeholders;

    auto result = std::make_shared<std::vector<DetectedObject>>();
    m_dbController.queryExecutor().executeSelect(
        std::bind(&EventsStorage::selectObjects, this, _1, std::move(filter), result.get()),
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
    using namespace std::placeholders;

    auto result = std::make_shared<QnTimePeriodList>();
    m_dbController.queryExecutor().executeSelect(
        std::bind(&EventsStorage::selectTimePeriods, this,
            _1, std::move(filter), std::move(options), result.get()),
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

    NX_VERBOSE(this, "Cleaning data of device %1 up to timestamp %2",
        deviceId, oldestDataToKeepTimestamp.count());

    m_dbController.queryExecutor().executeUpdate(
        std::bind(&EventsStorage::cleanupData, this, _1, deviceId, oldestDataToKeepTimestamp),
        [this, deviceId, oldestDataToKeepTimestamp](sql::DBResult resultCode)
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
        });
}

bool EventsStorage::readMaximumEventTimestamp()
{
    try
    {
        m_maxRecordedTimestamp = m_dbController.queryExecutor().executeSelectQuerySync(
            [](nx::sql::QueryContext* queryContext)
            {
                auto query = queryContext->connection()->createQuery();
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
        m_dbController.queryExecutor().executeSelectQuerySync(
            [this](nx::sql::QueryContext* queryContext)
            {
                loadObjectTypeDictionary(queryContext);
                loadDeviceDictionary(queryContext);
                fillCurrentTimePeriodsCache(queryContext);
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

void EventsStorage::loadObjectTypeDictionary(nx::sql::QueryContext* queryContext)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare("SELECT id, name FROM object_type");
    query->exec();
    while (query->next())
        addObjectTypeToDictionary(query->value(0).toLongLong(), query->value(1).toString());
}

void EventsStorage::loadDeviceDictionary(nx::sql::QueryContext* queryContext)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare("SELECT id, guid FROM device");
    query->exec();
    while (query->next())
    {
        addDeviceToDictionary(
            query->value(0).toLongLong(),
            QnSql::deserialized_field<QnUuid>(query->value(1)));
    }
}

void EventsStorage::addDeviceToDictionary(
    long long id, const QnUuid& deviceGuid)
{
    QnMutexLocker locker(&m_mutex);
    m_deviceGuidToId.emplace(deviceGuid, id);
    m_idToDeviceGuid.emplace(id, deviceGuid);
}

long long EventsStorage::deviceIdFromGuid(const QnUuid& deviceGuid) const
{
    QnMutexLocker locker(&m_mutex);
    auto it = m_deviceGuidToId.find(deviceGuid);
    return it != m_deviceGuidToId.end() ? it->second : -1;
}

QnUuid EventsStorage::deviceGuidFromId(long long id) const
{
    QnMutexLocker locker(&m_mutex);
    auto it = m_idToDeviceGuid.find(id);
    return it != m_idToDeviceGuid.end() ? it->second : QnUuid();
}

void EventsStorage::addObjectTypeToDictionary(
    long long id, const QString& name)
{
    QnMutexLocker locker(&m_mutex);
    m_objectTypeToId.emplace(name, id);
    m_idToObjectType.emplace(id, name);
}

long long EventsStorage::objectTypeIdFromName(const QString& name) const
{
    QnMutexLocker locker(&m_mutex);
    auto it = m_objectTypeToId.find(name);
    return it != m_objectTypeToId.end() ? it->second : -1;
}

QString EventsStorage::objectTypeFromId(long long id) const
{
    QnMutexLocker locker(&m_mutex);
    auto it = m_idToObjectType.find(id);
    return it != m_idToObjectType.end() ? it->second : QString();
}

sql::DBResult EventsStorage::savePacket(
    sql::QueryContext* queryContext,
    common::metadata::ConstDetectionMetadataPacketPtr packet)
{
    for (const auto& detectedObject: packet->objects)
    {
        updateDictionariesIfNeeded(queryContext, *packet, detectedObject);

        const auto attributesId = insertAttributes(queryContext, detectedObject.labels);

        const auto timePeriodId = insertOrUpdateTimePeriod(queryContext, *packet);

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
    updateDeviceDictionaryIfNeeded(queryContext, packet);
    updateObjectTypeDictionaryIfNeeded(queryContext, detectedObject);
}

void EventsStorage::updateDeviceDictionaryIfNeeded(
    nx::sql::QueryContext* queryContext,
    const common::metadata::DetectionMetadataPacket& packet)
{
    {
        QnMutexLocker locker(&m_mutex);
        if (m_deviceGuidToId.find(packet.deviceId) != m_deviceGuidToId.end())
            return;
    }

    auto query = queryContext->connection()->createQuery();
    query->prepare("INSERT INTO device(guid) VALUES (:guid)");
    query->bindValue(":guid", QnSql::serialized_field(packet.deviceId));
    query->exec();

    addDeviceToDictionary(
        query->impl().lastInsertId().toLongLong(),
        packet.deviceId);
}

void EventsStorage::updateObjectTypeDictionaryIfNeeded(
    nx::sql::QueryContext* queryContext,
    const common::metadata::DetectedObject& detectedObject)
{
    {
        QnMutexLocker locker(&m_mutex);
        if (m_objectTypeToId.find(detectedObject.objectTypeId) != m_objectTypeToId.end())
            return;
    }

    auto query = queryContext->connection()->createQuery();
    query->prepare("INSERT INTO object_type(name) VALUES (:name)");
    query->bindValue(":name", detectedObject.objectTypeId);
    query->exec();

    addObjectTypeToDictionary(
        query->impl().lastInsertId().toLongLong(),
        detectedObject.objectTypeId);
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
        VALUES(:timestampMs, :durationUsec, :deviceId,
            :objectTypeId, :objectAppearanceId, :attributesId,
            :boxTopLeftX, :boxTopLeftY, :boxBottomRightX, :boxBottomRightY)
    )sql"));
    insertEventQuery.bindValue(":timestampMs", packet.timestampUsec / kUsecPerMsec);
    insertEventQuery.bindValue(":durationUsec", packet.durationUsec);
    insertEventQuery.bindValue(":deviceId", deviceIdFromGuid(packet.deviceId));
    insertEventQuery.bindValue(
        ":objectTypeId",
        objectTypeIdFromName(detectedObject.objectTypeId));
    insertEventQuery.bindValue(
        ":objectAppearanceId",
        QnSql::serialized_field(detectedObject.objectId));

    insertEventQuery.bindValue(":attributesId", attributesId);

    insertEventQuery.bindValue(
        ":boxTopLeftX",
        packCoordinate(detectedObject.boundingBox.topLeft().x()));

    insertEventQuery.bindValue(
        ":boxTopLeftY",
        packCoordinate(detectedObject.boundingBox.topLeft().y()));

    insertEventQuery.bindValue(
        ":boxBottomRightX",
        packCoordinate(detectedObject.boundingBox.bottomRight().x()));

    insertEventQuery.bindValue(
        ":boxBottomRightY",
        packCoordinate(detectedObject.boundingBox.bottomRight().y()));

    insertEventQuery.exec();
}

long long EventsStorage::insertAttributes(
    sql::QueryContext* queryContext,
    const std::vector<common::metadata::Attribute>& eventAttributes)
{
    const auto content = QJson::serialized(eventAttributes);

    auto attributesId = findAttributesIdInCache(content);
    if (attributesId >= 0)
        return attributesId;

    auto findIdQuery = queryContext->connection()->createQuery();
    findIdQuery->prepare("SELECT id FROM unique_attributes WHERE content=:content");
    findIdQuery->bindValue(":content", content);
    findIdQuery->exec();
    if (findIdQuery->next())
    {
        const auto id = findIdQuery->value(0).toLongLong();
        addToAttributesCache(id, content);
        return id;
    }

    // No such value. Inserting.
    auto insertContentQuery = queryContext->connection()->createQuery();
    insertContentQuery->prepare("INSERT INTO unique_attributes(content) VALUES (:content)");
    insertContentQuery->bindValue(":content", content);
    insertContentQuery->exec();
    const auto id = insertContentQuery->impl().lastInsertId().toLongLong();
    addToAttributesCache(id, content);

    // NOTE: Following string is in non-reversable format. So, cannot use it to store attributes.
    // But, cannot use JSON for full text search since it contains additional information.
    const auto contentForTextSearch =
        containerString(eventAttributes, lit("; ") /*delimiter*/,
            QString() /*prefix*/, QString() /*suffix*/, QString() /*empty*/);

    insertContentQuery = queryContext->connection()->createQuery();
    insertContentQuery->prepare(
        "INSERT INTO attributes_text_index(docid, content) VALUES (:id, :content)");
    insertContentQuery->bindValue(":id", id);
    insertContentQuery->bindValue(":content", contentForTextSearch);
    insertContentQuery->exec();

    return id;
}

long long EventsStorage::insertOrUpdateTimePeriod(
    nx::sql::QueryContext* queryContext,
    const common::metadata::DetectionMetadataPacket& packet)
{
    using namespace std::chrono;

    const auto packetTimestamp = duration_cast<milliseconds>(microseconds(packet.timestampUsec));
    const auto packetDuration = duration_cast<milliseconds>(microseconds(packet.durationUsec));
    const auto deviceId = deviceIdFromGuid(packet.deviceId);

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

detail::TimePeriod* EventsStorage::insertOrFetchCurrentTimePeriod(
    nx::sql::QueryContext* queryContext,
    long long deviceId,
    std::chrono::milliseconds packetTimestamp,
    std::chrono::milliseconds packetDuration)
{
    auto periodIter = m_deviceIdToCurrentTimePeriod.find(deviceId);
    if (periodIter != m_deviceIdToCurrentTimePeriod.end())
        return &periodIter->second;

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
    query->bindValue(":periodStartMs", (long long) packetTimestamp.count());
    // Filling period end so that this period is used in queries.
    query->bindValue(":periodEndMs", (long long) (timePeriod.endTime + kTimePeriodPreemptiveLength).count());

    query->exec();

    timePeriod.id = query->lastInsertId().toLongLong();
    timePeriod.lastSavedEndTime = timePeriod.endTime + kTimePeriodPreemptiveLength;

    periodIter = m_deviceIdToCurrentTimePeriod.emplace(deviceId, timePeriod).first;
    m_idToTimePeriod[periodIter->second.id] = periodIter;

    NX_VERBOSE(this, "Added new time period %1 (device %2, start time %3)",
        timePeriod.id, deviceGuidFromId(timePeriod.deviceId), timePeriod.startTime);

    return &periodIter->second;
}

void EventsStorage::closeCurrentTimePeriod(
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
        periodIter->second.id, deviceGuidFromId(deviceId));

    m_idToTimePeriod.erase(periodIter->second.id);
    m_deviceIdToCurrentTimePeriod.erase(periodIter);
}

void EventsStorage::saveTimePeriodEnd(
    nx::sql::QueryContext* queryContext,
    long long id,
    std::chrono::milliseconds endTime)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(
        "UPDATE time_period_full SET period_end_ms=:periodEndMs WHERE id=:id");

    query->bindValue(":id", id);
    query->bindValue(":periodEndMs", (long long) endTime.count());
    query->exec();

    NX_VERBOSE(this, "Updated end of time period %1 to %2", id, endTime);
}

void EventsStorage::fillCurrentTimePeriodsCache(
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

        auto it = m_deviceIdToCurrentTimePeriod.emplace(timePeriod.deviceId, timePeriod).first;
        m_idToTimePeriod[timePeriod.id] = it;
    }
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

    QString sqlLimitStr;
    if (filter.maxObjectsToSelect > 0)
        sqlLimitStr = lm("LIMIT %1").args(filter.maxObjectsToSelect).toQString();

    // NOTE: Limiting filtered_events subquery to make query
    // CPU/memory requirements much less dependent of DB size.
    // Assuming that objects tracks are whether interleaved or quite short.
    // So, in situation, when there is a single 100,000 - records long object track
    // selected by filter less objects than requested filter.maxObjectsToSelect would be returned.
    constexpr int kMaxFilterEventsResultSize = 100000;

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

    if (!filter.deviceIds.empty())
    {
        auto condition = std::make_unique<nx::sql::SqlFilterFieldAnyOf>(
            "device_id", ":deviceId");
        for (const auto& deviceGuid: filter.deviceIds)
            condition->addValue(deviceIdFromGuid(deviceGuid));
        sqlFilter.addCondition(std::move(condition));
    }

    if (!filter.objectAppearanceId.isNull())
    {
        sqlFilter.addCondition(std::make_unique<nx::sql::SqlFilterFieldEqual>(
            "object_id", ":objectAppearanceId", QnSql::serialized_field(filter.objectAppearanceId)));
    }

    if (!filter.objectTypeId.empty())
        addObjectTypeIdToFilter(filter.objectTypeId, &sqlFilter);

    if (!filter.timePeriod.isNull())
        addTimePeriodToFilter(filter.timePeriod, &sqlFilter);

    if (!filter.boundingBox.isNull())
        addBoundingBoxToFilter(filter.boundingBox, &sqlFilter);

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

void EventsStorage::addObjectTypeIdToFilter(
    const std::vector<QString>& objectTypes,
    nx::sql::Filter* sqlFilter)
{
    auto condition = std::make_unique<nx::sql::SqlFilterFieldAnyOf>(
        "object_type_id", ":objectTypeId");
    for (const auto& objectType: objectTypes)
        condition->addValue(objectTypeIdFromName(objectType));
    sqlFilter->addCondition(std::move(condition));
}

void EventsStorage::addTimePeriodToFilter(
    const QnTimePeriod& timePeriod,
    nx::sql::Filter* sqlFilter)
{
    using namespace std::chrono;

    auto startTimeFilterField = std::make_unique<nx::sql::SqlFilterFieldGreaterOrEqual>(
        "timestamp_usec_utc",
        ":startTimeMs",
        QnSql::serialized_field(duration_cast<milliseconds>(
            timePeriod.startTime()).count()));
    sqlFilter->addCondition(std::move(startTimeFilterField));

    if (timePeriod.durationMs != QnTimePeriod::kInfiniteDuration &&
        timePeriod.startTimeMs + timePeriod.durationMs <
            duration_cast<milliseconds>(m_maxRecordedTimestamp).count())
    {
        auto endTimeFilterField = std::make_unique<nx::sql::SqlFilterFieldLess>(
            "timestamp_usec_utc",
            ":endTimeMs",
            QnSql::serialized_field(duration_cast<milliseconds>(
                timePeriod.endTime()).count()));
        sqlFilter->addCondition(std::move(endTimeFilterField));
    }
}

void EventsStorage::addBoundingBoxToFilter(
    const QRectF& boundingBox,
    nx::sql::Filter* sqlFilter)
{
    auto topLeftXFilter = std::make_unique<nx::sql::SqlFilterFieldLessOrEqual>(
        "box_top_left_x",
        ":boxTopLeftX",
        QnSql::serialized_field(packCoordinate(boundingBox.bottomRight().x())));
    sqlFilter->addCondition(std::move(topLeftXFilter));

    auto bottomRightXFilter = std::make_unique<nx::sql::SqlFilterFieldGreaterOrEqual>(
        "box_bottom_right_x",
        ":boxBottomRightX",
        QnSql::serialized_field(packCoordinate(boundingBox.topLeft().x())));
    sqlFilter->addCondition(std::move(bottomRightXFilter));

    auto topLeftYFilter = std::make_unique<nx::sql::SqlFilterFieldLessOrEqual>(
        "box_top_left_y",
        ":boxTopLeftY",
        QnSql::serialized_field(packCoordinate(boundingBox.bottomRight().y())));
    sqlFilter->addCondition(std::move(topLeftYFilter));

    auto bottomRightYFilter = std::make_unique<nx::sql::SqlFilterFieldGreaterOrEqual>(
        "box_bottom_right_y",
        ":boxBottomRightY",
        QnSql::serialized_field(packCoordinate(boundingBox.topLeft().y())));
    sqlFilter->addCondition(std::move(bottomRightYFilter));
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
        objectTypeFromId(selectEventsQuery->value("object_type_id").toLongLong());
    QJson::deserialize(
        selectEventsQuery->value("attributes").toString(),
        &object->attributes);

    object->track.push_back(ObjectPosition());
    ObjectPosition& objectPosition = object->track.back();

    objectPosition.deviceId = deviceGuidFromId(selectEventsQuery->value("device_id").toLongLong());
    objectPosition.timestampUsec =
        selectEventsQuery->value("timestamp_usec_utc").toLongLong() * kUsecPerMsec;
    objectPosition.durationUsec = selectEventsQuery->value("duration_usec").toLongLong();

    objectPosition.boundingBox.setTopLeft(QPointF(
        unpackCoordinate(selectEventsQuery->value("box_top_left_x").toInt()),
        unpackCoordinate(selectEventsQuery->value("box_top_left_y").toInt())));
    objectPosition.boundingBox.setBottomRight(QPointF(
        unpackCoordinate(selectEventsQuery->value("box_bottom_right_x").toInt()),
        unpackCoordinate(selectEventsQuery->value("box_bottom_right_y").toInt())));
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
        object.firstAppearanceTimeUsec = trackInfoQuery.value(0).toLongLong() * kUsecPerMsec;
        object.lastAppearanceTimeUsec = trackInfoQuery.value(1).toLongLong() * kUsecPerMsec;
    }
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
    loadTimePeriods(query.get(), options, result);

    return nx::sql::DBResult::ok;
}

void EventsStorage::prepareSelectTimePeriodsUnfilteredQuery(
    nx::sql::AbstractSqlQuery* query,
    const std::vector<QnUuid>& deviceGuids,
    const QnTimePeriod& /*timePeriod*/,
    const TimePeriodsLookupOptions& /*options*/)
{
    nx::sql::Filter sqlFilter;

    if (!deviceGuids.empty())
    {
        auto condition = std::make_unique<nx::sql::SqlFilterFieldAnyOf>(
            "device_id", ":deviceId");
        for (const auto& deviceGuid: deviceGuids)
            condition->addValue(deviceIdFromGuid(deviceGuid));
        sqlFilter.addCondition(std::move(condition));
    }

    // TODO: META-226 timePeriod

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
        SELECT -1 AS id, timestamp_usec_utc AS period_start_ms, duration_usec / 1000 AS duration_ms
        FROM %1
        %2
        ORDER BY timestamp_usec_utc ASC
    )sql").args(eventsFilteredByFreeTextSubQuery, sqlQueryFilterStr));
    sqlQueryFilter.bindFields(query);
}

void EventsStorage::loadTimePeriods(
    nx::sql::AbstractSqlQuery* query,
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

        if (auto it = m_idToTimePeriod.find(id); it != m_idToTimePeriod.end())
            timePeriod.setEndTime(it->second->second.endTime);

        result->push_back(timePeriod);
    }

    *result = QnTimePeriodList::aggregateTimePeriodsUnconstrained(
        *result, options.detailLevel);
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
    deleteEventsQuery.bindValue(":deviceId", deviceIdFromGuid(deviceId));
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

void EventsStorage::addToAttributesCache(
    long long id,
    const QByteArray& content)
{
    static constexpr int kCacheSize = 101;

    m_attributesCache.push_back({
        QCryptographicHash::hash(content, QCryptographicHash::Md5),
        id});

    if (m_attributesCache.size() > kCacheSize)
        m_attributesCache.pop_front();
}

long long EventsStorage::findAttributesIdInCache(
    const QByteArray& content)
{
    const auto md5 = QCryptographicHash::hash(content, QCryptographicHash::Md5);

    const auto it = std::find_if(
        m_attributesCache.rbegin(), m_attributesCache.rend(),
        [&md5](const auto& entry) { return entry.md5 == md5; });

    return it != m_attributesCache.rend() ? it->id : -1;
}

int EventsStorage::packCoordinate(double value)
{
    return (int) (value * kCoordinatesPrecision);
}

double EventsStorage::unpackCoordinate(int packedValue)
{
    return packedValue / (double) kCoordinatesPrecision;
}

//-------------------------------------------------------------------------------------------------

EventsStorageFactory::EventsStorageFactory():
    base_type(std::bind(&EventsStorageFactory::defaultFactoryFunction, this,
        std::placeholders::_1))
{
}

EventsStorageFactory& EventsStorageFactory::instance()
{
    static EventsStorageFactory staticInstance;
    return staticInstance;
}

std::unique_ptr<AbstractEventsStorage> EventsStorageFactory::defaultFactoryFunction(
    const Settings& settings)
{
    return std::make_unique<EventsStorage>(settings);
}

} // namespace storage
} // namespace analytics
} // namespace nx
