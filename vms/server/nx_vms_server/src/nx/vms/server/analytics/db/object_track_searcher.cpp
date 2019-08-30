#include "object_track_searcher.h"

#include <nx/fusion/serialization/sql_functions.h>
#include <nx/utils/match/wildcard.h>
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
    if (!m_filter.objectTrackId.isNull())
    {
        auto track = fetchTrackById(queryContext, m_filter.objectTrackId);
        if (!track)
            return {};

        NX_ASSERT_HEAVY_CONDITION(m_filter.acceptsTrack(*track));
        return {std::move(*track)};
    }
    else
    {
        return lookupTracksUsingArchive(queryContext);
    }
}

void ObjectTrackSearcher::prepareCursorQuery(nx::sql::SqlQuery* query)
{
    prepareCursorQueryImpl(query);
}

void ObjectTrackSearcher::loadCurrentRecord(
    nx::sql::SqlQuery* query,
    ObjectTrack* object)
{
    *object = *loadTrack(query, [](auto&&...) { return true; });
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

    TrackQueryResult result;
    for (int i = 0; i < kMaxObjectLookupIterations; ++i)
    {
        auto matchResult = m_analyticsArchive->matchObjects(
            m_filter.deviceIds,
            *archiveFilter);
        const int fetchedObjectGroupsCount = matchResult.trackGroups.size();

        const std::set<std::int64_t> uniqueTrackGroups(
            matchResult.trackGroups.begin(), matchResult.trackGroups.end());

        std::set<std::int64_t> newTrackGroups;
        std::set_difference(
            uniqueTrackGroups.begin(), uniqueTrackGroups.end(),
            analyzedObjectGroups.begin(), analyzedObjectGroups.end(),
            std::inserter(newTrackGroups, newTrackGroups.end()));

        analyzedObjectGroups.insert(newTrackGroups.begin(), newTrackGroups.end());

        fetchTracksFromDb(queryContext, newTrackGroups, &result);

        if ((int) result.tracks.size() >= m_filter.maxObjectTracksToSelect ||
            fetchedObjectGroupsCount < archiveFilter->limit)
        {
            if ((int) result.tracks.size() > m_filter.maxObjectTracksToSelect)
            {
                result.tracks.erase(
                    result.tracks.begin() + m_filter.maxObjectTracksToSelect,
                    result.tracks.end());
            }
            return std::move(result.tracks);
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

    return std::move(result.tracks);
}

void ObjectTrackSearcher::fetchTracksFromDb(
    nx::sql::QueryContext* queryContext,
    const std::set<std::int64_t>& objectTrackGroups,
    TrackQueryResult* result)
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

    for (auto& track: tracks)
    {
        if (result->ids.count(track.id) > 0)
            continue;

        NX_ASSERT_HEAVY_CONDITION(m_filter.acceptsTrack(track));
        result->ids.insert(track.id);
        result->tracks.push_back(std::move(track));
    }
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

std::vector<ObjectTrack> ObjectTrackSearcher::loadTracks(
    nx::sql::AbstractSqlQuery* query, int limit)
{
    std::vector<ObjectTrack> tracks;
    while (query->next())
    {
        auto track = loadTrack(
            query,
            [this](const auto& track) { return m_filter.acceptsTrack(track); });

        if (!track)
            continue;

        tracks.push_back(std::move(*track));

        if (limit > 0 && (int) tracks.size() >= limit)
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
