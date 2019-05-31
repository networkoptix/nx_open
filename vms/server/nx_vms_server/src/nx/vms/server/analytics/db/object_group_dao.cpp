#include "object_group_dao.h"

#include <memory>

namespace nx::analytics::db {

static constexpr int kCacheSize = 1001;

ObjectGroupDao::ObjectGroupDao()
{
    m_groupCache.setMaxCost(kCacheSize);
}

int64_t ObjectGroupDao::insertOrFetchGroup(
    nx::sql::QueryContext* queryContext,
    const std::set<int64_t>& objectIds)
{
    if (objectIds.empty())
        return -1;

    if (int64_t * id = m_groupCache.object(objectIds);
        id != nullptr)
    {
        // Found in the cache.
        return *id;
    }

    const auto groupId = saveToDb(queryContext, objectIds);

    m_groupCache.insert(
        objectIds,
        std::make_unique<int64_t>(groupId).release());

    queryContext->transaction()->addOnTransactionCompletionHandler(
        [this, objectIds](nx::sql::DBResult result)
        {
            if (result != nx::sql::DBResult::ok)
                m_groupCache.remove(objectIds);
        });

    return groupId;
}

int64_t ObjectGroupDao::saveToDb(
    nx::sql::QueryContext* queryContext,
    const std::set<int64_t>& objectIds)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(R"sql(
        INSERT INTO object_group (group_id, object_id) VALUES (?, ?)
    )sql");

    query->bindValue(0, -1);
    query->bindValue(1, (long long) *objectIds.begin());
    query->exec();

    const auto groupId = query->lastInsertId().toLongLong();

    // Replacing -1 with the correct value.
    queryContext->connection()->executeQuery(
        "UPDATE object_group SET group_id = ? WHERE rowid = ?",
        groupId, groupId);

    for (auto it = std::next(objectIds.begin()); it != objectIds.end(); ++it)
    {
        query->bindValue(0, groupId);
        query->bindValue(1, (long long)* it);
        query->exec();
    }

    return groupId;
}

} // namespace nx::analytics::db
