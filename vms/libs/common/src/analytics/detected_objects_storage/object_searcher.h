#pragma once

#include <nx/sql/filter.h>
#include <nx/sql/query.h>
#include <nx/sql/query_context.h>

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
        const ObjectTypeDao& objectTypeDao);

    /**
     * Throws on failure.
     */
    std::vector<DetectedObject> lookup(
        nx::sql::QueryContext* queryContext,
        const Filter& filter);

    static void addObjectFilterConditions(
        const Filter& filter,
        const DeviceDao& deviceDao,
        const ObjectTypeDao& objectTypeDao,
        const FieldNames& fieldNames,
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

    void prepareLookupQuery(
        const Filter& filter,
        nx::sql::AbstractSqlQuery* query);

    nx::sql::Filter prepareSqlFilterExpression(const Filter& filter);

    std::vector<DetectedObject> loadObjects(
        nx::sql::AbstractSqlQuery* query,
        const Filter& filter);

    DetectedObject loadObject(
        nx::sql::AbstractSqlQuery* query,
        const Filter& filter);

    static void addObjectTypeIdToFilter(
        const std::vector<QString>& objectTypes,
        const ObjectTypeDao& objectTypeDao,
        nx::sql::Filter* sqlFilter);
};

} // namespace nx::analytics::storage
