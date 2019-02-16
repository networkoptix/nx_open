#pragma once

#include <atomic>
#include <tuple>
#include <vector>

#include <nx/network/buffer.h>
#include <nx/utils/data_structures/safe_map.h>
#include <nx/utils/log/log.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/optional.h>
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

namespace nx::clusterdb::engine {

class AbstractOutgoingCommandDispatcher;

struct NX_DATA_SYNC_ENGINE_API ReadCommandsFilter
{
    std::optional<vms::api::TranState> from;
    std::optional<vms::api::TranState> to;
    int maxTransactionsToReturn = 0;
    /** List of command source peers. If empty, then command source is not restricted. */
    std::vector<QnUuid> sources;

    static const ReadCommandsFilter kEmptyFilter;
};

/**
 * Supports multiple transactions related to a single system at the same time.
 * In this case transactions will reported to AbstractOutgoingCommandDispatcher
 * in ascending sequence order.
 *
 * @note Calls with the same nx::sql::QueryContext object MUST happen within single thread.
 */
class NX_DATA_SYNC_ENGINE_API CommandLog
{
public:
    using NewTransactionHandler = nx::utils::MoveOnlyFunc<void()>;

    using TransactionsReadHandler = nx::utils::MoveOnlyFunc<void(
        ResultCode /*resultCode*/,
        std::vector<dao::TransactionLogRecord> /*serializedTransactions*/,
        vms::api::TranState /*readedUpTo*/)>;

    using OnTransactionReceivedHandler = nx::utils::MoveOnlyFunc<
        nx::sql::DBResult(
            nx::sql::QueryContext* /*queryContext*/,
            const std::string& /*systemId*/,
            const nx::clusterdb::engine::EditableSerializableCommand& /*transaction*/)>;

    /**
     * Fills internal cache.
     * @throw std::runtime_error In case of failure to pre-fill data cache.
     */
    CommandLog(
        const QnUuid& peerId,
        const ProtocolVersionRange& supportedProtocolRange,
        nx::sql::AsyncSqlQueryExecutor* const dbManager,
        AbstractOutgoingCommandDispatcher* const outgoingTransactionDispatcher);

    /**
     * Begins SQL DB transaction and passes that to dbOperationsFunc.
     * @note nx::sql::DBResult::retryLater can be reported to onDbUpdateCompleted if
     *      there are already too many requests for transaction
     * @note In case of error dbUpdateFunc can be skipped
     */
    void startDbTransaction(
        const std::string& systemId,
        nx::utils::MoveOnlyFunc<nx::sql::DBResult(nx::sql::QueryContext*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(nx::sql::DBResult)> onDbUpdateCompleted);

    /**
     * @note This call should be made only once when generating first transaction.
     */
    nx::sql::DBResult updateTimestampHiForSystem(
        nx::sql::QueryContext* connection,
        const std::string& systemId,
        quint64 newValue);

    /**
     * If transaction is not needed (it can be late or something),
     *      db::DBResult::cancelled is returned.
     */
    template<typename CommandDescriptor>
    nx::sql::DBResult checkIfNeededAndSaveToLog(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        const SerializableCommand<CommandDescriptor>& transaction)
    {
        const auto transactionHash = CommandDescriptor::hash(transaction.get().params);

        // Checking whether transaction should be saved or not.
        if (isShouldBeIgnored(
                queryContext,
                systemId,
                transaction.get(),
                transactionHash))
        {
            NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
                lm("systemId %1. Command (%2, hash %3) is skipped")
                    .args(systemId, engine::toString(transaction.header()),
                        CommandDescriptor::hash(transaction.get().params)));

            // Returning nx::sql::DBResult::cancelled if transaction should be skipped.
            return nx::sql::DBResult::cancelled;
        }

        auto resultCode = saveToDb(
            queryContext,
            systemId,
            transactionHash,
            transaction);
        if (resultCode != nx::sql::DBResult::ok)
            return resultCode;

        resultCode = invokeExternalProcessor<CommandDescriptor>(
            queryContext,
            systemId,
            transaction);
        if (resultCode != nx::sql::DBResult::ok)
            return resultCode;

        // TODO: #ak Get rid of transaction.clone().
        queryContext->transaction()->addOnSuccessfulCommitHandler(
            [this, systemId, commandSerializer = transaction.clone()]() mutable
            {
                m_outgoingTransactionDispatcher->dispatchTransaction(
                    systemId,
                    std::move(commandSerializer));
            });

        return nx::sql::DBResult::ok;
    }

    template<typename CommandDescriptor>
    nx::sql::DBResult generateTransactionAndSaveToLog(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        typename CommandDescriptor::Data transactionData)
    {
        auto transaction = prepareLocalTransaction(
            queryContext,
            systemId,
            CommandDescriptor::code,
            std::move(transactionData));

        const auto transactionHash = CommandDescriptor::hash(transaction.params);
        auto transactionSerializer = std::make_unique<
            UbjsonSerializedTransaction<CommandDescriptor>>(
                std::move(transaction),
                m_supportedProtocolRange.currentVersion());

        return saveLocalTransaction(
            queryContext,
            systemId,
            transactionHash,
            std::move(transactionSerializer));
    }

