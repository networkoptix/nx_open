#include "transaction_log.h"

#include <QtSql/QSqlQuery>

#include <nx/utils/time.h>

#include "outgoing_transaction_dispatcher.h"
#include "managers/managers_types.h"

namespace nx {
namespace cdb {
namespace ec2 {

// TODO: ak: clarify where to use m_systemIdToTransactionLog against m_dbTransactionContexts.

QString toString(const ::ec2::QnAbstractTransaction& tran)
{
    return lm("seq %1, ts %2")
        .arg(tran.persistentInfo.sequence).str(tran.persistentInfo.timestamp);
}

TransactionLog::TransactionLog(
    const QnUuid& peerId,
    nx::db::AsyncSqlQueryExecutor* const dbManager,
    OutgoingTransactionDispatcher* const outgoingTransactionDispatcher)
:
    m_peerId(peerId),
    m_dbManager(dbManager),
    m_outgoingTransactionDispatcher(outgoingTransactionDispatcher),
    m_transactionSequence(0)
{
    if (fillCache() != nx::db::DBResult::ok)
        throw std::runtime_error("Error loading transaction log from DB");
}

void TransactionLog::startDbTransaction(
    const nx::String& systemId,
    nx::utils::MoveOnlyFunc<nx::db::DBResult(nx::db::QueryContext*)> dbOperationsFunc,
    nx::utils::MoveOnlyFunc<void(nx::db::QueryContext*, nx::db::DBResult)> onDbUpdateCompleted)
{
    // TODO: execution of requests to the same system MUST be serialized
    // TODO: monitoring request queue size and returning api::ResultCode::retryLater if exceeded

    m_dbManager->executeUpdate(
        [this, systemId, dbOperationsFunc = std::move(dbOperationsFunc)](
            nx::db::QueryContext* queryContext) -> nx::db::DBResult
        {
            {
                QnMutexLocker lk(&m_mutex);
                DbTransactionContext& dbTranContext =
                    m_dbTransactionContexts[std::make_pair(queryContext, systemId)];
                dbTranContext.systemId = systemId;
            }
            return dbOperationsFunc(queryContext);
        },
        [this, systemId, onDbUpdateCompleted = std::move(onDbUpdateCompleted)](
            nx::db::QueryContext* queryContext,
            nx::db::DBResult dbResult)
        {
            onDbUpdateCompleted(queryContext, dbResult);
            onDbTransactionCompleted(queryContext, systemId, dbResult);
        });
}

nx::db::DBResult TransactionLog::updateTimestampHiForSystem(
    nx::db::QueryContext* queryContext,
    const nx::String& systemId,
    quint64 newValue)
{
    QnMutexLocker lk(&m_mutex);
    m_systemIdToTransactionLog[systemId].timestampSequence = newValue;
    getTransactionContext(lk, queryContext, systemId)
        .transactionLogUpdate.timestampSequence = newValue;

    QSqlQuery saveSystemTimestampSequence(*queryContext->connection());
    saveSystemTimestampSequence.prepare(
        R"sql(
        REPLACE INTO transaction_source_settings(system_id, timestamp_hi) VALUES (?, ?)
        )sql");
    saveSystemTimestampSequence.addBindValue(QLatin1String(systemId));
    saveSystemTimestampSequence.addBindValue(newValue);
    if (!saveSystemTimestampSequence.exec())
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("systemId %1. Error saving transaction timestamp sequence %2 to log. %3")
            .arg(systemId).arg(newValue).arg(saveSystemTimestampSequence.lastError().text()),
            cl_logWARNING);
        return nx::db::DBResult::ioError;
    }

