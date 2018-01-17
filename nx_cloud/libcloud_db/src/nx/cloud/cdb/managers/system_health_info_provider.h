#pragma once

#include <string>

#include <nx/cloud/cdb/api/result_code.h>

#include <nx/utils/basic_factory.h>
#include <nx/utils/counter.h>
#include <nx/utils/db/async_sql_query_executor.h>
#include <nx/utils/subscription.h>

#include "../access_control/auth_types.h"
#include "../dao/rdb/system_health_history_data_object.h"
#include "../data/system_data.h"

namespace nx {
namespace cdb {

namespace ec2 { class ConnectionManager; }

class AbstractSystemHealthInfoProvider
{
public:
    virtual ~AbstractSystemHealthInfoProvider() = default;

    virtual bool isSystemOnline(const std::string& systemId) const = 0;

    virtual void getSystemHealthHistory(
        const AuthorizationInfo& authzInfo,
        data::SystemId systemId,
        std::function<void(api::ResultCode, api::SystemHealthHistory)> completionHandler) = 0;
};

/**
 * Aggregates system health information from different sources.
 */
class SystemHealthInfoProvider:
    public AbstractSystemHealthInfoProvider
{
public:
    SystemHealthInfoProvider(
        ec2::ConnectionManager* ec2ConnectionManager,
        nx::utils::db::AsyncSqlQueryExecutor* const dbManager);
    virtual ~SystemHealthInfoProvider() override;

    virtual bool isSystemOnline(const std::string& systemId) const override;

    virtual void getSystemHealthHistory(
        const AuthorizationInfo& authzInfo,
        data::SystemId systemId,
        std::function<void(api::ResultCode, api::SystemHealthHistory)> completionHandler) override;

private:
    ec2::ConnectionManager* m_ec2ConnectionManager;
    nx::utils::db::AsyncSqlQueryExecutor* const m_dbManager;
    nx::utils::Counter m_startedAsyncCallsCounter;
    dao::rdb::SystemHealthHistoryDataObject m_systemHealthHistoryDataObject;
    nx::utils::SubscriptionId m_systemStatusChangedSubscriptionId;

    void onSystemStatusChanged(std::string systemId, api::SystemHealth systemHealth);
};

//-------------------------------------------------------------------------------------------------

using SystemHealthInfoProviderFactoryFunction =
    std::unique_ptr<AbstractSystemHealthInfoProvider>(
        ec2::ConnectionManager* ec2ConnectionManager,
        nx::utils::db::AsyncSqlQueryExecutor* const dbManager);

class SystemHealthInfoProviderFactory:
    public nx::utils::BasicFactory<SystemHealthInfoProviderFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<SystemHealthInfoProviderFactoryFunction>;

public:
    SystemHealthInfoProviderFactory();

    static SystemHealthInfoProviderFactory& instance();

private:
    std::unique_ptr<AbstractSystemHealthInfoProvider> defaultFactory(
        ec2::ConnectionManager* ec2ConnectionManager,
        nx::utils::db::AsyncSqlQueryExecutor* const dbManager);
};

} // namespace cdb
} // namespace nx
