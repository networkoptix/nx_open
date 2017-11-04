#pragma once

#include <nx/network/aio/timer.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/log/log.h>

#include <nx/cloud/cdb/api/result_code.h>
#include <common/common_globals.h>
#include <transaction/transaction_transport_header.h>
#include <transaction/transaction.h>
#include <nx/utils/db/async_sql_query_executor.h>

#include "serialization/transaction_deserializer.h"
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
        std::unique_ptr<TransactionUbjsonDataSource> dataSource,
        TransactionProcessedHandler completionHandler) = 0;
    /**
     * Parse and process Json-serialized transaction.
     */
    virtual void processTransaction(
        TransactionTransportHeader transportHeader,
        ::ec2::QnAbstractTransaction transaction,
        QJsonObject serializedTransactionData,
        TransactionProcessedHandler completionHandler) = 0;
};

/**
 * Does abstract transaction processing logic.
 * Specific transaction logic is implemented by specific manager.
 */
template<int TransactionCommandValue, typename TransactionDataType>
class BaseTransactionProcessor:
    public AbstractTransactionProcessor
{
public:
    typedef ::ec2::QnTransaction<TransactionDataType> Ec2Transaction;

    virtual ~BaseTransactionProcessor()
    {
        m_aioTimer.pleaseStopSync();
    }

    virtual void processTransaction(
        TransactionTransportHeader transportHeader,
        ::ec2::QnAbstractTransaction transactionHeader,
        std::unique_ptr<TransactionUbjsonDataSource> dataSource,
        TransactionProcessedHandler completionHandler) override
    {
        auto transaction = Ec2Transaction(std::move(transactionHeader));
        if (!TransactionDeserializer::deserialize(
                &dataSource->stream,
                &transaction.params,
                transportHeader.transactionFormatVersion))
        {
            reportTransactionDeserializationFailure(
                transportHeader, transaction.command, std::move(completionHandler));
            return;
        }

        UbjsonSerializedTransaction<TransactionDataType> serializableTransaction(
            std::move(transaction),
            std::move(dataSource->serializedTransaction),
            transportHeader.transactionFormatVersion);

        this->processTransaction(
            std::move(transportHeader),
            std::move(serializableTransaction),
            std::move(completionHandler));
    }

    virtual void processTransaction(
        TransactionTransportHeader transportHeader,
        ::ec2::QnAbstractTransaction transactionHeader,
        QJsonObject serializedTransactionData,
        TransactionProcessedHandler completionHandler) override
    {
        auto transaction = Ec2Transaction(std::move(transactionHeader));
        if (!QJson::deserialize(serializedTransactionData["params"], &transaction.params))
        {
            reportTransactionDeserializationFailure(
                transportHeader, transaction.command, std::move(completionHandler));
            return;
        }

        SerializableTransaction<TransactionDataType> serializableTransaction(
            std::move(transaction));

        this->processTransaction(
            std::move(transportHeader),
            std::move(serializableTransaction),
            std::move(completionHandler));
    }

protected:
    nx::network::aio::Timer m_aioTimer;

    virtual void processTransaction(
        TransactionTransportHeader transportHeader,
        SerializableTransaction<TransactionDataType> transaction,
        TransactionProcessedHandler handler) = 0;

private:
    template<typename DeserializeTransactionDataFunc>
    void processTransactionInternal(
        DeserializeTransactionDataFunc deserializeTransactionDataFunc,
        TransactionTransportHeader transportHeader,
        ::ec2::QnAbstractTransaction transactionHeader,
        TransactionProcessedHandler completionHandler)
    {
        auto transaction = Ec2Transaction(std::move(transactionHeader));
        if (!deserializeTransactionDataFunc(&transaction.params))
        {
            NX_LOGX(QnLog::EC2_TRAN_LOG,
                lm("Failed to deserialize transaction %1 received from %2")
                .arg(::ec2::ApiCommand::toString(transaction.command)).arg(transportHeader),
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

    void reportTransactionDeserializationFailure(
        const TransactionTransportHeader& transportHeader,
        ::ec2::ApiCommand::Value transactionType,
        TransactionProcessedHandler completionHandler)
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("Failed to deserialize transaction %1 received from %2")
            .arg(::ec2::ApiCommand::toString(transactionType)).arg(transportHeader),
            cl_logWARNING);
        m_aioTimer.post(
            [completionHandler = std::move(completionHandler)]
            {
                completionHandler(api::ResultCode::badRequest);
            });
    }
};

/**
 * Processes special transactions.
 * Those are usually transactions that does not modify business data help in data synchronization.
 */
template<int TransactionCommandValue, typename TransactionDataType>
class SpecialCommandProcessor:
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
        SerializableTransaction<TransactionDataType> transaction,
        TransactionProcessedHandler handler) override
    {
        const auto systemId = transportHeader.systemId;
        m_processorFunc(
            std::move(systemId),
            std::move(transportHeader),
            std::move(transaction.take()),
            std::move(handler));
    }
};

/**
 * Does abstract transaction processing logic.
 * Specific transaction logic is implemented by specific manager
 */