    return nx::db::DBResult::ok;
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
    TransactionsReadHandler completionHandler)
{
    using namespace std::placeholders;

    m_dbManager->executeSelect<TransactionReadResult>(
        std::bind(
            &TransactionLog::fetchTransactions, this,
            _1, systemId, from, to, maxTransactionsToReturn, _2),
        [completionHandler = std::move(completionHandler)](
            nx::db::QueryContext* /*queryContext*/,
            nx::db::DBResult dbResult,
            TransactionReadResult outputData)
        {
            completionHandler(
                dbResult == nx::db::DBResult::ok
                    ? outputData.resultCode
                    : dbResultToApiResult(dbResult),
                std::move(outputData.transactions),
                std::move(outputData.state));
        });
}

void TransactionLog::clearTransactionLogCacheForSystem(
    const nx::String& systemId)
{
    NX_LOGX(lm("Cleaning transaction log for system %1").arg(systemId), cl_logDEBUG2);

    QnMutexLocker lk(&m_mutex);
    m_systemIdToTransactionLog.erase(systemId);
}

nx::db::DBResult TransactionLog::fillCache()
{
    std::promise<db::DBResult> cacheFilledPromise;
    auto future = cacheFilledPromise.get_future();

    // Starting async operation.
    using namespace std::placeholders;
    m_dbManager->executeSelect<int>(
        std::bind(&TransactionLog::fetchTransactionState, this, _1, _2),
        [&cacheFilledPromise](
            nx::db::QueryContext* /*queryContext*/,
            db::DBResult dbResult,
            int /*dummyResult*/)
        {
            cacheFilledPromise.set_value(dbResult);
        });

    // Waiting for completion.
    future.wait();
    return future.get();
}

nx::db::DBResult TransactionLog::fetchTransactionState(
    nx::db::QueryContext* queryContext,
    int* const /*dummyResult*/)
{
    // TODO: Filling in m_systemIdToTransactionLog.
    QSqlQuery selectTransactionStateQuery(*queryContext->connection());
    selectTransactionStateQuery.setForwardOnly(true);
    selectTransactionStateQuery.prepare(
        R"sql(
        SELECT tl.system_id as system_id, 
               tss.timestamp_hi as settings_timestamp_hi, 
               peer_guid, 
               db_guid, 
               sequence, 
               tran_hash, 
               tl.timestamp_hi as timestamp_hi,
               timestamp
        FROM transaction_log tl
        LEFT JOIN transaction_source_settings tss ON tl.system_id = tss.system_id
        ORDER BY tl.system_id, timestamp_hi, timestamp DESC
        )sql");

    if (!selectTransactionStateQuery.exec())
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG, 
            lm("Error loading transaction log. %1")
            .arg(selectTransactionStateQuery.lastError().text()), cl_logERROR);
        return nx::db::DBResult::ioError;
    }

    nx::String prevSystemId;
    while (selectTransactionStateQuery.next())
    {
        const nx::String systemId = selectTransactionStateQuery.value("system_id").toString().toLatin1();
        const std::uint64_t settingsTimestampHi = selectTransactionStateQuery.value("settings_timestamp_hi").toULongLong();
        const nx::String peerGuid = selectTransactionStateQuery.value("peer_guid").toString().toLatin1();
        const nx::String dbGuid = selectTransactionStateQuery.value("db_guid").toString().toLatin1();
        const int sequence = selectTransactionStateQuery.value("sequence").toInt();
        const nx::Buffer tranHash = selectTransactionStateQuery.value("tran_hash").toString().toLatin1();
        ::ec2::Timestamp timestamp;
        timestamp.sequence = selectTransactionStateQuery.value("timestamp_hi").toLongLong();
        timestamp.ticks = selectTransactionStateQuery.value("timestamp").toLongLong();

        const ::ec2::QnTranStateKey tranStateKey(
            QnUuid::fromStringSafe(peerGuid),
            QnUuid::fromStringSafe(dbGuid));

        VmsTransactionLogData& vmsTranLog = m_systemIdToTransactionLog[systemId];
        if (vmsTranLog.systemId.isEmpty())
            vmsTranLog.systemId = systemId;
        vmsTranLog.timestampSequence = 
            std::max(vmsTranLog.timestampSequence, settingsTimestampHi);
        qint32& persistentSequence = vmsTranLog.transactionState.values[tranStateKey];
        if (persistentSequence < sequence)
            persistentSequence = sequence;
        vmsTranLog.transactionHashToUpdateAuthor[tranHash] = 
            UpdateHistoryData{tranStateKey, timestamp};

        if (systemId != prevSystemId)
        {
            // Switched to another system.
            vmsTranLog.timestampCalculator->init(timestamp);
            prevSystemId = systemId;
        }
    }

    return nx::db::DBResult::ok;
}

