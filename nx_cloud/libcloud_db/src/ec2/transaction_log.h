#pragma once

#include <vector>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/buffer.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/mutex.h>

#include <cdb/result_code.h>
#include <utils/db/async_sql_query_executor.h>

#include <transaction/transaction.h>

#include "transaction_serializer.h"
#include "transaction_transport_header.h"

namespace nx {

namespace db {
class AsyncSqlQueryExecutor;
} // namespace db

namespace cdb {
namespace ec2 {

class OutgoingTransactionDispatcher;

static const QnUuid kDbInstanceGuid("{DFD33CCE-92E5-48E4-AB7E-D4F164B2A94E}");

/** 
 * Comment will appear here later during class implementation
 */
class TransactionLog
{
public:
    typedef nx::utils::MoveOnlyFunc<void()> NewTransactionHandler;

    TransactionLog(
        const QnUuid& peerId,
        nx::db::AsyncSqlQueryExecutor* const dbManager,
        OutgoingTransactionDispatcher* const outgoingTransactionDispatcher);

    /** 
     * Begins SQL DB transaction and passes that to \a dbOperationsFunc.
     * @note nx::db::DBResult::retryLater can be reported to \a onDbUpdateCompleted if 
     *      there are already too many requests for transaction
     * @note In case of error \a dbUpdateFunc can be skipped
     */
    void startDbTransaction(
        const nx::String& systemId,
        nx::utils::MoveOnlyFunc<nx::db::DBResult(QSqlDatabase*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(QSqlDatabase*, nx::db::DBResult)> onDbUpdateCompleted);

    /** 
     * If transaction is not needed (it can be late or something), 
     *      \a db::DBResult::cancelled is returned
     */
    template<typename TransactionDataType>
    nx::db::DBResult checkIfNeededAndSaveToLog(
        QSqlDatabase* /*connection*/,
        const nx::String& /*systemId*/,
        ::ec2::QnTransaction<TransactionDataType> /*transaction*/,
        TransactionTransportHeader /*transportHeader*/)
    {
        // TODO: returning nx::db::DBResult::cancelled if transaction should be skipped
        return db::DBResult::ok;
    }

    /**
     * This method should be used when generating new transactions.
     */
    template<int TransactionCommandValue, typename TransactionDataType>
    nx::db::DBResult generateTransactionAndSaveToLog(
        QSqlDatabase* connection,
        const nx::String& systemId,
        TransactionDataType transactionData)
    {
        using namespace std::chrono;

        int tranSequence = 0;
        {
            QnMutexLocker lk(&m_mutex);
            int& currentSequence = m_transactionSequenceBySystemId[systemId];
            ++currentSequence;
            tranSequence = currentSequence;
        }

        // Generating transaction.
        ::ec2::QnTransaction<TransactionDataType> transaction;
        // Filling transaction header.
        transaction.command = static_cast<::ec2::ApiCommand::Value>(TransactionCommandValue);
        transaction.peerID = m_peerId;
        transaction.transactionType = ::ec2::TransactionType::Cloud;
        transaction.persistentInfo.dbID = kDbInstanceGuid;
        transaction.persistentInfo.sequence = tranSequence;
        transaction.persistentInfo.timestamp = 
            duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        transaction.params = std::move(transactionData);

        // Serializing transaction.
        auto serializedTransaction = QnUbjson::serialized(transaction);

        // TODO: Saving transaction to the log.

        auto transactionSerializer = std::make_shared<
            typename SerializedUbjsonTransaction<TransactionDataType>>(
                std::move(transaction),
                std::move(serializedTransaction));

        // Saving transactions, generated under current DB transaction,
        //  so that we can send "new transaction" notifications after commit
        QnMutexLocker lk(&m_mutex);
        DbTransactionContext& context = m_dbTransactionContexts[connection];
        context.systemId = systemId;
        context.transactions.push_back(std::move(transactionSerializer));

        return db::DBResult::ok;
    }

private:
    class DbTransactionContext
    {
    public:
        nx::String systemId;
        /** List of transactions, added within this DB transaction. */
        std::vector<std::shared_ptr<const TransactionSerializer>> transactions;

        DbTransactionContext() = default;
        DbTransactionContext(DbTransactionContext&&) = default;
        DbTransactionContext& operator=(DbTransactionContext&&) = default;
    };

    const QnUuid m_peerId;
    nx::db::AsyncSqlQueryExecutor* const m_dbManager;
    OutgoingTransactionDispatcher* const m_outgoingTransactionDispatcher;
    QnMutex m_mutex;
    std::map<QSqlDatabase*, DbTransactionContext> m_dbTransactionContexts;
    std::map<nx::String, int> m_transactionSequenceBySystemId;
};

/** 
 * Asynchronously reads transactions of specified system from log.
 * Returns data in specified format.
 */
class TransactionLogReader
:
    public network::aio::BasicPollable
{
public:
    TransactionLogReader(
        TransactionLog* const transactionLog,
        nx::String systemId,
        Qn::SerializationFormat dataFormat);
    ~TransactionLogReader();

    virtual void stopWhileInAioThread() override;

    void getTransactions(
        const ::ec2::QnTranState& from,
        const ::ec2::QnTranState& to,
        int maxTransactionsToReturn,
        nx::utils::MoveOnlyFunc<void(
            api::ResultCode resultCode,
            std::vector<nx::Buffer> serializedTransactions)> completionHandler);

    // TODO: #ak following method MUST be asynchronous
    ::ec2::QnTranState getCurrentState() const;

    /**
     * Called before returning ubjson-transaction to the caller.
     * Handler is allowed to modify transaction. E.g., add transport header
     */
    void setOnUbjsonTransactionReady(
        nx::utils::MoveOnlyFunc<void(nx::Buffer)> handler);
    /**
     * Called before returning JSON-transaction to the caller.
     * Handler is allowed to modify transaction. E.g., add transport header
     */
    void setOnJsonTransactionReady(
        nx::utils::MoveOnlyFunc<void(QJsonObject*)> handler);

private:
    nx::utils::MoveOnlyFunc<void(nx::Buffer)> m_onUbjsonTransactionReady;
    nx::utils::MoveOnlyFunc<void(QJsonObject*)> m_onJsonTransactionReady;
};

} // namespace ec2
} // namespace cdb
} // namespace nx
