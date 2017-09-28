#include "transaction_log.h"

#include <QtSql/QSqlQuery>

#include <nx/utils/time.h>

#include "outgoing_transaction_dispatcher.h"
#include "../managers/managers_types.h"

namespace nx {
namespace cdb {
namespace ec2 {

QString toString(const ::ec2::QnAbstractTransaction& tran)
{
    return lm("seq %1, ts %2")
        .arg(tran.persistentInfo.sequence).arg(tran.persistentInfo.timestamp);
}

TransactionLog::TransactionLog(
    const QnUuid& peerId,
    nx::utils::db::AsyncSqlQueryExecutor* const dbManager,
    AbstractOutgoingTransactionDispatcher* const outgoingTransactionDispatcher)
:
    m_peerId(peerId),
    m_dbManager(dbManager),
    m_outgoingTransactionDispatcher(outgoingTransactionDispatcher),
    m_transactionSequence(0)
{
    m_transactionDataObject = dao::TransactionDataObjectFactory::create();

    if (fillCache() != nx::utils::db::DBResult::ok)
        throw std::runtime_error("Error loading transaction log from DB");
}

void TransactionLog::startDbTransaction(
    const nx::String& systemId,
    nx::utils::MoveOnlyFunc<nx::utils::db::DBResult(nx::utils::db::QueryContext*)> dbOperationsFunc,
    nx::utils::MoveOnlyFunc<void(nx::utils::db::QueryContext*, nx::utils::db::DBResult)> onDbUpdateCompleted)
{
    // TODO: monitoring request queue size and returning api::ResultCode::retryLater if exceeded

    m_dbManager->executeUpdate(
        [this, systemId, dbOperationsFunc = std::move(dbOperationsFunc)](
            nx::utils::db::QueryContext* queryContext) -> nx::utils::db::DBResult
        {
            {
                QnMutexLocker lock(&m_mutex);
                getDbTransactionContext(lock, queryContext, systemId);
            }
            return dbOperationsFunc(queryContext);
        },
        [this, systemId, onDbUpdateCompleted = std::move(onDbUpdateCompleted)](
            nx::utils::db::QueryContext* queryContext,
            nx::utils::db::DBResult dbResult)
        {
            onDbUpdateCompleted(queryContext, dbResult);
        });
}

nx::utils::db::DBResult TransactionLog::updateTimestampHiForSystem(
    nx::utils::db::QueryContext* queryContext,
    const nx::String& systemId,
    quint64 newValue)
{
    updateTimestampHiInCache(queryContext, systemId, newValue);

    const auto dbResult = m_transactionDataObject->updateTimestampHiForSystem(
        queryContext, systemId, newValue);
    if (dbResult != nx::utils::db::DBResult::ok)
        return dbResult;

    return nx::utils::db::DBResult::ok;
}

::ec2::QnTranState TransactionLog::getTransactionState(const nx::String& systemId) const
{
    QnMutexLocker lock(&m_mutex);
    const auto it = m_systemIdToTransactionLog.find(systemId);
    if (it == m_systemIdToTransactionLog.cend())
        return ::ec2::QnTranState();
    return it->second->cache.committedTransactionState();
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
        ::ec2::ApiPersistentIdData maxTranStateKey;
        maxTranStateKey.id = QnUuid::fromStringSafe(lit("{FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF}"));
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
            nx::utils::db::QueryContext* /*queryContext*/,
            nx::utils::db::DBResult dbResult)
        {
            completionHandler(
                dbResult == nx::utils::db::DBResult::ok
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

    QnMutexLocker lock(&m_mutex);
    m_systemIdToTransactionLog.erase(systemId);
}

::ec2::Timestamp TransactionLog::generateTransactionTimestamp(
    const nx::String& systemId)
{
    QnMutexLocker lock(&m_mutex);

    return generateNewTransactionTimestamp(
        lock,
        VmsTransactionLogCache::InvalidTranId,
        systemId);
}

void TransactionLog::shiftLocalTransactionSequence(
    const nx::String& systemId,
    int delta)
{
    QnMutexLocker lock(&m_mutex);
    return getTransactionLogContext(lock, systemId)->cache.shiftTransactionSequence(
        ::ec2::ApiPersistentIdData(m_peerId, guidFromArbitraryData(systemId)),
        delta);
}

nx::utils::db::DBResult TransactionLog::fillCache()
{
    nx::utils::promise<nx::utils::db::DBResult> cacheFilledPromise;
    auto future = cacheFilledPromise.get_future();

    // Starting async operation.
    using namespace std::placeholders;
    m_dbManager->executeSelect(
        std::bind(&TransactionLog::fetchTransactionState, this, _1),
        [&cacheFilledPromise](
            nx::utils::db::QueryContext* /*queryContext*/,
            nx::utils::db::DBResult dbResult)
        {
            cacheFilledPromise.set_value(dbResult);
        });

    // Waiting for completion.
    future.wait();
    return future.get();
}

nx::utils::db::DBResult TransactionLog::fetchTransactionState(
    nx::utils::db::QueryContext* queryContext)
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
        return nx::utils::db::DBResult::ioError;
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

        ::ec2::ApiPersistentIdData tranStateKey(
            QnUuid::fromStringSafe(peerGuid),
            QnUuid::fromStringSafe(dbGuid));

        QnMutexLocker lock(&m_mutex);
        TransactionLogContext* vmsTranLog = getTransactionLogContext(lock, systemId);

        vmsTranLog->cache.restoreTransaction(
            std::move(tranStateKey), sequence, tranHash, settingsTimestampHi, timestamp);

        if (systemId != prevSystemId)
        {
            // Switched to another system.
            prevSystemId = systemId;
        }
    }

