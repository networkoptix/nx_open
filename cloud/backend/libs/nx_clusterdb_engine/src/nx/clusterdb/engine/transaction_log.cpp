#include "transaction_log.h"

#include <nx/sql/query.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/time.h>

#include "outgoing_transaction_dispatcher.h"

namespace nx::clusterdb::engine {

const ReadCommandsFilter ReadCommandsFilter::kEmptyFilter = {
    std::nullopt,
    std::nullopt,
    std::numeric_limits<int>::max(),
    {}};

CommandLog::CommandLog(
    const QnUuid& peerId,
    const ProtocolVersionRange& supportedProtocolRange,
    nx::sql::AsyncSqlQueryExecutor* const dbManager,
    AbstractOutgoingCommandDispatcher* const outgoingTransactionDispatcher)
    :
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
            {
                QnMutexLocker lock(&m_mutex);
                getDbTransactionContext(lock, queryContext, systemId);
            }
            return dbOperationsFunc(queryContext);
        },
        std::move(onDbUpdateCompleted));
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

    NX_DEBUG(
        QnLog::EC2_TRAN_LOG.join(this),
        lm("systemId %1. Generated new command %2 (hash %3)")
            .args(systemId, toString(transactionSerializer->header()), transactionHash));

    // Saving transaction to the log.
    const auto result = saveToDb(
        queryContext,
        systemId,
        transactionSerializer->header(),
        transactionHash,
        transactionSerializer->serialize(
            Qn::SerializationFormat::UbjsonFormat,
            m_supportedProtocolRange.currentVersion()));
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
    CommandHeader header;

    int transactionSequence = 0;
    vms::api::Timestamp transactionTimestamp;
    std::tie(transactionSequence, transactionTimestamp) =
        generateNewTransactionAttributes(queryContext, systemId);

    // Generating transaction.
    // Filling transaction header.
    header.command = static_cast<::ec2::ApiCommand::Value>(commandCode);
    header.peerID = m_peerId;
    header.transactionType = ::ec2::TransactionType::Cloud;
    header.persistentInfo.dbID = QnUuid::fromArbitraryData(systemId);
    header.persistentInfo.sequence = transactionSequence;
    header.persistentInfo.timestamp = transactionTimestamp;

    return header;
}

