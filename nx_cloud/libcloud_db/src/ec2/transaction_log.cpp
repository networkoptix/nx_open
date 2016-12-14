#include "transaction_log.h"

#include <QtSql/QSqlQuery>

#include <nx/utils/time.h>

#include "outgoing_transaction_dispatcher.h"
#include "managers/managers_types.h"

namespace nx {
namespace cdb {
namespace ec2 {

QString toString(const ::ec2::QnAbstractTransaction& tran)
{
    return lm("seq %1, ts %2")
        .arg(tran.persistentInfo.sequence).str(tran.persistentInfo.timestamp);
}

TransactionLog::TransactionLog(
    const QnUuid& peerId,
    nx::db::AsyncSqlQueryExecutor* const dbManager,
    AbstractOutgoingTransactionDispatcher* const outgoingTransactionDispatcher)
:
    m_peerId(peerId),
    m_dbManager(dbManager),
    m_outgoingTransactionDispatcher(outgoingTransactionDispatcher),
    m_transactionSequence(0)
{
    m_transactionDataObject = dao::TransactionDataObjectFactory::create();

    if (fillCache() != nx::db::DBResult::ok)
        throw std::runtime_error("Error loading transaction log from DB");
}

void TransactionLog::startDbTransaction(
    const nx::String& systemId,
    nx::utils::MoveOnlyFunc<nx::db::DBResult(nx::db::QueryContext*)> dbOperationsFunc,
    nx::utils::MoveOnlyFunc<void(nx::db::QueryContext*, nx::db::DBResult)> onDbUpdateCompleted)
{
    // TODO: monitoring request queue size and returning api::ResultCode::retryLater if exceeded

    m_dbManager->executeUpdate(
        [this, systemId, dbOperationsFunc = std::move(dbOperationsFunc)](
            nx::db::QueryContext* queryContext) -> nx::db::DBResult
        {
            {
                QnMutexLocker lk(&m_mutex);
                getDbTransactionContext(lk, queryContext, systemId);
            }
            return dbOperationsFunc(queryContext);
        },
        [this, systemId, onDbUpdateCompleted = std::move(onDbUpdateCompleted)](
            nx::db::QueryContext* queryContext,
            nx::db::DBResult dbResult)
        {
            onDbUpdateCompleted(queryContext, dbResult);
        });
}

nx::db::DBResult TransactionLog::updateTimestampHiForSystem(
    nx::db::QueryContext* queryContext,
    const nx::String& systemId,
    quint64 newValue)
{
    updateTimestampHiInCache(queryContext, systemId, newValue);

    const auto dbResult = m_transactionDataObject->updateTimestampHiForSystem(
        queryContext, systemId, newValue);
    if (dbResult != nx::db::DBResult::ok)
        return dbResult;

    return nx::db::DBResult::ok;
}

::ec2::QnTranState TransactionLog::getTransactionState(const nx::String& systemId) const
{
    QnMutexLocker lk(&m_mutex);
    const auto it = m_systemIdToTransactionLog.find(systemId);
    if (it == m_systemIdToTransactionLog.cend())
        return ::ec2::QnTranState();
    return it->second.committedTransactionState();
}

void TransactionLog::readTransactions(
    const nx::String& systemId,
    boost::optional<::ec2::QnTranState> from,
    boost::optional<::ec2::QnTranState> to,
    int maxTransactionsToReturn,
    TransactionsReadHandler completionHandler)
{
    using namespace std::placeholders;

    if (!from)
        from = ::ec2::QnTranState();

    if (!to)
    {
        ::ec2::QnTranStateKey maxTranStateKey;
        maxTranStateKey.peerID = QnUuid::fromStringSafe(lit("{FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF}"));
        ::ec2::QnTranState maxTranState;
        maxTranState.values.insert(std::move(maxTranStateKey), std::numeric_limits<qint32>::max());
        to = std::move(maxTranState);
    }

    auto outputData = std::make_unique<TransactionReadResult>();
    auto outputDataPtr = outputData.get();
    m_dbManager->executeSelect(
        std::bind(
            &TransactionLog::fetchTransactions, this,
            _1, systemId, std::move(*from), std::move(*to), maxTransactionsToReturn, outputDataPtr),
        [completionHandler = std::move(completionHandler), outputData = std::move(outputData)](
            nx::db::QueryContext* /*queryContext*/,
            nx::db::DBResult dbResult)
        {
            completionHandler(
                dbResult == nx::db::DBResult::ok
                    ? outputData->resultCode
                    : dbResultToApiResult(dbResult),
                std::move(outputData->transactions),
                std::move(outputData->state));
        });
}

void TransactionLog::clearTransactionLogCacheForSystem(
    const nx::String& systemId)
{
    NX_LOGX(lm("Cleaning transaction log for system %1").arg(systemId), cl_logDEBUG2);

    QnMutexLocker lk(&m_mutex);
    m_systemIdToTransactionLog.erase(systemId);
}

::ec2::Timestamp TransactionLog::generateTransactionTimestamp(
    const nx::String& systemId)
{
    return generateNewTransactionTimestamp(
        VmsTransactionLogCache::InvalidTranId,
        systemId);
}

void TransactionLog::shiftLocalTransactionSequence(
    const nx::String& systemId,
    int delta)
{
    QnMutexLocker lk(&m_mutex);
    return m_systemIdToTransactionLog[systemId].shiftTransactionSequence(
        ::ec2::QnTranStateKey(m_peerId, guidFromArbitraryData(systemId)),
        delta);
}

nx::db::DBResult TransactionLog::fillCache()
{
    std::promise<db::DBResult> cacheFilledPromise;
    auto future = cacheFilledPromise.get_future();

    // Starting async operation.
    using namespace std::placeholders;
    m_dbManager->executeSelect(
        std::bind(&TransactionLog::fetchTransactionState, this, _1),
        [&cacheFilledPromise](
            nx::db::QueryContext* /*queryContext*/,
            db::DBResult dbResult)
        {
            cacheFilledPromise.set_value(dbResult);
        });

    // Waiting for completion.
    future.wait();
    return future.get();
}

nx::db::DBResult TransactionLog::fetchTransactionState(
    nx::db::QueryContext* queryContext)
{
    // TODO: #ak move to TransactionDataObject
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

        ::ec2::QnTranStateKey tranStateKey(
            QnUuid::fromStringSafe(peerGuid),
            QnUuid::fromStringSafe(dbGuid));

        VmsTransactionLogCache& vmsTranLog = m_systemIdToTransactionLog[systemId];

        vmsTranLog.restoreTransaction(
            std::move(tranStateKey), sequence, tranHash, settingsTimestampHi, timestamp);

        if (systemId != prevSystemId)
        {
            // Switched to another system.
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
        VmsTransactionLogCache& transactionLogBySystem = m_systemIdToTransactionLog[systemId];
        // Using copy of current state since we must not hold mutex over sql request.
        currentState = transactionLogBySystem.committedTransactionState();
    }

    outputData->resultCode = api::ResultCode::dbError;

    for (auto it = currentState.values.begin();
         it != currentState.values.end();
         ++it)
    {
        const auto dbResult = m_transactionDataObject->fetchTransactionsOfAPeerQuery(
            queryContext,
            systemId,
            it.key().peerID.toSimpleString(),
            it.key().dbID.toSimpleString(),
            from.values.value(it.key()),
            to.values.value(it.key(), std::numeric_limits<qint32>::max()),
            &outputData->transactions);
        if (dbResult != nx::db::DBResult::ok)
            return dbResult;
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
    VmsTransactionLogCache* vmsTransactionLog = nullptr;
    {
        QnMutexLocker lk(&m_mutex);
        vmsTransactionLog = &m_systemIdToTransactionLog[systemId];
    }

    NX_CRITICAL(vmsTransactionLog != nullptr);

    return vmsTransactionLog->isShouldBeIgnored(systemId, tran, hash);
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

    auto dbResult = m_transactionDataObject->insertOrReplaceTransaction(
        queryContext,
        {systemId, transaction, transactionHash, ubjsonData});
    if (dbResult != nx::db::DBResult::ok)
        return dbResult;

    // Modifying transaction log cache.
    QnMutexLocker lk(&m_mutex);
    DbTransactionContext& dbTranContext = getDbTransactionContext(lk, queryContext, systemId);

    m_systemIdToTransactionLog[systemId].insertOrReplaceTransaction(
        dbTranContext.cacheTranId, transaction, transactionHash);

    return nx::db::DBResult::ok;
}

int TransactionLog::generateNewTransactionSequence(
    nx::db::QueryContext* /*queryContext*/,
    const nx::String& systemId)
{
    QnMutexLocker lk(&m_mutex);
    return m_systemIdToTransactionLog[systemId].generateTransactionSequence(
        ::ec2::QnTranStateKey(m_peerId, guidFromArbitraryData(systemId)));
}

::ec2::Timestamp TransactionLog::generateNewTransactionTimestamp(
    VmsTransactionLogCache::TranId cacheTranId,
    const nx::String& systemId)
{
    using namespace std::chrono;

    QnMutexLocker lk(&m_mutex);
    auto& transactionLogData = m_systemIdToTransactionLog[systemId];
    return transactionLogData.generateTransactionTimestamp(cacheTranId);
}

void TransactionLog::onDbTransactionCompleted(
    nx::db::QueryContext* queryContext,
    const nx::String& systemId,
    nx::db::DBResult dbResult)
{
    DbTransactionContext currentDbTranContext = 
        m_dbTransactionContexts.take(std::make_pair(queryContext, systemId));
    VmsTransactionLogCache* vmsTransactionLogData = nullptr;
    {
        QnMutexLocker lk(&m_mutex);
        vmsTransactionLogData = &m_systemIdToTransactionLog[systemId];
        NX_ASSERT(vmsTransactionLogData != nullptr);
    }

    if (dbResult != nx::db::DBResult::ok)
    {
        // Rolling back transaction log change.
        vmsTransactionLogData->rollback(currentDbTranContext.cacheTranId);
        return;
    }

    vmsTransactionLogData->commit(currentDbTranContext.cacheTranId);

    // Issuing "new transaction" event.
    for (auto& tran: currentDbTranContext.transactionsAdded)
        m_outgoingTransactionDispatcher->dispatchTransaction(systemId, std::move(tran));
}

TransactionLog::DbTransactionContext& TransactionLog::getDbTransactionContext(
    const QnMutexLockerBase&,
    nx::db::QueryContext* const queryContext,
    const nx::String& systemId)
{
    auto newElementIter = m_dbTransactionContexts.emplace(
        std::make_pair(queryContext, systemId),
        DbTransactionContext(),
        [this, &systemId, &queryContext](DbTransactionContextMap::iterator newElementIter)
        {
            newElementIter->second.cacheTranId =
                m_systemIdToTransactionLog[systemId].beginTran();
            queryContext->transaction()->addOnTransactionCompletionHandler(
                std::bind(&TransactionLog::onDbTransactionCompleted, this,
                    queryContext, systemId, std::placeholders::_1));
        }).first;
    return newElementIter->second;
}

void TransactionLog::updateTimestampHiInCache(
    nx::db::QueryContext* queryContext,
    const nx::String& systemId,
    quint64 newValue)
{
    QnMutexLocker lk(&m_mutex);
    m_systemIdToTransactionLog[systemId].updateTimestampSequence(
        getDbTransactionContext(lk, queryContext, systemId).cacheTranId, newValue);
}

} // namespace ec2
} // namespace cdb
} // namespace nx
