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

#include <nx/cloud/cdb/api/result_code.h>
#include <nx_ec/ec_proto_version.h>
#include <utils/common/id.h>
#include <nx/utils/db/async_sql_query_executor.h>

#include <transaction/transaction.h>
#include <transaction/transaction_descriptor.h>

#include "dao/abstract_transaction_data_object.h"
#include "outgoing_transaction_sorter.h"
#include "serialization/transaction_serializer.h"
#include "serialization/ubjson_serialized_transaction.h"
#include "transaction_log_cache.h"
#include "transaction_transport_header.h"

namespace nx {
namespace db {

class AsyncSqlQueryExecutor;

} // namespace db

namespace cdb {
namespace ec2 {

class AbstractOutgoingTransactionDispatcher;

QString toString(const ::ec2::QnAbstractTransaction& tran);

/**
 * Supports multiple transactions related to a single system at the same time.
 * In this case transactions will reported to AbstractOutgoingTransactionDispatcher
 * in ascending sequence order.
 *
 * @note Calls with the same nx::utils::db::QueryContext object MUST happen within single thread.
 */
class TransactionLog
{
public:
    typedef nx::utils::MoveOnlyFunc<void()> NewTransactionHandler;
    typedef nx::utils::MoveOnlyFunc<void(
        api::ResultCode /*resultCode*/,
        std::vector<dao::TransactionLogRecord> /*serializedTransactions*/,
        ::ec2::QnTranState /*readedUpTo*/)> TransactionsReadHandler;

    /**
     * Fills internal cache.
     * @throw std::runtime_error In case of failure to pre-fill data cache.
     */
    TransactionLog(
        const QnUuid& peerId,
        nx::utils::db::AsyncSqlQueryExecutor* const dbManager,
        AbstractOutgoingTransactionDispatcher* const outgoingTransactionDispatcher);

