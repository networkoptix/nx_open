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

    auto result = std::make_shared<std::vector<common::metadata::DetectionMetadataPacket>>();
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
    insertEventQuery.prepare(R"sql(
        INSERT INTO event(timestamp_usec_utc, duration_usec,
            device_guid, object_type_id, object_id, attributes,
            box_top_left_x, box_top_left_y, box_bottom_right_x, box_bottom_right_y)
        VALUES(:timestampUsec, :durationUsec, :deviceId, :objectTypeId, :objectId, :attributes,
            :boxTopLeftX, :boxTopLeftY, :boxBottomRightX, :boxBottomRightY)
    )sql");
    insertEventQuery.bindValue(":timestampUsec", packet.timestampUsec);
    insertEventQuery.bindValue(":durationUsec", packet.durationUsec);
    insertEventQuery.bindValue(":deviceId", QnSql::serialized_field(packet.deviceId));
    insertEventQuery.bindValue(
        ":objectTypeId",
        QnSql::serialized_field(detectedObject.objectTypeId));
    insertEventQuery.bindValue(
        ":objectId",
        QnSql::serialized_field(detectedObject.objectId));
    insertEventQuery.bindValue(":attributes", QJson::serialized(detectedObject.labels));

    insertEventQuery.bindValue(":boxTopLeftX", (int) detectedObject.boundingBox.topLeft().x());
    insertEventQuery.bindValue(":boxTopLeftY", (int)detectedObject.boundingBox.topLeft().y());
    insertEventQuery.bindValue(":boxBottomRightX", (int)detectedObject.boundingBox.bottomRight().x());
    insertEventQuery.bindValue(":boxBottomRightY", (int)detectedObject.boundingBox.bottomRight().y());

    insertEventQuery.exec();
    return insertEventQuery.impl().lastInsertId().toLongLong();
}

void EventsStorage::insertEventAttributes(
    QueryContext* queryContext,
    std::int64_t eventId,
    const std::vector<common::metadata::Attribute>& eventAttributes)
{
    SqlQuery insertEventAttributesQuery(*queryContext->connection());
    insertEventAttributesQuery.prepare(R"sql(
        INSERT INTO event_properties(docid, content)
        VALUES(:eventId, :content)
    )sql");
    insertEventAttributesQuery.bindValue(":eventId", static_cast<qint64>(eventId));
    insertEventAttributesQuery.bindValue(
        ":content",
        containerString(eventAttributes, "; ", "", "", ""));
    insertEventAttributesQuery.exec();
}

nx::utils::db::DBResult EventsStorage::selectEvents(
    nx::utils::db::QueryContext* queryContext,
    const Filter& /*filter*/,
    std::vector<common::metadata::DetectionMetadataPacket>* result)
{
    SqlQuery selectEventsQuery(*queryContext->connection());
    selectEventsQuery.setForwardOnly(true);
    selectEventsQuery.prepare(R"sql(
        SELECT timestamp_usec_utc, duration_usec, device_guid,
            object_type_id, object_id, attributes,
            box_top_left_x, box_top_left_y, box_bottom_right_x, box_bottom_right_y
        FROM event
        ORDER BY timestamp_usec_utc, duration_usec, device_guid;
    )sql");
    selectEventsQuery.exec();

    loadPackets(selectEventsQuery, result);
    return nx::utils::db::DBResult::ok;
}

void EventsStorage::loadPackets(
    SqlQuery& selectEventsQuery,
    std::vector<common::metadata::DetectionMetadataPacket>* result)
{
    while (selectEventsQuery.next())
    {
        result->push_back(common::metadata::DetectionMetadataPacket());
        loadPacket(selectEventsQuery, &result->back());
    }
}

void EventsStorage::loadPacket(
    SqlQuery& selectEventsQuery,
    common::metadata::DetectionMetadataPacket* packet)
{
    packet->deviceId = QnSql::deserialized_field<QnUuid>(
        selectEventsQuery.value("device_guid"));
    packet->timestampUsec = selectEventsQuery.value("timestamp_usec_utc").toLongLong();
    packet->durationUsec = selectEventsQuery.value("duration_usec").toLongLong();

    packet->objects.push_back(common::metadata::DetectedObject());
    common::metadata::DetectedObject& detectedObject = packet->objects.back();
    detectedObject.objectId = QnSql::deserialized_field<QnUuid>(
        selectEventsQuery.value("object_id"));
    detectedObject.objectTypeId = QnSql::deserialized_field<QnUuid>(
        selectEventsQuery.value("object_type_id"));
    QJson::deserialize(
        selectEventsQuery.value("attributes").toString(),
        &detectedObject.labels);

    detectedObject.boundingBox.setTopLeft(QPointF(
        selectEventsQuery.value("box_top_left_x").toInt(),
        selectEventsQuery.value("box_top_left_y").toInt()));
    detectedObject.boundingBox.setBottomRight(QPointF(
        selectEventsQuery.value("box_bottom_right_x").toInt(),
        selectEventsQuery.value("box_bottom_right_y").toInt()));
}

//-------------------------------------------------------------------------------------------------

EventsStorageFuncionFactory::EventsStorageFuncionFactory():
    base_type(std::bind(&EventsStorageFuncionFactory::defaultFactoryFunction, this,
        std::placeholders::_1))
{
}

EventsStorageFuncionFactory& EventsStorageFuncionFactory::instance()
{
    static EventsStorageFuncionFactory staticInstance;
    return staticInstance;
}

std::unique_ptr<AbstractEventsStorage> EventsStorageFuncionFactory::defaultFactoryFunction(
    const Settings& settings)
{
    return std::make_unique<EventsStorage>(settings);
}

} // namespace storage
} // namespace analytics
} // namespace mediaserver
} // namespace nx
