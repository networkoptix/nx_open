#pragma once

#include <vector>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/buffer.h>
#include <nx/utils/log/log.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/mutex.h>

#include <cdb/result_code.h>
#include <utils/db/async_sql_query_executor.h>

#include <transaction/transaction.h>
#include <transaction/transaction_descriptor.h>

#include "transaction_serializer.h"
#include "transaction_transport_header.h"

namespace nx {

namespace db {
class AsyncSqlQueryExecutor;
} // namespace db

namespace cdb {
namespace ec2 {

class OutgoingTransactionDispatcher;

// TODO: #ak this constant should be stored in DB and generated 
static const QnUuid kDbInstanceGuid("{DFD33CCE-92E5-48E4-AB7E-D4F164B2A94E}");

QString toString(const ::ec2::QnAbstractTransaction& tran);

/** 
 * Comment will appear here later during class implementation.
 */
class TransactionLog
{
public:
    typedef nx::utils::MoveOnlyFunc<void()> NewTransactionHandler;

    /**
     * Fills internal cache.
     * @throw \a std::runtime_error In case of failure to pre-fill data cache.
     */
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
     *      \a db::DBResult::cancelled is returned.
     */
    template<typename TransactionDataType>
    nx::db::DBResult checkIfNeededAndSaveToLog(
        QSqlDatabase* connection,
        const nx::String& systemId,
        ::ec2::QnTransaction<TransactionDataType> transaction,
        TransactionTransportHeader transportHeader)
    {
        const auto transactionHash = calculateTransactionHash(transaction);

        // Checking whether transaction should be saved or not.
        if (isShouldBeIgnored(
                connection,
                systemId,
                transaction,
                transactionHash))
        {
            NX_LOGX(
                QnLog::EC2_TRAN_LOG,
                lm("systemId %1. Transaction %2 (%3, hash %4) is skipped")
                    .arg(systemId).arg(::ec2::ApiCommand::toString(transaction.command))
                    .str(transaction).arg(calculateTransactionHash(transaction)),
                cl_logDEBUG1);
            // Returning nx::db::DBResult::cancelled if transaction should be skipped.
            return nx::db::DBResult::cancelled;
        }

        // TODO: Should not serialize transaction here, but receive already serialized version.
        return saveToDb(
            connection,
            systemId,
            transaction,
            transactionHash,
            QnUbjson::serialized(transaction));
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
        const int tranSequence = generateNewTransactionSequence(connection, systemId);

        // Generating transaction.
        ::ec2::QnTransaction<TransactionDataType> transaction;
        // Filling transaction header.
        transaction.command = static_cast<::ec2::ApiCommand::Value>(TransactionCommandValue);
        transaction.peerID = m_peerId;
        transaction.transactionType = ::ec2::TransactionType::Cloud;
        transaction.persistentInfo.dbID = kDbInstanceGuid;
        transaction.persistentInfo.sequence = tranSequence;
        transaction.persistentInfo.timestamp =
            generateNewTransactionTimestamp(connection, systemId);
        transaction.params = std::move(transactionData);

        const auto transactionHash = calculateTransactionHash(transaction);
        NX_LOGX(
            QnLog::EC2_TRAN_LOG,
            lm("systemId %1. Generated new transaction %2 (%3, hash %4)")
                .arg(systemId).arg(::ec2::ApiCommand::toString(transaction.command))
                .str(transaction).arg(transactionHash),
            cl_logDEBUG1);

        // Serializing transaction.
        auto serializedTransaction = QnUbjson::serialized(transaction);

        // Saving transaction to the log.
        const auto result = saveToDb(
            connection,
            systemId,
            transaction,
            transactionHash,
            serializedTransaction);
        if (result != nx::db::DBResult::ok)
            return result;

        auto transactionSerializer = std::make_shared<
            typename SerializedUbjsonTransaction<TransactionDataType>>(
                std::move(transaction),
                std::move(serializedTransaction));

        // Saving transactions, generated under current DB transaction,
        //  so that we can send "new transaction" notifications after commit.
        QnMutexLocker lk(&m_mutex);
        DbTransactionContext& dbTranContext = m_dbTransactionContexts[connection];
        dbTranContext.systemId = systemId;
        dbTranContext.transactions.push_back(std::move(transactionSerializer));

        return db::DBResult::ok;
    }

private:
    struct UpdateHistoryData
    {
        ::ec2::QnTranStateKey updatedBy;
        qint64 timestamp;

        //UpdateHistoryData():
        //    timestamp(0)
        //{
        //}
        //UpdateHistoryData(
        //    const ::ec2::QnTranStateKey& updatedBy,
        //    const qint64& timestamp)
        //    :
        //    updatedBy(updatedBy),
        //    timestamp(timestamp)
        //{
        //}
    };

    struct VmsTransactionLogData
    {
        nx::String systemId;
        /** map<transaction hash, peer which updated transaction> */
        std::map<nx::Buffer, UpdateHistoryData> transactionHashToUpdateAuthor;
        /** map<peer, transport sequence> */
        std::map<::ec2::QnTranStateKey, int> lastTransportSeq;
        ::ec2::QnTranState transactionState;
        int persistentSequence;

        VmsTransactionLogData(): persistentSequence(0) {}
    };

    struct DbTransactionContext
    {
    public:
        nx::String systemId;
        /** List of transactions, added within this DB transaction. */
        std::vector<std::shared_ptr<const TransactionSerializer>> transactions;
        /** Changes done to vms transaction log under Db transaction. */
        VmsTransactionLogData transactionLogUpdate;

        //DbTransactionContext() = default;
        //DbTransactionContext(DbTransactionContext&&) = default;
        //DbTransactionContext& operator=(DbTransactionContext&&) = default;
    };

    const QnUuid m_peerId;
    nx::db::AsyncSqlQueryExecutor* const m_dbManager;
    OutgoingTransactionDispatcher* const m_outgoingTransactionDispatcher;
    QnMutex m_mutex;
    std::map<QSqlDatabase*, DbTransactionContext> m_dbTransactionContexts;
    std::map<nx::String, VmsTransactionLogData> m_systemIdToTransactionLog;

    /** Fills transaction state cache. Throws in case of error. */
    nx::db::DBResult fillCache();
    nx::db::DBResult fetchTransactionState(
        QSqlDatabase* connection,
        int* const /*dummyResult*/);
    bool isShouldBeIgnored(
        QSqlDatabase* connection,
        const nx::String& systemId,
        const ::ec2::QnAbstractTransaction& transaction,
        const QByteArray& transactionHash);
    nx::db::DBResult saveToDb(
        QSqlDatabase* connection,
        const nx::String& systemId,
        const ::ec2::QnAbstractTransaction& transaction,
        const QByteArray& transactionHash,
        const QByteArray& ubjsonData);

    template<typename TransactionDataType>
    nx::Buffer calculateTransactionHash(
        const ::ec2::QnTransaction<TransactionDataType>& tran)
    {
        return ::ec2::transactionHash(tran.params).toSimpleByteArray();
    }

    int generateNewTransactionSequence(
        QSqlDatabase* connection,
        const nx::String& systemId);
    qint64 generateNewTransactionTimestamp(
        QSqlDatabase* connection,
        const nx::String& systemId);
    void onDbTransactionCompleted(
        QSqlDatabase* dbConnection,
        nx::db::DBResult dbResult);
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
