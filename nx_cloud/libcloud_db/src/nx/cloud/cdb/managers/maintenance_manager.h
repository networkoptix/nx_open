#pragma once

#include <functional>

#include <nx/cloud/cdb/client/data/maintenance_data.h>
#include <nx_ec/data/api_fwd.h>
#include <nx/network/aio/timer.h>
#include <nx/utils/counter.h>
#include <utils/db/db_instance_controller.h>

#include "../data/statistics_data.h"
#include "../data/system_data.h"
#include "../ec2/serialization/transaction_serializer.h"
#include "../ec2/transaction_log.h"

namespace nx {
namespace cdb {

class AuthorizationInfo;

namespace ec2 {

class SyncronizationEngine;

} // namespace ec2

class MaintenanceManager
{
public:
    MaintenanceManager(
        const QnUuid& moduleGuid,
        ec2::SyncronizationEngine* const syncronizationEngine,
        const db::InstanceController& dbInstanceController);
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
    ec2::SyncronizationEngine* const m_syncronizationEngine;
    const db::InstanceController& m_dbInstanceController;
    nx::network::aio::Timer m_timer;
    nx::utils::Counter m_startedAsyncCallsCounter;

    void onTransactionLogRead(
        nx::utils::Counter::ScopedIncrement /*asyncCallLocker*/,
        const std::string& systemId,
        api::ResultCode resultCode,
        std::vector<ec2::dao::TransactionLogRecord> serializedTransactions,
        ::ec2::QnTranState readedUpTo,
        std::function<void(
            api::ResultCode,
            ::ec2::ApiTransactionDataList)> completionHandler);
};

} // namespace cdb
} // namespace nx
