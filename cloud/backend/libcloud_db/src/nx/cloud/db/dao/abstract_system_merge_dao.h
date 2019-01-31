#pragma once

#include <chrono>

#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/basic_factory.h>
#include <nx/sql/async_sql_query_executor.h>
#include <nx/sql/filter.h>
#include <nx/sql/types.h>
#include <nx/sql/query_context.h>

#include "../data/system_data.h"

namespace nx::cloud::db {
namespace dao {

struct MergeInfo
{
    std::string masterSystemId;
    std::string slaveSystemId;
    std::chrono::system_clock::time_point startTime;

    api::SystemMergeInfo masterSystemInfo() const;
    api::SystemMergeInfo slaveSystemInfo() const;
};

#define MergeInfo_Fields (masterSystemId)(slaveSystemId)(startTime)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((MergeInfo), (sql_record))

class AbstractSystemMergeDao
{
public:
    virtual ~AbstractSystemMergeDao() = default;

    virtual std::vector<MergeInfo> fetchAll(sql::QueryContext* queryContext) = 0;

    virtual void save(
        sql::QueryContext* queryContext,
        const MergeInfo& mergeInfo) = 0;

    virtual void removeMergeBySlaveSystemId(
        sql::QueryContext* queryContext,
        const std::string& slaveSystemId) = 0;
};

//-------------------------------------------------------------------------------------------------

using SystemMergeDaoFactoryFunction =
    std::unique_ptr<AbstractSystemMergeDao>(
        nx::sql::AsyncSqlQueryExecutor* queryExecutor);

class SystemMergeDaoFactory:
    public nx::utils::BasicFactory<SystemMergeDaoFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<SystemMergeDaoFactoryFunction>;

public:
    SystemMergeDaoFactory();

    static SystemMergeDaoFactory& instance();

private:
    std::unique_ptr<AbstractSystemMergeDao> defaultFactoryFunction(
        nx::sql::AsyncSqlQueryExecutor* queryExecutor);
};

} // namespace dao
} // namespace nx::cloud::db
