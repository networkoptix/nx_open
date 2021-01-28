#include "object_track_searcher.h"

#include <nx/fusion/serialization/sql_functions.h>
#include <nx/utils/match/wildcard.h>
#include <nx/utils/std/algorithm.h>

#include <analytics/db/abstract_object_type_dictionary.h>
#include <analytics/db/config.h>

#include "analytics_archive_directory.h"
#include "attributes_dao.h"
#include "object_track_cache.h"
#include "serializers.h"

namespace nx::analytics::db {

static constexpr int kMaxObjectLookupIterations = kMaxObjectLookupResultSet;

ObjectTrackSearcher::ObjectTrackSearcher(
    const DeviceDao& deviceDao,
    const ObjectTypeDao& objectTypeDao,
    const AbstractObjectTypeDictionary& objectTypeDictionary,
    const ObjectTrackCache& objectTrackCache,
    AttributesDao* attributesDao,
    AnalyticsArchiveDirectory* analyticsArchive,
    Filter filter)
    :
    m_deviceDao(deviceDao),
    m_objectTypeDao(objectTypeDao),
    m_objectTypeDictionary(objectTypeDictionary),
    m_objectTrackCache(objectTrackCache),
    m_attributesDao(attributesDao),
    m_analyticsArchive(analyticsArchive),
    m_filter(std::move(filter))
{
    if (m_filter.maxObjectTracksToSelect == 0 || m_filter.maxObjectTracksToSelect > kMaxObjectLookupResultSet)
        m_filter.maxObjectTracksToSelect = kMaxObjectLookupResultSet;
}

std::vector<ObjectTrackEx> ObjectTrackSearcher::lookup(nx::sql::QueryContext* queryContext)
{
    auto dbLookupResult = lookupInDb(queryContext);
    auto cacheLookupResult = lookupInCache();

    if (cacheLookupResult.empty())
        return dbLookupResult;

    if (m_filter.maxObjectTracksToSelect > 0 &&
        (int)cacheLookupResult.size() > m_filter.maxObjectTracksToSelect)
    {
        cacheLookupResult.erase(cacheLookupResult.begin() + m_filter.maxObjectTracksToSelect, cacheLookupResult.end());
    }

    // DB stores the following fields with millisecond precision. Doing the same here.
    for (auto& track: cacheLookupResult)
    {
        track.firstAppearanceTimeUs = (track.firstAppearanceTimeUs / 1000) * 1000;
        track.lastAppearanceTimeUs = (track.lastAppearanceTimeUs / 1000) * 1000;
        track.bestShot.timestampUs = (track.bestShot.timestampUs / 1000) * 1000;
    }

    auto result = mergeResults(std::move(dbLookupResult), std::move(cacheLookupResult));

    auto comparator =
        [this](auto& left, auto& right)
        {
            if (left.firstAppearanceTimeUs != right.firstAppearanceTimeUs)
            {
                return m_filter.sortOrder == Qt::SortOrder::AscendingOrder
                    ? left.firstAppearanceTimeUs < right.firstAppearanceTimeUs
                    : left.firstAppearanceTimeUs > right.firstAppearanceTimeUs;
            }

            return m_filter.sortOrder == Qt::SortOrder::AscendingOrder
                ? left.id < right.id
                : left.id > right.id;
        };

    std::sort(result.begin(), result.end(), comparator);

    // NOTE: Satisfying maxObjectTracksToSelect in the very end to ensure we select the most recent
    // tracks in the case when tracks from both DB and cache are present in the result.
    if (m_filter.maxObjectTracksToSelect > 0 &&
        (int) result.size() > m_filter.maxObjectTracksToSelect)
    {
        result.erase(result.begin() + m_filter.maxObjectTracksToSelect, result.end());
    }

    return result;
}

BestShotEx ObjectTrackSearcher::lookupBestShot(nx::sql::QueryContext* queryContext)
{
    if (auto track = m_objectTrackCache.getTrackById(m_filter.objectTrackId))
    {
        BestShotEx bestShot(std::move(track->bestShot));
        bestShot.deviceId = std::move(track->deviceId);
        return bestShot;;
    }

    auto query = queryContext->connection()->createQuery();
    query->setForwardOnly(true);
    query->prepare(R"sql(
        SELECT image_data, data_format, best_shot_timestamp_ms, best_shot_rect, stream_index, device_id
        FROM track
        LEFT JOIN best_shot_image on best_shot_image.track_id = track.id
        WHERE guid = ?
    )sql");
    query->addBindValue(QnSql::serialized_field(m_filter.objectTrackId));
    query->exec();

    BestShotEx result;
    if (!query->next())
        return result;
    result.image.imageData = query->value("image_data").toByteArray();
    result.image.imageDataFormat = query->value("data_format").toByteArray();
    result.timestampUs = query->value("best_shot_timestamp_ms").toLongLong() * 1000L;
    result.rect =TrackSerializer::deserialized<QRectF>(
        query->value("best_shot_rect").toByteArray());
    result.streamIndex = (nx::vms::api::StreamIndex) query->value("stream_index").toInt();
    result.deviceId = m_deviceDao.deviceGuidFromId(query->value("device_id").toLongLong());

    return result;
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

std::vector<ObjectTrackEx> ObjectTrackSearcher::lookupInDb(nx::sql::QueryContext* queryContext)
{
    if (!m_filter.objectTrackId.isNull())
    {
        auto track = fetchTrackById(
            queryContext,
            m_filter.objectTrackId,
            m_filter.withBestShotOnly);

        if (!track)
            return {};

        NX_ASSERT_HEAVY_CONDITION(m_filter.acceptsTrack(*track, m_objectTypeDictionary));
        return { std::move(*track) };
    }
    else
    {
        return lookupTracksUsingArchive(queryContext);
    }
}

std::vector<ObjectTrackEx> ObjectTrackSearcher::lookupInCache()
{
    return m_objectTrackCache.lookup(m_filter, m_objectTypeDictionary);
}

static void mergeObjectTracks(ObjectTrackEx* inOutDbTrack, ObjectTrackEx cachedTrack)
{
    if (cachedTrack.bestShot.initialized())
        inOutDbTrack->bestShot = cachedTrack.bestShot;
    inOutDbTrack->lastAppearanceTimeUs =
        std::max(inOutDbTrack->lastAppearanceTimeUs, cachedTrack.lastAppearanceTimeUs);

    std::move(
        cachedTrack.objectPositionSequence.begin(),
        cachedTrack.objectPositionSequence.end(),
        std::back_inserter(inOutDbTrack->objectPositionSequence));
}

std::vector<ObjectTrackEx> ObjectTrackSearcher::mergeResults(
    std::vector<ObjectTrackEx> dbTracks,
    std::vector<ObjectTrackEx> cachedTracks)
{
    std::map<QnUuid /*trackId*/, std::size_t /*pos in vector*/> dbTracksById;
    for (std::size_t i = 0; i < dbTracks.size(); ++i)
        dbTracksById.emplace(dbTracks[i].id, i);

    for (auto& cachedTrack: cachedTracks)
    {
        if (const auto it = dbTracksById.find(cachedTrack.id); it != dbTracksById.cend())
            mergeObjectTracks(&dbTracks[it->second], std::move(cachedTrack));
        else
            dbTracks.push_back(std::move(cachedTrack));
    }

    return dbTracks;
}

std::optional<ObjectTrack> ObjectTrackSearcher::fetchTrackById(
    nx::sql::QueryContext* queryContext,
    const QnUuid& trackGuid,
    bool withBestShotOnly)
{
    auto query = queryContext->connection()->createQuery();
    query->setForwardOnly(true);

    QString request = R"sql(
        SELECT device_id, object_type_id, guid, track_start_ms, track_end_ms, track_detail,
            ua.content AS content, best_shot_timestamp_ms, best_shot_rect, stream_index
        FROM track t, unique_attributes ua
        WHERE t.attributes_id=ua.id AND guid=?
    )sql";
    if (withBestShotOnly)
        request += " AND best_shot_timestamp_ms > 0";

    query->prepare(request);
    query->addBindValue(QnSql::serialized_field(trackGuid));

    query->exec();

    auto tracks = loadTracks(query.get(), 1);
    if (tracks.empty())
        return std::nullopt;

    return std::move(tracks.front());
}

std::vector<ObjectTrackEx> ObjectTrackSearcher::lookupTracksUsingArchive(
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
        return std::vector<ObjectTrackEx>(); //< No records with such text.
    }

