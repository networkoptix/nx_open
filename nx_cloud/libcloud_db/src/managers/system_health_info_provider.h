#pragma once

#include <string>

#include <cdb/result_code.h>

#include <utils/common/counter.h>
#include <utils/common/subscription.h>
#include <utils/db/async_sql_query_executor.h>

#include "access_control/auth_types.h"
#include "dao/rdb/system_health_history_data_object.h"
#include "data/system_data.h"

namespace nx {
namespace cdb {

namespace ec2 { class ConnectionManager; }

/**
 * Aggregates system health information from different sources.
 */
class SystemHealthInfoProvider
{
public:
    SystemHealthInfoProvider(
        ec2::ConnectionManager* ec2ConnectionManager,
        nx::db::AsyncSqlQueryExecutor* const dbManager);
    ~SystemHealthInfoProvider();

    bool isSystemOnline(const std::string& systemId) const;

    void getSystemHealthHistory(
        const AuthorizationInfo& authzInfo,
        data::SystemId systemId,
        std::function<void(api::ResultCode, api::SystemHealthHistory)> completionHandler);

private:
    ec2::ConnectionManager* m_ec2ConnectionManager;
    nx::db::AsyncSqlQueryExecutor* const m_dbManager;
    QnCounter m_startedAsyncCallsCounter;
    dao::rdb::SystemHealthHistoryDataObject m_systemHealthHistoryDataObject;
    nx::utils::SubscriptionId m_systemStatusChangedSubscriptionId;

    void onSystemStatusChanged(std::string systemId, api::SystemHealth systemHealth);
};

} // namespace cdb
} // namespace nx
