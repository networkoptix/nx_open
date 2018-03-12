#pragma once

#include <chrono>

#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/basic_factory.h>
#include <nx/utils/db/async_sql_query_executor.h>
#include <nx/utils/db/filter.h>
#include <nx/utils/db/types.h>
#include <nx/utils/db/query_context.h>

#include "../data/system_data.h"

namespace nx {
namespace cdb {
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

    virtual std::vector<MergeInfo> fetchAll(utils::db::QueryContext* queryContext) = 0;

    virtual void save(
        utils::db::QueryContext* queryContext,
        const MergeInfo& mergeInfo) = 0;

    virtual void removeMergeBySlaveSystemId(
        utils::db::QueryContext* queryContext,
        const std::string& slaveSystemId) = 0;
};

//-------------------------------------------------------------------------------------------------

using SystemMergeDaoFactoryFunction =
    std::unique_ptr<AbstractSystemMergeDao>(
        nx::utils::db::AsyncSqlQueryExecutor* queryExecutor);

class SystemMergeDaoFactory:
    public nx::utils::BasicFactory<SystemMergeDaoFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<SystemMergeDaoFactoryFunction>;

public:
    SystemMergeDaoFactory();

    static SystemMergeDaoFactory& instance();

private:
    std::unique_ptr<AbstractSystemMergeDao> defaultFactoryFunction(
        nx::utils::db::AsyncSqlQueryExecutor* queryExecutor);
};

} // namespace dao
} // namespace cdb
} // namespace nx
