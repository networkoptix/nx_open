#include "analytics_events_storage.h"

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/sql_functions.h>
#include <nx/sql/sql_cursor.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace analytics {
namespace storage {

static const int kUsecInMs = 1000;

EventsStorage::EventsStorage(const Settings& settings):
    m_settings(settings),
    m_dbController(settings.dbConnectionOptions)
{
}

bool EventsStorage::initialize()
{
    return m_dbController.initialize();
}

void EventsStorage::save(
    common::metadata::ConstDetectionMetadataPacketPtr packet,
    StoreCompletionHandler completionHandler)
{
    using namespace std::placeholders;

    NX_VERBOSE(this, lm("Saving packet %1").args(*packet));

    m_dbController.queryExecutor().executeUpdate(
        std::bind(&EventsStorage::savePacket, this, _1, std::move(packet)),
        [this, completionHandler = std::move(completionHandler)](
            sql::DBResult resultCode)
        {
            completionHandler(dbResultToResultCode(resultCode));
        });
}

void EventsStorage::createLookupCursor(
    Filter filter,
    CreateCursorCompletionHandler completionHandler)
{
    using namespace std::placeholders;
    using namespace nx::utils;

    NX_VERBOSE(this, lm("Requested cursor with filter %1").args(filter));

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

    NX_VERBOSE(this, lm("Cleaning data of device %1 up to timestamp %2")
        .args(deviceId, oldestDataToKeepTimestamp.count()));

    m_dbController.queryExecutor().executeUpdate(
        std::bind(&EventsStorage::cleanupData, this, _1, deviceId, oldestDataToKeepTimestamp),
        [this, deviceId, oldestDataToKeepTimestamp](sql::DBResult resultCode)
        {
            if (resultCode == sql::DBResult::ok)
            {
                NX_VERBOSE(this, lm("Cleaned data of device %1 up to timestamp %2")
                    .args(deviceId, oldestDataToKeepTimestamp));
            }
            else
            {
                NX_DEBUG(this, lm("Error (%1) while cleaning up data of device %2 up to timestamp %3")
                    .args(toString(resultCode), deviceId, oldestDataToKeepTimestamp));
            }
        });
}

sql::DBResult EventsStorage::savePacket(
    sql::QueryContext* queryContext,
    common::metadata::ConstDetectionMetadataPacketPtr packet)
{
    for (const auto& detectedObject: packet->objects)
    {
        const auto eventId = insertEvent(queryContext, *packet, detectedObject);
        insertEventAttributes(queryContext, eventId, detectedObject.labels);
    }

    return sql::DBResult::ok;
}

std::int64_t EventsStorage::insertEvent(
    sql::QueryContext* queryContext,
    const common::metadata::DetectionMetadataPacket& packet,
    const common::metadata::DetectedObject& detectedObject)
{
    sql::SqlQuery insertEventQuery(queryContext->connection());
    insertEventQuery.prepare(QString::fromLatin1(R"sql(
        INSERT INTO event(timestamp_usec_utc, duration_usec,
            device_guid, object_type_id, object_id, attributes,
            box_top_left_x, box_top_left_y, box_bottom_right_x, box_bottom_right_y)
        VALUES(:timestampUsec, :durationUsec, :deviceId, :objectTypeId, :objectId, :attributes,
            :boxTopLeftX, :boxTopLeftY, :boxBottomRightX, :boxBottomRightY)
    )sql"));
    insertEventQuery.bindValue(lit(":timestampUsec"), packet.timestampUsec);
    insertEventQuery.bindValue(lit(":durationUsec"), packet.durationUsec);
    insertEventQuery.bindValue(lit(":deviceId"), QnSql::serialized_field(packet.deviceId));
    insertEventQuery.bindValue(
        lit(":objectTypeId"),
        QnSql::serialized_field(detectedObject.objectTypeId));
    insertEventQuery.bindValue(
        lit(":objectId"),
        QnSql::serialized_field(detectedObject.objectId));
    insertEventQuery.bindValue(lit(":attributes"), QJson::serialized(detectedObject.labels));

    insertEventQuery.bindValue(lit(":boxTopLeftX"), detectedObject.boundingBox.topLeft().x());
    insertEventQuery.bindValue(lit(":boxTopLeftY"), detectedObject.boundingBox.topLeft().y());
    insertEventQuery.bindValue(lit(":boxBottomRightX"), detectedObject.boundingBox.bottomRight().x());
    insertEventQuery.bindValue(lit(":boxBottomRightY"), detectedObject.boundingBox.bottomRight().y());

    insertEventQuery.exec();
    return insertEventQuery.impl().lastInsertId().toLongLong();
}

void EventsStorage::insertEventAttributes(
    sql::QueryContext* queryContext,
    std::int64_t eventId,
    const std::vector<common::metadata::Attribute>& eventAttributes)
{
    sql::SqlQuery insertEventAttributesQuery(queryContext->connection());
    insertEventAttributesQuery.prepare(QString::fromLatin1(R"sql(
        INSERT INTO event_properties(docid, content)
        VALUES(:eventId, :content)
    )sql"));
    insertEventAttributesQuery.bindValue(lit(":eventId"), static_cast<qint64>(eventId));
    insertEventAttributesQuery.bindValue(
        lit(":content"),
        containerString(eventAttributes, lit("; ") /*delimiter*/,
            QString() /*prefix*/, QString() /*suffix*/, QString() /*empty*/));
    insertEventAttributesQuery.exec();
}

void EventsStorage::prepareCursorQuery(
    const Filter& filter,
    nx::sql::SqlQuery* query)
{
    QString eventsFilteredByFreeTextSubQuery;
    const auto sqlQueryFilter =
        prepareSqlFilterExpression(filter, &eventsFilteredByFreeTextSubQuery);
    QString sqlQueryFilterStr;
    if (!sqlQueryFilter.empty())
    {
        sqlQueryFilterStr = lm("WHERE %1").args(
            nx::sql::generateWhereClauseExpression(sqlQueryFilter));
    }

    // TODO: #ak Think over limit in the following query.
    query->prepare(lm(R"sql(
        SELECT timestamp_usec_utc, duration_usec, device_guid,
            object_type_id, object_id, attributes,
            box_top_left_x, box_top_left_y, box_bottom_right_x, box_bottom_right_y
        FROM %1
        %2
        ORDER BY timestamp_usec_utc %3, object_id %3
        LIMIT 30000;
    )sql").args(
        eventsFilteredByFreeTextSubQuery,
        sqlQueryFilterStr,
        filter.sortOrder == Qt::SortOrder::AscendingOrder ? "ASC" : "DESC").toQString());
    nx::sql::bindFields(query, sqlQueryFilter);
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
    QString sqlQueryFilterStr;
    if (!sqlQueryFilter.empty())
    {
        sqlQueryFilterStr = lm("WHERE %1").args(
            nx::sql::generateWhereClauseExpression(sqlQueryFilter));
    }

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
        (SELECT timestamp_usec_utc, object_id
         FROM %1
         %2
         ORDER BY timestamp_usec_utc DESC
         LIMIT %3)
        SELECT timestamp_usec_utc, duration_usec, device_guid,
            object_type_id, e.object_id, attributes,
            box_top_left_x, box_top_left_y, box_bottom_right_x, box_bottom_right_y
        FROM event e,
            (SELECT object_id, MIN(timestamp_usec_utc) AS min_timestamp_usec_utc
             FROM filtered_events
             GROUP BY object_id
             ORDER BY MIN(timestamp_usec_utc) DESC
             %4) objects
        WHERE e.timestamp_usec_utc=objects.min_timestamp_usec_utc AND e.object_id=objects.object_id
        ORDER BY e.timestamp_usec_utc %5
    )sql").args(
        eventsFilteredByFreeTextSubQuery,
        sqlQueryFilterStr,
        kMaxFilterEventsResultSize,
        sqlLimitStr,
        filter.sortOrder == Qt::SortOrder::AscendingOrder ? "ASC" : "DESC").toQString());
    nx::sql::bindFields(query, sqlQueryFilter);
}

