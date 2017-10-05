#include "system_merge_manager.h"

#include <nx/utils/log/log.h>

#include "system_manager.h"

namespace nx {
namespace cdb {

SystemMergeManager::SystemMergeManager(
    AbstractSystemManager* systemManager,
    nx::utils::db::AsyncSqlQueryExecutor* dbManager)
    :
    m_systemManager(systemManager),
    m_dbManager(dbManager)
{
}

void SystemMergeManager::startMergingSystems(
    const AuthorizationInfo& /*authzInfo*/,
    const std::string& idOfSystemToMergeTo,
    data::SystemId idOfSystemToBeMerged,
    std::function<void(api::ResultCode)> completionHandler)
{
    NX_VERBOSE(this, lm("Requested merge of system %1 into %2")
        .args(idOfSystemToBeMerged.systemId, idOfSystemToMergeTo));

    completionHandler(api::ResultCode::notImplemented);
}

} // namespace cdb
} // namespace nx
