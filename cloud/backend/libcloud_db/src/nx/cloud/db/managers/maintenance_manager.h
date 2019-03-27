#pragma once

#include <functional>

#include <nx/cloud/db/client/data/maintenance_data.h>
#include <nx_ec/data/api_fwd.h>
#include <nx/network/aio/timer.h>
#include <nx/utils/counter.h>
#include <nx/sql/db_instance_controller.h>

#include <nx/clusterdb/engine/serialization/transaction_serializer.h>
#include <nx/clusterdb/engine/transaction_log.h>

#include "../data/statistics_data.h"
#include "../data/system_data.h"

namespace nx::clusterdb::engine { class SynchronizationEngine; }

namespace nx::cloud::db {

class AuthorizationInfo;

class MaintenanceManager
{
public:
    MaintenanceManager(
        clusterdb::engine::SynchronizationEngine* const synchronizationEngine,
        const nx::sql::InstanceController& dbInstanceController);
    ~MaintenanceManager();

    void getVmsConnections(
        const AuthorizationInfo& authzInfo,
        std::function<void(
            api::Result,
            api::VmsConnectionDataList)> completionHandler);
    /**
     * @return vms transaction log in the same format as \a /ec2/getTransactionLog request.
     */
    void getTransactionLog(
        const AuthorizationInfo& authzInfo,
        data::SystemId systemId,
        std::function<void(
            api::Result,
            ::ec2::ApiTransactionDataList)> completionHandler);

    void getStatistics(
        const AuthorizationInfo& authzInfo,
        std::function<void(api::Result, data::Statistics)> completionHandler);

private:
    const QnUuid m_moduleGuid;
    clusterdb::engine::SynchronizationEngine* const m_synchronizationEngine;
    const nx::sql::InstanceController& m_dbInstanceController;
    nx::network::aio::Timer m_timer;
    nx::utils::Counter m_startedAsyncCallsCounter;

    void onTransactionLogRead(
        nx::utils::Counter::ScopedIncrement /*asyncCallLocker*/,
        const std::string& systemId,
        clusterdb::engine::ResultCode resultCode,
        std::vector<clusterdb::engine::dao::TransactionLogRecord> serializedTransactions,
        vms::api::TranState readedUpTo,
        std::function<void(
            api::Result,
            ::ec2::ApiTransactionDataList)> completionHandler);
};

} // namespace nx::cloud::db