nx::sql::InnerJoinFilterFields EventsStorage::prepareSqlFilterExpression(
    const Filter& filter,
    QString* eventsFilteredByFreeTextSubQuery)
{
    nx::sql::InnerJoinFilterFields sqlFilter;
    if (!filter.deviceId.isNull())
    {
        nx::sql::SqlFilterFieldEqual filterField(
            "device_guid", ":deviceId", QnSql::serialized_field(filter.deviceId));
        sqlFilter.push_back(std::move(filterField));
    }

    if (!filter.objectId.isNull())
    {
        nx::sql::SqlFilterFieldEqual filterField(
            "object_id", ":objectId", QnSql::serialized_field(filter.objectId));
        sqlFilter.push_back(std::move(filterField));
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
            (SELECT rowid, *
             FROM event
             WHERE rowid IN (SELECT docid FROM event_properties WHERE content MATCH '%1'))
        )sql").args(filter.freeText);
    }
    else
    {
        *eventsFilteredByFreeTextSubQuery = lit("event");
    }

    return sqlFilter;
}

void EventsStorage::addObjectTypeIdToFilter(
    const std::vector<QnUuid>& objectTypeIds,
    nx::sql::InnerJoinFilterFields* sqlFilter)
{
    // TODO: #ak Add support for every objectTypeId specified.

    nx::sql::SqlFilterFieldEqual filterField(
        "object_type_id", ":objectTypeId", QnSql::serialized_field(objectTypeIds.front()));
    sqlFilter->push_back(std::move(filterField));
}

