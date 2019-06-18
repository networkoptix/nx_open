#pragma once

#include <nx/sql/async_sql_query_executor.h>

#include "../abstract_system_merge_dao.h"

namespace nx::cloud::db {
namespace dao {
namespace rdb {

class SystemMergeDao:
    public AbstractSystemMergeDao
{
public:
    virtual std::vector<MergeInfo> fetchAll(
        sql::QueryContext* queryContext) override;

    virtual void save(
        sql::QueryContext* queryContext,
        const MergeInfo& mergeInfo) override;

    virtual void removeMergeBySlaveSystemId(
        sql::QueryContext* queryContext,
        const std::string& slaveSystemId) override;
};

} // namespace rdb
} // namespace dao
} // namespace nx::cloud::db