nx::db::DBResult TransactionLog::fetchTransactions(
    nx::db::QueryContext* queryContext,
    const nx::String& systemId,
    const ::ec2::QnTranState& from,
    const ::ec2::QnTranState& to,
    int /*maxTransactionsToReturn*/,
    TransactionReadResult* const outputData)
{
    // TODO: Taking into account maxTransactionsToReturn

    //QMap<QnTranStateKey, qint32> values
    ::ec2::QnTranState currentState;
    {
        // Merging "from" with local state.
        QnMutexLocker lk(&m_mutex);
        VmsTransactionLogData& transactionLogBySystem = m_systemIdToTransactionLog[systemId];
        // Using copy of current state since we must not hold mutex over sql request.
        currentState = transactionLogBySystem.transactionState;
    }

    outputData->resultCode = api::ResultCode::dbError;

    for (auto it = currentState.values.begin();
         it != currentState.values.end();
         ++it)
    {
        QSqlQuery fetchTransactionsOfAPeerQuery(*queryContext->connection());
        fetchTransactionsOfAPeerQuery.prepare(R"sql(
            SELECT tran_data, tran_hash, timestamp, sequence
            FROM transaction_log
            WHERE system_id=? AND peer_guid=? AND db_guid=? AND sequence>? AND sequence<=?
            ORDER BY sequence
            )sql");
        fetchTransactionsOfAPeerQuery.addBindValue(QLatin1String(systemId));
        fetchTransactionsOfAPeerQuery.addBindValue(it.key().peerID.toSimpleString());
        fetchTransactionsOfAPeerQuery.addBindValue(it.key().dbID.toSimpleString());
        fetchTransactionsOfAPeerQuery.addBindValue(from.values.value(it.key()));
        fetchTransactionsOfAPeerQuery.addBindValue(
            to.values.value(it.key(), std::numeric_limits<qint32>::max()));
        if (!fetchTransactionsOfAPeerQuery.exec())
        {
            NX_LOGX(QnLog::EC2_TRAN_LOG,
                lm("systemId %1. Error executing fetch_transactions request "
                   "for peer (%2; %3). %4")
                    .arg(it.key().peerID.toSimpleString()).arg(it.key().dbID.toSimpleString())
                    .arg(fetchTransactionsOfAPeerQuery.lastError().text()),
                cl_logERROR);
            return nx::db::DBResult::ioError;
        }

        while (fetchTransactionsOfAPeerQuery.next())
        {
            outputData->transactions.push_back(
                TransactionLogRecord{
                    fetchTransactionsOfAPeerQuery.value("tran_hash").toByteArray(),
                    std::make_unique<UbjsonTransactionPresentation>(
                        fetchTransactionsOfAPeerQuery.value("tran_data").toByteArray(),
                        nx_ec::EC2_PROTO_VERSION)
                });
        }
    }

    // TODO #ak currentState is not correct here since it can be limited by "to" and "maxTransactionsToReturn"
    outputData->state = currentState;
    outputData->resultCode = api::ResultCode::ok;

    return nx::db::DBResult::ok;
}

bool TransactionLog::isShouldBeIgnored(
    nx::db::QueryContext* /*queryContext*/,
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
            .arg(hash).str(tran.persistentInfo.timestamp).str(lastTime),
            cl_logDEBUG1);
        return true;    //< Transaction should be ignored.
    }

    return false;   //< Transaction should be processed.
}

