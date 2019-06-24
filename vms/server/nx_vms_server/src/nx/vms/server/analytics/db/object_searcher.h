#pragma once

#include <nx/sql/filter.h>
#include <nx/sql/query.h>
#include <nx/sql/query_context.h>
#include <nx/sql/sql_cursor.h>

#include <analytics/db/abstract_cursor.h>
#include <analytics/db/analytics_db_types.h>

#include "device_dao.h"
#include "object_type_dao.h"

namespace nx::analytics::db {

class AttributesDao;
class AnalyticsArchiveDirectory;

struct TimeRangeFields
{
    const char* timeRangeStart;
    const char* timeRangeEnd;

    TimeRangeFields() = delete;
};

struct ObjectFields
{
    const char* objectId;
    TimeRangeFields timeRange;

    ObjectFields() = delete;
};

class ObjectSearcher
{
public:
    ObjectSearcher(
        const DeviceDao& deviceDao,
        const ObjectTypeDao& objectTypeDao,
        AttributesDao* attributesDao,
        AnalyticsArchiveDirectory* analyticsArchive,
        Filter filter);

    /**
     * Throws on failure.
     */
    std::vector<DetectedObject> lookup(nx::sql::QueryContext* queryContext);

    void prepareCursorQuery(nx::sql::SqlQuery* query);

    void loadCurrentRecord(nx::sql::SqlQuery*, DetectedObject*);

    static std::unique_ptr<AbstractCursor> createCursor(
        std::unique_ptr<nx::sql::Cursor<DetectedObject>> sqlCursor);

    static void addObjectFilterConditions(
        const Filter& filter,
        const DeviceDao& deviceDao,
        const ObjectTypeDao& objectTypeDao,
        const ObjectFields& fieldNames,
        nx::sql::Filter* sqlFilter);

    static void addDeviceFilterCondition(
        const std::vector<QnUuid>& deviceIds,
        const DeviceDao& deviceDao,
        nx::sql::Filter* sqlFilter);

    static void addBoundingBoxToFilter(
        const QRect& boundingBox,
        nx::sql::Filter* sqlFilter);

    /**
     * @param FieldType. One of std::chrono::duration types.
     */
    template<typename FieldType>
    static void addTimePeriodToFilter(
        const QnTimePeriod& timePeriod,
        const TimeRangeFields& timeRangeFields,
        nx::sql::Filter* sqlFilter);

    static bool satisfiesFilter(
        const Filter& filter, const DetectedObject& detectedObject);

    static bool matchAttributes(
        const std::vector<nx::common::metadata::Attribute>& attributes,
        const QString& filter);

private:
    const DeviceDao& m_deviceDao;
    const ObjectTypeDao& m_objectTypeDao;
    AttributesDao* m_attributesDao = nullptr;
    AnalyticsArchiveDirectory* m_analyticsArchive = nullptr;
    Filter m_filter;

    std::optional<DetectedObject> fetchObjectById(
        nx::sql::QueryContext* queryContext,
        const QnUuid& objectGuid);

    std::vector<DetectedObject> lookupObjectsUsingArchive(nx::sql::QueryContext* queryContext);

    void fetchObjectsFromDb(
        nx::sql::QueryContext* queryContext,
        const std::vector<std::int64_t>& objectGroups,
        std::vector<DetectedObject>* result);

    void prepareCursorQueryImpl(nx::sql::AbstractSqlQuery* query);

    void prepareLookupQuery(nx::sql::AbstractSqlQuery* query);

    std::tuple<QString /*query text*/, nx::sql::Filter> prepareBoxFilterSubQuery();

    nx::sql::Filter prepareFilterObjectSqlExpression();

    std::vector<DetectedObject> loadObjects(nx::sql::AbstractSqlQuery* query);
    DetectedObject loadObject(nx::sql::AbstractSqlQuery* query);

    void filterTrack(std::vector<ObjectPosition>* const track);

    static void addObjectTypeIdToFilter(
        const std::vector<QString>& objectTypes,
        const ObjectTypeDao& objectTypeDao,
        nx::sql::Filter* sqlFilter);
};

} // namespace nx::analytics::db
