#pragma once

#include <atomic>
#include <tuple>
#include <vector>

#include <nx/network/buffer.h>
#include <nx/utils/data_structures/safe_map.h>
#include <nx/utils/log/log.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/mutex.h>

#include <nx/sql/async_sql_query_executor.h>

#include "compatible_ec2_protocol_version.h"
#include "command.h"
#include "dao/abstract_transaction_data_object.h"
#include "outgoing_transaction_sorter.h"
#include "result_code.h"
#include "serialization/transaction_serializer.h"
#include "serialization/ubjson_serialized_transaction.h"
#include "transaction_log_cache.h"
#include "transaction_transport_header.h"

namespace nx {

namespace db { class AsyncSqlQueryExecutor; } // namespace db

namespace data_sync_engine {

class AbstractOutgoingTransactionDispatcher;

QString toString(const CommandHeader& tran);

/**
 * Supports multiple transactions related to a single system at the same time.
 * In this case transactions will reported to AbstractOutgoingTransactionDispatcher
 * in ascending sequence order.
 *
 * @note Calls with the same nx::sql::QueryContext object MUST happen within single thread.
 */
class NX_DATA_SYNC_ENGINE_API TransactionLog
{
public:
    typedef nx::utils::MoveOnlyFunc<void()> NewTransactionHandler;
    typedef nx::utils::MoveOnlyFunc<void(
        ResultCode /*resultCode*/,
        std::vector<dao::TransactionLogRecord> /*serializedTransactions*/,
        vms::api::TranState /*readedUpTo*/)> TransactionsReadHandler;

    /**
     * Fills internal cache.
     * @throw std::runtime_error In case of failure to pre-fill data cache.
     */
    TransactionLog(
        const QnUuid& peerId,
        const ProtocolVersionRange& supportedProtocolRange,
        nx::sql::AsyncSqlQueryExecutor* const dbManager,
        AbstractOutgoingTransactionDispatcher* const outgoingTransactionDispatcher);

