#include "analytics_events_storage.h"

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/sql_functions.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace analytics {
namespace storage {

using namespace nx::utils::db;

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

    NX_VERBOSE(this, lm("Saving packet %1").arg(*packet));

    m_dbController.queryExecutor().executeUpdate(
        std::bind(&EventsStorage::savePacket, this, _1, std::move(packet)),
        [this, completionHandler = std::move(completionHandler)](
            QueryContext*, DBResult resultCode)
        {
            completionHandler(dbResultToResultCode(resultCode));
        });
}

void EventsStorage::createLookupCursor(
    Filter /*filter*/,
    CreateCursorCompletionHandler completionHandler)
{
    completionHandler(ResultCode::error, nullptr);
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
            QueryContext*, DBResult resultCode)
        {
            completionHandler(
                dbResultToResultCode(resultCode),
                std::move(*result));
        });
}

void EventsStorage::markDataAsDeprecated(
    QnUuid /*deviceId*/,
    qint64 /*oldestNeededDataTimestamp*/)
{
    // TODO
}

DBResult EventsStorage::savePacket(
    QueryContext* queryContext,
    common::metadata::ConstDetectionMetadataPacketPtr packet)
{
    for (const auto& detectedObject: packet->objects)
    {
        const auto eventId = insertEvent(queryContext, *packet, detectedObject);
        insertEventAttributes(queryContext, eventId, detectedObject.labels);
    }

    return DBResult::ok;
}

std::int64_t EventsStorage::insertEvent(
    QueryContext* queryContext,
    const common::metadata::DetectionMetadataPacket& packet,
    const common::metadata::DetectedObject& detectedObject)
{
    SqlQuery insertEventQuery(*queryContext->connection());
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

    insertEventQuery.bindValue(lit(":boxTopLeftX"), (int) detectedObject.boundingBox.topLeft().x());
    insertEventQuery.bindValue(lit(":boxTopLeftY"), (int)detectedObject.boundingBox.topLeft().y());
    insertEventQuery.bindValue(lit(":boxBottomRightX"), (int)detectedObject.boundingBox.bottomRight().x());
    insertEventQuery.bindValue(lit(":boxBottomRightY"), (int)detectedObject.boundingBox.bottomRight().y());

    insertEventQuery.exec();
    return insertEventQuery.impl().lastInsertId().toLongLong();
}

void EventsStorage::insertEventAttributes(
    QueryContext* queryContext,
    std::int64_t eventId,
    const std::vector<common::metadata::Attribute>& eventAttributes)
{
    SqlQuery insertEventAttributesQuery(*queryContext->connection());
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

nx::utils::db::DBResult EventsStorage::selectObjects(
    nx::utils::db::QueryContext* queryContext,
    const Filter& filter,
    std::vector<DetectedObject>* result)
{
    const auto sqlQueryFilter = prepareSqlFilterExpression(filter);
    QString sqlQueryFilterStr;
    if (!sqlQueryFilter.empty())
    {
        sqlQueryFilterStr = lm("WHERE %1").args(
            nx::utils::db::generateWhereClauseExpression(sqlQueryFilter));
    }

    SqlQuery selectEventsQuery(*queryContext->connection());
    selectEventsQuery.setForwardOnly(true);
    selectEventsQuery.prepare(lm(R"sql(
        SELECT timestamp_usec_utc, duration_usec, device_guid,
            object_type_id, object_id, attributes,
            box_top_left_x, box_top_left_y, box_bottom_right_x, box_bottom_right_y
        FROM event
        %1
        ORDER BY timestamp_usec_utc %2, object_id %2;
    )sql").args(
        sqlQueryFilterStr,
        filter.sortOrder == Qt::SortOrder::AscendingOrder ? "ASC" : "DESC").toQString());
    nx::utils::db::bindFields(&selectEventsQuery, sqlQueryFilter);
    selectEventsQuery.exec();

    loadObjects(selectEventsQuery, filter, result);

    return nx::utils::db::DBResult::ok;
}

