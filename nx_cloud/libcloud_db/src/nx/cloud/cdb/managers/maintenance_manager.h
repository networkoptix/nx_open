#pragma once

#include <functional>

#include <nx/cloud/cdb/client/data/maintenance_data.h>
#include <nx_ec/data/api_fwd.h>
#include <nx/network/aio/timer.h>
#include <nx/utils/counter.h>
#include <nx/utils/db/db_instance_controller.h>

#include <nx/data_sync_engine/serialization/transaction_serializer.h>
#include <nx/data_sync_engine/transaction_log.h>

#include "../data/statistics_data.h"
#include "../data/system_data.h"

namespace nx { namespace data_sync_engine { class SyncronizationEngine; } }

namespace nx {
namespace cdb {

class AuthorizationInfo;

class MaintenanceManager
{
public:
    MaintenanceManager(
        const QnUuid& moduleGuid,
        data_sync_engine::SyncronizationEngine* const syncronizationEngine,
        const nx::utils::db::InstanceController& dbInstanceController);
    ~MaintenanceManager();

    void getVmsConnections(
        const AuthorizationInfo& authzInfo,
        std::function<void(
            api::ResultCode,
            api::VmsConnectionDataList)> completionHandler);
    /**
     * @return vms transaction log in the same format as \a /ec2/getTransactionLog request.
     */
    void getTransactionLog(
        const AuthorizationInfo& authzInfo,
        data::SystemId systemId,
        std::function<void(
            api::ResultCode,
            ::ec2::ApiTransactionDataList)> completionHandler);

    void getStatistics(
        const AuthorizationInfo& authzInfo,
        std::function<void(api::ResultCode, data::Statistics)> completionHandler);

private:
    const QnUuid m_moduleGuid;
    data_sync_engine::SyncronizationEngine* const m_syncronizationEngine;
    const nx::utils::db::InstanceController& m_dbInstanceController;
    nx::network::aio::Timer m_timer;
    nx::utils::Counter m_startedAsyncCallsCounter;

    void onTransactionLogRead(
        nx::utils::Counter::ScopedIncrement /*asyncCallLocker*/,
        const std::string& systemId,
        data_sync_engine::ResultCode resultCode,
        std::vector<data_sync_engine::dao::TransactionLogRecord> serializedTransactions,
        vms::api::TranState readedUpTo,
        std::function<void(
            api::ResultCode,
            ::ec2::ApiTransactionDataList)> completionHandler);
};

} // namespace cdb
} // namespace nx
