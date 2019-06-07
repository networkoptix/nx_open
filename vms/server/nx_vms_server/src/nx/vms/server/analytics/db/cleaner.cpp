#include "cleaner.h"

#include <analytics/db/config.h>

#include "attributes_dao.h"

namespace nx::analytics::db {

static constexpr int kRecordsToRemoveAtATime = 1000;

Cleaner::Cleaner(
    AttributesDao* attributesDao,
    int deviceId,
    std::chrono::milliseconds oldestDataToKeepTimestamp)
    :
    m_attributesDao(attributesDao),
    m_deviceId(deviceId),
    m_oldestDataToKeepTimestamp(oldestDataToKeepTimestamp)
{
}

Cleaner::Result Cleaner::clean(nx::sql::QueryContext* queryContext)
{
    if (!kLookupObjectsInAnalyticsArchive)
    {
        if (cleanObjectSearch(queryContext) >= kRecordsToRemoveAtATime)
            return Result::incomplete;
    }

    if (cleanObjectSearchToObject(queryContext) >= kRecordsToRemoveAtATime)
        return Result::incomplete;

    if (cleanObject(queryContext) >= kRecordsToRemoveAtATime)
        return Result::incomplete;

    // Cleaning unused unique_attributes and text index.
    if (cleanAttributesTextIndex(queryContext) >= kRecordsToRemoveAtATime)
        return Result::incomplete;

    return Result::done;
}

int Cleaner::cleanObjectSearch(nx::sql::QueryContext* queryContext)
{
    return executeObjectDataCleanUpQuery(
        queryContext,
        R"sql(
            DELETE FROM object_search WHERE id IN
                (SELECT id FROM object_search WHERE object_group_id IN
	                (SELECT DISTINCT group_id FROM object_group WHERE object_id IN
		                (SELECT id FROM object WHERE device_id=? AND track_start_ms<?))
                LIMIT ?)
        )sql");
}

int Cleaner::cleanObjectSearchToObject(nx::sql::QueryContext* queryContext)
{
    return executeObjectDataCleanUpQuery(
        queryContext,
        R"sql(
            DELETE FROM object_group WHERE rowid IN
                (SELECT rowid FROM object_group WHERE object_id IN
                    (SELECT id FROM object WHERE device_id=? AND track_start_ms<?)
                LIMIT ?)
        )sql");
}

int Cleaner::cleanObject(nx::sql::QueryContext* queryContext)
{
    return executeObjectDataCleanUpQuery(
        queryContext,
        R"sql(
            DELETE FROM object WHERE id IN
                (SELECT id FROM object WHERE device_id=? AND track_start_ms<? LIMIT ?)
        )sql");
}

int Cleaner::executeObjectDataCleanUpQuery(
    nx::sql::QueryContext* queryContext,
    const char* queryText)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(queryText);

    query->addBindValue(m_deviceId);
    query->addBindValue((long long) m_oldestDataToKeepTimestamp.count());
    query->addBindValue(kRecordsToRemoveAtATime);

    query->exec();

    return query->numRowsAffected();
}

int Cleaner::cleanAttributesTextIndex(nx::sql::QueryContext* queryContext)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(R"sql(
        DELETE FROM attributes_text_index WHERE docid IN(
            SELECT attrs.id
            FROM unique_attributes attrs LEFT JOIN object o ON attrs.id = o.attributes_id
            WHERE o.id IS NULL
            LIMIT ?)
    )sql");

    query->addBindValue(kRecordsToRemoveAtATime);
    query->exec();

    query->prepare(R"sql(
        DELETE FROM unique_attributes WHERE id IN(
            SELECT attrs.id
            FROM unique_attributes attrs LEFT JOIN object o ON attrs.id = o.attributes_id
            WHERE o.id IS NULL
            LIMIT ?)
    )sql");

    query->addBindValue(kRecordsToRemoveAtATime);
    query->exec();

    m_attributesDao->clear();

    return query->numRowsAffected();
}

} // namespace nx::analytics::db