vms::api::TranState CommandLog::getTransactionState(
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

    if (!filter.from)
        filter.from = vms::api::TranState{};

    if (!filter.to)
    {
        vms::api::PersistentIdData maxTranStateKey;
        maxTranStateKey.id = QnUuid::fromStringSafe(lit("{FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF}"));
        vms::api::TranState maxTranState;
        maxTranState.values.insert(std::move(maxTranStateKey), std::numeric_limits<qint32>::max());
        filter.to = std::move(maxTranState);
    }

    auto outputData = std::make_unique<TransactionReadResult>();
    auto outputDataPtr = outputData.get();
    m_dbManager->executeSelect(
        std::bind(
            &CommandLog::fetchTransactions, this,
            _1, systemId, filter, outputDataPtr),
        [completionHandler = std::move(completionHandler), outputData = std::move(outputData)](
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

vms::api::Timestamp CommandLog::generateTransactionTimestamp(
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
        vms::api::PersistentIdData(m_peerId, QnUuid::fromArbitraryData(systemId)),
        delta);
}

void CommandLog::setOnTransactionReceived(
    OnTransactionReceivedHandler handler)
{
    m_onTransactionReceivedHandler = std::move(handler);
}

nx::sql::DBResult CommandLog::fillCache()
{
    nx::utils::promise<nx::sql::DBResult> cacheFilledPromise;
    auto future = cacheFilledPromise.get_future();

    NX_DEBUG(this, lm("Filling tranaction log cache"));

    // Starting async operation.
    using namespace std::placeholders;
    m_dbManager->executeSelect(
        std::bind(&CommandLog::fetchTransactionState, this, _1),
        [&cacheFilledPromise](nx::sql::DBResult dbResult)
        {
            cacheFilledPromise.set_value(dbResult);
        });

    NX_DEBUG(this, lm("Tranaction log cache filled up"));

    // Waiting for completion.
    future.wait();
    return future.get();
}

nx::sql::DBResult CommandLog::fetchTransactionState(
    nx::sql::QueryContext* queryContext)
{
    NX_DEBUG(this, lm("Fetching transactions"));

    // TODO: #ak move to TransactionDataObject
    nx::sql::SqlQuery selectTransactionStateQuery(queryContext->connection());
    selectTransactionStateQuery.setForwardOnly(true);
    selectTransactionStateQuery.prepare(R"sql(
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

    try
    {
        selectTransactionStateQuery.exec();
    }
    catch (const std::exception& e)
    {
        NX_ERROR(QnLog::EC2_TRAN_LOG.join(this),
            lm("Error loading transaction log. %1").arg(e.what()));
        throw;
    }

    NX_DEBUG(this, lm("Fetched transactions"));

    std::string prevSystemId;
    int count = 0;
    while (selectTransactionStateQuery.next())
    {
        const auto systemId = selectTransactionStateQuery.value("system_id").toString().toStdString();
        const auto peerGuid = selectTransactionStateQuery.value("peer_guid").toString().toStdString();
        const auto dbGuid = selectTransactionStateQuery.value("db_guid").toString().toStdString();
        const int sequence = selectTransactionStateQuery.value("sequence").toInt();
        const nx::Buffer tranHash = selectTransactionStateQuery.value("tran_hash").toString().toLatin1();
        vms::api::Timestamp timestamp;
        timestamp.sequence = selectTransactionStateQuery.value("timestamp_hi").toLongLong();
        timestamp.ticks = selectTransactionStateQuery.value("timestamp").toLongLong();

        vms::api::PersistentIdData tranStateKey(
            QnUuid::fromStringSafe(peerGuid),
            QnUuid::fromStringSafe(dbGuid));

        QnMutexLocker lock(&m_mutex);
        TransactionLogContext* vmsTranLog = getTransactionLogContext(lock, systemId);

        vmsTranLog->cache.restoreTransaction(
            std::move(tranStateKey), sequence, tranHash, timestamp);

        if (systemId != prevSystemId)
        {
            // Switched to another system.
            prevSystemId = systemId;
        }

        ++count;
    }

    NX_DEBUG(this, lm("Restored transaction logs of %1 systems. %2 states total")
        .args(m_systemIdToTransactionLog.size(), count));

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult CommandLog::fetchTransactions(
    nx::sql::QueryContext* queryContext,
    const std::string& systemId,
    const ReadCommandsFilter& filter,
    TransactionReadResult* const outputData)
{
    // TODO: Taking into account maxTransactionsToReturn

    //QMap<QnTranStateKey, qint32> values
    vms::api::TranState currentState;
    {
        // Merging "from" with local state.
        QnMutexLocker lock(&m_mutex);
        TransactionLogContext* transactionLogBySystem = getTransactionLogContext(lock, systemId);
        // Using copy of current state since we must not hold mutex over sql request.
        currentState = transactionLogBySystem->cache.committedTransactionState();
    }

    outputData->resultCode = ResultCode::dbError;

    for (auto it = currentState.values.begin();
         it != currentState.values.end();
         ++it)
    {
        if (!filter.sources.empty() && !nx::utils::contains(filter.sources, it.key().id))
            continue;

        const auto dbResult = m_transactionDataObject->fetchTransactionsOfAPeerQuery(
            queryContext,
            systemId,
            it.key().id.toSimpleString(),
            it.key().persistentId.toSimpleString(),
            filter.from->values.value(it.key()),
            filter.to->values.value(it.key(), std::numeric_limits<qint32>::max()),
            &outputData->transactions);
        if (dbResult != nx::sql::DBResult::ok)
            return dbResult;
    }

    // TODO #ak currentState is not correct here since it can be limited by "to" and 
    // "maxTransactionsToReturn".
    outputData->state = currentState;
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
    const CommandHeader& commandHeader,
    const QByteArray& commandHash,
    const QByteArray& ubjsonData)
{
    NX_DEBUG(QnLog::EC2_TRAN_LOG,
        lm("systemId %1. Saving command %2 (hash %3) to log")
            .args(systemId, toString(commandHeader), commandHash));

    auto dbResult = m_transactionDataObject->insertOrReplaceTransaction(
        queryContext,
        {systemId, commandHeader, commandHash, ubjsonData});
    if (dbResult != nx::sql::DBResult::ok)
        return dbResult;

    // Modifying transaction log cache.
    QnMutexLocker lock(&m_mutex);
    DbTransactionContext& dbTranContext = getDbTransactionContext(lock, queryContext, systemId);

    getTransactionLogContext(lock, systemId)->cache.insertOrReplaceTransaction(
        dbTranContext.cacheTranId, commandHeader, commandHash);

    return nx::sql::DBResult::ok;
}

int CommandLog::generateNewTransactionSequence(
    const QnMutexLockerBase& lock,
    nx::sql::QueryContext* /*queryContext*/,
    const std::string& systemId)
{
    return getTransactionLogContext(lock, systemId)->cache.generateTransactionSequence(
        vms::api::PersistentIdData(m_peerId, QnUuid::fromArbitraryData(systemId)));
}

vms::api::Timestamp CommandLog::generateNewTransactionTimestamp(
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
    auto insertionPair = m_systemIdToTransactionLog.emplace(systemId, nullptr);
    if (insertionPair.second)
    {
        insertionPair.first->second = std::make_unique<TransactionLogContext>(
            systemId,
            m_outgoingTransactionDispatcher);
    }

    return insertionPair.first->second.get();
}

std::tuple<int, vms::api::Timestamp> CommandLog::generateNewTransactionAttributes(
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
