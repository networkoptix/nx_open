#pragma once

#include <functional>

#include <cloud_db_client/src/data/maintenance_data.h>
#include <nx_ec/data/api_fwd.h>
#include <nx/network/aio/timer.h>
#include <utils/common/counter.h>

#include "data/system_data.h"
#include "ec2/transaction_log.h"
#include "ec2/transaction_serializer.h"

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
        ec2::SyncronizationEngine* const syncronizationEngine);
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
        data::SystemID systemID,
        std::function<void(
            api::ResultCode,
            ::ec2::ApiTransactionDataList)> completionHandler);

private:
    const QnUuid m_moduleGuid;
    ec2::SyncronizationEngine* const m_syncronizationEngine;
    nx::network::aio::Timer m_timer;
    QnCounter m_startedAsyncCallsCounter;

    void onTransactionLogRead(
        QnCounter::ScopedIncrement /*asyncCallLocker*/,
        const std::string& systemId,
        api::ResultCode resultCode,
        std::vector<ec2::TransactionData> serializedTransactions,
        ::ec2::QnTranState readedUpTo,
        std::function<void(
            api::ResultCode,
            ::ec2::ApiTransactionDataList)> completionHandler);
};

} // namespace cdb
} // namespace nx
