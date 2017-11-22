#include "analytics_events_storage.h"

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/sql_functions.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace mediaserver {
namespace analytics {
namespace storage {

using namespace nx::utils::db;

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
    CreateCursorCompletionHandler /*completionHandler*/)
{
    // TODO
}

void EventsStorage::lookup(
    Filter filter,
    LookupCompletionHandler completionHandler)
{
    using namespace std::placeholders;

    auto result = std::make_shared<std::vector<DetectionEvent>>();
    m_dbController.queryExecutor().executeSelect(
        std::bind(&EventsStorage::selectEvents, this, _1, std::move(filter), result.get()),
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
    insertEventQuery.prepare(lit(R"sql(
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
    insertEventAttributesQuery.prepare(lit(R"sql(
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

nx::utils::db::DBResult EventsStorage::selectEvents(
    nx::utils::db::QueryContext* queryContext,
    const Filter& /*filter*/,
    std::vector<DetectionEvent>* result)
{
    SqlQuery selectEventsQuery(*queryContext->connection());
    selectEventsQuery.setForwardOnly(true);
    selectEventsQuery.prepare(lit(R"sql(
        SELECT timestamp_usec_utc, duration_usec, device_guid,
            object_type_id, object_id, attributes,
            box_top_left_x, box_top_left_y, box_bottom_right_x, box_bottom_right_y
        FROM event
        ORDER BY timestamp_usec_utc, duration_usec, device_guid;
    )sql"));
    selectEventsQuery.exec();

    loadEvents(selectEventsQuery, result);
    return nx::utils::db::DBResult::ok;
}

void EventsStorage::loadEvents(
    SqlQuery& selectEventsQuery,
    std::vector<DetectionEvent>* result)
{
    while (selectEventsQuery.next())
    {
        result->push_back(DetectionEvent());
        loadEvent(selectEventsQuery, &result->back());
    }
}

void EventsStorage::loadEvent(
    SqlQuery& selectEventsQuery,
    DetectionEvent* event)
{
    event->deviceId = QnSql::deserialized_field<QnUuid>(
        selectEventsQuery.value(lit("device_guid")));
    event->timestampUsec = selectEventsQuery.value(lit("timestamp_usec_utc")).toLongLong();
    event->durationUsec = selectEventsQuery.value(lit("duration_usec")).toLongLong();

    common::metadata::DetectedObject& detectedObject = event->object;
    detectedObject.objectId = QnSql::deserialized_field<QnUuid>(
        selectEventsQuery.value(lit("object_id")));
    detectedObject.objectTypeId = QnSql::deserialized_field<QnUuid>(
        selectEventsQuery.value(lit("object_type_id")));
    QJson::deserialize(
        selectEventsQuery.value(lit("attributes")).toString(),
        &detectedObject.labels);

    detectedObject.boundingBox.setTopLeft(QPointF(
        selectEventsQuery.value(lit("box_top_left_x")).toInt(),
        selectEventsQuery.value(lit("box_top_left_y")).toInt()));
    detectedObject.boundingBox.setBottomRight(QPointF(
        selectEventsQuery.value(lit("box_bottom_right_x")).toInt(),
        selectEventsQuery.value(lit("box_bottom_right_y")).toInt()));
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
} // namespace mediaserver
} // namespace nx
