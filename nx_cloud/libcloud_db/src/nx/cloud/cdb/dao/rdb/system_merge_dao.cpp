#include "system_merge_dao.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/db/query.h>

namespace nx {
namespace cdb {
namespace dao {
namespace rdb {

SystemMergeDao::SystemMergeDao(
    utils::db::AsyncSqlQueryExecutor* queryExecutor)
    :
    m_queryExecutor(queryExecutor)
{
}

std::vector<MergeInfo> SystemMergeDao::fetchAll(
    utils::db::QueryContext* queryContext)
{
    utils::db::SqlQuery query(*queryContext->connection());
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
    utils::db::QueryContext* queryContext,
    const MergeInfo& mergeInfo)
{
    utils::db::SqlQuery query(*queryContext->connection());
    query.prepare(R"sql(
        INSERT INTO system_merge_info (master_system_id, slave_system_id, start_time_utc)
        VALUES (:masterSystemId, :slaveSystemId, :startTime)
    )sql");
    QnSql::bind(mergeInfo, &query.impl());
    query.exec();
}

void SystemMergeDao::removeMergeBySlaveSystemId(
    utils::db::QueryContext* queryContext,
    const std::string& slaveSystemId)
{
    utils::db::SqlQuery query(*queryContext->connection());
    query.prepare(R"sql(
        DELETE FROM system_merge_info WHERE slave_system_id = :slaveSystemId
    )sql");
    query.bindValue(":slaveSystemId", slaveSystemId);
    query.exec();
}

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
