#pragma once

#include <set>

#include <nx/sql/filter.h>
#include <nx/sql/query.h>
#include <nx/sql/query_context.h>
#include <nx/sql/sql_cursor.h>

#include <analytics/db/analytics_db_types.h>

#include "abstract_cursor.h"
#include "device_dao.h"
#include "object_type_dao.h"

namespace nx::analytics::db {

class AttributesDao;
class AnalyticsArchiveDirectory;
class AbstractObjectTypeDictionary;

struct TimeRangeFields
{
    const char* timeRangeStart;
    const char* timeRangeEnd;

    TimeRangeFields() = delete;
};

struct ObjectFields
{
    const char* trackId;
    TimeRangeFields timeRange;

    ObjectFields() = delete;
};

class ObjectTrackSearcher
{
public:
    ObjectTrackSearcher(
        const DeviceDao& deviceDao,
        const ObjectTypeDao& objectTypeDao,
        const AbstractObjectTypeDictionary& objectTypeDictionary,
        AttributesDao* attributesDao,
        AnalyticsArchiveDirectory* analyticsArchive,
        Filter filter);

    /**
     * Throws on failure.
     */
    std::vector<ObjectTrackEx> lookup(nx::sql::QueryContext* queryContext);

    void prepareCursorQuery(nx::sql::SqlQuery* query);

    void loadCurrentRecord(nx::sql::SqlQuery*, ObjectTrack*);

    static void addDeviceFilterCondition(
        const std::vector<QnUuid>& deviceIds,
        const DeviceDao& deviceDao,
        nx::sql::Filter* sqlFilter);

    /**
     * @param FieldType. One of std::chrono::duration types.
     */
    template<typename FieldType>
    static void addTimePeriodToFilter(
        const QnTimePeriod& timePeriod,
        const TimeRangeFields& timeRangeFields,
        nx::sql::Filter* sqlFilter);

private:
    struct TrackQueryResult
    {
        std::vector<ObjectTrackEx> tracks;
        std::set<QnUuid> ids;
    };

    const DeviceDao& m_deviceDao;
    const ObjectTypeDao& m_objectTypeDao;
    const AbstractObjectTypeDictionary& m_objectTypeDictionary;
    AttributesDao* m_attributesDao = nullptr;
    AnalyticsArchiveDirectory* m_analyticsArchive = nullptr;
    Filter m_filter;

    std::optional<ObjectTrack> fetchTrackById(
        nx::sql::QueryContext* queryContext,
        const QnUuid& trackGuid);

    std::vector<ObjectTrackEx> lookupTracksUsingArchive(nx::sql::QueryContext* queryContext);

    void fetchTracksFromDb(
        nx::sql::QueryContext* queryContext,
        const std::set<std::int64_t>& trackGroups,
        TrackQueryResult* result);

    void prepareCursorQueryImpl(nx::sql::AbstractSqlQuery* query);

    std::vector<ObjectTrack> loadTracks(
        nx::sql::AbstractSqlQuery* query,
        int limit,
        Filter::Options filterOptions = Filter::Option::none);

    /**
     * Loads current record from query as an ObjectTrack.
     * @return std::nullopt if the record is not matched by the filter.
     */
    template<typename FilterFunc>
    // requires std::is_same_v<std::invoke_result_t<FilterFunc, const ObjectTrack&>, bool>
    std::optional<ObjectTrack> loadTrack(
        nx::sql::AbstractSqlQuery* query,
        FilterFunc filter);

    static void addObjectTypeIdToFilter(
        const std::vector<QString>& objectTypes,
        const ObjectTypeDao& objectTypeDao,
        nx::sql::Filter* sqlFilter);
};

} // namespace nx::analytics::db
