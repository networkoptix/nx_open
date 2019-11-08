#include "command_log.h"

#include <nx/sql/query.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/time.h>

#include "outgoing_command_dispatcher.h"

namespace nx::clusterdb::engine {

const ReadCommandsFilter ReadCommandsFilter::kEmptyFilter = {
    std::nullopt,
    std::nullopt,
    std::numeric_limits<int>::max(),
    {}};

CommandLog::CommandLog(
    const SynchronizationSettings& settings,
    const QnUuid& peerId,
    const ProtocolVersionRange& supportedProtocolRange,
    nx::sql::AbstractAsyncSqlQueryExecutor* const dbManager,
    AbstractOutgoingCommandDispatcher* const outgoingTransactionDispatcher)
    :
    m_settings(settings),
    m_peerId(peerId),
    m_supportedProtocolRange(supportedProtocolRange),
    m_dbManager(dbManager),
    m_outgoingTransactionDispatcher(outgoingTransactionDispatcher),
    m_transactionSequence(0)
{
    m_transactionDataObject = dao::TransactionDataObjectFactory::instance().create(
        supportedProtocolRange.currentVersion());

    if (fillCache() != nx::sql::DBResult::ok)
        throw std::runtime_error("Error loading transaction log from DB");
}

CommandLog::~CommandLog()
{
    pleaseStopSync();
}

void CommandLog::startDbTransaction(
    const std::string& systemId,
    nx::utils::MoveOnlyFunc<nx::sql::DBResult(nx::sql::QueryContext*)> dbOperationsFunc,
    nx::utils::MoveOnlyFunc<void(nx::sql::DBResult)> onDbUpdateCompleted)
{
    // TODO: monitoring request queue size and returning ResultCode::retryLater if exceeded
    m_dbManager->executeUpdate(
        [this, systemId, dbOperationsFunc = std::move(dbOperationsFunc)](
            nx::sql::QueryContext* queryContext) -> nx::sql::DBResult
        {
            // TODO: #ak This DB transaction is supposed for queries accessing systemId data only.
            // There should be some kind of a validation (e.g., asserts in some methods of this class).

            {
                QnMutexLocker lock(&m_mutex);
                getDbTransactionContext(lock, queryContext, systemId);
            }

            return dbOperationsFunc(queryContext);
        },
        [lock = m_startedAsyncCallsCounter.getScopedIncrement(),
            onDbUpdateCompleted = std::move(onDbUpdateCompleted)](nx::sql::DBResult dbResult)
        {
            onDbUpdateCompleted(dbResult);
        },
        m_settings.groupCommandsUnderDbTransaction ? "commands_group" : "");
}

nx::sql::DBResult CommandLog::updateTimestampHiForSystem(
    nx::sql::QueryContext* queryContext,
    const std::string& systemId,
    quint64 newValue)
{
    updateTimestampHiInCache(queryContext, systemId, newValue);

    const auto dbResult = m_transactionDataObject->updateTimestampHiForSystem(
        queryContext, systemId, newValue);
    if (dbResult != nx::sql::DBResult::ok)
        return dbResult;

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult CommandLog::saveLocalTransaction(
    nx::sql::QueryContext* queryContext,
    const std::string& systemId,
    const nx::Buffer& transactionHash,
    std::unique_ptr<SerializableAbstractCommand> transactionSerializer)
{
    TransactionLogContext* vmsTransactionLogData = nullptr;

    QnMutexLocker lock(&m_mutex);
    DbTransactionContext& dbTranContext =
        getDbTransactionContext(lock, queryContext, systemId);
    vmsTransactionLogData = getTransactionLogContext(lock, systemId);
    lock.unlock();

    NX_DEBUG(this, "systemId %1. Generated new command %2 (hash %3)",
        systemId, transactionSerializer->header(), transactionHash);

    // Saving transaction to the log.
    auto result = saveToDb(
        queryContext,
        systemId,
        transactionHash,
        *transactionSerializer);
    if (result != nx::sql::DBResult::ok)
        return result;

    result = saveActualSequence(queryContext, systemId);
    if (result != nx::sql::DBResult::ok)
        return result;

    // Saving transactions, generated under current DB transaction,
    //  so that we can send "new transaction" notifications after commit.
    vmsTransactionLogData->outgoingTransactionsSorter.addTransaction(
        dbTranContext.cacheTranId,
        std::move(transactionSerializer));

    return nx::sql::DBResult::ok;
}

CommandHeader CommandLog::prepareLocalTransactionHeader(
    nx::sql::QueryContext* queryContext,
    const std::string& systemId,
    int commandCode)
{
    int transactionSequence = 0;
    Timestamp transactionTimestamp;
    std::tie(transactionSequence, transactionTimestamp) =
        generateNewTransactionAttributes(queryContext, systemId);

    // Generating transaction.
    // Filling transaction header.
    CommandHeader header(commandCode, m_peerId);
    header.persistentInfo.dbID = QnUuid::fromArbitraryData(systemId);
    header.persistentInfo.sequence = transactionSequence;
    header.persistentInfo.timestamp = transactionTimestamp;

    return header;
}

NodeState CommandLog::getTransactionState(
    const std::string& systemId) const
{
    QnMutexLocker lock(&m_mutex);
    const auto it = m_systemIdToTransactionLog.find(systemId);
    if (it == m_systemIdToTransactionLog.cend())
        return {};
    return it->second->cache.committedTransactionState();
}

void CommandLog::readTransactions(
    const std::string& systemId,
    ReadCommandsFilter filter,
    TransactionsReadHandler completionHandler)
{
    using namespace std::placeholders;

    auto outputData = std::make_unique<TransactionReadResult>();
    auto outputDataPtr = outputData.get();
    m_dbManager->executeSelect(
        std::bind(
            &CommandLog::fetchTransactions, this,
            _1, systemId, filter, outputDataPtr),
        [completionHandler = std::move(completionHandler), outputData = std::move(outputData),
            lock = m_startedAsyncCallsCounter.getScopedIncrement()](
                nx::sql::DBResult dbResult)
        {
            completionHandler(
                dbResult == nx::sql::DBResult::ok
                    ? outputData->resultCode
                    : dbResultToApiResult(dbResult),
                std::move(outputData->transactions),
                std::move(outputData->state));
        });
}

void CommandLog::markSystemForDeletion(const std::string& systemId)
{
    NX_VERBOSE(this, lm("Marking system %1 for deletion").arg(systemId));

    QnMutexLocker lock(&m_mutex);
    m_systemsMarkedForDeletion.push_back(systemId);
}

Timestamp CommandLog::generateTransactionTimestamp(
    const std::string& systemId)
{
    QnMutexLocker lock(&m_mutex);

    return generateNewTransactionTimestamp(
        lock,
        CommandLogCache::InvalidTranId,
        systemId);
}

void CommandLog::shiftLocalTransactionSequence(
    const std::string& systemId,
    int delta)
{
    QnMutexLocker lock(&m_mutex);
    return getTransactionLogContext(lock, systemId)->cache.shiftTransactionSequence(
        NodeStateKey{m_peerId, QnUuid::fromArbitraryData(systemId)},
        delta);
}

void CommandLog::setOnTransactionReceived(
    OnTransactionReceivedHandler handler)
{
    m_onTransactionReceivedHandler = std::move(handler);
}

void CommandLog::pleaseStopSync()
{
    m_startedAsyncCallsCounter.wait();
}

nx::sql::DBResult CommandLog::fillCache()
{
    nx::utils::promise<nx::sql::DBResult> cacheFilledPromise;
    auto future = cacheFilledPromise.get_future();

    NX_DEBUG(this, "Filling transaction log cache");

    try
    {
        m_dbManager->executeSelectQuerySync(
            [this](nx::sql::QueryContext* queryContext)
            {
                fetchTransactionState(queryContext);
                restoreTransactionSequence(queryContext);
                // TODO: #ak Remove when executeSelectQuerySync supports void as return type.
                return /*dummy*/ 0;
            });

        NX_DEBUG(this, "Transaction log cache filled up");
        return nx::sql::DBResult::ok;
    }
    catch (const nx::sql::Exception& e)
    {
        NX_DEBUG(this, "Error filling in transaction log cache. %1", e.what());
        return e.dbResult();
    }
}

void CommandLog::fetchTransactionState(nx::sql::QueryContext* queryContext)
{
    NX_DEBUG(this, lm("Fetching transactions"));

    // TODO: #ak move to TransactionDataObject
    nx::sql::SqlQuery selectTransactionStateQuery(queryContext->connection());
    selectTransactionStateQuery.setForwardOnly(true);
    selectTransactionStateQuery.prepare(R"sql(
        SELECT system_id,
               peer_guid,
               db_guid,
               sequence,
               tran_hash,
               timestamp_hi,
               timestamp
        FROM transaction_log
        ORDER BY system_id, timestamp_hi, timestamp DESC
        )sql");

    selectTransactionStateQuery.exec();

    NX_DEBUG(this, lm("Fetched transactions"));

    int count = 0;
    while (selectTransactionStateQuery.next())
    {
        const auto systemId = selectTransactionStateQuery.value(0).toString().toStdString();
        const auto peerGuid = selectTransactionStateQuery.value(1).toString();
        const auto dbGuid = selectTransactionStateQuery.value(2).toString();
        const int sequence = selectTransactionStateQuery.value(3).toInt();
        const nx::Buffer tranHash = selectTransactionStateQuery.value(4).toString().toLatin1();

        Timestamp timestamp;
        timestamp.sequence = selectTransactionStateQuery.value(5).toLongLong();
        timestamp.ticks = selectTransactionStateQuery.value(6).toLongLong();

        NodeStateKey tranStateKey{
            QnUuid::fromStringSafe(peerGuid),
            QnUuid::fromStringSafe(dbGuid)};

        QnMutexLocker lock(&m_mutex);

        TransactionLogContext* vmsTranLog = getTransactionLogContext(lock, systemId);
        vmsTranLog->cache.restoreTransaction(
            std::move(tranStateKey), sequence, tranHash, timestamp);

        ++count;
    }

    NX_DEBUG(this, lm("Restored transaction logs of %1 systems. %2 states total")
        .args(m_systemIdToTransactionLog.size(), count));
}

void CommandLog::restoreTransactionSequence(nx::sql::QueryContext* queryContext)
{
    // Restoring only sequence of the current peer.
    const auto currentPeerSequenceBySystemId =
        m_transactionDataObject->fetchRecentTransactionSequence(
            queryContext,
            m_peerId.toSimpleString().toStdString());

    QnMutexLocker lock(&m_mutex);

    for (const auto& systemIdAndSequence: currentPeerSequenceBySystemId)
    {
        NodeStateKey nodeKey{
            m_peerId, QnUuid::fromArbitraryData(systemIdAndSequence.first)};

        auto vmsTranLog = getTransactionLogContext(lock, systemIdAndSequence.first);
        vmsTranLog->cache.shiftTransactionSequenceTo(nodeKey, systemIdAndSequence.second);
    }
}

nx::sql::DBResult CommandLog::fetchTransactions(
    nx::sql::QueryContext* queryContext,
    const std::string& systemId,
    const ReadCommandsFilter& filter,
    TransactionReadResult* const outputData)
{
    // TODO: Taking into account maxTransactionsToReturn

    NodeState currentState;
    {
        // Merging "from" with local state.
        QnMutexLocker lock(&m_mutex);
        TransactionLogContext* transactionLogBySystem = getTransactionLogContext(lock, systemId);
        // Using copy of current state since we must not hold mutex over sql request.
        currentState = transactionLogBySystem->cache.committedTransactionState();
    }

    outputData->resultCode = ResultCode::dbError;

    outputData->state = {};

    for (auto it = currentState.nodeSequence.begin();
         it != currentState.nodeSequence.end();
         ++it)
    {
        if (!filter.sources.empty() && !nx::utils::contains(filter.sources, it->first.nodeId))
            continue;

        const auto minSequence =
            filter.from ? filter.from->sequence(it->first) : 0;
        const auto maxSequence =
            filter.to ? filter.to->sequence(it->first, it->second) : it->second;
        const auto dbResult = m_transactionDataObject->fetchTransactionsOfAPeerQuery(
            queryContext,
            systemId,
            it->first.nodeId.toSimpleString(),
            it->first.dbId.toSimpleString(),
            minSequence,
            maxSequence,
            &outputData->transactions);
        if (dbResult != nx::sql::DBResult::ok)
            return dbResult;

        outputData->state.nodeSequence[it->first] = maxSequence;
    }

    outputData->resultCode = ResultCode::ok;

    return nx::sql::DBResult::ok;
}

bool CommandLog::isShouldBeIgnored(
    nx::sql::QueryContext* /*queryContext*/,
    const std::string& systemId,
    const CommandHeader& tran,
    const QByteArray& hash)
{
    TransactionLogContext* vmsTransactionLog = nullptr;
    {
        QnMutexLocker lock(&m_mutex);
        vmsTransactionLog = getTransactionLogContext(lock, systemId);
    }

    return vmsTransactionLog->cache.isShouldBeIgnored(systemId, tran, hash);
}

nx::sql::DBResult CommandLog::saveToDb(
    nx::sql::QueryContext* queryContext,
    const std::string& systemId,
    const QByteArray& commandHash,
    const SerializableAbstractCommand& transactionSerializer)
{
    const auto& commandHeader = transactionSerializer.header();
    const auto ubjsonData = transactionSerializer.serialize(
        Qn::SerializationFormat::UbjsonFormat,
        m_supportedProtocolRange.currentVersion());

    NX_DEBUG(this, "systemId %1. Saving command %2 (hash %3) to log",
        systemId, commandHeader, commandHash);

    auto dbResult = m_transactionDataObject->insertOrReplaceTransaction(
        queryContext,
        {systemId, commandHeader, commandHash, ubjsonData});
    if (dbResult != nx::sql::DBResult::ok)
        return dbResult;

    // Modifying transaction log cache.
    QnMutexLocker lock(&m_mutex);
    DbTransactionContext& dbTranContext = getDbTransactionContext(lock, queryContext, systemId);

    TransactionLogContext* vmsTransactionLogData = getTransactionLogContext(lock, systemId);

    vmsTransactionLogData->cache.insertOrReplaceTransaction(
        dbTranContext.cacheTranId, commandHeader, commandHash);

    return nx::sql::DBResult::ok;
}

int CommandLog::generateNewTransactionSequence(
    const QnMutexLockerBase& lock,
    nx::sql::QueryContext* /*queryContext*/,
    const std::string& systemId)
{
    return getTransactionLogContext(lock, systemId)->cache.generateTransactionSequence(
        NodeStateKey{m_peerId, QnUuid::fromArbitraryData(systemId)});
}

Timestamp CommandLog::generateNewTransactionTimestamp(
    const QnMutexLockerBase& lock,
    CommandLogCache::TranId cacheTranId,
    const std::string& systemId)
{
    using namespace std::chrono;

    TransactionLogContext* transactionLogData = getTransactionLogContext(lock, systemId);
    return transactionLogData->cache.generateTransactionTimestamp(cacheTranId);
}

void CommandLog::onDbTransactionCompleted(
    DbTransactionContextMap::iterator queryIterator,
    const std::string& systemId,
    nx::sql::DBResult dbResult)
{
    DbTransactionContext currentDbTranContext =
        m_dbTransactionContexts.take(queryIterator);
    TransactionLogContext* vmsTransactionLogData = nullptr;
    {
        QnMutexLocker lock(&m_mutex);
        vmsTransactionLogData = getTransactionLogContext(lock, systemId);
    }

    if (dbResult != nx::sql::DBResult::ok)
    {
        // Rolling back transaction log change.
        vmsTransactionLogData->outgoingTransactionsSorter.rollback(
            currentDbTranContext.cacheTranId);
        return;
    }

    vmsTransactionLogData->outgoingTransactionsSorter.commit(
        currentDbTranContext.cacheTranId);

    QnMutexLocker lock(&m_mutex);
    removeSystemsMarkedForDeletion(lock);
}

CommandLog::DbTransactionContext& CommandLog::getDbTransactionContext(
    const QnMutexLockerBase& lock,
    nx::sql::QueryContext* const queryContext,
    const std::string& systemId)
{
    auto initializeNewElement =
        [this, &lock, &systemId, queryContext](
            DbTransactionContextMap::iterator newElementIter)
        {
            newElementIter->second.cacheTranId =
                getTransactionLogContext(lock, systemId)->cache.beginTran();

            queryContext->transaction()->addOnTransactionCompletionHandler(
                std::bind(&CommandLog::onDbTransactionCompleted, this,
                    newElementIter, systemId, std::placeholders::_1));
        };

    auto newElementIter = m_dbTransactionContexts.emplace(
        std::make_pair(queryContext, systemId),
        DbTransactionContext{CommandLogCache::InvalidTranId, queryContext},
        initializeNewElement).first;
    return newElementIter->second;
}

CommandLog::TransactionLogContext* CommandLog::getTransactionLogContext(
    const QnMutexLockerBase& /*lock*/,
    const std::string& systemId)
{
    auto& ctx = m_systemIdToTransactionLog[systemId];
    if (!ctx)
    {
        ctx = std::make_unique<TransactionLogContext>(
            systemId,
            m_outgoingTransactionDispatcher);
    }

    return ctx.get();
}

std::tuple</*command sequence*/ int, Timestamp> CommandLog::generateNewTransactionAttributes(
    nx::sql::QueryContext* queryContext,
    const std::string& systemId)
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

void CommandLog::updateTimestampHiInCache(
    nx::sql::QueryContext* queryContext,
    const std::string& systemId,
    quint64 newValue)
{
    QnMutexLocker lock(&m_mutex);
    getTransactionLogContext(lock, systemId)->cache.updateTimestampSequence(
        getDbTransactionContext(lock, queryContext, systemId).cacheTranId, newValue);
}

nx::sql::DBResult CommandLog::saveActualSequence(
    nx::sql::QueryContext* queryContext,
    const std::string& systemId)
{
    int sequence = 0;

    {
        QnMutexLocker lock(&m_mutex);
        sequence = getTransactionLogContext(lock, systemId)->cache.lastTransactionSequence(
            NodeStateKey{m_peerId, QnUuid::fromArbitraryData(systemId)});
    }

    m_transactionDataObject->saveRecentTransactionSequence(
        queryContext,
        systemId,
        m_peerId.toSimpleString().toStdString(),
        sequence);

    return nx::sql::DBResult::ok;
}

void CommandLog::removeSystemsMarkedForDeletion(
    const QnMutexLockerBase& lock)
{
    for (auto it = m_systemsMarkedForDeletion.begin();
        it != m_systemsMarkedForDeletion.end();
        )
    {
        if (clearTransactionLogCacheForSystem(lock, *it))
            it = m_systemsMarkedForDeletion.erase(it);
        else
            ++it;
    }
}

bool CommandLog::clearTransactionLogCacheForSystem(
    const QnMutexLockerBase& /*lock*/,
    const std::string& systemId)
{
    NX_VERBOSE(this, lm("Cleaning transaction log for system %1").arg(systemId));

    auto systemLogIter = m_systemIdToTransactionLog.find(systemId);
    if (systemLogIter == m_systemIdToTransactionLog.end())
        return true;

    if (systemLogIter->second->cache.activeTransactionCount() == 0)
    {
        m_systemIdToTransactionLog.erase(systemLogIter);
        return true;
    }

    // There are active transactions. Will wait for them to finish.
    return false;
}

ResultCode CommandLog::dbResultToApiResult(
    nx::sql::DBResult dbResult)
{
    switch (dbResult)
    {
        case nx::sql::DBResult::ok:
        case nx::sql::DBResult::endOfData:
            return ResultCode::ok;

        case nx::sql::DBResult::notFound:
            return ResultCode::notFound;

        case nx::sql::DBResult::cancelled:
            return ResultCode::retryLater;

        case nx::sql::DBResult::ioError:
        case nx::sql::DBResult::statementError:
        case nx::sql::DBResult::connectionError:
            return ResultCode::dbError;

        case nx::sql::DBResult::retryLater:
            return ResultCode::retryLater;

        default:
            return ResultCode::dbError;
    }
}

} // namespace nx::clusterdb::engine
