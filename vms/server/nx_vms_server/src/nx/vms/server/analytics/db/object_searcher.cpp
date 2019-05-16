#include "object_searcher.h"

#include <nx/fusion/serialization/sql_functions.h>

#include <analytics/db/config.h>

#include "attributes_dao.h"
#include "cursor.h"
#include "serializers.h"

namespace nx::analytics::db {

static const QSize kSearchResolution(
    kTrackSearchResolutionX,
    kTrackSearchResolutionY);

ObjectSearcher::ObjectSearcher(
    const DeviceDao& deviceDao,
    const ObjectTypeDao& objectTypeDao,
    Filter filter)
    :
    m_deviceDao(deviceDao),
    m_objectTypeDao(objectTypeDao),
    m_filter(std::move(filter))
{
    if (m_filter.maxObjectsToSelect == 0 || m_filter.maxObjectsToSelect > kMaxObjectLookupResultSet)
        m_filter.maxObjectsToSelect = kMaxObjectLookupResultSet;
}

std::vector<DetectedObject> ObjectSearcher::lookup(nx::sql::QueryContext* queryContext)
{
    auto selectEventsQuery = queryContext->connection()->createQuery();
    selectEventsQuery->setForwardOnly(true);
    prepareLookupQuery(selectEventsQuery.get());
    selectEventsQuery->exec();

    return loadObjects(selectEventsQuery.get());
}

void ObjectSearcher::prepareCursorQuery(nx::sql::SqlQuery* query)
{
    prepareLookupQuery(query);
}

void ObjectSearcher::loadCurrentRecord(
    nx::sql::SqlQuery* query,
    DetectedObject* object)
{
    *object = loadObject(query);
}

std::unique_ptr<AbstractCursor> ObjectSearcher::createCursor(
    std::unique_ptr<nx::sql::Cursor<DetectedObject>> sqlCursor)
{
    return std::make_unique<Cursor>(std::move(sqlCursor));
}

void ObjectSearcher::addObjectFilterConditions(
    const Filter& filter,
    const DeviceDao& deviceDao,
    const ObjectTypeDao& objectTypeDao,
    const ObjectFields& fieldNames,
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
        addTimePeriodToFilter<std::chrono::milliseconds>(
            filter.timePeriod,
            fieldNames.timeRange,
            sqlFilter);
    }
}

void ObjectSearcher::addBoundingBoxToFilter(
    const QRect& boundingBox,
    nx::sql::Filter* sqlFilter)
{
    auto topLeftXFilter = std::make_unique<nx::sql::SqlFilterFieldLessOrEqual>(
        "box_top_left_x",
        ":boxBottomRightX",
        QnSql::serialized_field(boundingBox.bottomRight().x()));
    sqlFilter->addCondition(std::move(topLeftXFilter));

    auto bottomRightXFilter = std::make_unique<nx::sql::SqlFilterFieldGreaterOrEqual>(
        "box_bottom_right_x",
        ":boxTopLeftX",
        QnSql::serialized_field(boundingBox.topLeft().x()));
    sqlFilter->addCondition(std::move(bottomRightXFilter));

    auto topLeftYFilter = std::make_unique<nx::sql::SqlFilterFieldLessOrEqual>(
        "box_top_left_y",
        ":boxBottomRightY",
        QnSql::serialized_field(boundingBox.bottomRight().y()));
    sqlFilter->addCondition(std::move(topLeftYFilter));

    auto bottomRightYFilter = std::make_unique<nx::sql::SqlFilterFieldGreaterOrEqual>(
        "box_bottom_right_y",
        ":boxTopLeftY",
        QnSql::serialized_field(boundingBox.topLeft().y()));
    sqlFilter->addCondition(std::move(bottomRightYFilter));
}

