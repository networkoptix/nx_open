#include "object_searcher.h"

#include <nx/fusion/serialization/sql_functions.h>

#include "attributes_dao.h"
#include "config.h"
#include "serializers.h"

namespace nx::analytics::storage {

ObjectSearcher::ObjectSearcher(
    const DeviceDao& deviceDao,
    const ObjectTypeDao& objectTypeDao)
    :
    m_deviceDao(deviceDao),
    m_objectTypeDao(objectTypeDao)
{
}

std::vector<DetectedObject> ObjectSearcher::lookup(
    nx::sql::QueryContext* queryContext,
    const Filter& filter)
{
    auto selectEventsQuery = queryContext->connection()->createQuery();
    selectEventsQuery->setForwardOnly(true);
    prepareLookupQuery(filter, selectEventsQuery.get());
    selectEventsQuery->exec();

    return loadObjects(selectEventsQuery.get(), filter);
}

void ObjectSearcher::addObjectFilterConditions(
    const Filter& filter,
    const DeviceDao& deviceDao,
    const ObjectTypeDao& objectTypeDao,
    const FieldNames& fieldNames,
    nx::sql::Filter* sqlFilter)
{
    if (!filter.deviceIds.empty())
    {
        auto condition = std::make_unique<nx::sql::SqlFilterFieldAnyOf>(
            "device_id", ":deviceId");
        for (const auto& deviceGuid: filter.deviceIds)
            condition->addValue(deviceDao.deviceIdFromGuid(deviceGuid));
        sqlFilter->addCondition(std::move(condition));
    }

    if (!filter.objectAppearanceId.isNull())
    {
        sqlFilter->addCondition(std::make_unique<nx::sql::SqlFilterFieldEqual>(
            fieldNames.objectId, ":objectAppearanceId",
            QnSql::serialized_field(filter.objectAppearanceId)));
    }

    if (!filter.objectTypeId.empty())
        addObjectTypeIdToFilter(filter.objectTypeId, objectTypeDao, sqlFilter);
}

void ObjectSearcher::prepareLookupQuery(
    const Filter& filter,
    nx::sql::AbstractSqlQuery* query)
{
    const auto sqlQueryFilter = prepareSqlFilterExpression(filter);
    auto sqlQueryFilterStr = sqlQueryFilter.toString();
    if (!sqlQueryFilterStr.empty())
        sqlQueryFilterStr = " AND " + sqlQueryFilterStr;

    // TODO: Full-text search.
    // TODO: Object count limit.

    auto queryText = lm(R"sql(
        SELECT device_id, object_type_id, guid, track_start_timestamp_ms, track_detail, content
        FROM object o, unique_attributes attrs
        WHERE o.attributes_id = attrs.id %1
        ORDER BY track_start_timestamp_ms %2
    )sql").args(
        sqlQueryFilterStr,
        filter.sortOrder == Qt::SortOrder::AscendingOrder ? "ASC" : "DESC");

    query->prepare(queryText);
    sqlQueryFilter.bindFields(query);
}

nx::sql::Filter ObjectSearcher::prepareSqlFilterExpression(const Filter& filter)
{
    nx::sql::Filter sqlFilter;

    addObjectFilterConditions(
        filter,
        m_deviceDao,
        m_objectTypeDao,
        {"guid"},
        &sqlFilter);

    // TODO

    return sqlFilter;
}

std::vector<DetectedObject> ObjectSearcher::loadObjects(
    nx::sql::AbstractSqlQuery* query,
    const Filter& filter)
{
    std::vector<DetectedObject> objects;

    while (query->next())
    {
        auto object = loadObject(query, filter);
        objects.push_back(std::move(object));
    }

    return objects;
}

DetectedObject ObjectSearcher::loadObject(
    nx::sql::AbstractSqlQuery* query,
    const Filter& filter)
{
    DetectedObject detectedObject;

    detectedObject.deviceId = m_deviceDao.deviceGuidFromId(
        query->value("device_id").toLongLong());
    detectedObject.objectTypeId = m_objectTypeDao.objectTypeFromId(
        query->value("object_type_id").toLongLong());
    detectedObject.objectAppearanceId = QnSql::deserialized_field<QnUuid>(query->value("guid"));
    detectedObject.attributes = AttributesDao::deserialize(query->value("content").toString());

    detectedObject.track = TrackSerializer::deserialized(
        query->value("track_detail").toByteArray());
    if (!detectedObject.track.empty())
    {
        detectedObject.firstAppearanceTimeUsec = detectedObject.track.front().timestampUsec;
        detectedObject.lastAppearanceTimeUsec = detectedObject.track.back().timestampUsec;
        for (auto& position: detectedObject.track)
            position.deviceId = detectedObject.deviceId;
    }

    return detectedObject;
}

void ObjectSearcher::addObjectTypeIdToFilter(
    const std::vector<QString>& objectTypes,
    const ObjectTypeDao& objectTypeDao,
    nx::sql::Filter* sqlFilter)
{
    auto condition = std::make_unique<nx::sql::SqlFilterFieldAnyOf>(
        "object_type_id", ":objectTypeId");
    for (const auto& objectType: objectTypes)
        condition->addValue(objectTypeDao.objectTypeIdFromName(objectType));
    sqlFilter->addCondition(std::move(condition));
}

} // namespace nx::analytics::storage
