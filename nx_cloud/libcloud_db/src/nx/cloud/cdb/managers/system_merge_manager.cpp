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
    using namespace std::placeholders;

    NX_VERBOSE(this, lm("Requested merge of system %1 into %2")
        .args(idOfSystemToBeMerged.systemId, idOfSystemToMergeTo));

    m_dbManager->executeUpdate(
        std::bind(&SystemMergeManager::updateSystemStateInDb, this, _1,
            idOfSystemToMergeTo, idOfSystemToBeMerged.systemId),
        [this, completionHandler = std::move(completionHandler)](
            nx::utils::db::QueryContext* /*queryContext*/,
            nx::utils::db::DBResult dbResult)
        {
            completionHandler(dbResultToApiResult(dbResult));
        });
}

nx::utils::db::DBResult SystemMergeManager::updateSystemStateInDb(
    nx::utils::db::QueryContext* queryContext,
    const std::string& /*idOfSystemToMergeTo*/,
    const std::string& idOfSystemToMergeBeMerged)
{
    return m_systemManager->updateSystemStatus(
        queryContext,
        idOfSystemToMergeBeMerged,
        api::SystemStatus::beingMerged);
}

} // namespace cdb
} // namespace nx