nx::db::DBResult TransactionLog::saveToDb(
    nx::db::QueryContext* queryContext,
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

    QSqlQuery saveTranQuery(*queryContext->connection());
    saveTranQuery.prepare(
        R"sql(
        REPLACE INTO transaction_log(system_id, peer_guid, db_guid, sequence,
                                     timestamp_hi, timestamp, tran_hash, tran_data)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        )sql");
    saveTranQuery.addBindValue(QLatin1String(systemId));
    saveTranQuery.addBindValue(transaction.peerID.toSimpleString());
    saveTranQuery.addBindValue(transaction.persistentInfo.dbID.toSimpleString());
    saveTranQuery.addBindValue(transaction.persistentInfo.sequence);
    saveTranQuery.addBindValue(transaction.persistentInfo.timestamp.sequence);
    saveTranQuery.addBindValue(transaction.persistentInfo.timestamp.ticks);
    saveTranQuery.addBindValue(QLatin1String(transactionHash));
    saveTranQuery.addBindValue(ubjsonData);
    if (!saveTranQuery.exec())
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("systemId %1. Error saving transaction %2 (%3, hash %4) to log. %5")
                .arg(systemId).arg(::ec2::ApiCommand::toString(transaction.command))
                .str(transaction).arg(transactionHash).arg(saveTranQuery.lastError().text()),
            cl_logWARNING);
        return nx::db::DBResult::ioError;
    }

    if (transaction.peerID == m_peerId)
    {
        // Updating cloud_db sequence in DB
        QSqlQuery updateCdbSequence(*queryContext->connection());
        updateCdbSequence.prepare(
            R"sql(
            UPDATE cloud_db_transaction_sequence
            SET max_sequence = :newSequence
            WHERE max_sequence < :newSequence
            )sql");
        updateCdbSequence.bindValue(":newSequence", transaction.persistentInfo.sequence);
        if (!updateCdbSequence.exec())
        {
            NX_LOGX(QnLog::EC2_TRAN_LOG,
                lm("systemId %1. Error saving transaction %2 (%3, hash %4) to log. "
                    "Error updating cdb sequence. %5")
                .arg(systemId).arg(::ec2::ApiCommand::toString(transaction.command))
                .str(transaction).arg(transactionHash).arg(saveTranQuery.lastError().text()),
                cl_logWARNING);
            return nx::db::DBResult::ioError;
        }
    }

    // Modifying transaction log cache.
    QnMutexLocker lk(&m_mutex);
    DbTransactionContext& dbTranContext = getTransactionContext(lk, queryContext, systemId);
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
    nx::db::QueryContext* /*queryContext*/,
    const nx::String& systemId)
{
    QnMutexLocker lk(&m_mutex);
    int& currentSequence =
        m_systemIdToTransactionLog[systemId].transactionState.
            values[::ec2::QnTranStateKey(m_peerId, guidFromArbitraryData(systemId))];
    ++currentSequence;
    return currentSequence;
}

::ec2::Timestamp TransactionLog::generateNewTransactionTimestamp(
    nx::db::QueryContext* /*queryContext*/,
    const nx::String& systemId)
{
    using namespace std::chrono;

    QnMutexLocker lk(&m_mutex);

    auto& transactionLogData = m_systemIdToTransactionLog[systemId];
    ::ec2::Timestamp timestamp;
    timestamp.sequence = transactionLogData.timestampSequence;
    timestamp.ticks = 
        m_systemIdToTransactionLog[systemId]
            .timestampCalculator->calculateNextTimeStamp().ticks;
    return timestamp;
}

void TransactionLog::onDbTransactionCompleted(
    nx::db::QueryContext* queryContext,
    const nx::String& systemId,
    nx::db::DBResult dbResult)
{
    DbTransactionContext currentDbTranContext;
    // TODO: #ak Access to vmsTransactionLogData should probably be synchronized in case of 
    // mutiple connections from servers of the same system.
    VmsTransactionLogData* vmsTransactionLogData = nullptr;
    {
        QnMutexLocker lk(&m_mutex);
        auto it = m_dbTransactionContexts.find(std::make_pair(queryContext, systemId));
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

TransactionLog::DbTransactionContext& TransactionLog::getTransactionContext(
    const QnMutexLockerBase&,
    nx::db::QueryContext* const queryContext,
    const nx::String& systemId)
{
    auto insertionPair = m_dbTransactionContexts.emplace(
        std::make_pair(queryContext, systemId),
        DbTransactionContext());
    DbTransactionContext& ctx = insertionPair.first->second;
    if (insertionPair.second)
    {
        ctx.systemId = systemId;
        queryContext->transaction()->addOnTransactionCompletionHandler(
            [this, queryContext, systemId](nx::db::DBResult dbResult)
            {
                onDbTransactionCompleted(queryContext, systemId, dbResult);
            });
    }
    return ctx;
}

} // namespace ec2
} // namespace cdb
} // namespace nx