template<typename FieldType>
void ObjectSearcher::addTimePeriodToFilter(
    const QnTimePeriod& timePeriod,
    const TimeRangeFields& timeRangeFields,
    nx::sql::Filter* sqlFilter)
{
    using namespace std::chrono;

    auto startTimeFilterField = std::make_unique<nx::sql::SqlFilterFieldGreaterOrEqual>(
        timeRangeFields.timeRangeEnd,
        std::string(":start_") + timeRangeFields.timeRangeEnd,
        QnSql::serialized_field(duration_cast<FieldType>(
            timePeriod.startTime()).count()));
    sqlFilter->addCondition(std::move(startTimeFilterField));

    if (timePeriod.durationMs != QnTimePeriod::kInfiniteDuration)
    {
        auto endTimeFilterField = std::make_unique<nx::sql::SqlFilterFieldLess>(
            timeRangeFields.timeRangeStart,
            std::string(":end_") + timeRangeFields.timeRangeStart,
            QnSql::serialized_field(duration_cast<FieldType>(
                timePeriod.endTime()).count()));
        sqlFilter->addCondition(std::move(endTimeFilterField));
    }
}

template void ObjectSearcher::addTimePeriodToFilter<std::chrono::milliseconds>(
    const QnTimePeriod& timePeriod,
    const TimeRangeFields& timeRangeFields,
    nx::sql::Filter* sqlFilter);

template void ObjectSearcher::addTimePeriodToFilter<std::chrono::seconds>(
    const QnTimePeriod& timePeriod,
    const TimeRangeFields& timeRangeFields,
    nx::sql::Filter* sqlFilter);

static constexpr char kBoxFilteredTable[] = "%boxFilteredTable%";
static constexpr char kFromBoxFilteredTable[] = "%fromBoxFilteredTable%";
static constexpr char kJoinBoxFilteredTable[] = "%joinBoxFilteredTable%";
static constexpr char kObjectFilteredByTextExpr[] = "%objectFilteredByText%";
static constexpr char kObjectExpr[] = "%objectExpr%";
static constexpr char kLimitObjectCount[] = "%limitObjectCount%";
static constexpr char kObjectOrderByTrackStart[] = "%objectOrderByTrackStart%";

void ObjectSearcher::prepareLookupQuery(nx::sql::AbstractSqlQuery* query)
{
    QString queryText = R"sql(
        WITH
          %boxFilteredTable%
          filtered_object AS
            (SELECT o.*
            FROM
               %fromBoxFilteredTable%
               %objectFilteredByText%
            WHERE
              %joinBoxFilteredTable%
              %objectExpr%
              1 = 1
            ORDER BY track_start_ms DESC
            %limitObjectCount%)
        SELECT o.id, device_id, object_type_id, guid, track_start_ms, track_end_ms,
            track_detail, attrs.content AS content, best_shot_timestamp_ms, best_shot_rect
        FROM filtered_object o, unique_attributes attrs
        WHERE o.attributes_id = attrs.id
        ORDER BY track_start_ms %objectOrderByTrackStart%
    )sql";

    std::map<QString, QString> queryTextParams({
        {kBoxFilteredTable, ""},
        {kFromBoxFilteredTable, ""},
        {kJoinBoxFilteredTable, ""},
        {kObjectFilteredByTextExpr, "object o"},
        {kObjectExpr, ""},
        {kLimitObjectCount, ""},
        {kObjectOrderByTrackStart, "DESC"}});

    nx::sql::Filter boxSubqueryFilter;
    if (!m_filter.boundingBox.isEmpty())
    {
        QString queryText;
        std::tie(queryText, boxSubqueryFilter) = prepareBoxFilterSubQuery();
        queryTextParams[kBoxFilteredTable] = "box_filtered_ids AS (" + queryText + "),";
        queryTextParams[kFromBoxFilteredTable] = "box_filtered_ids,";
        queryTextParams[kJoinBoxFilteredTable] = "o.id = box_filtered_ids.object_id AND ";
    }

    const auto sqlQueryFilter = prepareFilterObjectSqlExpression();
    auto objectFilterSqlText = sqlQueryFilter.toString();
    if (!objectFilterSqlText.empty())
        queryTextParams[kObjectExpr] = (objectFilterSqlText + " AND ").c_str();

    if (m_filter.maxObjectsToSelect > 0)
    {
        queryTextParams[kLimitObjectCount] =
            lm("LIMIT %1").args(m_filter.maxObjectsToSelect).toQString();
    }

    if (!m_filter.freeText.isEmpty())
    {
        queryTextParams[kObjectFilteredByTextExpr] =
            "(SELECT * FROM object WHERE attributes_id IN "
                "(SELECT docid FROM attributes_text_index WHERE content MATCH :textQuery)) o";
    }

    queryTextParams[kObjectOrderByTrackStart] =
        m_filter.sortOrder == Qt::SortOrder::AscendingOrder ? "ASC" : "DESC";

    for (const auto& [name, value]: queryTextParams)
        queryText.replace(name, value);

    query->prepare(queryText);

    sqlQueryFilter.bindFields(query);
    boxSubqueryFilter.bindFields(query);
    if (!m_filter.freeText.isEmpty())
        query->bindValue(":textQuery", m_filter.freeText + "*");
}