template<int TransactionCommandValue, typename TransactionDataType, typename AuxiliaryArgType>
class TransactionProcessor:
    public BaseTransactionProcessor<TransactionCommandValue, TransactionDataType>
{
public:
    typedef ::ec2::QnTransaction<TransactionDataType> Ec2Transaction;

    typedef nx::utils::MoveOnlyFunc<
        nx::utils::db::DBResult(
            nx::utils::db::QueryContext*, nx::String /*systemId*/, Ec2Transaction, AuxiliaryArgType*)
    > ProcessEc2TransactionFunc;

    typedef nx::utils::MoveOnlyFunc<
        void(nx::utils::db::QueryContext*, nx::utils::db::DBResult, AuxiliaryArgType)
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
        SerializableTransaction<TransactionDataType> transaction;
    };

    TransactionLog* const m_transactionLog;
    ProcessEc2TransactionFunc m_processTranFunc;
    OnTranProcessedFunc m_onTranProcessedFunc;
    nx::network::aio::Timer m_aioTimer;

    virtual void processTransaction(
        TransactionTransportHeader transportHeader,
        SerializableTransaction<TransactionDataType> transaction,
        TransactionProcessedHandler handler) override
    {
        using namespace std::placeholders;

        auto auxiliaryArg = std::make_unique<AuxiliaryArgType>();
        auto auxiliaryArgPtr = auxiliaryArg.get();
        TransactionContext transactionContext{
            std::move(transportHeader),
            std::move(transaction)};
        m_transactionLog->startDbTransaction(
            transportHeader.systemId,
            [this,
                auxiliaryArgPtr,
                transactionContext = std::move(transactionContext)](
                    nx::utils::db::QueryContext* queryContext) mutable
            {
                return processTransactionInDbConnectionThread(
                    queryContext,
                    std::move(transactionContext),
                    auxiliaryArgPtr);
            },
            [this, auxiliaryArg = std::move(auxiliaryArg), handler = std::move(handler)](
                nx::utils::db::QueryContext* queryContext,
                nx::utils::db::DBResult dbResult) mutable
            {
                dbProcessingCompleted(
                    queryContext,
                    dbResult,
                    std::move(*auxiliaryArg),
                    std::move(handler));
            });
    }

    nx::utils::db::DBResult processTransactionInDbConnectionThread(
        nx::utils::db::QueryContext* queryContext,
        TransactionContext transactionContext,
        AuxiliaryArgType* const auxiliaryArg)
    {
        auto dbResultCode =
            m_transactionLog->checkIfNeededAndSaveToLog(
                queryContext,
                transactionContext.transportHeader.systemId,
                transactionContext.transaction);

        const auto transactionCommand =
            transactionContext.transaction.get().command;

        if (dbResultCode == nx::utils::db::DBResult::cancelled)
        {
            NX_LOGX(QnLog::EC2_TRAN_LOG,
                lm("Ec2 transaction log skipped transaction %1 received from (%2, %3)")
                .arg(::ec2::ApiCommand::toString(transactionCommand))
                .arg(transactionContext.transportHeader.systemId)
                .arg(transactionContext.transportHeader.endpoint),
                cl_logDEBUG1);
            return dbResultCode;
        }
        else if (dbResultCode != nx::utils::db::DBResult::ok)
        {
            NX_LOGX(QnLog::EC2_TRAN_LOG,
                lm("Error saving transaction %1 received from (%2, %3) to the log. %4")
                .arg(::ec2::ApiCommand::toString(transactionCommand))
                .arg(transactionContext.transportHeader.systemId)
                .arg(transactionContext.transportHeader.endpoint)
                .arg(queryContext->connection()->lastError().text()),
                cl_logWARNING);
            return dbResultCode;
        }

        dbResultCode = m_processTranFunc(
            queryContext,
            transactionContext.transportHeader.systemId,
            std::move(transactionContext.transaction.take()),
            auxiliaryArg);
        if (dbResultCode != nx::utils::db::DBResult::ok)
        {
            NX_LOGX(QnLog::EC2_TRAN_LOG,
                lm("Error processing transaction %1 received from %2. %3")
                .arg(::ec2::ApiCommand::toString(transactionCommand))
                .arg(transactionContext.transportHeader)
                .arg(queryContext->connection()->lastError().text()),
                cl_logWARNING);
        }
        return dbResultCode;
    }

    void dbProcessingCompleted(
        nx::utils::db::QueryContext* queryContext,
        nx::utils::db::DBResult dbResult,
        AuxiliaryArgType auxiliaryArg,
        TransactionProcessedHandler completionHandler)
    {
        if (m_onTranProcessedFunc)
            m_onTranProcessedFunc(queryContext, dbResult, std::move(auxiliaryArg));

        switch (dbResult)
        {
            case nx::utils::db::DBResult::ok:
            case nx::utils::db::DBResult::cancelled:
                return completionHandler(api::ResultCode::ok);
            default:
                return completionHandler(
                    dbResult == nx::utils::db::DBResult::retryLater
                    ? api::ResultCode::retryLater
                    : api::ResultCode::dbError);
        }
    }
};

} // namespace ec2
} // namespace cdb
} // namespace nx