void EventsStorage::addTimePeriodToFilter(
    const QnTimePeriod& timePeriod,
    nx::sql::InnerJoinFilterFields* sqlFilter)
{
    nx::sql::SqlFilterFieldGreaterOrEqual startTimeFilterField(
        "timestamp_usec_utc",
        ":startTimeUsec",
        QnSql::serialized_field(timePeriod.startTimeMs * kUsecInMs));
    sqlFilter->push_back(std::move(startTimeFilterField));

    const auto endTimeMs = timePeriod.startTimeMs + timePeriod.durationMs;
    nx::sql::SqlFilterFieldLess endTimeFilterField(
        "timestamp_usec_utc",
        ":endTimeUsec",
        QnSql::serialized_field(endTimeMs * kUsecInMs));
    sqlFilter->push_back(std::move(endTimeFilterField));
}

void EventsStorage::addBoundingBoxToFilter(
    const QRectF& boundingBox,
    nx::sql::InnerJoinFilterFields* sqlFilter)
{
    nx::sql::SqlFilterFieldLessOrEqual topLeftXFilter(
        "box_top_left_x",
        ":boxTopLeftX",
        QnSql::serialized_field(boundingBox.bottomRight().x()));
    sqlFilter->push_back(std::move(topLeftXFilter));

    nx::sql::SqlFilterFieldGreaterOrEqual bottomRightXFilter(
        "box_bottom_right_x",
        ":boxBottomRightX",
        QnSql::serialized_field(boundingBox.topLeft().x()));
    sqlFilter->push_back(std::move(bottomRightXFilter));

    nx::sql::SqlFilterFieldLessOrEqual topLeftYFilter(
        "box_top_left_y",
        ":boxTopLeftY",
        QnSql::serialized_field(boundingBox.bottomRight().y()));
    sqlFilter->push_back(std::move(topLeftYFilter));

    nx::sql::SqlFilterFieldGreaterOrEqual bottomRightYFilter(
        "box_bottom_right_y",
        ":boxBottomRightY",
        QnSql::serialized_field(boundingBox.topLeft().y()));
    sqlFilter->push_back(std::move(bottomRightYFilter));
}

void EventsStorage::loadObjects(
    sql::SqlQuery& selectEventsQuery,
    const Filter& filter,
    std::vector<DetectedObject>* result)
{
    std::map<QnUuid, std::vector<DetectedObject>::size_type> objectIdToPosition;

    for (int count = 0; selectEventsQuery.next(); ++count)
    {
        if (filter.maxObjectsToSelect > 0 && count >= filter.maxObjectsToSelect)
            break;

        DetectedObject detectedObject;
        loadObject(&selectEventsQuery, &detectedObject);

        auto iterAndIsInsertedFlag =
            objectIdToPosition.emplace(detectedObject.objectId, result->size());
        if (iterAndIsInsertedFlag.second)
        {
            //if (filter.maxObjectsToSelect > 0 && (int) result->size() >= filter.maxObjectsToSelect)
            //    break;
            result->push_back(std::move(detectedObject));
        }
        else
        {
            DetectedObject& existingObject = result->at(iterAndIsInsertedFlag.first->second);
            if (filter.sortOrder == Qt::AscendingOrder)
            {
                mergeObjects(std::move(detectedObject), &existingObject);
            }
            else
            {
                mergeObjects(std::move(existingObject), &detectedObject);
                existingObject = std::move(detectedObject);
            }
        }
    }
}