    /**
     * Begins SQL DB transaction and passes that to dbOperationsFunc.
     * @note nx::sql::DBResult::retryLater can be reported to onDbUpdateCompleted if
     *      there are already too many requests for transaction
     * @note In case of error dbUpdateFunc can be skipped
     */
    void startDbTransaction(
        const nx::String& systemId,
        nx::utils::MoveOnlyFunc<nx::sql::DBResult(nx::sql::QueryContext*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(nx::sql::DBResult)> onDbUpdateCompleted);

    /**
     * @note This call should be made only once when generating first transaction.
     */
    nx::sql::DBResult updateTimestampHiForSystem(
        nx::sql::QueryContext* connection,
        const nx::String& systemId,
        quint64 newValue);

    /**
     * If transaction is not needed (it can be late or something),
     *      db::DBResult::cancelled is returned.
     */
    template<typename CommandDescriptor>
    nx::sql::DBResult checkIfNeededAndSaveToLog(
        nx::sql::QueryContext* connection,
        const nx::String& systemId,
        const SerializableTransaction<typename CommandDescriptor::Data>& transaction)
    {
        const auto transactionHash = CommandDescriptor::hash(transaction.get().params);

        // Checking whether transaction should be saved or not.
        if (isShouldBeIgnored(
                connection,
                systemId,
                transaction.get(),
                transactionHash))
        {
            NX_DEBUG(
                QnLog::EC2_TRAN_LOG.join(this),
                lm("systemId %1. Transaction %2 (%3, hash %4) is skipped")
                    .arg(systemId).arg(CommandDescriptor::name).arg(transaction.get())
                    .arg(CommandDescriptor::hash(transaction.get().params)));


            // Returning nx::sql::DBResult::cancelled if transaction should be skipped.
            return nx::sql::DBResult::cancelled;
        }

        return saveToDb(
            connection,
            systemId,
            transaction.get(),
            transactionHash,
            transaction.serialize(Qn::UbjsonFormat, m_supportedProtocolRange.currentVersion()));
    }

    template<typename CommandDescriptor>
    nx::sql::DBResult generateTransactionAndSaveToLog(
        nx::sql::QueryContext* queryContext,
        const nx::String& systemId,
        typename CommandDescriptor::Data transactionData)
    {
        return saveLocalTransaction<CommandDescriptor>(
            queryContext,
            systemId,
            prepareLocalTransaction(
                queryContext,
                systemId,
                CommandDescriptor::code,
                std::move(transactionData)));
    }

    /**
     * This method should be used when generating new transactions.
     */
    template<typename CommandDescriptor>
    nx::sql::DBResult saveLocalTransaction(
        nx::sql::QueryContext* queryContext,
        const nx::String& systemId,
        Command<typename CommandDescriptor::Data> transaction)
    {
        TransactionLogContext* vmsTransactionLogData = nullptr;

        QnMutexLocker lock(&m_mutex);
        DbTransactionContext& dbTranContext =
            getDbTransactionContext(lock, queryContext, systemId);
        vmsTransactionLogData = getTransactionLogContext(lock, systemId);
        lock.unlock();

        const auto transactionHash = CommandDescriptor::hash(transaction.params);
        NX_DEBUG(
            QnLog::EC2_TRAN_LOG.join(this),
            lm("systemId %1. Generated new transaction %2 (%3, hash %4)")
                .arg(systemId).arg(CommandDescriptor::name)
                .arg(transaction).arg(transactionHash));


        // Serializing transaction.
        auto serializedTransaction = QnUbjson::serialized(transaction);

        // Saving transaction to the log.
        const auto result = saveToDb(
            queryContext,
            systemId,
            transaction,
            transactionHash,
            serializedTransaction);
        if (result != nx::sql::DBResult::ok)
            return result;

        auto transactionSerializer = std::make_unique<
            UbjsonSerializedTransaction<typename CommandDescriptor::Data>>(
                std::move(transaction),
                std::move(serializedTransaction),
                m_supportedProtocolRange.currentVersion());

        // Saving transactions, generated under current DB transaction,
        //  so that we can send "new transaction" notifications after commit.
        vmsTransactionLogData->outgoingTransactionsSorter.addTransaction(
            dbTranContext.cacheTranId,
            std::move(transactionSerializer));

        return nx::sql::DBResult::ok;
    }

    template<typename TransactionDataType>
    Command<TransactionDataType> prepareLocalTransaction(
        nx::sql::QueryContext* queryContext,
        const nx::String& systemId,
        int commandCode,
        TransactionDataType transactionData)
    {
        int transactionSequence = 0;
        vms::api::Timestamp transactionTimestamp;
        std::tie(transactionSequence, transactionTimestamp) =
            generateNewTransactionAttributes(queryContext, systemId);

        // Generating transaction.
        Command<TransactionDataType> transaction(m_peerId);
        // Filling transaction header.
        transaction.command = static_cast<::ec2::ApiCommand::Value>(commandCode);
        transaction.peerID = m_peerId;
        transaction.transactionType = ::ec2::TransactionType::Cloud;
        transaction.persistentInfo.dbID = QnUuid::fromArbitraryData(systemId);
        transaction.persistentInfo.sequence = transactionSequence;
        transaction.persistentInfo.timestamp = transactionTimestamp;
        transaction.params = std::move(transactionData);

        return transaction;
    }

    vms::api::TranState getTransactionState(const nx::String& systemId) const;

    /**
     * Asynchronously reads requested transactions from Db.
     * @param to State (transaction source id, sequence) to read up to. Boundary is inclusive
     * @param completionHandler is called within unspecified DB connection thread.
     * In case of high load request can be cancelled internaly.
     * In this case ResultCode::retryLater will be reported
     * @note If there more transactions then maxTransactionsToReturn then
     *      ResultCode::partialContent result code will be reported
     *      to the completionHandler.
     */
    void readTransactions(
        const nx::String& systemId,
        boost::optional<vms::api::TranState> from,
        boost::optional<vms::api::TranState> to,
        int maxTransactionsToReturn,
        TransactionsReadHandler completionHandler);

    void clearTransactionLogCacheForSystem(const nx::String& systemId);

    vms::api::Timestamp generateTransactionTimestamp(const nx::String& systemId);

    void shiftLocalTransactionSequence(
        const nx::String& systemId,
        int delta);

private:
    struct DbTransactionContext
    {
        VmsTransactionLogCache::TranId cacheTranId = VmsTransactionLogCache::InvalidTranId;
        nx::sql::QueryContext* queryContext = nullptr;
    };

    struct TransactionLogContext
    {
        VmsTransactionLogCache cache;
        OutgoingTransactionSorter outgoingTransactionsSorter;

        TransactionLogContext(
            const nx::String& systemId,
            AbstractOutgoingTransactionDispatcher* outgoingTransactionDispatcher)
            :
            outgoingTransactionsSorter(
                systemId,
                &cache,
                outgoingTransactionDispatcher)
        {
        }
    };

    typedef nx::utils::SafeMap<
        std::pair<nx::sql::QueryContext*, nx::String>,
        DbTransactionContext
    > DbTransactionContextMap;

    struct TransactionReadResult
    {
        ResultCode resultCode;
        std::vector<dao::TransactionLogRecord> transactions;
        /** (Read start state) + (readed transactions). */
        vms::api::TranState state;
    };

    const QnUuid m_peerId;
    const ProtocolVersionRange m_supportedProtocolRange;
    nx::sql::AsyncSqlQueryExecutor* const m_dbManager;
    AbstractOutgoingTransactionDispatcher* const m_outgoingTransactionDispatcher;
    mutable QnMutex m_mutex;
    DbTransactionContextMap m_dbTransactionContexts;
    std::map<nx::String, std::unique_ptr<TransactionLogContext>> m_systemIdToTransactionLog;
    std::atomic<std::uint64_t> m_transactionSequence;
    std::unique_ptr<dao::AbstractTransactionDataObject> m_transactionDataObject;

    /** Fills transaction state cache. */
    nx::sql::DBResult fillCache();
    nx::sql::DBResult fetchTransactionState(nx::sql::QueryContext* connection);
    /**
     * Selects transactions from DB by condition.
     */
    nx::sql::DBResult fetchTransactions(
        nx::sql::QueryContext* connection,
        const nx::String& systemId,
        const vms::api::TranState& from,
        const vms::api::TranState& to,
        int maxTransactionsToReturn,
        TransactionReadResult* const outputData);

    bool isShouldBeIgnored(
        nx::sql::QueryContext* connection,
        const nx::String& systemId,
        const CommandHeader& transaction,
        const QByteArray& transactionHash);

    nx::sql::DBResult saveToDb(
        nx::sql::QueryContext* connection,
        const nx::String& systemId,
        const CommandHeader& transaction,
        const QByteArray& transactionHash,
        const QByteArray& ubjsonData);

    int generateNewTransactionSequence(
        const QnMutexLockerBase& lock,
        nx::sql::QueryContext* connection,
        const nx::String& systemId);

    vms::api::Timestamp generateNewTransactionTimestamp(
        const QnMutexLockerBase& lock,
        VmsTransactionLogCache::TranId cacheTranId,
        const nx::String& systemId);

    void onDbTransactionCompleted(
        DbTransactionContextMap::iterator queryIterator,
        const nx::String& systemId,
        nx::sql::DBResult dbResult);

    DbTransactionContext& getDbTransactionContext(
        const QnMutexLockerBase& lock,
        nx::sql::QueryContext* const queryContext,
        const nx::String& systemId);

    TransactionLogContext* getTransactionLogContext(
        const QnMutexLockerBase& lock,
        const nx::String& systemId);

    std::tuple<int, vms::api::Timestamp> generateNewTransactionAttributes(
        nx::sql::QueryContext* queryContext,
        const nx::String& systemId);

    void updateTimestampHiInCache(
        nx::sql::QueryContext* queryContext,
        const nx::String& systemId,
        quint64 newValue);

    static ResultCode dbResultToApiResult(nx::sql::DBResult dbResult);
};

} // namespace data_sync_engine
} // namespace nx
