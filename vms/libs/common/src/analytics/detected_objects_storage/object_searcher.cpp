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

    if (!filter.timePeriod.isNull())
    {
        addTimePeriodToFilter(
            filter.timePeriod,
            sqlFilter,
            fieldNames.timeRangeStart,
            fieldNames.timeRangeEnd);
    }
}

void ObjectSearcher::addTimePeriodToFilter(
    const QnTimePeriod& timePeriod,
    nx::sql::Filter* sqlFilter,
    const char* leftBoundaryFieldName,
    const char* rightBoundaryFieldName,
    std::optional<std::chrono::milliseconds> maxRecordedTimestamp)
{
    using namespace std::chrono;

    auto startTimeFilterField = std::make_unique<nx::sql::SqlFilterFieldGreaterOrEqual>(
        rightBoundaryFieldName,
        ":startTimeMs",
        QnSql::serialized_field(duration_cast<milliseconds>(
            timePeriod.startTime()).count()));
    sqlFilter->addCondition(std::move(startTimeFilterField));

    if (timePeriod.durationMs != QnTimePeriod::kInfiniteDuration &&
        (!maxRecordedTimestamp ||
            timePeriod.startTime() + timePeriod.duration() <= *maxRecordedTimestamp))
    {
        auto endTimeFilterField = std::make_unique<nx::sql::SqlFilterFieldLess>(
            leftBoundaryFieldName,
            ":endTimeMs",
            QnSql::serialized_field(duration_cast<milliseconds>(
                timePeriod.endTime()).count()));
        sqlFilter->addCondition(std::move(endTimeFilterField));
    }
}

void ObjectSearcher::prepareLookupQuery(
    const Filter& filter,
    nx::sql::AbstractSqlQuery* query)
{
    const auto sqlQueryFilter = prepareSqlFilterExpression(filter);
    auto sqlQueryFilterStr = sqlQueryFilter.toString();
    if (!sqlQueryFilterStr.empty())
        sqlQueryFilterStr += sqlQueryFilterStr + " AND ";

    std::string limitStr;
    if (filter.maxObjectsToSelect > 0)
        limitStr = lm("LIMIT %1").args(filter.maxObjectsToSelect).toStdString();

    QString queryTemplate;
    if (filter.freeText.isEmpty())
    {
        queryTemplate = R"sql(
            SELECT device_id, object_type_id, guid, track_start_ms, track_end_ms, track_detail, content
            FROM object o, unique_attributes attrs
            WHERE %1
                o.attributes_id = attrs.id
            ORDER BY track_start_ms %2
            %3
        )sql";
    }
    else
    {
        queryTemplate = R"sql(
            SELECT device_id, object_type_id, guid, track_start_ms, track_end_ms, track_detail,
                attrs.content AS content
            FROM object o, unique_attributes attrs, attributes_text_index fts
            WHERE %1
                fts.content MATCH :textQuery AND fts.docid = o.attributes_id AND
                o.attributes_id = attrs.id
            ORDER BY track_start_ms %2
            %3
        )sql";
    }

    auto queryText = lm(queryTemplate).args(
        sqlQueryFilterStr,
        filter.sortOrder == Qt::SortOrder::AscendingOrder ? "ASC" : "DESC",
        limitStr);

    query->prepare(queryText);

    sqlQueryFilter.bindFields(query);
    if (!filter.freeText.isEmpty())
        query->bindValue(":textQuery", filter.freeText + "*");
}

nx::sql::Filter ObjectSearcher::prepareSqlFilterExpression(const Filter& filter)
{
    nx::sql::Filter sqlFilter;

    addObjectFilterConditions(
        filter,
        m_deviceDao,
        m_objectTypeDao,
        {"guid", "track_start_ms", "track_end_ms"},
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
    detectedObject.firstAppearanceTimeUsec =
        query->value("track_start_ms").toLongLong() * kUsecInMs;
    detectedObject.lastAppearanceTimeUsec =
        query->value("track_end_ms").toLongLong() * kUsecInMs;

    detectedObject.track = TrackSerializer::deserialized(
        query->value("track_detail").toByteArray());
    filterTrack(filter, &detectedObject.track);
    if (!detectedObject.track.empty())
    {
        for (auto& position: detectedObject.track)
            position.deviceId = detectedObject.deviceId;
    }

    return detectedObject;
}

void ObjectSearcher::filterTrack(
    const Filter& filter,
    std::vector<ObjectPosition>* const track)
{
    using namespace std::chrono;

    auto doesNotSatisfiesFilter =
        [&filter](const ObjectPosition& position)
        {
            if (!filter.timePeriod.isNull())
            {
                if (microseconds(position.timestampUsec) >= filter.timePeriod.endTime() ||
                    microseconds(position.timestampUsec + position.durationUsec) < filter.timePeriod.startTime())
                {
                    return true;
                }
            }

            // TODO: Filter box

            return false;
        };

    track->erase(
        std::remove_if(track->begin(), track->end(), doesNotSatisfiesFilter),
        track->end());

    std::sort(
        track->begin(), track->end(),
        [](const auto& left, const auto& right) { return left.timestampUsec < right.timestampUsec; });

    if (filter.maxTrackSize > 0 && (int) track->size() > filter.maxTrackSize)
        track->erase(track->begin() + filter.maxTrackSize, track->end());
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
