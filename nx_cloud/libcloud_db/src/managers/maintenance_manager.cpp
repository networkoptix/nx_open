#include "maintenance_manager.h"

#include <nx/utils/log/log.h>
#include <transaction/transaction.h>

#include "ec2/connection_manager.h"

namespace nx {
namespace cdb {

MaintenanceManager::MaintenanceManager(
    const QnUuid& moduleGuid,
    const ec2::ConnectionManager& connectionManager,
    ec2::TransactionLog* const transactionLog)
    :
    m_moduleGuid(moduleGuid),
    m_connectionManager(connectionManager),
    m_transactionLog(transactionLog)
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
                m_connectionManager.getVmsConnections());
        });
}

void MaintenanceManager::getTransactionLog(
    const AuthorizationInfo& /*authzInfo*/,
    data::SystemID systemID,
    std::function<void(
        api::ResultCode,
        ::ec2::ApiTransactionDataList)> completionHandler)
{
    using namespace std::placeholders;

    ::ec2::QnTranStateKey maxTranStateKey;
    maxTranStateKey.peerID = QnUuid::fromStringSafe(lit("{FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF}"));
    ::ec2::QnTranState maxTranState;
    maxTranState.values.insert(std::move(maxTranStateKey), std::numeric_limits<qint32>::max());

    m_transactionLog->readTransactions(
        systemID.systemID.c_str(),
        ::ec2::QnTranState(),
        maxTranState,
        std::numeric_limits<int>::max(),
        std::bind(&MaintenanceManager::onTransactionLogRead, this, 
            m_startedAsyncCallsCounter.getScopedIncrement(),
            systemID.systemID,
            _1, _2, _3, std::move(completionHandler)));
}

void MaintenanceManager::onTransactionLogRead(
    QnCounter::ScopedIncrement /*asyncCallLocker*/,
    const std::string& systemId,
    api::ResultCode resultCode,
    std::vector<ec2::TransactionData> serializedTransactions,
    ::ec2::QnTranState readedUpTo,
    std::function<void(
        api::ResultCode,
        ::ec2::ApiTransactionDataList)> completionHandler)
{
    NX_LOGX(QnLog::EC2_TRAN_LOG,
        lm("system %1. Read %2 transactions. Result code %3")
            .arg(systemId).arg(serializedTransactions.size()).str(resultCode),
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
            transactionContext.serializer->serialize(Qn::UbjsonFormat);
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
