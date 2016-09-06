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
} // namespace ec2

namespace nx {
namespace cdb {
namespace ec2 {

class TransactionLog;

typedef nx::utils::MoveOnlyFunc<void(api::ResultCode)> TransactionProcessedHandler;

class AbstractTransactionProcessor
{
public:
    virtual ~AbstractTransactionProcessor() {}

    /**
     * Parse and process UbJson-serialized transaction.
     */
    virtual void processTransaction(
        TransactionTransportHeader transportHeader,
        ::ec2::QnAbstractTransaction transaction,
        QnUbjsonReader<QByteArray>* const stream,
        TransactionProcessedHandler completionHandler) = 0;
    /**
     * Parse and process Json-serialized transaction.
     */
    virtual void processTransaction(
        TransactionTransportHeader transportHeader,
        ::ec2::QnAbstractTransaction transaction,
        const QJsonObject& serializedTransactionData,
        TransactionProcessedHandler completionHandler) = 0;
};

/**
 * Does abstract transaction processing logic.
 * Specific transaction logic is implemented by specific manager.
 */
template<int TransactionCommandValue, typename TransactionDataType>
class BaseTransactionProcessor
    :
    public AbstractTransactionProcessor
{
public:
    typedef ::ec2::QnTransaction<TransactionDataType> Ec2Transaction;

    virtual void processTransaction(
        TransactionTransportHeader transportHeader,
        ::ec2::QnAbstractTransaction abstractTransaction,
        QnUbjsonReader<QByteArray>* const stream,
        TransactionProcessedHandler completionHandler) override
    {
        auto transaction = Ec2Transaction(std::move(abstractTransaction));
        if (!QnUbjson::deserialize(stream, &transaction.params))
        {
            NX_LOGX(lm("Failed to deserialize ubjson transaction %1 received from %2")
                .arg(::ec2::ApiCommand::toString(transaction.command)).str(transportHeader),
                cl_logWARNING);
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

    virtual void processTransaction(
        TransactionTransportHeader transportHeader,
        ::ec2::QnAbstractTransaction abstractTransaction,
        const QJsonObject& serializedTransactionData,
        TransactionProcessedHandler completionHandler) override
    {
        auto transaction = Ec2Transaction(std::move(abstractTransaction));
        if (!QJson::deserialize(serializedTransactionData["params"], &transaction.params))
        {
            NX_LOGX(lm("Failed to deserialize json transaction %1 received from %2")
                .arg(::ec2::ApiCommand::toString(transaction.command)).str(transportHeader),
                cl_logWARNING);
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

protected:
    nx::network::aio::Timer m_aioTimer;

    virtual void processTransaction(
        TransactionTransportHeader transportHeader,
        Ec2Transaction transaction,
        TransactionProcessedHandler handler) = 0;
};


/**
 * Processes special transactions.
 * Those are usually transactions that does not modify business data help in data synchronization.
*/
template<int TransactionCommandValue, typename TransactionDataType>
class SpecialCommandProcessor
:
    public BaseTransactionProcessor<TransactionCommandValue, TransactionDataType>
{
    typedef BaseTransactionProcessor<TransactionCommandValue, TransactionDataType> BaseType;

public:
    typedef nx::utils::MoveOnlyFunc<void(
        const nx::String& /*systemId*/,
        TransactionTransportHeader /*transportHeader*/,
        ::ec2::QnTransaction<TransactionDataType> /*data*/,
        TransactionProcessedHandler /*handler*/)> ProcessorFunc;

    SpecialCommandProcessor(ProcessorFunc processorFunc)
    :
        m_processorFunc(std::move(processorFunc))
    {
    }

private:
    ProcessorFunc m_processorFunc;

    virtual void processTransaction(
        TransactionTransportHeader transportHeader,
        typename BaseType::Ec2Transaction transaction,
        TransactionProcessedHandler handler) override
    {
        const auto systemId = transportHeader.systemId;
        m_processorFunc(
            std::move(systemId),
            std::move(transportHeader),
            std::move(transaction),
            std::move(handler));
    }
};

/**
 * Does abstract transaction processing logic.
 * Specific transaction logic is implemented by specific manager
 */
template<int TransactionCommandValue, typename TransactionDataType, typename AuxiliaryArgType>
class TransactionProcessor
:
    public BaseTransactionProcessor<TransactionCommandValue, TransactionDataType>
{
public:
    typedef ::ec2::QnTransaction<TransactionDataType> Ec2Transaction;
    typedef nx::utils::MoveOnlyFunc<
        nx::db::DBResult(
            QSqlDatabase*, nx::String /*systemId*/, TransactionDataType, AuxiliaryArgType*)
    > ProcessEc2TransactionFunc;
    typedef nx::utils::MoveOnlyFunc<
        void(QSqlDatabase*, nx::db::DBResult, AuxiliaryArgType)
    > OnTranProcessedFunc;

    /**
     * @param processTranFunc This function does transaction-specific logic: e.g., saves data to DB
     */
    TransactionProcessor(
        TransactionLog* const transactionLog,
        ProcessEc2TransactionFunc processTranFunc,
        OnTranProcessedFunc onTranProcessedFunc)
    :
        m_transactionLog(transactionLog),
        m_processTranFunc(std::move(processTranFunc)),
        m_onTranProcessedFunc(std::move(onTranProcessedFunc))
    {
    }

private:
    struct TransactionContext
    {
        TransactionTransportHeader transportHeader;
        Ec2Transaction transaction;
    };

    TransactionLog* const m_transactionLog;
    ProcessEc2TransactionFunc m_processTranFunc;
    OnTranProcessedFunc m_onTranProcessedFunc;
    nx::network::aio::Timer m_aioTimer;

    virtual void processTransaction(
        TransactionTransportHeader transportHeader,
        Ec2Transaction transaction,
        TransactionProcessedHandler handler) override
    {
        using namespace std::placeholders;
        
        auto auxiliaryArg = std::make_unique<AuxiliaryArgType>();
        auto auxiliaryArgPtr = auxiliaryArg.get();
        m_transactionLog->startDbTransaction(
            transportHeader.systemId,
            std::bind(
                &TransactionProcessor::processTransactionInDbConnectionThread, this,
                _1,
                TransactionContext{ std::move(transportHeader), std::move(transaction) },
                auxiliaryArgPtr),
            [this, auxiliaryArg = std::move(auxiliaryArg), handler = std::move(handler)](
                QSqlDatabase* connection,
                nx::db::DBResult dbResult) mutable
            {
                dbProcessingCompleted(
                    connection,
                    dbResult, 
                    std::move(*auxiliaryArg),
                    std::move(handler));
            });
    }

    nx::db::DBResult processTransactionInDbConnectionThread(
        QSqlDatabase* connection,
        const TransactionContext& transactionContext,
        AuxiliaryArgType* const auxiliaryArg)
    {
        //DB transaction is created down the stack.

        auto dbResultCode =
            m_transactionLog->checkIfNeededAndSaveToLog(
                connection,
                transactionContext.transportHeader.systemId,
                transactionContext.transaction,
                transactionContext.transportHeader);

        if (dbResultCode == nx::db::DBResult::cancelled)
        {
            NX_LOGX(lm("Ec2 transaction log skipped transaction %1 received from (%2, %3)")
                .arg(::ec2::ApiCommand::toString(transactionContext.transaction.command))
                .arg(transactionContext.transportHeader.systemId)
                .str(transactionContext.transportHeader.endpoint),
                cl_logDEBUG1);
            return dbResultCode;
        }
        else if (dbResultCode != nx::db::DBResult::ok)
        {
            NX_LOGX(lm("Error saving transaction %1 received from (%2, %3) to the log. %4")
                .arg(::ec2::ApiCommand::toString(transactionContext.transaction.command))
                .arg(transactionContext.transportHeader.systemId)
                .str(transactionContext.transportHeader.endpoint)
                .arg(connection->lastError().text()),
                cl_logWARNING);
            return dbResultCode;
        }

        dbResultCode = m_processTranFunc(
            connection,
            transactionContext.transportHeader.systemId,
            std::move(transactionContext.transaction.params),
            auxiliaryArg);
        if (dbResultCode != nx::db::DBResult::ok)
        {
            NX_LOGX(lm("Error processing transaction %1 received from %2. %3")
                .arg(::ec2::ApiCommand::toString(transactionContext.transaction.command))
                .str(transactionContext.transportHeader)
                .arg(connection->lastError().text()),
                cl_logWARNING);
        }
        return dbResultCode;
    }

    void dbProcessingCompleted(
        QSqlDatabase* connection,
        nx::db::DBResult dbResult,
        AuxiliaryArgType auxiliaryArg,
        TransactionProcessedHandler completionHandler)
    {
        if (m_onTranProcessedFunc)
            m_onTranProcessedFunc(connection, dbResult, std::move(auxiliaryArg));

        switch (dbResult)
        {
            case nx::db::DBResult::ok:
            case nx::db::DBResult::cancelled:
                return completionHandler(api::ResultCode::ok);
            default:
                return completionHandler(
                    dbResult == nx::db::DBResult::retryLater
                    ? api::ResultCode::retryLater
                    : api::ResultCode::dbError);
        }
    }
};

} // namespace ec2
} // namespace cdb
} // namespace nx