    archiveFilter->sortOrder = Qt::DescendingOrder;

    std::set<std::int64_t> analyzedObjectGroups;

    TrackQueryResult result;
    std::map<QnUuid, QnTimePeriod> previousPeriods;
    for (int i = 0; i < kMaxObjectLookupIterations; ++i)
    {
        const auto matchResult = m_analyticsArchive->matchObjects(
            m_filter.deviceIds,
            *archiveFilter,
            &previousPeriods);
        const int fetchedObjectGroupsCount = (int) matchResult.trackGroups.size();

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
        for (const auto& [deviceId, period]: matchResult.timePeriods)
            previousPeriods[deviceId].addPeriod(period);
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
    static constexpr std::size_t maxGroupsToQuery = 999;

    std::vector<long long> allGroups(objectTrackGroups.begin(), objectTrackGroups.end());

    for (std::size_t start = 0; start < allGroups.size();)
    {
        const auto end = std::min(start + maxGroupsToQuery, allGroups.size());

        nx::sql::SqlFilterFieldAnyOf objectGroupFilter(
            "tg.group_id",
            ":groupId",
            std::vector<long long>(allGroups.begin() + start, allGroups.begin() + end));

        start = end;

        std::string limitExpr;
        if (m_filter.maxObjectTracksToSelect > 0)
            limitExpr = "LIMIT " + std::to_string(m_filter.maxObjectTracksToSelect);

        auto query = queryContext->connection()->createQuery();
        query->setForwardOnly(true);

        auto filterExpr = objectGroupFilter.toString();
        if (m_filter.withBestShotOnly)
            filterExpr += " AND best_shot_timestamp_ms > 0";

        query->prepare(lm(R"sql(
            SELECT device_id, object_type_id, guid, track_start_ms, track_end_ms, track_detail,
                ua.content AS content, best_shot_timestamp_ms, best_shot_rect, stream_index
            FROM track t, unique_attributes ua, track_group tg
            WHERE t.attributes_id=ua.id AND t.id=tg.track_id AND %1
            ORDER BY track_start_ms DESC
        )sql").args(filterExpr));
        objectGroupFilter.bindFields(&query->impl());

        query->exec();

        auto tracks = loadTracks(
            query.get(),
            m_filter.maxObjectTracksToSelect);

        for (auto& track: tracks)
        {
            if (result->ids.count(track.id) > 0)
                continue;

            // Filter does not accept track here cause of bounding box limitations.
            // NX_ASSERT_HEAVY_CONDITION(m_filter.acceptsTrack(track));
            result->ids.insert(track.id);

            // Searching for a position to insert to keep result vector sorted.
            const auto pos = std::upper_bound(
                result->tracks.begin(), result->tracks.end(),
                track,
                [](const auto& left, const auto& right)
                {
                    return left.firstAppearanceTimeUs > right.firstAppearanceTimeUs;
                });
            result->tracks.insert(pos, std::move(track));
        }

        if (m_filter.maxObjectTracksToSelect > 0 &&
            (int) result->tracks.size() >= m_filter.maxObjectTracksToSelect)
        {
            result->tracks.resize(m_filter.maxObjectTracksToSelect);
            break;
        }
    }

    if (m_filter.sortOrder == Qt::AscendingOrder)
        std::reverse(result->tracks.begin(), result->tracks.end());
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
    if (m_filter.withBestShotOnly)
        filterExpr += " AND best_shot_timestamp_ms > 0";

    std::string limitExpr;
    if (m_filter.maxObjectTracksToSelect > 0)
        limitExpr = "LIMIT " + std::to_string(m_filter.maxObjectTracksToSelect);

    query->setForwardOnly(true);

    query->prepare(lm(R"sql(
        SELECT device_id, object_type_id, guid, track_start_ms, track_end_ms, track_detail,
            ua.content AS content, best_shot_timestamp_ms, best_shot_rect, stream_index
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
    nx::sql::AbstractSqlQuery* query, int limit, Filter::Options filterOptions)
{
    std::vector<ObjectTrack> tracks;
    while (query->next())
    {
        auto track = loadTrack(
            query,
            [this, filterOptions](const auto& track)
            {
                return m_filter.acceptsTrack(track, m_objectTypeDictionary, filterOptions);
            });

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
    track.bestShot.streamIndex = (nx::vms::api::StreamIndex) query->value("stream_index").toInt();
    if (track.bestShot.initialized())
    {
        track.bestShot.rect =
            TrackSerializer::deserialized<QRectF>(
                query->value("best_shot_rect").toByteArray());
    }

    track.objectPosition = TrackSerializer::deserialized<ObjectRegion>
        (query->value("track_detail").toByteArray());

    if (!filter(track))
        return std::nullopt;

    return track;
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
