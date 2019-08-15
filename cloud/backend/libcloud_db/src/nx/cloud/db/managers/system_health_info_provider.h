#pragma once

#include <string>

#include <nx/cloud/db/api/result_code.h>

#include <nx/utils/basic_factory.h>
#include <nx/utils/counter.h>
#include <nx/sql/async_sql_query_executor.h>
#include <nx/utils/subscription.h>

#include <nx/clusterdb/engine/connection_manager.h>

#include "../access_control/auth_types.h"
#include "../dao/rdb/system_health_history_data_object.h"
#include "../data/system_data.h"
#include "../settings.h"

namespace nx::clusterdb::engine { class ConnectionManager; }

namespace nx::cloud::db {

class AbstractSystemHealthInfoProvider
{
public:
    virtual ~AbstractSystemHealthInfoProvider() = default;

    virtual bool isSystemOnline(const std::string& systemId) const = 0;

    virtual void getSystemHealthHistory(
        const AuthorizationInfo& authzInfo,
        data::SystemId systemId,
        std::function<void(api::Result, api::SystemHealthHistory)> completionHandler) = 0;
};

/**
 * Aggregates system health information from different sources.
 */
class SystemHealthInfoProvider:
    public AbstractSystemHealthInfoProvider
{
public:
    SystemHealthInfoProvider(
        const conf::Settings& settings,
        clusterdb::engine::ConnectionManager* ec2ConnectionManager,
        nx::sql::AbstractAsyncSqlQueryExecutor* const dbManager);
    virtual ~SystemHealthInfoProvider() override;

    virtual bool isSystemOnline(const std::string& systemId) const override;

    virtual void getSystemHealthHistory(
        const AuthorizationInfo& authzInfo,
        data::SystemId systemId,
        std::function<void(api::Result, api::SystemHealthHistory)> completionHandler) override;

private:
    const conf::Settings& m_settings;
    clusterdb::engine::ConnectionManager* m_ec2ConnectionManager;
    nx::sql::AbstractAsyncSqlQueryExecutor* const m_dbManager;
    nx::utils::Counter m_startedAsyncCallsCounter;
    dao::rdb::SystemHealthHistoryDataObject m_systemHealthHistoryDataObject;
    nx::utils::SubscriptionId m_systemStatusChangedSubscriptionId;

    void onSystemStatusChanged(
        const std::string& systemId,
        clusterdb::engine::NodeStatusDescriptor statusDescription);
};

//-------------------------------------------------------------------------------------------------

using SystemHealthInfoProviderFactoryFunction =
    std::unique_ptr<AbstractSystemHealthInfoProvider>(
        const conf::Settings& settings,
        clusterdb::engine::ConnectionManager* ec2ConnectionManager,
        nx::sql::AbstractAsyncSqlQueryExecutor* const dbManager);

class SystemHealthInfoProviderFactory:
    public nx::utils::BasicFactory<SystemHealthInfoProviderFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<SystemHealthInfoProviderFactoryFunction>;

public:
    SystemHealthInfoProviderFactory();

    static SystemHealthInfoProviderFactory& instance();

private:
    std::unique_ptr<AbstractSystemHealthInfoProvider> defaultFactory(
        const conf::Settings& settings,
        clusterdb::engine::ConnectionManager* ec2ConnectionManager,
        nx::sql::AbstractAsyncSqlQueryExecutor* const dbManager);
};

} // namespace nx::cloud::db
