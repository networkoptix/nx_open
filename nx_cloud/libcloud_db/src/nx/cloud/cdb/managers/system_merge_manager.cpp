#include "system_merge_manager.h"

#include <nx/fusion/serialization/lexical.h>
#include <nx/utils/log/log.h>

#include <nx/cloud/cdb/client/data/types.h>

#include "../settings.h"
#include "system_health_info_provider.h"
#include "system_manager.h"

namespace nx {
namespace cdb {

SystemMergeManager::SystemMergeManager(
    AbstractSystemManager* systemManager,
    const AbstractSystemHealthInfoProvider& systemHealthInfoProvider,
    AbstractVmsGateway* vmsGateway,
    nx::utils::db::AsyncSqlQueryExecutor* dbManager)
    :
    m_systemManager(systemManager),
    m_systemHealthInfoProvider(systemHealthInfoProvider),
    m_vmsGateway(vmsGateway),
    m_dbManager(dbManager)
{
}

SystemMergeManager::~SystemMergeManager()
{
    m_runningRequestCounter.wait();
}

void SystemMergeManager::startMergingSystems(
    const AuthorizationInfo& /*authzInfo*/,
    const std::string& idOfSystemToMergeTo,
    const std::string& idOfSystemToBeMerged,
    std::function<void(api::ResultCode)> completionHandler)
{
    NX_VERBOSE(this, lm("Requested merge of system %1 into %2")
        .args(idOfSystemToBeMerged, idOfSystemToMergeTo));

    const auto resultCode = validateRequestInput(
        idOfSystemToMergeTo,
        idOfSystemToBeMerged);
    if (resultCode != api::ResultCode::ok)
    {
        NX_DEBUG(this, lm("Merge system %1 into %2 request failed input check. %3")
            .args(idOfSystemToBeMerged, idOfSystemToMergeTo, QnLexical::serialized(resultCode)));
        return completionHandler(resultCode);
    }

    auto mergeRequestContext = std::make_unique<MergeRequestContext>();
    mergeRequestContext->idOfSystemToMergeTo = idOfSystemToMergeTo;
    mergeRequestContext->idOfSystemToBeMerged = idOfSystemToBeMerged;
    mergeRequestContext->completionHandler = std::move(completionHandler);
    mergeRequestContext->callLock = m_runningRequestCounter.getScopedIncrement();

    auto mergeRequestContextPtr = mergeRequestContext.get();

    QnMutexLocker lock(&m_mutex);
    m_currentRequests.emplace(mergeRequestContextPtr, std::move(mergeRequestContext));
    start(mergeRequestContextPtr);
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

void SystemMergeManager::start(MergeRequestContext* mergeRequestContext)
{
    using namespace std::placeholders;

    NX_VERBOSE(this, lm("Merge %1 into %2. Issuing request to system %1")
        .args(mergeRequestContext->idOfSystemToBeMerged, 
            mergeRequestContext->idOfSystemToMergeTo));

    m_vmsGateway->merge(
        std::string(), // TODO: Pass username.
        mergeRequestContext->idOfSystemToBeMerged,
        std::bind(&SystemMergeManager::processVmsMergeRequestResult, this, 
            mergeRequestContext, _1));
}

void SystemMergeManager::processVmsMergeRequestResult(
    MergeRequestContext* mergeRequestContext,
    VmsRequestResult vmsRequestResult)
{
    using namespace std::placeholders;

    NX_VERBOSE(this, lm("Merge %1 into %2. Request to system %1 completed")
        .args(mergeRequestContext->idOfSystemToBeMerged,
            mergeRequestContext->idOfSystemToMergeTo));

    if (vmsRequestResult.resultCode != VmsResultCode::ok)
    {
        finishMerge(mergeRequestContext, api::ResultCode::vmsRequestFailure);
        return;
    }

    m_dbManager->executeUpdate(
        std::bind(&SystemMergeManager::updateSystemStateInDb, this, _1,
            mergeRequestContext->idOfSystemToMergeTo, 
            mergeRequestContext->idOfSystemToBeMerged),
        std::bind(&SystemMergeManager::processUpdateSystemResult, this, 
            mergeRequestContext, _1, _2));
}

void SystemMergeManager::processUpdateSystemResult(
    MergeRequestContext* mergeRequestContextPtr,
    nx::utils::db::QueryContext* /*queryContext*/,
    nx::utils::db::DBResult dbResult)
{
    NX_VERBOSE(this, lm("Merge %1 into %2. Updating system %1 status completed with result %3")
        .args(mergeRequestContextPtr->idOfSystemToBeMerged,
            mergeRequestContextPtr->idOfSystemToMergeTo,
            nx::utils::db::toString(dbResult)));

    finishMerge(mergeRequestContextPtr, dbResultToApiResult(dbResult));
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

void SystemMergeManager::finishMerge(
    MergeRequestContext* mergeRequestContextPtr,
    api::ResultCode resultCode)
{
    NX_VERBOSE(this, lm("Merge %1 into %2. Reporting %3")
        .args(mergeRequestContextPtr->idOfSystemToBeMerged,
            mergeRequestContextPtr->idOfSystemToMergeTo,
            QnLexical::serialized(resultCode)));
        
    std::unique_ptr<MergeRequestContext> mergeRequestContext;
    {
        QnMutexLocker lock(&m_mutex);
        const auto it = m_currentRequests.find(mergeRequestContextPtr);
        NX_CRITICAL(it != m_currentRequests.end());
        mergeRequestContext.swap(it->second);
    }

    mergeRequestContext->completionHandler(resultCode);
}

} // namespace cdb
} // namespace nx