nx::utils::db::InnerJoinFilterFields EventsStorage::prepareSqlFilterExpression(
    const Filter& filter)
{
    nx::utils::db::InnerJoinFilterFields sqlFilter;
    if (!filter.deviceId.isNull())
    {
        nx::utils::db::SqlFilterFieldEqual filterField(
            "device_guid", ":deviceId", QnSql::serialized_field(filter.deviceId));
        sqlFilter.push_back(std::move(filterField));
    }

    if (!filter.timePeriod.isNull())
    {
        nx::utils::db::SqlFilterFieldGreaterOrEqual startTimeFilterField(
            "timestamp_usec_utc",
            ":startTimeUsec",
            QnSql::serialized_field(filter.timePeriod.startTimeMs * kUsecInMs));
        sqlFilter.push_back(std::move(startTimeFilterField));

        const auto endTimeMs = filter.timePeriod.startTimeMs + filter.timePeriod.durationMs;
        nx::utils::db::SqlFilterFieldLess endTimeFilterField(
            "timestamp_usec_utc",
            ":endTimeUsec",
            QnSql::serialized_field(endTimeMs * kUsecInMs));
        sqlFilter.push_back(std::move(endTimeFilterField));
    }

    return sqlFilter;
}

void EventsStorage::loadObjects(
    SqlQuery& selectEventsQuery,
    const Filter& filter,
    std::vector<DetectedObject>* result)
{
    std::map<QnUuid, std::vector<DetectedObject>::size_type> objectIdToPosition;

    while (selectEventsQuery.next())
    {
        DetectedObject detectedObject;
        loadObject(selectEventsQuery, &detectedObject);

        auto iterAndIsInsertedFlag =
            objectIdToPosition.emplace(detectedObject.objectId, result->size());
        if (iterAndIsInsertedFlag.second)
        {
            if (filter.maxObjectsToSelect > 0 && (int) result->size() >= filter.maxObjectsToSelect)
                break;
            result->push_back(std::move(detectedObject));
        }
        else
        {
            DetectedObject& existingObject = result->at(iterAndIsInsertedFlag.first->second);
            mergeObjects(std::move(detectedObject), &existingObject);
        }
    }
}

void EventsStorage::loadObject(
    SqlQuery& selectEventsQuery,
    DetectedObject* object)
{
    object->objectId = QnSql::deserialized_field<QnUuid>(
        selectEventsQuery.value(lit("object_id")));
    object->objectTypeId = QnSql::deserialized_field<QnUuid>(
        selectEventsQuery.value(lit("object_type_id")));
    QJson::deserialize(
        selectEventsQuery.value(lit("attributes")).toString(),
        &object->attributes);

    object->track.push_back(ObjectPosition());
    ObjectPosition& objectPosition = object->track.back();

    objectPosition.deviceId = QnSql::deserialized_field<QnUuid>(
        selectEventsQuery.value(lit("device_guid")));
    objectPosition.timestampUsec = selectEventsQuery.value(lit("timestamp_usec_utc")).toLongLong();
    objectPosition.durationUsec = selectEventsQuery.value(lit("duration_usec")).toLongLong();

    objectPosition.boundingBox.setTopLeft(QPointF(
        selectEventsQuery.value(lit("box_top_left_x")).toInt(),
        selectEventsQuery.value(lit("box_top_left_y")).toInt()));
    objectPosition.boundingBox.setBottomRight(QPointF(
        selectEventsQuery.value(lit("box_bottom_right_x")).toInt(),
        selectEventsQuery.value(lit("box_bottom_right_y")).toInt()));
}

void EventsStorage::mergeObjects(DetectedObject from, DetectedObject* to)
{
    to->track.insert(
        to->track.end(),
        std::make_move_iterator(from.track.begin()),
        std::make_move_iterator(from.track.end()));

    // TODO: #ak moving attributes.
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
