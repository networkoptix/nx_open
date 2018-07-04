#pragma once

#include <nx/utils/db/async_sql_query_executor.h>

#include "../abstract_system_merge_dao.h"

namespace nx {
namespace cdb {
namespace dao {
namespace rdb {

class SystemMergeDao:
    public AbstractSystemMergeDao
{
public:
    SystemMergeDao(sql::AsyncSqlQueryExecutor* queryExecutor);

    virtual std::vector<MergeInfo> fetchAll(
        sql::QueryContext* queryContext) override;

    virtual void save(
        sql::QueryContext* queryContext,
        const MergeInfo& mergeInfo) override;

    virtual void removeMergeBySlaveSystemId(
        sql::QueryContext* queryContext,
        const std::string& slaveSystemId) override;

private:
    sql::AsyncSqlQueryExecutor* m_queryExecutor;
};

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
