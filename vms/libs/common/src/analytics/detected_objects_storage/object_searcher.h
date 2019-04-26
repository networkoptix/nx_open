#pragma once

#include <nx/sql/filter.h>
#include <nx/sql/query.h>
#include <nx/sql/query_context.h>
#include <nx/sql/sql_cursor.h>

#include "abstract_cursor.h"
#include "analytics_events_storage_types.h"
#include "device_dao.h"
#include "object_type_dao.h"

namespace nx::analytics::storage {

struct FieldNames
{
    const char* objectId;
    const char* timeRangeStart;
    const char* timeRangeEnd;
};

class ObjectSearcher
{
public:
    ObjectSearcher(
        const DeviceDao& deviceDao,
        const ObjectTypeDao& objectTypeDao,
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
        const FieldNames& fieldNames,
        nx::sql::Filter* sqlFilter);

    static void addBoundingBoxToFilter(
        const QRect& boundingBox,
        nx::sql::Filter* sqlFilter);

    static void addTimePeriodToFilter(
        const QnTimePeriod& timePeriod,
        nx::sql::Filter* sqlFilter,
        const char* leftBoundaryFieldName,
        const char* rightBoundaryFieldName,
        std::optional<std::chrono::milliseconds> maxRecordedTimestamp = std::nullopt);

private:
    const DeviceDao& m_deviceDao;
    const ObjectTypeDao& m_objectTypeDao;
    Filter m_filter;

    void prepareLookupQuery(nx::sql::AbstractSqlQuery* query);
    nx::sql::Filter prepareSqlFilterExpression();

    std::vector<DetectedObject> loadObjects(nx::sql::AbstractSqlQuery* query);
    DetectedObject loadObject(nx::sql::AbstractSqlQuery* query);
    void filterTrack(std::vector<ObjectPosition>* const track);

    static void addObjectTypeIdToFilter(
        const std::vector<QString>& objectTypes,
        const ObjectTypeDao& objectTypeDao,
        nx::sql::Filter* sqlFilter);
};

} // namespace nx::analytics::storage
