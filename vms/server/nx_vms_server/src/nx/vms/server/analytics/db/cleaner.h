#pragma once

#include <chrono>

#include <nx/sql/query_context.h>
#include <nx/utils/uuid.h>

namespace nx::analytics::db {

class AttributesDao;

class Cleaner
{
public:
    enum class Result
    {
        done,
        /** Some data has been cleaned up. But some data still remains. */
        incomplete,
    };

    Cleaner(
        AttributesDao* attributesDao,
        int deviceId,
        std::chrono::milliseconds oldestDataToKeepTimestamp);

    Result clean(nx::sql::QueryContext* queryContext);

private:
    AttributesDao* m_attributesDao = nullptr;
    const int m_deviceId;
    const std::chrono::milliseconds m_oldestDataToKeepTimestamp;

    /** All following functions return removed record count. */

    int cleanObjectSearch(nx::sql::QueryContext* queryContext);
    int cleanObjectSearchToObject(nx::sql::QueryContext* queryContext);
    int cleanObject(nx::sql::QueryContext* queryContext);

    int executeObjectDataCleanUpQuery(
        nx::sql::QueryContext* queryContext,
        const char* queryText);

    int cleanAttributesTextIndex(nx::sql::QueryContext* queryContext);
};

} // namespace nx::analytics::db