    /**
     * Begins SQL DB transaction and passes that to dbOperationsFunc.
     * @note nx::utils::db::DBResult::retryLater can be reported to onDbUpdateCompleted if
     *      there are already too many requests for transaction
     * @note In case of error dbUpdateFunc can be skipped
     */
    void startDbTransaction(
        const nx::String& systemId,
        nx::utils::MoveOnlyFunc<nx::utils::db::DBResult(nx::utils::db::QueryContext*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(nx::utils::db::QueryContext*, nx::utils::db::DBResult)> onDbUpdateCompleted);

    /**
     * @note This call should be made only once when generating first transaction.
     */
    nx::utils::db::DBResult updateTimestampHiForSystem(
        nx::utils::db::QueryContext* connection,
        const nx::String& systemId,
        quint64 newValue);

    /**
     * If transaction is not needed (it can be late or something),
     *      db::DBResult::cancelled is returned.
     */
    template<typename TransactionDataType>
    nx::utils::db::DBResult checkIfNeededAndSaveToLog(
        nx::utils::db::QueryContext* connection,
        const nx::String& systemId,
        const SerializableTransaction<TransactionDataType>& transaction)
    {
        const auto transactionHash = calculateTransactionHash(transaction.get());

        // Checking whether transaction should be saved or not.
        if (isShouldBeIgnored(
                connection,
                systemId,
                transaction.get(),
                transactionHash))
        {
            NX_LOGX(
                QnLog::EC2_TRAN_LOG,
                lm("systemId %1. Transaction %2 (%3, hash %4) is skipped")
                    .arg(systemId).arg(::ec2::ApiCommand::toString(transaction.get().command))
                    .arg(transaction.get())
                    .arg(calculateTransactionHash(transaction.get())),
                cl_logDEBUG1);
            // Returning nx::utils::db::DBResult::cancelled if transaction should be skipped.
            return nx::utils::db::DBResult::cancelled;
        }

        return saveToDb(
            connection,
            systemId,
            transaction.get(),
            transactionHash,
            transaction.serialize(Qn::UbjsonFormat, nx_ec::EC2_PROTO_VERSION));
    }

    template<typename TransactionDataType>
    nx::utils::db::DBResult generateTransactionAndSaveToLog(
        nx::utils::db::QueryContext* queryContext,
        const nx::String& systemId,
        ::ec2::ApiCommand::Value commandCode,
        TransactionDataType transactionData)
    {
        return saveLocalTransaction(
            queryContext,
            systemId,
            prepareLocalTransaction(
                queryContext, systemId, commandCode, std::move(transactionData)));
    }

    /**
     * This method should be used when generating new transactions.
     */
    template<typename TransactionDataType>
    nx::utils::db::DBResult saveLocalTransaction(
        nx::utils::db::QueryContext* queryContext,
        const nx::String& systemId,
        ::ec2::QnTransaction<TransactionDataType> transaction)
    {
        TransactionLogContext* vmsTransactionLogData = nullptr;

        QnMutexLocker lock(&m_mutex);
        DbTransactionContext& dbTranContext =
            getDbTransactionContext(lock, queryContext, systemId);
        vmsTransactionLogData = getTransactionLogContext(lock, systemId);
        lock.unlock();

        const auto transactionHash = calculateTransactionHash(transaction);
        NX_LOGX(
            QnLog::EC2_TRAN_LOG,
            lm("systemId %1. Generated new transaction %2 (%3, hash %4)")
                .arg(systemId).arg(::ec2::ApiCommand::toString(transaction.command))
                .arg(transaction).arg(transactionHash),
            cl_logDEBUG1);

        // Serializing transaction.
        auto serializedTransaction = QnUbjson::serialized(transaction);

        // Saving transaction to the log.
        const auto result = saveToDb(
            queryContext,
            systemId,
            transaction,
            transactionHash,
            serializedTransaction);
        if (result != nx::utils::db::DBResult::ok)
            return result;

        auto transactionSerializer = std::make_unique<
            UbjsonSerializedTransaction<TransactionDataType>>(
                std::move(transaction),
                std::move(serializedTransaction),
                nx_ec::EC2_PROTO_VERSION);

        // Saving transactions, generated under current DB transaction,
        //  so that we can send "new transaction" notifications after commit.
        vmsTransactionLogData->outgoingTransactionsSorter.addTransaction(
            dbTranContext.cacheTranId,
            std::move(transactionSerializer));

        return nx::utils::db::DBResult::ok;
    }

    template<typename TransactionDataType>
    ::ec2::QnTransaction<TransactionDataType> prepareLocalTransaction(
        nx::utils::db::QueryContext* queryContext,
        const nx::String& systemId,
        ::ec2::ApiCommand::Value commandCode,
        TransactionDataType transactionData)
    {
        int transactionSequence = 0;
        ::ec2::Timestamp transactionTimestamp;
        std::tie(transactionSequence, transactionTimestamp) =
            generateNewTransactionAttributes(queryContext, systemId);

        // Generating transaction.
        ::ec2::QnTransaction<TransactionDataType> transaction(m_peerId);
        // Filling transaction header.
        transaction.command = commandCode;
        transaction.peerID = m_peerId;
        transaction.transactionType = ::ec2::TransactionType::Cloud;
        transaction.persistentInfo.dbID = guidFromArbitraryData(systemId);
        transaction.persistentInfo.sequence = transactionSequence;
        transaction.persistentInfo.timestamp = transactionTimestamp;
        transaction.params = std::move(transactionData);

        return transaction;
    }

    ::ec2::QnTranState getTransactionState(const nx::String& systemId) const;

    /**
     * Asynchronously reads requested transactions from Db.
     * @param to State (transaction source id, sequence) to read up to. Boundary is inclusive
     * @param completionHandler is called within unspecified DB connection thread.
     * In case of high load request can be cancelled internaly.
     * In this case api::ResultCode::retryLater will be reported
     * @note If there more transactions then maxTransactionsToReturn then
     *      api::ResultCode::partialContent result code will be reported
     *      to the completionHandler.
     */
    void readTransactions(
        const nx::String& systemId,
        boost::optional<::ec2::QnTranState> from,
        boost::optional<::ec2::QnTranState> to,
        int maxTransactionsToReturn,
        TransactionsReadHandler completionHandler);

    void clearTransactionLogCacheForSystem(const nx::String& systemId);

    ::ec2::Timestamp generateTransactionTimestamp(const nx::String& systemId);

    void shiftLocalTransactionSequence(
        const nx::String& systemId,
        int delta);

private:
    struct DbTransactionContext
    {
    public:
        VmsTransactionLogCache::TranId cacheTranId;

        DbTransactionContext():
            cacheTranId(VmsTransactionLogCache::InvalidTranId)
        {
        }
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
        std::pair<nx::utils::db::QueryContext*, nx::String>,
        DbTransactionContext
    > DbTransactionContextMap;

    struct TransactionReadResult
    {
        api::ResultCode resultCode;
        std::vector<dao::TransactionLogRecord> transactions;
        /** (Read start state) + (readed transactions). */
        ::ec2::QnTranState state;
    };

    const QnUuid m_peerId;
    nx::utils::db::AsyncSqlQueryExecutor* const m_dbManager;
    AbstractOutgoingTransactionDispatcher* const m_outgoingTransactionDispatcher;
    mutable QnMutex m_mutex;
    DbTransactionContextMap m_dbTransactionContexts;
    std::map<nx::String, std::unique_ptr<TransactionLogContext>> m_systemIdToTransactionLog;
    std::atomic<std::uint64_t> m_transactionSequence;
    std::unique_ptr<dao::AbstractTransactionDataObject> m_transactionDataObject;

    /** Fills transaction state cache. */
    nx::utils::db::DBResult fillCache();
    nx::utils::db::DBResult fetchTransactionState(nx::utils::db::QueryContext* connection);
    /**
     * Selects transactions from DB by condition.
     */
    nx::utils::db::DBResult fetchTransactions(
        nx::utils::db::QueryContext* connection,
        const nx::String& systemId,
        const ::ec2::QnTranState& from,
        const ::ec2::QnTranState& to,
        int maxTransactionsToReturn,
        TransactionReadResult* const outputData);

    bool isShouldBeIgnored(
        nx::utils::db::QueryContext* connection,
        const nx::String& systemId,
        const ::ec2::QnAbstractTransaction& transaction,
        const QByteArray& transactionHash);

    nx::utils::db::DBResult saveToDb(
        nx::utils::db::QueryContext* connection,
        const nx::String& systemId,
        const ::ec2::QnAbstractTransaction& transaction,
        const QByteArray& transactionHash,
        const QByteArray& ubjsonData);

    template<typename TransactionDataType>
    nx::Buffer calculateTransactionHash(
        const ::ec2::QnTransaction<TransactionDataType>& tran)
    {
        return ::ec2::transactionHash(tran.command, tran.params).toSimpleByteArray();
    }

    int generateNewTransactionSequence(
        const QnMutexLockerBase& lock,
        nx::utils::db::QueryContext* connection,
        const nx::String& systemId);

    ::ec2::Timestamp generateNewTransactionTimestamp(
        const QnMutexLockerBase& lock,
        VmsTransactionLogCache::TranId cacheTranId,
        const nx::String& systemId);

    void onDbTransactionCompleted(
        nx::utils::db::QueryContext* dbConnection,
        const nx::String& systemId,
        nx::utils::db::DBResult dbResult);

    DbTransactionContext& getDbTransactionContext(
        const QnMutexLockerBase& lock,
        nx::utils::db::QueryContext* const queryContext,
        const nx::String& systemId);

    TransactionLogContext* getTransactionLogContext(
        const QnMutexLockerBase& lock,
        const nx::String& systemId);

    std::tuple<int, ::ec2::Timestamp> generateNewTransactionAttributes(
        nx::utils::db::QueryContext* queryContext,
        const nx::String& systemId);

    void updateTimestampHiInCache(
        nx::utils::db::QueryContext* queryContext,
        const nx::String& systemId,
        quint64 newValue);
};

} // namespace ec2
} // namespace cdb
} // namespace nx
