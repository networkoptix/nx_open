#include "system_merge_manager.h"

#include <nx/fusion/serialization/lexical.h>
#include <nx/utils/log/log.h>
#include <nx/utils/time.h>

#include <nx/cloud/cdb/client/data/types.h>

#include "../settings.h"
#include "../stree/cdb_ns.h"
#include "system_health_info_provider.h"

namespace nx {
namespace cdb {

SystemMergeManager::SystemMergeManager(
    AbstractSystemManager* systemManager,
    const AbstractSystemHealthInfoProvider& systemHealthInfoProvider,
    AbstractVmsGateway* vmsGateway,
    nx::utils::db::AsyncSqlQueryExecutor* queryExecutor)
    :
    m_systemManager(systemManager),
    m_systemHealthInfoProvider(systemHealthInfoProvider),
    m_vmsGateway(vmsGateway),
    m_queryExecutor(queryExecutor),
    m_dao(dao::SystemMergeDaoFactory::instance().create(queryExecutor))
{
    loadSystemMergeInfo();

    m_systemManager->addExtension(this);
}

SystemMergeManager::~SystemMergeManager()
{
    m_runningRequestCounter.wait();
    m_systemManager->removeExtension(this);
}

void SystemMergeManager::startMergingSystems(
    const AuthorizationInfo& authzInfo,
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
    issueVmsMergeRequest(authzInfo, mergeRequestContextPtr);
}

void SystemMergeManager::processMergeHistoryRecord(
    const ::ec2::ApiSystemMergeHistoryRecord& mergeHistoryRecord,
    std::function<void(api::ResultCode)> completionHandler)
{
    m_queryExecutor->executeUpdate(
        [this, mergeHistoryRecord](
            utils::db::QueryContext* queryContext)
        {
            processMergeHistoryRecord(queryContext, mergeHistoryRecord);
            return utils::db::DBResult::ok;
        },
        [completionHandler = std::move(completionHandler)](
            nx::utils::db::QueryContext* /*queryContext*/,
            utils::db::DBResult dbResultCode)
        {
            completionHandler(dbResultToApiResult(dbResultCode));
        });
}

void SystemMergeManager::processMergeHistoryRecord(
    nx::utils::db::QueryContext* queryContext,
    const ::ec2::ApiSystemMergeHistoryRecord& mergeHistoryRecord)
{
    NX_DEBUG(this, lm("Received notification that system %1 merge is complete. "
        "Marking system for deletion").args(mergeHistoryRecord.mergedSystemCloudId));

    data::SystemData system;
    auto resultCode = m_systemManager->fetchSystemById(
        queryContext,
        mergeHistoryRecord.mergedSystemCloudId.toStdString(),
        &system);
    if (resultCode == nx::utils::db::DBResult::notFound)
    {
        NX_INFO(this, lm("Ignoring merge history record for unknown system %1...")
            .args(mergeHistoryRecord.mergedSystemCloudId));
        return;
    }

    if (resultCode != nx::utils::db::DBResult::ok)
        throw nx::utils::db::Exception(resultCode);

    if (!verifyMergeHistoryRecord(mergeHistoryRecord, system))
        return;

    updateCompletedMergeData(queryContext, mergeHistoryRecord);
}

bool SystemMergeManager::verifyMergeHistoryRecord(
    const ::ec2::ApiSystemMergeHistoryRecord& mergeHistoryRecord,
    const data::SystemData& system)
{
    if (!mergeHistoryRecord.verify(system.authKey.c_str()))
    {
        NX_WARNING(this, lm("Could not verify merge history record for system %1. Ignoring...")
            .args(system.id));
        return false;
    }

    if (system.status != api::SystemStatus::beingMerged)
    {
        NX_DEBUG(this, lm("Received merge history record for system %1 "
            "that is in unexpected state %2. Proceeding...")
            .args(system.id, QnLexical::serialized(system.status)));
    }

    return true;
}

void SystemMergeManager::updateCompletedMergeData(
    nx::utils::db::QueryContext* queryContext,
    const ::ec2::ApiSystemMergeHistoryRecord& mergeHistoryRecord)
{
    using namespace std::placeholders;

    NX_DEBUG(this, lm("Cleaning up after completed merge. Slave system %1")
        .args(mergeHistoryRecord.mergedSystemCloudId));

    m_dao->removeMergeBySlaveSystemId(
        queryContext,
        mergeHistoryRecord.mergedSystemCloudId.toStdString());

    auto resultCode = m_systemManager->markSystemForDeletion(
        queryContext, mergeHistoryRecord.mergedSystemCloudId.toStdString());
    if (resultCode != nx::utils::db::DBResult::ok)
        throw nx::utils::db::Exception(resultCode);

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        std::bind(&SystemMergeManager::removeMergeInfoFromCache, this, mergeHistoryRecord));
}

void SystemMergeManager::removeMergeInfoFromCache(
    const ::ec2::ApiSystemMergeHistoryRecord& mergeHistoryRecord)
{
    QnMutexLocker lock(&m_mutex);

    auto slaveSystemInfoIter =
        m_mergedSystems.find(mergeHistoryRecord.mergedSystemCloudId.toStdString());
    if (slaveSystemInfoIter == m_mergedSystems.end())
        return;

    auto masterSystemInfoIter =
        m_mergedSystems.find(slaveSystemInfoIter->second.anotherSystemId);
    if (masterSystemInfoIter != m_mergedSystems.end())
        m_mergedSystems.erase(masterSystemInfoIter);

    m_mergedSystems.erase(slaveSystemInfoIter);
}

void SystemMergeManager::modifySystemBeforeProviding(
    api::SystemDataEx* system)
{
    QnMutexLocker lock(&m_mutex);
    auto it = m_mergedSystems.find(system->id);
    if (it != m_mergedSystems.end())
        system->mergeInfo = it->second;
}

void SystemMergeManager::loadSystemMergeInfo()
{
    using namespace std::placeholders;

    NX_DEBUG(this, lm("Loading ongoing merges"));

    std::vector<dao::MergeInfo> ongoingMerges =
        m_queryExecutor->executeSelectQuerySync(
            std::bind(&dao::AbstractSystemMergeDao::fetchAll, m_dao.get(), _1));

    for (const auto& mergeInfo: ongoingMerges)
    {
        m_mergedSystems.emplace(mergeInfo.masterSystemId, mergeInfo.masterSystemInfo());
        m_mergedSystems.emplace(mergeInfo.slaveSystemId, mergeInfo.slaveSystemInfo());
    }

    NX_DEBUG(this, lm("Loaded %1 ongoing merges").args(ongoingMerges.size()));
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

void SystemMergeManager::issueVmsMergeRequest(
    const AuthorizationInfo& authzInfo,
    MergeRequestContext* mergeRequestContext)
{
    using namespace std::placeholders;

    NX_VERBOSE(this, lm("Merge %1 into %2. Issuing request to system %1")
        .args(mergeRequestContext->idOfSystemToBeMerged,
            mergeRequestContext->idOfSystemToMergeTo));

    const auto username = authzInfo.get<std::string>(attr::authAccountEmail);

    m_vmsGateway->merge(
        username ? *username : std::string(),
        mergeRequestContext->idOfSystemToBeMerged,
        mergeRequestContext->idOfSystemToMergeTo,
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
        NX_VERBOSE(this, lm("Merge %1 into %2. Request to system %1 failed with result %3")
            .args(mergeRequestContext->idOfSystemToBeMerged,
                mergeRequestContext->idOfSystemToMergeTo,
                QnLexical::serialized(vmsRequestResult.resultCode)));
        finishMerge(mergeRequestContext, api::ResultCode::vmsRequestFailure);
        return;
    }

    m_queryExecutor->executeUpdate(
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
    const std::string& idOfSystemToMergeTo,
    const std::string& idOfSystemToMergeBeMerged)
{
    dao::MergeInfo mergeInfo;
    mergeInfo.masterSystemId = idOfSystemToMergeTo;
    mergeInfo.slaveSystemId = idOfSystemToMergeBeMerged;
    mergeInfo.startTime = nx::utils::utcTime();
    m_dao->save(queryContext, mergeInfo);

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        std::bind(&SystemMergeManager::saveSystemMergeInfoToCache, this, mergeInfo));

    return m_systemManager->updateSystemStatus(
        queryContext,
        idOfSystemToMergeBeMerged,
        api::SystemStatus::beingMerged);
}

void SystemMergeManager::saveSystemMergeInfoToCache(const dao::MergeInfo& mergeInfo)
{
    NX_VERBOSE(this, lm("Merge %1 into %2. Updating cache")
        .args(mergeInfo.slaveSystemId, mergeInfo.masterSystemId));

    QnMutexLocker lock(&m_mutex);
    m_mergedSystems[mergeInfo.masterSystemId] = mergeInfo.masterSystemInfo();
    m_mergedSystems[mergeInfo.slaveSystemId] = mergeInfo.slaveSystemInfo();
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
