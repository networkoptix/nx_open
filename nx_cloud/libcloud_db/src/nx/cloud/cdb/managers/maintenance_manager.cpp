#include "maintenance_manager.h"

#include <nx/utils/log/log.h>

#include "../ec2/synchronization_engine.h"

namespace nx {
namespace cdb {

MaintenanceManager::MaintenanceManager(
    const QnUuid& moduleGuid,
    ec2::SyncronizationEngine* const syncronizationEngine,
    const nx::utils::db::InstanceController& dbInstanceController)
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
        api::ResultCode,
        api::VmsConnectionDataList)> completionHandler)
{
    m_timer.post(
        [this,
            lock = m_startedAsyncCallsCounter.getScopedIncrement(),
            completionHandler = std::move(completionHandler)]()
        {
            completionHandler(
                api::ResultCode::ok,
                m_syncronizationEngine->connectionManager().getVmsConnections());
        });
}

void MaintenanceManager::getTransactionLog(
    const AuthorizationInfo& /*authzInfo*/,
    data::SystemId systemId,
    std::function<void(
        api::ResultCode,
        ::ec2::ApiTransactionDataList)> completionHandler)
{
    using namespace std::placeholders;

    m_syncronizationEngine->transactionLog().readTransactions(
        systemId.systemId.c_str(),
        boost::none,
        boost::none,
        std::numeric_limits<int>::max(),
        std::bind(&MaintenanceManager::onTransactionLogRead, this,
            m_startedAsyncCallsCounter.getScopedIncrement(),
            systemId.systemId,
            _1, _2, _3, std::move(completionHandler)));
}

void MaintenanceManager::getStatistics(
    const AuthorizationInfo& /*authzInfo*/,
    std::function<void(api::ResultCode, data::Statistics)> completionHandler)
{
    m_timer.post(
        [this,
            lock = m_startedAsyncCallsCounter.getScopedIncrement(),
            completionHandler = std::move(completionHandler)]()
        {
            data::Statistics statistics;
            statistics.onlineServerCount =
                (int)m_syncronizationEngine->connectionManager().getVmsConnectionCount();
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
    api::ResultCode resultCode,
    std::vector<ec2::dao::TransactionLogRecord> serializedTransactions,
    ::ec2::QnTranState /*readedUpTo*/,
    std::function<void(
        api::ResultCode,
        ::ec2::ApiTransactionDataList)> completionHandler)
{
    NX_LOGX(QnLog::EC2_TRAN_LOG,
        lm("system %1. Read %2 transactions. Result code %3")
            .arg(systemId).arg(serializedTransactions.size()).arg(api::toString(resultCode)),
        cl_logDEBUG1);

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

} // maintenance cdb
} // maintenance nx
