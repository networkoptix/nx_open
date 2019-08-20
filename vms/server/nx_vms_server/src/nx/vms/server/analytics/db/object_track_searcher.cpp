#include "object_track_searcher.h"

#include <nx/fusion/serialization/sql_functions.h>
#include <nx/utils/std/algorithm.h>

#include <analytics/db/config.h>

#include "analytics_archive_directory.h"
#include "attributes_dao.h"
#include "cursor.h"
#include "serializers.h"

namespace nx::analytics::db {

static const QSize kSearchResolution(
    kTrackSearchResolutionX,
    kTrackSearchResolutionY);

static constexpr int kMaxObjectLookupIterations = kMaxObjectLookupResultSet;

ObjectTrackSearcher::ObjectTrackSearcher(
    const DeviceDao& deviceDao,
    const ObjectTypeDao& objectTypeDao,
    AttributesDao* attributesDao,
    AnalyticsArchiveDirectory* analyticsArchive,
    Filter filter)
    :
    m_deviceDao(deviceDao),
    m_objectTypeDao(objectTypeDao),
    m_attributesDao(attributesDao),
    m_analyticsArchive(analyticsArchive),
    m_filter(std::move(filter))
{
    if (m_filter.maxObjectTracksToSelect == 0 || m_filter.maxObjectTracksToSelect > kMaxObjectLookupResultSet)
        m_filter.maxObjectTracksToSelect = kMaxObjectLookupResultSet;
}

std::vector<ObjectTrack> ObjectTrackSearcher::lookup(nx::sql::QueryContext* queryContext)
{
    if (kLookupObjectsInAnalyticsArchive)
    {
        if (!m_filter.objectTrackId.isNull())
        {
            auto track = fetchTrackById(queryContext, m_filter.objectTrackId);
            if (!track)
                return {};
            return {std::move(*track)};
        }
        else
        {
            return lookupTracksUsingArchive(queryContext);
        }
    }
    else
    {
        auto selectEventsQuery = queryContext->connection()->createQuery();
        selectEventsQuery->setForwardOnly(true);
        prepareLookupQuery(selectEventsQuery.get());
        selectEventsQuery->exec();

        return loadTracks(selectEventsQuery.get());
    }
}

void ObjectTrackSearcher::prepareCursorQuery(nx::sql::SqlQuery* query)
{
    if (kLookupObjectsInAnalyticsArchive)
        prepareCursorQueryImpl(query);
    else
        prepareLookupQuery(query);
}

void ObjectTrackSearcher::loadCurrentRecord(
    nx::sql::SqlQuery* query,
    ObjectTrack* object)
{
    *object = *loadTrack(query, [](auto&&...) { return true; });
}

void ObjectTrackSearcher::addTrackFilterConditions(
    const Filter& filter,
    const DeviceDao& deviceDao,
    const ObjectTypeDao& objectTypeDao,
    const ObjectFields& fieldNames,
    nx::sql::Filter* sqlFilter)
{
    if (!filter.deviceIds.empty())
        addDeviceFilterCondition(filter.deviceIds, deviceDao, sqlFilter);

    if (!filter.objectTrackId.isNull())
    {
        sqlFilter->addCondition(std::make_unique<nx::sql::SqlFilterFieldEqual>(
            fieldNames.trackId, ":trackId",
            QnSql::serialized_field(filter.objectTrackId)));
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

void ObjectTrackSearcher::addDeviceFilterCondition(
    const std::vector<QnUuid>& deviceIds,
    const DeviceDao& deviceDao,
    nx::sql::Filter* sqlFilter)
{
    auto condition = std::make_unique<nx::sql::SqlFilterFieldAnyOf>(
        "device_id", ":deviceId");
    for (const auto& deviceGuid: deviceIds)
        condition->addValue(deviceDao.deviceIdFromGuid(deviceGuid));
    sqlFilter->addCondition(std::move(condition));
}

void ObjectTrackSearcher::addBoundingBoxToFilter(
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
void ObjectTrackSearcher::addTimePeriodToFilter(
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

template void ObjectTrackSearcher::addTimePeriodToFilter<std::chrono::milliseconds>(
    const QnTimePeriod& timePeriod,
    const TimeRangeFields& timeRangeFields,
    nx::sql::Filter* sqlFilter);

template void ObjectTrackSearcher::addTimePeriodToFilter<std::chrono::seconds>(
    const QnTimePeriod& timePeriod,
    const TimeRangeFields& timeRangeFields,
    nx::sql::Filter* sqlFilter);

bool ObjectTrackSearcher::satisfiesFilter(
    const Filter& filter, const ObjectTrack& track)
{
    using namespace std::chrono;

    if (!filter.objectTrackId.isNull() &&
        track.id != filter.objectTrackId)
    {
        return false;
    }

    // Matching every device if device list is empty.
    if (!filter.deviceIds.empty() &&
        !nx::utils::contains(filter.deviceIds, track.deviceId))
    {
        return false;
    }

    if (!(microseconds(track.lastAppearanceTimeUs) >= filter.timePeriod.startTime() &&
          (filter.timePeriod.isInfinite() ||
              microseconds(track.firstAppearanceTimeUs) < filter.timePeriod.endTime())))
    {
        return false;
    }

    if (!filter.objectTypeId.empty() &&
        std::find(
            filter.objectTypeId.begin(), filter.objectTypeId.end(),
            track.objectTypeId) == filter.objectTypeId.end())
    {
        return false;
    }

    if (!filter.freeText.isEmpty() &&
        !matchAttributes(track.attributes, filter.freeText))
    {
        return false;
    }

    // Checking the track.
    if (filter.boundingBox)
    {
        bool instersects = false;
        for (const auto& position: track.objectPositionSequence)
        {
            if (rectsIntersectToSearchPrecision(*filter.boundingBox, position.boundingBox))
            {
                instersects = true;
                break;
            }
        }

        if (!instersects)
            return false;
    }

    return true;
}

bool ObjectTrackSearcher::matchAttributes(
    const std::vector<nx::common::metadata::Attribute>& attributes,
    const QString& filter)
{
    const auto filterWords = filter.split(L' ');
    // Attributes have to contain all words.
    for (const auto& filterWord: filterWords)
    {
        bool found = false;
        for (const auto& attribute : attributes)
        {
            if (attribute.name.indexOf(filterWord, 0, Qt::CaseInsensitive) != -1 ||
                attribute.value.indexOf(filterWord, 0, Qt::CaseInsensitive) != -1)
            {
                found = true;
                break;
            }
        }

        if (!found)
            return false;
    }

    return true;
}

static constexpr char kBoxFilteredTable[] = "%boxFilteredTable%";
static constexpr char kFromBoxFilteredTable[] = "%fromBoxFilteredTable%";
static constexpr char kJoinBoxFilteredTable[] = "%joinBoxFilteredTable%";
static constexpr char kObjectFilteredByTextExpr[] = "%objectFilteredByText%";
static constexpr char kObjectExpr[] = "%objectExpr%";
static constexpr char kLimitObjectCount[] = "%limitObjectCount%";
static constexpr char kObjectOrderByTrackStart[] = "%objectOrderByTrackStart%";

std::optional<ObjectTrack> ObjectTrackSearcher::fetchTrackById(
    nx::sql::QueryContext* queryContext,
    const QnUuid& trackGuid)
{
    auto query = queryContext->connection()->createQuery();
    query->setForwardOnly(true);
    query->prepare(R"sql(
        SELECT device_id, object_type_id, guid, track_start_ms, track_end_ms, track_detail,
            ua.content AS content, best_shot_timestamp_ms, best_shot_rect
        FROM track t, unique_attributes ua
        WHERE t.attributes_id=ua.id AND guid=?
    )sql");
    query->addBindValue(QnSql::serialized_field(trackGuid));

    query->exec();

    auto tracks = loadTracks(query.get(), 1);
    if (tracks.empty())
        return std::nullopt;

    return std::move(tracks.front());
}

std::vector<ObjectTrack> ObjectTrackSearcher::lookupTracksUsingArchive(
    nx::sql::QueryContext* queryContext)
{
    auto archiveFilter =
        AnalyticsArchiveDirectory::prepareArchiveFilter(
            queryContext, m_filter, m_objectTypeDao, m_attributesDao);
    // NOTE: We are always searching for newest objects.
    // The sortOrder is applied to the lookup result.

    if (!archiveFilter)
    {
        NX_DEBUG(this, "Object lookup canceled. The filter is %1", m_filter);
        return std::vector<ObjectTrack>(); //< No records with such text.
    }

    archiveFilter->sortOrder = Qt::DescendingOrder;

    std::set<std::int64_t> analyzedObjectGroups;

    std::vector<ObjectTrack> result;
    for (int i = 0; i < kMaxObjectLookupIterations; ++i)
    {
        auto matchResult = m_analyticsArchive->matchObjects(
            m_filter.deviceIds,
            *archiveFilter);
        const int fetchedObjectGroupsCount = matchResult.trackGroups.size();

        matchResult.trackGroups.erase(
            std::remove_if(
                matchResult.trackGroups.begin(), matchResult.trackGroups.end(),
                [&analyzedObjectGroups](auto id) { return analyzedObjectGroups.count(id) > 0; }),
            matchResult.trackGroups.end());
        std::copy(
            matchResult.trackGroups.begin(), matchResult.trackGroups.end(),
            std::inserter(analyzedObjectGroups, analyzedObjectGroups.end()));

        fetchTracksFromDb(queryContext, matchResult.trackGroups, &result);

        if ((int) result.size() >= m_filter.maxObjectTracksToSelect ||
            fetchedObjectGroupsCount < archiveFilter->limit)
        {
            if ((int) result.size() > m_filter.maxObjectTracksToSelect)
                result.erase(result.begin() + m_filter.maxObjectTracksToSelect, result.end());
            return result;
        }

        // Repeating match.
        // NOTE: Both time period boundaries are inclusive.
        if (m_filter.sortOrder == Qt::AscendingOrder)
            archiveFilter->startTime = matchResult.timePeriod.endTime() + std::chrono::milliseconds(1);
        else
            archiveFilter->endTime = matchResult.timePeriod.startTime();
    }

    NX_WARNING(this, "Failed to select all required objects in %1 iterations. Filter %2",
        kMaxObjectLookupIterations, m_filter);

    return result;
}

void ObjectTrackSearcher::fetchTracksFromDb(
    nx::sql::QueryContext* queryContext,
    const std::vector<std::int64_t>& objectTrackGroups,
    std::vector<ObjectTrack>* result)
{
    nx::sql::SqlFilterFieldAnyOf objectGroupFilter(
        "tg.group_id",
        ":groupId",
        std::vector<long long>(objectTrackGroups.begin(), objectTrackGroups.end()));

    std::string limitExpr;
    if (m_filter.maxObjectTracksToSelect > 0)
        limitExpr = "LIMIT " + std::to_string(m_filter.maxObjectTracksToSelect);

    auto query = queryContext->connection()->createQuery();
    query->setForwardOnly(true);
    query->prepare(lm(R"sql(
        SELECT device_id, object_type_id, guid, track_start_ms, track_end_ms, track_detail,
            ua.content AS content, best_shot_timestamp_ms, best_shot_rect
        FROM track t, unique_attributes ua, track_group tg
        WHERE t.attributes_id=ua.id AND t.id=tg.track_id AND %1
        ORDER BY track_start_ms DESC
    )sql").args(objectGroupFilter.toString()));
    objectGroupFilter.bindFields(&query->impl());

    query->exec();

    auto tracks = loadTracks(query.get(), m_filter.maxObjectTracksToSelect);
    if (m_filter.sortOrder == Qt::AscendingOrder)
        std::reverse(tracks.begin(), tracks.end());

    std::move(tracks.begin(), tracks.end(), std::back_inserter(*result));
}

void ObjectTrackSearcher::prepareCursorQueryImpl(nx::sql::AbstractSqlQuery* query)
{
    nx::sql::Filter sqlFilter;
    if (!m_filter.deviceIds.empty())
        addDeviceFilterCondition(m_filter.deviceIds, m_deviceDao, &sqlFilter);

    if (!m_filter.timePeriod.isNull())
    {
        addTimePeriodToFilter<std::chrono::milliseconds>(
            m_filter.timePeriod,
            {"track_start_ms", "track_end_ms"},
            &sqlFilter);
    }

    auto filterExpr = sqlFilter.toString();
    if (!filterExpr.empty())
        filterExpr = " AND " + filterExpr;

    std::string limitExpr;
    if (m_filter.maxObjectTracksToSelect > 0)
        limitExpr = "LIMIT " + std::to_string(m_filter.maxObjectTracksToSelect);

    query->setForwardOnly(true);

    query->prepare(lm(R"sql(
        SELECT device_id, object_type_id, guid, track_start_ms, track_end_ms, track_detail,
            ua.content AS content, best_shot_timestamp_ms, best_shot_rect
        FROM track t, unique_attributes ua
        WHERE t.attributes_id=ua.id %1
        ORDER BY track_start_ms %2
        %3
    )sql").args(
        filterExpr,
        m_filter.sortOrder == Qt::AscendingOrder ? "ASC" : "DESC",
        limitExpr));

    sqlFilter.bindFields(&query->impl());
}

void ObjectTrackSearcher::prepareLookupQuery(nx::sql::AbstractSqlQuery* query)
{
    QString queryText = R"sql(
        WITH
          %boxFilteredTable%
          filtered_object AS
            (SELECT t.*
            FROM
               %fromBoxFilteredTable%
               %objectFilteredByText%
            WHERE
              %joinBoxFilteredTable%
              %objectExpr%
              1 = 1
            ORDER BY track_start_ms DESC
            %limitObjectCount%)
        SELECT t.id, device_id, object_type_id, guid, track_start_ms, track_end_ms,
            track_detail, attrs.content AS content, best_shot_timestamp_ms, best_shot_rect
        FROM filtered_object t, unique_attributes attrs
        WHERE t.attributes_id = attrs.id
        ORDER BY track_start_ms %objectOrderByTrackStart%
    )sql";

    std::map<QString, QString> queryTextParams({
        {kBoxFilteredTable, ""},
        {kFromBoxFilteredTable, ""},
        {kJoinBoxFilteredTable, ""},
        {kObjectFilteredByTextExpr, "track t"},
        {kObjectExpr, ""},
        {kLimitObjectCount, ""},
        {kObjectOrderByTrackStart, "DESC"}});

    nx::sql::Filter boxSubqueryFilter;
    if (m_filter.boundingBox)
    {
        QString queryText;
        std::tie(queryText, boxSubqueryFilter) = prepareBoxFilterSubQuery();
        queryTextParams[kBoxFilteredTable] = "box_filtered_ids AS (" + queryText + "),";
        queryTextParams[kFromBoxFilteredTable] = "box_filtered_ids,";
        queryTextParams[kJoinBoxFilteredTable] = "t.id = box_filtered_ids.object_id AND ";
    }

    const auto sqlQueryFilter = prepareTrackFilterSqlExpression();
    auto objectFilterSqlText = sqlQueryFilter.toString();
    if (!objectFilterSqlText.empty())
        queryTextParams[kObjectExpr] = (objectFilterSqlText + " AND ").c_str();

    if (m_filter.maxObjectTracksToSelect > 0)
    {
        queryTextParams[kLimitObjectCount] =
            lm("LIMIT %1").args(m_filter.maxObjectTracksToSelect).toQString();
    }

    if (!m_filter.freeText.isEmpty())
    {
        queryTextParams[kObjectFilteredByTextExpr] =
            "(SELECT * FROM track WHERE attributes_id IN "
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

std::tuple<QString /*query text*/, nx::sql::Filter> ObjectTrackSearcher::prepareBoxFilterSubQuery()
{
    using namespace std::chrono;

    NX_ASSERT(m_filter.boundingBox);

    nx::sql::Filter sqlFilter;
    addBoundingBoxToFilter(
        translateToSearchGrid(*m_filter.boundingBox),
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
        SELECT DISTINCT og.object_id
          FROM object_group og, object_search os
          WHERE
            %1 AND
            og.group_id = os.object_group_id
    )sql").args(sqlFilter.toString());

    return std::make_tuple(std::move(queryText), std::move(sqlFilter));
}

nx::sql::Filter ObjectTrackSearcher::prepareTrackFilterSqlExpression()
{
    nx::sql::Filter sqlFilter;

    addTrackFilterConditions(
        m_filter,
        m_deviceDao,
        m_objectTypeDao,
        {"guid", {"track_start_ms", "track_end_ms"}},
        &sqlFilter);

    return sqlFilter;
}

std::vector<ObjectTrack> ObjectTrackSearcher::loadTracks(
    nx::sql::AbstractSqlQuery* query, int limit)
{
    std::vector<ObjectTrack> tracks;
    while (query->next())
    {
        auto track = loadTrack(
            query,
            [this](const auto& track) { return satisfiesFilter(m_filter, track); });

        if (!track)
            continue;

        tracks.push_back(std::move(*track));

        if (limit > 0 && tracks.size() >= limit)
            break;
    }

    return tracks;
}

template<typename FilterFunc>
std::optional<ObjectTrack> ObjectTrackSearcher::loadTrack(
    nx::sql::AbstractSqlQuery* query,
    FilterFunc filter)
{
    ObjectTrack track;

    track.deviceId = m_deviceDao.deviceGuidFromId(
        query->value("device_id").toLongLong());
    track.objectTypeId = m_objectTypeDao.objectTypeFromId(
        query->value("object_type_id").toLongLong());
    track.id = QnSql::deserialized_field<QnUuid>(query->value("guid"));
    track.attributes = AttributesDao::deserialize(query->value("content").toString());
    track.firstAppearanceTimeUs =
        query->value("track_start_ms").toLongLong() * kUsecInMs;
    track.lastAppearanceTimeUs =
        query->value("track_end_ms").toLongLong() * kUsecInMs;

    track.bestShot.timestampUs =
        query->value("best_shot_timestamp_ms").toLongLong() * kUsecInMs;
    if (track.bestShot.initialized())
    {
        track.bestShot.rect =
            TrackSerializer::deserialized<QRectF>(
                query->value("best_shot_rect").toByteArray());
    }

    track.objectPositionSequence = TrackSerializer::deserialized<decltype(track.objectPositionSequence)>(
        query->value("track_detail").toByteArray());

    filterTrack(&track.objectPositionSequence);
    if (!filter(track))
        return std::nullopt;

    truncateTrack(&track.objectPositionSequence, m_filter.maxObjectTrackSize);
    if (!track.objectPositionSequence.empty())
    {
        for (auto& position: track.objectPositionSequence)
            position.deviceId = track.deviceId;
    }

    return track;
}

void ObjectTrackSearcher::filterTrack(std::vector<ObjectPosition>* const track)
{
    using namespace std::chrono;

    auto doesNotSatisfiesFilter =
        [this](const ObjectPosition& position)
        {
            if (!m_filter.timePeriod.isNull())
            {
                const auto timestamp = microseconds(position.timestampUs);
                if (!m_filter.timePeriod.contains(duration_cast<milliseconds>(timestamp)))
                    return true;
            }

            return false;
        };

    track->erase(
        std::remove_if(track->begin(), track->end(), doesNotSatisfiesFilter),
        track->end());

    std::sort(
        track->begin(), track->end(),
        [](const auto& left, const auto& right) { return left.timestampUs < right.timestampUs; });
}

void ObjectTrackSearcher::truncateTrack(
    std::vector<ObjectPosition>* const track,
    int maxSize)
{
    if (maxSize > 0 && (int)track->size() > maxSize)
        track->erase(track->begin() + maxSize, track->end());
}

void ObjectTrackSearcher::addObjectTypeIdToFilter(
    const std::vector<QString>& objectTypes,
    const ObjectTypeDao& objectTypeDao,
    nx::sql::Filter* sqlFilter)
{
    auto condition = std::make_unique<nx::sql::SqlFilterFieldAnyOf>(
        "object_type_id", ":objectTypeId");
    for (const auto& objectType: objectTypes)
        condition->addValue((long long) objectTypeDao.objectTypeIdFromName(objectType));
    sqlFilter->addCondition(std::move(condition));
}

} // namespace nx::analytics::db
