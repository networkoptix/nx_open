#include "transaction_log.h"

#include <QtSql/QSqlQuery>

#include "outgoing_transaction_dispatcher.h"
#include "managers/managers_types.h"

namespace nx {
namespace cdb {
namespace ec2 {

QString toString(const ::ec2::QnAbstractTransaction& tran)
{
    return lit("seq %1, ts %2")
        .arg(tran.persistentInfo.sequence).arg(tran.persistentInfo.timestamp);
}

TransactionLog::TransactionLog(
    const QnUuid& peerId,
    nx::db::AsyncSqlQueryExecutor* const dbManager,
    OutgoingTransactionDispatcher* const outgoingTransactionDispatcher)
:
    m_peerId(peerId),
    m_dbManager(dbManager),
    m_outgoingTransactionDispatcher(outgoingTransactionDispatcher)
{
    if (fillCache() != nx::db::DBResult::ok)
        throw std::runtime_error("Error loading transaction log from DB");
}

void TransactionLog::startDbTransaction(
    const nx::String& /*systemId*/,
    nx::utils::MoveOnlyFunc<nx::db::DBResult(QSqlDatabase*)> dbOperationsFunc,
    nx::utils::MoveOnlyFunc<void(QSqlDatabase*, nx::db::DBResult)> onDbUpdateCompleted)
{
    // TODO: execution of requests to the same system MUST be serialized
    // TODO: monitoring request queue size and returning api::ResultCode::retryLater if exceeded

    m_dbManager->executeUpdate(
        [dbOperationsFunc = std::move(dbOperationsFunc)](
            QSqlDatabase* dbConnection) -> nx::db::DBResult
        {
            return dbOperationsFunc(dbConnection);
        },
        [this, onDbUpdateCompleted = std::move(onDbUpdateCompleted)](
            QSqlDatabase* dbConnection,
            nx::db::DBResult dbResult)
        {
            onDbUpdateCompleted(dbConnection, dbResult);
            onDbTransactionCompleted(dbConnection, dbResult);
        });
}

::ec2::QnTranState TransactionLog::getTransactionState(const nx::String& systemId) const
{
    QnMutexLocker lk(&m_mutex);
    const auto it = m_systemIdToTransactionLog.find(systemId);
    if (it == m_systemIdToTransactionLog.cend())
        return ::ec2::QnTranState();
    return it->second.transactionState;
}

void TransactionLog::readTransactions(
    const nx::String& systemId,
    const ::ec2::QnTranState& from,
    const ::ec2::QnTranState& to,
    int maxTransactionsToReturn,
    nx::utils::MoveOnlyFunc<void(
        api::ResultCode /*resultCode*/,
        std::vector<nx::Buffer> /*serializedTransactions*/)> completionHandler)
{
    using namespace std::placeholders;

    m_dbManager->executeSelect<std::vector<nx::Buffer>>(
        std::bind(
            &TransactionLog::fetchTransactions, this,
            _1, systemId, from, to, maxTransactionsToReturn, _2),
        [completionHandler = std::move(completionHandler)](
            QSqlDatabase* /*connection*/,
            nx::db::DBResult dbResult,
            std::vector<nx::Buffer> outputData)
        {
            completionHandler(fromDbResultCode(dbResult), std::move(outputData));
        });
}

nx::db::DBResult TransactionLog::fillCache()
{
    std::promise<db::DBResult> cacheFilledPromise;
    auto future = cacheFilledPromise.get_future();

    //starting async operation
    using namespace std::placeholders;
    m_dbManager->executeSelect<int>(
        std::bind(&TransactionLog::fetchTransactionState, this, _1, _2),
        [&cacheFilledPromise](
            QSqlDatabase* /*connection*/,
            db::DBResult dbResult,
            int /*dummyResult*/)
        {
            cacheFilledPromise.set_value(dbResult);
        });

    //waiting for completion
    future.wait();
    return future.get();
}

nx::db::DBResult TransactionLog::fetchTransactionState(
    QSqlDatabase* connection,
    int* const /*dummyResult*/)
{
    // TODO: Filling in m_systemIdToTransactionLog.
    QSqlQuery selectTransactionStateQuery(*connection);
    selectTransactionStateQuery.setForwardOnly(true);
    selectTransactionStateQuery.prepare(
        R"sql(
        SELECT system_id, peer_guid, db_guid, sequence, tran_hash, timestamp
        FROM transaction_log
        ORDER BY system_id, peer_guid, db_guid
        )sql");

    if (!selectTransactionStateQuery.exec())
    {
        NX_LOGX(lm("Error loading transaction log. %1")
            .arg(connection->lastError().text()), cl_logERROR);
        return nx::db::DBResult::ioError;
    }

    while (selectTransactionStateQuery.next())
    {
        const nx::String systemId = selectTransactionStateQuery.value("system_id").toString().toLatin1();
        const nx::String peerGuid = selectTransactionStateQuery.value("peer_guid").toString().toLatin1();
        const nx::String dbGuid = selectTransactionStateQuery.value("db_guid").toString().toLatin1();
        const int sequence = selectTransactionStateQuery.value("sequence").toInt();
        const nx::Buffer tranHash = selectTransactionStateQuery.value("tran_hash").toString().toLatin1();
        const qint64 timestamp = selectTransactionStateQuery.value("timestamp").toLongLong();

        const ::ec2::QnTranStateKey tranStateKey(
            QnUuid::fromStringSafe(peerGuid),
            QnUuid::fromStringSafe(dbGuid));

        VmsTransactionLogData& vmsTranLog = m_systemIdToTransactionLog[systemId];
       if (vmsTranLog.systemId.isEmpty())
            vmsTranLog.systemId = systemId;
        qint32& persistentSequence = vmsTranLog.transactionState.values[tranStateKey];
        if (persistentSequence < sequence)
            persistentSequence = sequence;
        vmsTranLog.transactionHashToUpdateAuthor[tranHash] = 
            UpdateHistoryData{tranStateKey, timestamp};
    }

    return nx::db::DBResult::ok;
}

nx::db::DBResult TransactionLog::fetchTransactions(
    QSqlDatabase* connection,
    const nx::String& systemId,
    const ::ec2::QnTranState& from,
    const ::ec2::QnTranState& to,
    int maxTransactionsToReturn,
    std::vector<nx::Buffer>* const outputData)
{
    // TODO: 
    return nx::db::DBResult::ok;
}

bool TransactionLog::isShouldBeIgnored(
    QSqlDatabase* /*connection*/,
    const nx::String& systemId,
    const ::ec2::QnAbstractTransaction& tran,
    const QByteArray& hash)
{
    using namespace ::ec2;
    VmsTransactionLogData* vmsTransactionLog = nullptr;
    {
        QnMutexLocker lk(&m_mutex);
        vmsTransactionLog = &m_systemIdToTransactionLog[systemId];
    }

    QnTranStateKey key(tran.peerID, tran.persistentInfo.dbID);
    NX_ASSERT(tran.persistentInfo.sequence != 0);
    const auto currentSequence = vmsTransactionLog->transactionState.values.value(key);
    if (currentSequence >= tran.persistentInfo.sequence)
    {
        NX_LOG(QnLog::EC2_TRAN_LOG,
            lm("systemId %1. Ignoring transaction %2 (%3, hash %4)"
                "because of persistent sequence: %5 <= %6")
                .arg(systemId).arg(ApiCommand::toString(tran.command)).str(tran)
                .arg(hash).str(tran.persistentInfo.sequence).arg(currentSequence),
            cl_logDEBUG1);
        return true;    //< Transaction should be ignored.
    }

    const auto itr = vmsTransactionLog->transactionHashToUpdateAuthor.find(hash);
    if (itr == vmsTransactionLog->transactionHashToUpdateAuthor.cend())
        return false;   //< Transaction should be processed.

    const auto lastTime = itr->second.timestamp;
    bool rez = lastTime > tran.persistentInfo.timestamp;
    if (lastTime == tran.persistentInfo.timestamp)
        rez = key < itr->second.updatedBy;
    if (rez)
    {
        NX_LOG(QnLog::EC2_TRAN_LOG,
            lm("systemId %1. Ignoring transaction %2 (%3, hash %4)"
                "because of timestamp: %5 <= %6")
            .arg(systemId).arg(ApiCommand::toString(tran.command)).str(tran)
            .arg(hash).str(tran.persistentInfo.timestamp).arg(lastTime),
            cl_logDEBUG1);
        return true;    //< Transaction should be ignored.
    }

    return false;   //< Transaction should be processed.
}

//bool TransactionLog::checkTransactionSequence(
//    QSqlDatabase* /*connection*/,
//    const nx::String& systemId,
//    const ::ec2::QnAbstractTransaction& transaction,
//    const TransactionTransportHeader& cdbTransportHeader)
//{
//    using namespace ::ec2;
//    VmsTransactionLogData* vmsTransactionLog = nullptr;
//    {
//        QnMutexLocker lk(&m_mutex);
//        vmsTransactionLog = &m_systemIdToTransactionLog[systemId];
//    }
//    
//    const QnTransactionTransportHeader& transportHeader = cdbTransportHeader.vmsTransportHeader;
//
//    if (transportHeader.sender.isNull())
//        return true; // old version, nothing to check
//
//    // 1. check transport sequence
//    QnTranStateKey ttSenderKey(transportHeader.sender, transportHeader.senderRuntimeID);
//    int transportSeq = vmsTransactionLog->lastTransportSeq[ttSenderKey];
//    if (transportSeq >= transportHeader.sequence)
//    {
//        NX_LOG(QnLog::EC2_TRAN_LOG,
//            lm("systemId %1. Ignoring transaction %2 (%3) received "
//                "from %4 because of transport sequence: %5 <= %6")
//                .arg(systemId).arg(ApiCommand::toString(transaction.command)).str(transaction)
//                .str(cdbTransportHeader).arg(transportHeader.sequence).arg(transportSeq),
//            cl_logDEBUG1);
//        return false; // already processed
//    }
//    vmsTransactionLog->lastTransportSeq[ttSenderKey] = transportHeader.sequence;
//
//    // 2. check persistent sequence
//    if (transaction.persistentInfo.isNull())
//    {
//        NX_LOG(QnLog::EC2_TRAN_LOG,
//            lm("systemId %1. Transaction %2 (%3) received "
//                "from %4 has no persistent info")
//                .arg(systemId).arg(ApiCommand::toString(transaction.command))
//                .str(transaction).str(cdbTransportHeader),
//            cl_logDEBUG1);
//        return true; // nothing to check
//    }
//
//    return true;
//
//#if 0
//    QnTranStateKey persistentKey(transaction.peerID, transaction.persistentInfo.dbID);
//    const int persistentSeq = vmsTransactionLog->transactionState.values[persistentKey];
//
//    //if (QnLog::instance(QnLog::EC2_TRAN_LOG)->logLevel() >= cl_logWARNING)
//    //    if (!transport->isSyncDone() && transport->isReadSync(ApiCommand::NotDefined) && transportHeader.sender != transport->remotePeer().id)
//    //    {
//    //        NX_LOG(QnLog::EC2_TRAN_LOG, lit("Got transaction from peer %1 while sync with peer %2 in progress").
//    //            arg(transportHeader.sender.toString()).arg(transport->remotePeer().id.toString()), cl_logWARNING);
//    //    }
//
//    if (transaction.persistentInfo.sequence > persistentSeq + 1)
//    {
//        if (transport->isSyncDone())
//        {
//            // gap in persistent data detect, do resync
//            NX_LOG(QnLog::EC2_TRAN_LOG, lit("GAP in persistent data detected! for peer %1 Expected seq=%2, but got seq=%3").
//                arg(tran.peerID.toString()).arg(persistentSeq + 1).arg(tran.persistentInfo.sequence), cl_logDEBUG1);
//
//            if (!transport->remotePeer().isClient() && !ApiPeerData::isClient(m_localPeerType))
//                queueSyncRequest(transport);
//            else
//                transport->setState(QnTransactionTransport::Error); // reopen
//            return false;
//        }
//        else
//        {
//            NX_LOG(QnLog::EC2_TRAN_LOG, lit("GAP in persistent data, but sync in progress %1. Expected seq=%2, but got seq=%3").
//                arg(tran.peerID.toString()).arg(persistentSeq + 1).arg(tran.persistentInfo.sequence), cl_logDEBUG1);
//        }
//    }
//    return true;
//#endif
//}

nx::db::DBResult TransactionLog::saveToDb(
    QSqlDatabase* connection,
    const nx::String& systemId,
    const ::ec2::QnAbstractTransaction& transaction,
    const QByteArray& transactionHash,
    const QByteArray& ubjsonData)
{
    NX_LOG(
        QnLog::EC2_TRAN_LOG,
        lm("systemId %1. Saving transaction %2 (%3, hash %4) to log")
            .arg(systemId).arg(::ec2::ApiCommand::toString(transaction.command))
            .str(transaction).arg(transactionHash),
        cl_logDEBUG1);

    QSqlQuery saveTranQuery(*connection);
    saveTranQuery.prepare(
        R"sql(
        REPLACE INTO transaction_log(system_id, peer_guid, db_guid, sequence,
                                     timestamp, tran_hash, tran_data)
        VALUES (?, ?, ?, ?, ?, ?, ?)")
        )sql");
    saveTranQuery.addBindValue(systemId);
    saveTranQuery.addBindValue(transaction.peerID.toSimpleString());
    saveTranQuery.addBindValue(transaction.persistentInfo.dbID.toSimpleString());
    saveTranQuery.addBindValue(transaction.persistentInfo.sequence);
    saveTranQuery.addBindValue(transaction.persistentInfo.timestamp);
    saveTranQuery.addBindValue(transactionHash);
    saveTranQuery.addBindValue(ubjsonData);
    if (!saveTranQuery.exec())
    {
        NX_LOGX(lm("systemId %1. Error saving transaction %2 (%3, hash %4) to log. %5")
            .arg(systemId).arg(::ec2::ApiCommand::toString(transaction.command))
            .str(transaction).arg(transactionHash).arg(connection->lastError().text()),
            cl_logWARNING);
        return nx::db::DBResult::ioError;
    }

    // Modifying transaction log cache.
    QnMutexLocker lk(&m_mutex);
    DbTransactionContext& dbTranContext = m_dbTransactionContexts[connection];
    const ::ec2::QnTranStateKey tranKey(transaction.peerID, transaction.persistentInfo.dbID);
    dbTranContext.transactionLogUpdate.transactionHashToUpdateAuthor[transactionHash] =
        UpdateHistoryData{
            tranKey,
            transaction.persistentInfo.timestamp};
    dbTranContext.transactionLogUpdate.transactionState.values[tranKey] =
        transaction.persistentInfo.sequence;

    return nx::db::DBResult::ok;
}

int TransactionLog::generateNewTransactionSequence(
    QSqlDatabase* connection,
    const nx::String& systemId)
{
    QnMutexLocker lk(&m_mutex);
    int& currentSequence =
        m_systemIdToTransactionLog[systemId].transactionState.
            values[::ec2::QnTranStateKey(m_peerId, kDbInstanceGuid)];
    ++currentSequence;
    return currentSequence;
}

qint64 TransactionLog::generateNewTransactionTimestamp(
    QSqlDatabase* connection,
    const nx::String& systemId)
{
    // TODO: copy function from ::ec2::QnTransactionLog
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

void TransactionLog::onDbTransactionCompleted(
    QSqlDatabase* dbConnection,
    nx::db::DBResult dbResult)
{
    DbTransactionContext currentDbTranContext;
    VmsTransactionLogData* vmsTransactionLogData = nullptr;
    {
        QnMutexLocker lk(&m_mutex);
        auto it = m_dbTransactionContexts.find(dbConnection);
        if (it != m_dbTransactionContexts.end())
        {
            currentDbTranContext = std::move(it->second);
            m_dbTransactionContexts.erase(it);
            vmsTransactionLogData =
                &m_systemIdToTransactionLog[currentDbTranContext.systemId];
        }
    }

    if (dbResult != nx::db::DBResult::ok)
    {
        // Rolling back transaction log change.
        return;
    }

    // Applying transaction log changes.
    for (const auto& elem: currentDbTranContext.transactionLogUpdate.transactionHashToUpdateAuthor)
        vmsTransactionLogData->transactionHashToUpdateAuthor[elem.first] = elem.second;

    for (auto it = currentDbTranContext.transactionLogUpdate.transactionState.values.cbegin();
         it != currentDbTranContext.transactionLogUpdate.transactionState.values.cend();
         ++it)
    {
        vmsTransactionLogData->transactionState.values[it.key()] = it.value();
    }

    // Issuing "new transaction" event.
    for (auto& tran : currentDbTranContext.transactions)
        m_outgoingTransactionDispatcher->dispatchTransaction(
            currentDbTranContext.systemId,
            std::move(tran));
}

} // namespace ec2
} // namespace cdb
} // namespace nx
