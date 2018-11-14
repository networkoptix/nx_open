#include "system_merge_dao.h"

#include <nx/fusion/model_functions.h>
#include <nx/sql/query.h>

namespace nx::cloud::db {
namespace dao {
namespace rdb {

SystemMergeDao::SystemMergeDao(
    sql::AsyncSqlQueryExecutor* queryExecutor)
    :
    m_queryExecutor(queryExecutor)
{
}

std::vector<MergeInfo> SystemMergeDao::fetchAll(
    sql::QueryContext* queryContext)
{
    sql::SqlQuery query(queryContext->connection());
    query.prepare(R"sql(
        SELECT master_system_id AS masterSystemId, slave_system_id AS slaveSystemId,
            start_time_utc AS startTime
        FROM system_merge_info
    )sql");
    query.exec();

    std::vector<MergeInfo> mergeInfo;
    QnSql::fetch_many(query.impl(), &mergeInfo);
    return mergeInfo;
}

void SystemMergeDao::save(
    sql::QueryContext* queryContext,
    const MergeInfo& mergeInfo)
{
    sql::SqlQuery query(queryContext->connection());
    query.prepare(R"sql(
        INSERT INTO system_merge_info (master_system_id, slave_system_id, start_time_utc)
        VALUES (:masterSystemId, :slaveSystemId, :startTime)
    )sql");
    QnSql::bind(mergeInfo, &query.impl());
    query.exec();
}

void SystemMergeDao::removeMergeBySlaveSystemId(
    sql::QueryContext* queryContext,
    const std::string& slaveSystemId)
{
    sql::SqlQuery query(queryContext->connection());
    query.prepare(R"sql(
        DELETE FROM system_merge_info WHERE slave_system_id = :slaveSystemId
    )sql");
    query.bindValue(":slaveSystemId", slaveSystemId);
    query.exec();
}

} // namespace rdb
} // namespace dao
} // namespace nx::cloud::db
