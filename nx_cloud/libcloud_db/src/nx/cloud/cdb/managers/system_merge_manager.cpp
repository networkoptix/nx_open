#include "system_merge_manager.h"

#include <nx/utils/log/log.h>

#include "system_health_info_provider.h"
#include "system_manager.h"

namespace nx {
namespace cdb {

SystemMergeManager::SystemMergeManager(
    AbstractSystemManager* systemManager,
    const AbstractSystemHealthInfoProvider& systemHealthInfoProvider,
    nx::utils::db::AsyncSqlQueryExecutor* dbManager)
    :
    m_systemManager(systemManager),
    m_systemHealthInfoProvider(systemHealthInfoProvider),
    m_dbManager(dbManager)
{
}

void SystemMergeManager::startMergingSystems(
    const AuthorizationInfo& authzInfo,
    const std::string& idOfSystemToMergeTo,
    data::SystemId idOfSystemToBeMerged,
    std::function<void(api::ResultCode)> completionHandler)
{
    using namespace std::placeholders;

    NX_VERBOSE(this, lm("Requested merge of system %1 into %2")
        .args(idOfSystemToBeMerged.systemId, idOfSystemToMergeTo));

    const auto resultCode = validateRequestInput(
        idOfSystemToMergeTo,
        idOfSystemToBeMerged.systemId);
    if (resultCode != api::ResultCode::ok)
        return completionHandler(resultCode);

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

api::ResultCode SystemMergeManager::validateRequestInput(
    const std::string& idOfSystemToMergeTo,
    const std::string& idOfSystemToBeMerged)
{
    if (!m_systemHealthInfoProvider.isSystemOnline(idOfSystemToMergeTo))
    {
        NX_DEBUG(this, lm("Cannot merge system %1 to offline system %2")
            .args(idOfSystemToBeMerged, idOfSystemToMergeTo));
        return api::ResultCode::mergedSystemIsOffline;
    }

    if (!m_systemHealthInfoProvider.isSystemOnline(idOfSystemToBeMerged))
    {
        NX_DEBUG(this, lm("Cannot merge offline system %1 to %2")
            .args(idOfSystemToBeMerged, idOfSystemToMergeTo));
        return api::ResultCode::mergedSystemIsOffline;
    }

    const auto systemToMergeTo = m_systemManager->findSystemById(idOfSystemToMergeTo);
    if (!systemToMergeTo)
        return api::ResultCode::notFound;

    const auto systemToBeMerged = m_systemManager->findSystemById(idOfSystemToBeMerged);
    if (!systemToBeMerged)
        return api::ResultCode::notFound;
    
    if (systemToMergeTo->ownerAccountEmail != systemToBeMerged->ownerAccountEmail)
    {
        NX_DEBUG(this, lm("Cannot merge system %1 (owner %2) to system %3 (owner %4). "
            "Both systems MUST have same owner")
            .args(idOfSystemToBeMerged, systemToBeMerged->ownerAccountEmail,
                idOfSystemToMergeTo, systemToMergeTo->ownerAccountEmail));
        return api::ResultCode::forbidden;
    }

    return api::ResultCode::ok;
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
