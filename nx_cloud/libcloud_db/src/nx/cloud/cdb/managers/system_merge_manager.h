#pragma once

#include <functional>
#include <set>
#include <string>

#include <nx/utils/db/async_sql_query_executor.h>
#include <nx/utils/counter.h>

#include <nx/cloud/cdb/api/result_code.h>

#include "managers_types.h"
#include "vms_gateway.h"
#include "../access_control/auth_types.h"
#include "../data/system_data.h"

namespace nx {
namespace cdb {

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
        std::function<void(api::ResultCode)> completionHandler) = 0;
};

class SystemMergeManager:
    public AbstractSystemMergeManager
{
public:
    SystemMergeManager(
        AbstractSystemManager* systemManager,
        const AbstractSystemHealthInfoProvider& systemHealthInfoProvider,
        AbstractVmsGateway* vmsGateway,
        nx::utils::db::AsyncSqlQueryExecutor* dbManager);
    virtual ~SystemMergeManager() override;

    virtual void startMergingSystems(
        const AuthorizationInfo& authzInfo,
        const std::string& idOfSystemToMergeTo,
        const std::string& idOfSystemToBeMerged,
        std::function<void(api::ResultCode)> completionHandler) override;

private:
    struct MergeRequestContext
    {
        std::string idOfSystemToMergeTo;
        std::string idOfSystemToBeMerged;
        std::function<void(api::ResultCode)> completionHandler;
        nx::utils::Counter::ScopedIncrement callLock;
    };

    AbstractSystemManager* m_systemManager = nullptr;
    const AbstractSystemHealthInfoProvider& m_systemHealthInfoProvider;
    AbstractVmsGateway* m_vmsGateway;
    nx::utils::db::AsyncSqlQueryExecutor* m_dbManager = nullptr;
    QnMutex m_mutex;
    // TODO: #ak Replace with std::set when c++17 is supported.
    std::map<MergeRequestContext*, std::unique_ptr<MergeRequestContext>> m_currentRequests;
    nx::utils::Counter m_runningRequestCounter;

    api::ResultCode validateRequestInput(
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
        nx::utils::db::QueryContext* queryContext,
        nx::utils::db::DBResult dbResult);

    nx::utils::db::DBResult updateSystemStateInDb(
        nx::utils::db::QueryContext* queryContext,
        const std::string& idOfSystemToMergeTo,
        const std::string& idOfSystemToMergeBeMerged);

    void finishMerge(
        MergeRequestContext* mergeRequestContextPtr,
        api::ResultCode resultCode);
};

} // namespace cdb
} // namespace nx