    /**
     * This method should be used when generating new transactions.
     */
    nx::sql::DBResult saveLocalTransaction(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        const nx::Buffer& transactionHash,
        std::unique_ptr<SerializableAbstractCommand> transactionSerializer);

    template<typename TransactionSerializerType>
    nx::sql::DBResult saveLocalTransaction(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        std::unique_ptr<TransactionSerializerType> transactionSerializer)
    {
        const auto hash = transactionSerializer->hash();
        return saveLocalTransaction(queryContext, systemId, hash, std::move(transactionSerializer));
    }

    template<typename TransactionDataType>
    Command<TransactionDataType> prepareLocalTransaction(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        int commandCode,
        TransactionDataType transactionData)
    {
        Command<TransactionDataType> transaction;
        transaction.setHeader(
            prepareLocalTransactionHeader(queryContext, systemId, commandCode));
        transaction.params = std::move(transactionData);

        return transaction;
    }

    CommandHeader prepareLocalTransactionHeader(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        int commandCode);

    vms::api::TranState getTransactionState(const std::string& systemId) const;

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
        const std::string& systemId,
        ReadCommandsFilter filter,
        TransactionsReadHandler completionHandler);

    void markSystemForDeletion(const std::string& systemId);

    vms::api::Timestamp generateTransactionTimestamp(const std::string& systemId);

    void shiftLocalTransactionSequence(
        const std::string& systemId,
        int delta);

    void setOnTransactionReceived(OnTransactionReceivedHandler handler);

private:
    struct DbTransactionContext
    {
        CommandLogCache::TranId cacheTranId = CommandLogCache::InvalidTranId;
        nx::sql::QueryContext* queryContext = nullptr;
    };

    struct TransactionLogContext
    {
        CommandLogCache cache;
        OutgoingTransactionSorter outgoingTransactionsSorter;

        TransactionLogContext(
            const std::string& systemId,
            AbstractOutgoingCommandDispatcher* outgoingTransactionDispatcher)
            :
            outgoingTransactionsSorter(
                systemId,
                &cache,
                outgoingTransactionDispatcher)
        {
        }
    };

    typedef nx::utils::SafeMap<
        std::pair<nx::sql::QueryContext*, std::string /*systemId*/>,
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
    nx::sql::AsyncSqlQueryExecutor* m_dbManager = nullptr;
    AbstractOutgoingCommandDispatcher* m_outgoingTransactionDispatcher = nullptr;
    mutable QnMutex m_mutex;
    DbTransactionContextMap m_dbTransactionContexts;
    std::map<std::string, std::unique_ptr<TransactionLogContext>> m_systemIdToTransactionLog;
    std::atomic<std::uint64_t> m_transactionSequence;
    std::unique_ptr<dao::AbstractTransactionDataObject> m_transactionDataObject;
    std::list<std::string> m_systemsMarkedForDeletion;
    OnTransactionReceivedHandler m_onTransactionReceivedHandler;

    /** Fills transaction state cache. */
    nx::sql::DBResult fillCache();
    nx::sql::DBResult fetchTransactionState(nx::sql::QueryContext* connection);
    /**
     * Selects transactions from DB by condition.
     */
    nx::sql::DBResult fetchTransactions(
        nx::sql::QueryContext* connection,
        const std::string& systemId,
        const ReadCommandsFilter& filter,
        TransactionReadResult* const outputData);

    bool isShouldBeIgnored(
        nx::sql::QueryContext* connection,
        const std::string& systemId,
        const CommandHeader& transaction,
        const QByteArray& transactionHash);

    nx::sql::DBResult saveToDb(
        nx::sql::QueryContext* connection,
        const std::string& systemId,
        const QByteArray& transactionHash,
        const SerializableAbstractCommand& transactionSerializer);

    template<typename CommandDescriptor>
    nx::sql::DBResult invokeExternalProcessor(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        const SerializableCommand<CommandDescriptor>& transaction)
    {
        if (!m_onTransactionReceivedHandler)
            return nx::sql::DBResult::ok;

        return m_onTransactionReceivedHandler(
            queryContext,
            systemId,
            transaction);
    }

    int generateNewTransactionSequence(
        const QnMutexLockerBase& lock,
        nx::sql::QueryContext* connection,
        const std::string& systemId);

    vms::api::Timestamp generateNewTransactionTimestamp(
        const QnMutexLockerBase& lock,
        CommandLogCache::TranId cacheTranId,
        const std::string& systemId);

    void onDbTransactionCompleted(
        DbTransactionContextMap::iterator queryIterator,
        const std::string& systemId,
        nx::sql::DBResult dbResult);

    DbTransactionContext& getDbTransactionContext(
        const QnMutexLockerBase& lock,
        nx::sql::QueryContext* const queryContext,
        const std::string& systemId);

    TransactionLogContext* getTransactionLogContext(
        const QnMutexLockerBase& lock,
        const std::string& systemId);

    std::tuple<int, vms::api::Timestamp> generateNewTransactionAttributes(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId);

    void updateTimestampHiInCache(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        quint64 newValue);

    bool clearTransactionLogCacheForSystem(
        const QnMutexLockerBase& lock,
        const std::string& systemId);

    void removeSystemsMarkedForDeletion(const QnMutexLockerBase& /*lock*/);

    static ResultCode dbResultToApiResult(nx::sql::DBResult dbResult);
};

} // namespace nx::clusterdb::engine
