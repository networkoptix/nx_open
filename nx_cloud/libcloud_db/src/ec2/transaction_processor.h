/**********************************************************
* Aug 12, 2016
* a.kolesnikov
***********************************************************/

#pragma once

#include <nx/network/aio/timer.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/log/log.h>

#include <cdb/result_code.h>
#include <common/common_globals.h>
#include <transaction/transaction_transport_header.h>
#include <transaction/transaction.h>
#include <utils/db/async_sql_query_executor.h>

#include "transaction_log.h"
#include "transaction_transport_header.h"


namespace ec2 {
class QnAbstractTransaction;
}   // namespace ec2

namespace nx {
namespace cdb {
namespace ec2 {

class TransactionLog;

typedef nx::utils::MoveOnlyFunc<void(api::ResultCode)> TransactionProcessedHandler;

class AbstractTransactionProcessor
{
public:
    virtual ~AbstractTransactionProcessor() {}

    /** Parse and process UbJson-serialized transaction */
    virtual void processTransaction(
        TransactionTransportHeader transportHeader,
        ::ec2::QnAbstractTransaction transaction,
        QnUbjsonReader<QByteArray>* const stream,
        TransactionProcessedHandler completionHandler) = 0;
    /** Parse and process Json-serialized transaction */
    virtual void processTransaction(
        TransactionTransportHeader transportHeader,
        ::ec2::QnAbstractTransaction transaction,
        const QJsonObject& serializedTransactionData,
        TransactionProcessedHandler completionHandler) = 0;
};

/** Does abstract transaction processing logic.
    Specific transaction logic is implemented by specific manager
*/
template<int TransactionCommandValue, typename TransactionDataType>
class TransactionProcessor
:
    public AbstractTransactionProcessor
{
public:
    typedef ::ec2::QnTransaction<TransactionDataType> Ec2Transaction;
    typedef nx::utils::MoveOnlyFunc<
        nx::db::DBResult(QSqlDatabase*, nx::String /*systemId*/, TransactionDataType)
    > ProcessEc2TransactionFunc;

    /**
       @param processTranFunc This function does transaction-specific logic: e.g., saves data to DB
    */
    TransactionProcessor(
        TransactionLog* const transactionLog,
        nx::db::AsyncSqlQueryExecutor* const dbManager,
        ProcessEc2TransactionFunc processTranFunc)
    :
        m_transactionLog(transactionLog),
        m_dbManager(dbManager),
        m_processTranFunc(std::move(processTranFunc))
    {
    }

    virtual void processTransaction(
        TransactionTransportHeader /*transportHeader*/,
        ::ec2::QnAbstractTransaction /*transaction*/,
        QnUbjsonReader<QByteArray>* const /*stream*/,
        TransactionProcessedHandler completionHandler)
    {
        //TODO
    }

    virtual void processTransaction(
        TransactionTransportHeader transportHeader,
        ::ec2::QnAbstractTransaction abstractTransaction,
        const QJsonObject& serializedTransactionData,
        TransactionProcessedHandler completionHandler)
    {
        auto transaction = Ec2Transaction(std::move(abstractTransaction));
        if (!QJson::deserialize(serializedTransactionData["params"], &transaction.params))
        {
            NX_LOGX(lm("Failed to deserialize json transaction %1 received from (%2, %3)")
                .arg(::ec2::ApiCommand::toString(transaction.command))
                .arg(transportHeader.systemId), cl_logWARNING);
            m_aioTimer.post(
                [completionHandler = std::move(completionHandler)]
                {
                    completionHandler(api::ResultCode::badRequest);
                });
            return;
        }

        this->processTransaction(
            std::move(transportHeader),
            std::move(transaction),
            std::move(completionHandler));
    }

private:
    struct TransactionContext
    {
        TransactionTransportHeader transportHeader;
        Ec2Transaction transaction;
    };

    TransactionLog* const m_transactionLog;
    nx::db::AsyncSqlQueryExecutor* const m_dbManager;
    ProcessEc2TransactionFunc m_processTranFunc;
    nx::network::aio::Timer m_aioTimer;

    void processTransaction(
        TransactionTransportHeader transportHeader,
        Ec2Transaction transaction,
        TransactionProcessedHandler handler)
    {
        using namespace std::placeholders;
        m_dbManager->executeUpdate<TransactionContext, api::ResultCode>(
            std::bind(
                &TransactionProcessor::processTransactionInDbConnectionThread,
                this, _1, _2, _3),
            TransactionContext{ std::move(transportHeader), std::move(transaction) },
            [this, handler = std::move(handler)](
                nx::db::DBResult dbResult,
                TransactionContext transactionContext,
                api::ResultCode resultCode) mutable
            {
                dbProcessingCompleted(
                    dbResult,
                    std::move(transactionContext),
                    resultCode,
                    std::move(handler));
            });
    }

    nx::db::DBResult processTransactionInDbConnectionThread(
        QSqlDatabase* connection,
        const TransactionContext& transactionContext,
        api::ResultCode* const resultCode)
    {
        //DB transaction is created down the stack

        if (!m_transactionLog->checkIfNeededAndSaveToLog(
                connection,
                transactionContext.transportHeader.systemId,
                transactionContext.transaction))
        {
            *resultCode = api::ResultCode::ok;
            return nx::db::DBResult::ok;    //transaction skipped by transaction log
        }

        return m_processTranFunc(
            connection,
            transactionContext.transportHeader.systemId,
            transactionContext.transaction.params);
    }

    void dbProcessingCompleted(
        nx::db::DBResult /*dbResult*/,
        TransactionContext /*transactionContext*/,
        api::ResultCode resultCode,
        TransactionProcessedHandler completionHandler)
    {
        completionHandler(resultCode);
    }
};

}   // namespace ec2
}   // namespace cdb
}   // namespace nx