std::tuple<QString /*query text*/, nx::sql::Filter> ObjectSearcher::prepareBoxFilterSubQuery()
{
    using namespace std::chrono;

    nx::sql::Filter sqlFilter;
    addBoundingBoxToFilter(
        translate(m_filter.boundingBox, kSearchResolution),
        &sqlFilter);

    if (!m_filter.timePeriod.isEmpty())
    {
        // Making time period's right boundary inclusive to take into account
        // "to seconds" conversion error.
        auto localTimePeriod = m_filter.timePeriod;
        if (!localTimePeriod.isInfinite())
        {
            localTimePeriod.setEndTime(
                duration_cast<seconds>(localTimePeriod.endTime()) + seconds(1));
        }

        addTimePeriodToFilter<std::chrono::seconds>(
            localTimePeriod,
            {"timestamp_seconds_utc", "timestamp_seconds_utc"},
            &sqlFilter);
    }

    QString queryText = lm(R"sql(
        SELECT distinct os_to_o.object_id
          FROM object_search_to_object os_to_o, object_search os
          WHERE
            %1 AND
            os_to_o.object_search_id = os.id
    )sql").args(sqlFilter.toString());

    return std::make_tuple(std::move(queryText), std::move(sqlFilter));
}

nx::sql::Filter ObjectSearcher::prepareFilterObjectSqlExpression()
{
    nx::sql::Filter sqlFilter;

    addObjectFilterConditions(
        m_filter,
        m_deviceDao,
        m_objectTypeDao,
        {"guid", {"track_start_ms", "track_end_ms"}},
        &sqlFilter);

    return sqlFilter;
}

std::vector<DetectedObject> ObjectSearcher::loadObjects(nx::sql::AbstractSqlQuery* query)
{
    std::vector<DetectedObject> objects;

    while (query->next())
    {
        auto object = loadObject(query);
        objects.push_back(std::move(object));
    }

    return objects;
}

DetectedObject ObjectSearcher::loadObject(nx::sql::AbstractSqlQuery* query)
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

    detectedObject.bestShot.timestampUsec =
        query->value("best_shot_timestamp_ms").toLongLong() * kUsecInMs;
    if (detectedObject.bestShot.initialized())
    {
        detectedObject.bestShot.rect =
            TrackSerializer::deserialized<QRectF>(
                query->value("best_shot_rect").toByteArray());
    }

    detectedObject.track = TrackSerializer::deserialized<decltype(detectedObject.track)>(
        query->value("track_detail").toByteArray());

    filterTrack(&detectedObject.track);
    if (!detectedObject.track.empty())
    {
        for (auto& position: detectedObject.track)
            position.deviceId = detectedObject.deviceId;
    }

    return detectedObject;
}

void ObjectSearcher::filterTrack(std::vector<ObjectPosition>* const track)
{
    using namespace std::chrono;

    auto doesNotSatisfiesFilter =
        [this](const ObjectPosition& position)
        {
            if (!m_filter.timePeriod.isNull())
            {
                const auto timestamp = microseconds(position.timestampUsec);
                if (!m_filter.timePeriod.contains(duration_cast<milliseconds>(timestamp)))
                    return true;
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

    if (m_filter.maxTrackSize > 0 && (int) track->size() > m_filter.maxTrackSize)
        track->erase(track->begin() + m_filter.maxTrackSize, track->end());
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

} // namespace nx::analytics::db
