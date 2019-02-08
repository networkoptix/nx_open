#include "maintenance_manager.h"

#include <nx/utils/log/log.h>

#include <nx/cloud/db/managers/managers_types.h>
#include <nx/clusterdb/engine/synchronization_engine.h>

#include <nx_ec/ec_proto_version.h>

namespace nx::cloud::db {

MaintenanceManager::MaintenanceManager(
    const QnUuid& moduleGuid,
    clusterdb::engine::SyncronizationEngine* const syncronizationEngine,
    const nx::sql::InstanceController& dbInstanceController)
    :
    m_moduleGuid(moduleGuid),
    m_syncronizationEngine(syncronizationEngine),
    m_dbInstanceController(dbInstanceController)
{
}

MaintenanceManager::~MaintenanceManager()
{
    m_startedAsyncCallsCounter.wait();
    m_timer.pleaseStopSync();
}

void MaintenanceManager::getVmsConnections(
    const AuthorizationInfo& /*authzInfo*/,
    std::function<void(
        api::Result,
        api::VmsConnectionDataList)> completionHandler)
{
    m_timer.post(
        [this,
            lock = m_startedAsyncCallsCounter.getScopedIncrement(),
            completionHandler = std::move(completionHandler)]()
        {
            const auto ec2Connections =
                m_syncronizationEngine->connectionManager().getConnections();
            api::VmsConnectionDataList result;
            result.connections.reserve(ec2Connections.size());
            for (const auto& connection: ec2Connections)
            {
                result.connections.push_back(
                    {connection.systemId, connection.peerEndpoint.toStdString()});
            }

            completionHandler(api::ResultCode::ok, std::move(result));
        });
}

void MaintenanceManager::getTransactionLog(
    const AuthorizationInfo& /*authzInfo*/,
    data::SystemId systemId,
    std::function<void(
        api::Result,
        ::ec2::ApiTransactionDataList)> completionHandler)
{
    using namespace std::placeholders;

    m_syncronizationEngine->transactionLog().readTransactions(
        systemId.systemId.c_str(),
        nx::clusterdb::engine::ReadCommandsFilter::kEmptyFilter,
        std::bind(&MaintenanceManager::onTransactionLogRead, this,
            m_startedAsyncCallsCounter.getScopedIncrement(),
            systemId.systemId,
            _1, _2, _3, std::move(completionHandler)));
}

void MaintenanceManager::getStatistics(
    const AuthorizationInfo& /*authzInfo*/,
    std::function<void(api::Result, data::Statistics)> completionHandler)
{
    m_timer.post(
        [this,
            lock = m_startedAsyncCallsCounter.getScopedIncrement(),
            completionHandler = std::move(completionHandler)]()
        {
            data::Statistics statistics;
            statistics.onlineServerCount =
                (int)m_syncronizationEngine->connectionManager().getConnectionCount();
            statistics.dbQueryStatistics =
                m_dbInstanceController.statisticsCollector().getQueryStatistics();
            statistics.pendingSqlQueryCount =
                m_dbInstanceController.queryExecutor().pendingQueryCount();

            completionHandler(api::ResultCode::ok, std::move(statistics));
        });
}

void MaintenanceManager::onTransactionLogRead(
    nx::utils::Counter::ScopedIncrement /*asyncCallLocker*/,
    const std::string& systemId,
    clusterdb::engine::ResultCode ec2ResultCode,
    std::vector<clusterdb::engine::dao::TransactionLogRecord> serializedTransactions,
    vms::api::TranState /*readedUpTo*/,
    std::function<void(
        api::Result,
        ::ec2::ApiTransactionDataList)> completionHandler)
{
    api::ResultCode resultCode = ec2ResultToResult(ec2ResultCode);

    NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this), lm("system %1. Read %2 transactions. Result code %3")
            .arg(systemId).arg(serializedTransactions.size()).arg(api::toString(resultCode)));

    NX_ASSERT(resultCode != api::ResultCode::partialContent);
    if (resultCode != api::ResultCode::ok)
    {
        return completionHandler(
            resultCode,
            ::ec2::ApiTransactionDataList());
    }

    ::ec2::ApiTransactionDataList outData;
    for (const auto& transactionContext: serializedTransactions)
    {
        ::ec2::ApiTransactionData tran(m_moduleGuid);
        tran.tranGuid = QnUuid::fromStringSafe(transactionContext.hash);
        nx::Buffer serializedTransaction =
            transactionContext.serializer->serialize(Qn::UbjsonFormat, nx_ec::EC2_PROTO_VERSION);
        tran.dataSize = serializedTransaction.size();
        QnUbjsonReader<nx::Buffer> stream(&serializedTransaction);
        if (QnUbjson::deserialize(&stream, &tran.tran))
        {
            outData.push_back(std::move(tran));
        }
        else
        {
            NX_ASSERT(
                false,
                lm("system %1. Error deserializing transaction with hash %2")
                    .arg(systemId).arg(transactionContext.hash));
            return completionHandler(
                api::ResultCode::invalidFormat,
                ::ec2::ApiTransactionDataList());
        }
    }

    return completionHandler(
        api::ResultCode::ok,
        std::move(outData));
}

} // namespace nx::cloud::db
