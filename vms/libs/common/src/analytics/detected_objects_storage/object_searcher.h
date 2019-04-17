#pragma once

#include <nx/sql/query.h>
#include <nx/sql/query_context.h>

#include "analytics_events_storage_types.h"
#include "device_dao.h"
#include "object_type_dao.h"

namespace nx::analytics::storage {

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

private:
    const DeviceDao& m_deviceDao;
    const ObjectTypeDao& m_objectTypeDao;

    void prepareLookupQuery(
        const Filter& filter,
        nx::sql::AbstractSqlQuery* query);

    std::vector<DetectedObject> loadObjects(
        nx::sql::AbstractSqlQuery* query,
        const Filter& filter);

    DetectedObject loadObject(
        nx::sql::AbstractSqlQuery* query,
        const Filter& filter);
};

} // namespace nx::analytics::storage
