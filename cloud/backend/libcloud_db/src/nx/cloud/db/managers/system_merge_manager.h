#pragma once

#include <functional>
#include <set>
#include <string>

#include <nx/sql/async_sql_query_executor.h>
#include <nx/utils/counter.h>

#include <nx/cloud/db/api/result_code.h>
#include <nx/cloud/db/api/system_data.h>
#include <nx/vms/api/data/system_merge_history_record.h>

#include "managers_types.h"
#include "system_manager.h"
#include "vms_gateway.h"
#include "../access_control/auth_types.h"
#include "../dao/abstract_system_merge_dao.h"
#include "../data/system_data.h"

namespace nx::cloud::db {

class AbstractSystemManager;
class AbstractSystemHealthInfoProvider;
class AbstractVmsGateway;

class AbstractSystemMergeManager
{
public:
    virtual ~AbstractSystemMergeManager() = default;

    virtual void startMergingSystems(
        const AuthorizationInfo& authzInfo,
        const std::string& idOfSystemToMergeTo,
        const std::string& idOfSystemToBeMerged,
        std::function<void(api::Result)> completionHandler) = 0;

    virtual void processMergeHistoryRecord(
        const nx::vms::api::SystemMergeHistoryRecord& mergeHistoryRecord,
        std::function<void(api::Result)> completionHandler) = 0;

    virtual void processMergeHistoryRecord(
        nx::sql::QueryContext* queryContext,
        const nx::vms::api::SystemMergeHistoryRecord& mergeHistoryRecord) = 0;
};

class SystemMergeManager:
    public AbstractSystemMergeManager,
    public AbstractSystemExtension
{
public:
    SystemMergeManager(
        AbstractSystemManager* systemManager,
        const AbstractSystemHealthInfoProvider& systemHealthInfoProvider,
        AbstractVmsGateway* vmsGateway,
        nx::sql::AsyncSqlQueryExecutor* queryExecutor);
    virtual ~SystemMergeManager() override;

    virtual void startMergingSystems(
        const AuthorizationInfo& authzInfo,
        const std::string& idOfSystemToMergeTo,
        const std::string& idOfSystemToBeMerged,
        std::function<void(api::Result)> completionHandler) override;

    virtual void processMergeHistoryRecord(
        const nx::vms::api::SystemMergeHistoryRecord& mergeHistoryRecord,
        std::function<void(api::Result)> completionHandler) override;

    virtual void processMergeHistoryRecord(
        nx::sql::QueryContext* queryContext,
        const nx::vms::api::SystemMergeHistoryRecord& mergeHistoryRecord) override;

    virtual void modifySystemBeforeProviding(api::SystemDataEx* system) override;

private:
    struct MergeRequestContext
    {
        std::string idOfSystemToMergeTo;
        std::string idOfSystemToBeMerged;
        std::function<void(api::Result)> completionHandler;
        nx::utils::Counter::ScopedIncrement callLock;
    };

    AbstractSystemManager* m_systemManager = nullptr;
    const AbstractSystemHealthInfoProvider& m_systemHealthInfoProvider;
    AbstractVmsGateway* m_vmsGateway;
    nx::sql::AsyncSqlQueryExecutor* m_queryExecutor = nullptr;
    QnMutex m_mutex;
    // TODO: #ak Replace with std::set when c++17 is supported.
    std::map<MergeRequestContext*, std::unique_ptr<MergeRequestContext>> m_currentRequests;
    nx::utils::Counter m_runningRequestCounter;
    // map<system id, merge info>.
    std::map<std::string, api::SystemMergeInfo> m_mergedSystems;
    std::unique_ptr<dao::AbstractSystemMergeDao> m_dao;

    void loadSystemMergeInfo();

    api::Result validateRequestInput(
        const std::string& idOfSystemToMergeTo,
        const std::string& idOfSystemToBeMerged);

    void issueVmsMergeRequest(
        const AuthorizationInfo& authzInfo,
        MergeRequestContext* mergeRequestContext);

    void processVmsMergeRequestResult(
        MergeRequestContext* mergeRequestContext,
        VmsRequestResult vmsRequestResult);

    void processUpdateSystemResult(
        MergeRequestContext* mergeRequestContext,
        nx::sql::DBResult dbResult);

    nx::sql::DBResult updateSystemStateInDb(
        nx::sql::QueryContext* queryContext,
        const std::string& idOfSystemToMergeTo,
        const std::string& idOfSystemToMergeBeMerged);

    void saveSystemMergeInfoToCache(const dao::MergeInfo& mergeInfo);

    bool verifyMergeHistoryRecord(
        const nx::vms::api::SystemMergeHistoryRecord& mergeHistoryRecord,
        const data::SystemData& system);

    void updateCompletedMergeData(
        nx::sql::QueryContext* queryContext,
        const nx::vms::api::SystemMergeHistoryRecord& mergeHistoryRecord);

    void removeMergeInfoFromCache(
        const nx::vms::api::SystemMergeHistoryRecord& mergeHistoryRecord);

    void finishMerge(
        MergeRequestContext* mergeRequestContextPtr,
        api::Result result);
};

} // namespace nx::cloud::db
