#pragma once

#include <functional>
#include <string>

#include <nx/cloud/cdb/api/result_code.h>
#include <nx/utils/db/async_sql_query_executor.h>

#include "managers_types.h"
#include "../access_control/auth_types.h"
#include "../data/system_data.h"

namespace nx {
namespace cdb {

class AbstractSystemManager;

class AbstractSystemMergeManager
{
public:
    virtual ~AbstractSystemMergeManager() = default;

    virtual void startMergingSystems(
        const AuthorizationInfo& authzInfo,
        const std::string& idOfSystemToMergeTo,
        data::SystemId idOfSystemToBeMerged,
        std::function<void(api::ResultCode)> completionHandler) = 0;
};

class SystemMergeManager:
    public AbstractSystemMergeManager
{
public:
    SystemMergeManager(
        AbstractSystemManager* systemManager,
        nx::utils::db::AsyncSqlQueryExecutor* dbManager);

    virtual void startMergingSystems(
        const AuthorizationInfo& authzInfo,
        const std::string& idOfSystemToMergeTo,
        data::SystemId idOfSystemToBeMerged,
        std::function<void(api::ResultCode)> completionHandler) override;

private:
    AbstractSystemManager* m_systemManager = nullptr;
    nx::utils::db::AsyncSqlQueryExecutor* m_dbManager = nullptr;
};

} // namespace cdb
} // namespace nx