    return nx::utils::db::DBResult::ok;
}

nx::utils::db::DBResult TransactionLog::fetchTransactions(
    nx::utils::db::QueryContext* queryContext,
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
        QnMutexLocker lock(&m_mutex);
        TransactionLogContext* transactionLogBySystem = getTransactionLogContext(lock, systemId);
        // Using copy of current state since we must not hold mutex over sql request.
        currentState = transactionLogBySystem->cache.committedTransactionState();
    }

    outputData->resultCode = api::ResultCode::dbError;

    for (auto it = currentState.values.begin();
         it != currentState.values.end();
         ++it)
    {
        const auto dbResult = m_transactionDataObject->fetchTransactionsOfAPeerQuery(
            queryContext,
            systemId,
            it.key().id.toSimpleString(),
            it.key().persistentId.toSimpleString(),
            from.values.value(it.key()),
            to.values.value(it.key(), std::numeric_limits<qint32>::max()),
            &outputData->transactions);
        if (dbResult != nx::utils::db::DBResult::ok)
            return dbResult;
    }

    // TODO #ak currentState is not correct here since it can be limited by "to" and "maxTransactionsToReturn"
    outputData->state = currentState;
    outputData->resultCode = api::ResultCode::ok;

    return nx::utils::db::DBResult::ok;
}

bool TransactionLog::isShouldBeIgnored(
    nx::utils::db::QueryContext* /*queryContext*/,
    const nx::String& systemId,
    const ::ec2::QnAbstractTransaction& tran,
    const QByteArray& hash)
{
    TransactionLogContext* vmsTransactionLog = nullptr;
    {
        QnMutexLocker lock(&m_mutex);
        vmsTransactionLog = getTransactionLogContext(lock, systemId);
    }

    return vmsTransactionLog->cache.isShouldBeIgnored(systemId, tran, hash);
}

nx::utils::db::DBResult TransactionLog::saveToDb(
    nx::utils::db::QueryContext* queryContext,
    const nx::String& systemId,
    const ::ec2::QnAbstractTransaction& transaction,
    const QByteArray& transactionHash,
    const QByteArray& ubjsonData)
{
    NX_LOG(
        QnLog::EC2_TRAN_LOG,
        lm("systemId %1. Saving transaction %2 (%3, hash %4) to log")
            .arg(systemId).arg(::ec2::ApiCommand::toString(transaction.command))
            .arg(transaction).arg(transactionHash),
        cl_logDEBUG1);

    auto dbResult = m_transactionDataObject->insertOrReplaceTransaction(
        queryContext,
        {systemId, transaction, transactionHash, ubjsonData});
    if (dbResult != nx::utils::db::DBResult::ok)
        return dbResult;

    // Modifying transaction log cache.
    QnMutexLocker lock(&m_mutex);
    DbTransactionContext& dbTranContext = getDbTransactionContext(lock, queryContext, systemId);

    getTransactionLogContext(lock, systemId)->cache.insertOrReplaceTransaction(
        dbTranContext.cacheTranId, transaction, transactionHash);

    return nx::utils::db::DBResult::ok;
}

int TransactionLog::generateNewTransactionSequence(
    const QnMutexLockerBase& lock,
    nx::utils::db::QueryContext* /*queryContext*/,
    const nx::String& systemId)
{
    return getTransactionLogContext(lock, systemId)->cache.generateTransactionSequence(
        ::ec2::ApiPersistentIdData(m_peerId, guidFromArbitraryData(systemId)));
}

::ec2::Timestamp TransactionLog::generateNewTransactionTimestamp(
    const QnMutexLockerBase& lock,
    VmsTransactionLogCache::TranId cacheTranId,
    const nx::String& systemId)
{
    using namespace std::chrono;

    TransactionLogContext* transactionLogData = getTransactionLogContext(lock, systemId);
    return transactionLogData->cache.generateTransactionTimestamp(cacheTranId);
}

void TransactionLog::onDbTransactionCompleted(
    nx::utils::db::QueryContext* queryContext,
    const nx::String& systemId,
    nx::utils::db::DBResult dbResult)
{
    DbTransactionContext currentDbTranContext =
        m_dbTransactionContexts.take(std::make_pair(queryContext, systemId));
    TransactionLogContext* vmsTransactionLogData = nullptr;
    {
        QnMutexLocker lock(&m_mutex);
        vmsTransactionLogData = getTransactionLogContext(lock, systemId);
    }

    if (dbResult != nx::utils::db::DBResult::ok)
    {
        // Rolling back transaction log change.
        vmsTransactionLogData->outgoingTransactionsSorter.rollback(
            currentDbTranContext.cacheTranId);
        return;
    }

    vmsTransactionLogData->outgoingTransactionsSorter.commit(
        currentDbTranContext.cacheTranId);
}

TransactionLog::DbTransactionContext& TransactionLog::getDbTransactionContext(
    const QnMutexLockerBase& lock,
    nx::utils::db::QueryContext* const queryContext,
    const nx::String& systemId)
{
    auto newElementIter = m_dbTransactionContexts.emplace(
        std::make_pair(queryContext, systemId),
        DbTransactionContext(),
        [this, &lock, &systemId, &queryContext](DbTransactionContextMap::iterator newElementIter)
        {
            newElementIter->second.cacheTranId =
                getTransactionLogContext(lock, systemId)->cache.beginTran();
            queryContext->transaction()->addOnTransactionCompletionHandler(
                std::bind(&TransactionLog::onDbTransactionCompleted, this,
                    queryContext, systemId, std::placeholders::_1));
        }).first;
    return newElementIter->second;
}

TransactionLog::TransactionLogContext* TransactionLog::getTransactionLogContext(
    const QnMutexLockerBase& /*lock*/,
    const nx::String& systemId)
{
    auto insertionPair = m_systemIdToTransactionLog.emplace(systemId, nullptr);
    if (insertionPair.second)
    {
        insertionPair.first->second = std::make_unique<TransactionLogContext>(
            systemId,
            m_outgoingTransactionDispatcher);
    }

    return insertionPair.first->second.get();
}

std::tuple<int, ::ec2::Timestamp> TransactionLog::generateNewTransactionAttributes(
    nx::utils::db::QueryContext* queryContext,
    const nx::String& systemId)
{
    QnMutexLocker lock(&m_mutex);

    DbTransactionContext& dbTranContext = getDbTransactionContext(lock, queryContext, systemId);
    TransactionLogContext* vmsTransactionLogData = getTransactionLogContext(lock, systemId);

    const int transactionSequence = generateNewTransactionSequence(lock, queryContext, systemId);
    vmsTransactionLogData->outgoingTransactionsSorter.registerTransactionSequence(
        dbTranContext.cacheTranId,
        transactionSequence);

    auto transactionTimestamp = generateNewTransactionTimestamp(
        lock, dbTranContext.cacheTranId, systemId);

    return std::tie(transactionSequence, transactionTimestamp);
}

void TransactionLog::updateTimestampHiInCache(
    nx::utils::db::QueryContext* queryContext,
    const nx::String& systemId,
    quint64 newValue)
{
    QnMutexLocker lock(&m_mutex);
    getTransactionLogContext(lock, systemId)->cache.updateTimestampSequence(
        getDbTransactionContext(lock, queryContext, systemId).cacheTranId, newValue);
}

} // namespace ec2
} // namespace cdb
} // namespace nx
