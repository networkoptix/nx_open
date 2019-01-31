#include "abstract_system_merge_dao.h"

#include <nx/fusion/model_functions.h>

#include "rdb/system_merge_dao.h"

namespace nx::cloud::db {
namespace dao {

api::SystemMergeInfo MergeInfo::masterSystemInfo() const
{
    api::SystemMergeInfo masterSystem;
    masterSystem.anotherSystemId = slaveSystemId;
    masterSystem.role = api::MergeRole::master;
    masterSystem.startTime = startTime;
    return masterSystem;
}

api::SystemMergeInfo MergeInfo::slaveSystemInfo() const
{
    api::SystemMergeInfo slaveSystem;
    slaveSystem.anotherSystemId = masterSystemId;
    slaveSystem.role = api::MergeRole::slave;
    slaveSystem.startTime = startTime;
    return slaveSystem;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (MergeInfo),
    (sql_record),
    _Fields)

//-------------------------------------------------------------------------------------------------

SystemMergeDaoFactory::SystemMergeDaoFactory():
    base_type(std::bind(&SystemMergeDaoFactory::defaultFactoryFunction, this,
        std::placeholders::_1))
{
}

SystemMergeDaoFactory& SystemMergeDaoFactory::instance()
{
    static SystemMergeDaoFactory staticInstance;
    return staticInstance;
}

std::unique_ptr<AbstractSystemMergeDao> SystemMergeDaoFactory::defaultFactoryFunction(
    nx::sql::AsyncSqlQueryExecutor* queryExecutor)
{
    return std::make_unique<rdb::SystemMergeDao>(queryExecutor);
}

} // namespace dao
} // namespace nx::cloud::db