void EventsStorage::loadObject(
    sql::SqlQuery* selectEventsQuery,
    DetectedObject* object)
{
    object->objectId = QnSql::deserialized_field<QnUuid>(
        selectEventsQuery->value(lit("object_id")));
    object->objectTypeId = QnSql::deserialized_field<QnUuid>(
        selectEventsQuery->value(lit("object_type_id")));
    QJson::deserialize(
        selectEventsQuery->value(lit("attributes")).toString(),
        &object->attributes);

    object->track.push_back(ObjectPosition());
    ObjectPosition& objectPosition = object->track.back();

    objectPosition.deviceId = QnSql::deserialized_field<QnUuid>(
        selectEventsQuery->value(lit("device_guid")));
    objectPosition.timestampUsec = selectEventsQuery->value(lit("timestamp_usec_utc")).toLongLong();
    objectPosition.durationUsec = selectEventsQuery->value(lit("duration_usec")).toLongLong();

    objectPosition.boundingBox.setTopLeft(QPointF(
        selectEventsQuery->value(lit("box_top_left_x")).toDouble(),
        selectEventsQuery->value(lit("box_top_left_y")).toDouble()));
    objectPosition.boundingBox.setBottomRight(QPointF(
        selectEventsQuery->value(lit("box_bottom_right_x")).toDouble(),
        selectEventsQuery->value(lit("box_bottom_right_y")).toDouble()));
}

void EventsStorage::mergeObjects(DetectedObject from, DetectedObject* to)
{
    to->track.insert(
        to->track.end(),
        std::make_move_iterator(from.track.begin()),
        std::make_move_iterator(from.track.end()));

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
        WHERE object_id = :objectId
    )sql"));

    for (auto& object: *result)
    {
        trackInfoQuery.bindValue(lit(":objectId"), QnSql::serialized_field(object.objectId));
        trackInfoQuery.exec();
        if (!trackInfoQuery.next())
            continue;
        object.firstAppearanceTimeUsec = trackInfoQuery.value(0).toLongLong();
        object.lastAppearanceTimeUsec = trackInfoQuery.value(1).toLongLong();
    }
}

nx::sql::DBResult EventsStorage::selectTimePeriods(
    nx::sql::QueryContext* queryContext,
    const Filter& filter,
    const TimePeriodsLookupOptions& options,
    QnTimePeriodList* result)
{
    QString eventsFilteredByFreeTextSubQuery;
    const auto sqlQueryFilter =
        prepareSqlFilterExpression(filter, &eventsFilteredByFreeTextSubQuery);
    QString sqlQueryFilterStr;
    if (!sqlQueryFilter.empty())
    {
        sqlQueryFilterStr = lm("WHERE %1").args(
            nx::sql::generateWhereClauseExpression(sqlQueryFilter));
    }

    // TODO: #ak Aggregate in query.

    sql::SqlQuery query(queryContext->connection());
    query.setForwardOnly(true);
    query.prepare(lm(R"sql(
        SELECT timestamp_usec_utc, duration_usec
        FROM %1
        %2
        ORDER BY timestamp_usec_utc ASC
    )sql").args(eventsFilteredByFreeTextSubQuery, sqlQueryFilterStr));
    nx::sql::bindFields(&query, sqlQueryFilter);

    query.exec();
    loadTimePeriods(query, options, result);
    return nx::sql::DBResult::ok;
}

void EventsStorage::loadTimePeriods(
    nx::sql::SqlQuery& query,
    const TimePeriodsLookupOptions& options,
    QnTimePeriodList* result)
{
    using namespace std::chrono;

    constexpr int kUsecPerMs = 1000;

    while (query.next())
    {
        QnTimePeriod timePeriod(
            milliseconds(query.value(lit("timestamp_usec_utc")).toLongLong() / kUsecPerMs),
            milliseconds(query.value(lit("duration_usec")).toLongLong() / kUsecPerMs));
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
    using namespace std::chrono;

    sql::SqlQuery deleteEventsQuery(queryContext->connection());
    deleteEventsQuery.prepare(QString::fromLatin1(R"sql(
        DELETE FROM event
        WHERE device_guid=:deviceId AND timestamp_usec_utc < :timestampUsec
    )sql"));
    deleteEventsQuery.bindValue(
        lit(":deviceId"),
        QnSql::serialized_field(deviceId));
    deleteEventsQuery.bindValue(
        lit(":timestampUsec"),
        (qint64) microseconds(oldestDataToKeepTimestamp).count());

    deleteEventsQuery.exec();
    return nx::sql::DBResult::ok;
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
