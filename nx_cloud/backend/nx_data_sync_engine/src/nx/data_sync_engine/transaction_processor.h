#pragma once

#include <nx/network/aio/timer.h>
#include <nx/sql/async_sql_query_executor.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/log/log.h>

#include "serialization/transaction_deserializer.h"
#include "transaction_log.h"
#include "transaction_transport_header.h"

namespace nx {
namespace data_sync_engine {

class TransactionLog;

typedef nx::utils::MoveOnlyFunc<void(ResultCode)> TransactionProcessedHandler;

class AbstractTransactionProcessor
{
public:
    virtual ~AbstractTransactionProcessor() {}

    /**
     * Parse and process UbJson-serialized transaction.
     */
    virtual void processTransaction(
        TransactionTransportHeader transportHeader,
        CommandHeader transaction,
        std::unique_ptr<TransactionUbjsonDataSource> dataSource,
        TransactionProcessedHandler completionHandler) = 0;
    /**
     * Parse and process Json-serialized transaction.
     */
    virtual void processTransaction(
        TransactionTransportHeader transportHeader,
        CommandHeader transaction,
        QJsonObject serializedTransactionData,
        TransactionProcessedHandler completionHandler) = 0;
};

/**
 * Does abstract transaction processing logic.
 * Specific transaction logic is implemented by specific manager.
 */
template<typename CommandDescriptor>
class BaseTransactionProcessor:
    public AbstractTransactionProcessor
{
public:
    using Ec2Transaction = Command<typename CommandDescriptor::Data>;

    virtual ~BaseTransactionProcessor()
    {
        m_aioTimer.pleaseStopSync();
    }

    virtual void processTransaction(
        TransactionTransportHeader transportHeader,
        CommandHeader transactionHeader,
        std::unique_ptr<TransactionUbjsonDataSource> dataSource,
        TransactionProcessedHandler completionHandler) override
    {
        NX_ASSERT(transactionHeader.command == CommandDescriptor::code);

        auto transaction = Ec2Transaction(std::move(transactionHeader));
        if (!TransactionDeserializer::deserialize(
                &dataSource->stream,
                &transaction.params,
                transportHeader.transactionFormatVersion))
        {
            reportTransactionDeserializationFailure(
                transportHeader, std::move(completionHandler));
            return;
        }

        UbjsonSerializedTransaction<typename CommandDescriptor::Data> serializableTransaction(
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
        CommandHeader transactionHeader,
        QJsonObject serializedTransactionData,
        TransactionProcessedHandler completionHandler) override
    {
        NX_ASSERT(transactionHeader.command == CommandDescriptor::code);

        auto transaction = Ec2Transaction(std::move(transactionHeader));
        if (!QJson::deserialize(serializedTransactionData["params"], &transaction.params))
        {
            reportTransactionDeserializationFailure(
                transportHeader, std::move(completionHandler));
            return;
        }

        SerializableTransaction<typename CommandDescriptor::Data> serializableTransaction(
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
        SerializableTransaction<typename CommandDescriptor::Data> transaction,
        TransactionProcessedHandler handler) = 0;

private:
    template<typename DeserializeTransactionDataFunc>
    void processTransactionInternal(
        DeserializeTransactionDataFunc deserializeTransactionDataFunc,
        TransactionTransportHeader transportHeader,
        CommandHeader transactionHeader,
        TransactionProcessedHandler completionHandler)
    {
        auto transaction = Ec2Transaction(std::move(transactionHeader));
        if (!deserializeTransactionDataFunc(&transaction.params))
        {
            NX_WARNING(QnLog::EC2_TRAN_LOG.join(this), lm("Failed to deserialize transaction %1 received from %2")
                .arg(CommandDescriptor::name).arg(transportHeader));
            m_aioTimer.post(
                [completionHandler = std::move(completionHandler)]
                {
                    completionHandler(ResultCode::badRequest);
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
        TransactionProcessedHandler completionHandler)
    {
        NX_WARNING(QnLog::EC2_TRAN_LOG.join(this), lm("Failed to deserialize transaction %1 received from %2")
            .arg(CommandDescriptor::name).arg(transportHeader));
        m_aioTimer.post(
            [completionHandler = std::move(completionHandler)]
            {
                completionHandler(ResultCode::badRequest);
            });
    }
};

/**
 * Processes special transactions.
 * Those are usually transactions that does not modify business data help in data synchronization.
 */
template<typename CommandDescriptor>
class SpecialCommandProcessor:
    public BaseTransactionProcessor<CommandDescriptor>
{
    using base_type = BaseTransactionProcessor<CommandDescriptor>;

public:
    typedef nx::utils::MoveOnlyFunc<void(
        const std::string& /*systemId*/,
        TransactionTransportHeader /*transportHeader*/,
        Command<typename CommandDescriptor::Data> /*data*/,
        TransactionProcessedHandler /*handler*/)> ProcessorFunc;

    SpecialCommandProcessor(ProcessorFunc processorFunc):
        m_processorFunc(std::move(processorFunc))
    {
    }

private:
    ProcessorFunc m_processorFunc;

    virtual void processTransaction(
        TransactionTransportHeader transportHeader,
        SerializableTransaction<typename CommandDescriptor::Data> transaction,
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
template<typename CommandDescriptor>
class TransactionProcessor:
    public BaseTransactionProcessor<CommandDescriptor>
{
public:
    using Ec2Transaction = Command<typename CommandDescriptor::Data>;

    using ProcessEc2TransactionFunc = nx::utils::MoveOnlyFunc<
        nx::sql::DBResult(
            nx::sql::QueryContext*, std::string /*systemId*/, Ec2Transaction)>;

    /**
     * @param processTranFunc This function does transaction-specific logic: e.g., saves data to DB
     */
    TransactionProcessor(
        TransactionLog* const transactionLog,
        ProcessEc2TransactionFunc processTranFunc)
    :
        m_transactionLog(transactionLog),
        m_processTranFunc(std::move(processTranFunc))
    {
    }

private:
    struct TransactionContext
    {
        TransactionTransportHeader transportHeader;
        SerializableTransaction<typename CommandDescriptor::Data> transaction;
    };

    TransactionLog* const m_transactionLog;
    ProcessEc2TransactionFunc m_processTranFunc;
    nx::network::aio::Timer m_aioTimer;

    virtual void processTransaction(
        TransactionTransportHeader transportHeader,
        SerializableTransaction<typename CommandDescriptor::Data> transaction,
        TransactionProcessedHandler handler) override
    {
        using namespace std::placeholders;

        const auto systemId = transportHeader.systemId;
        TransactionContext transactionContext{
            std::move(transportHeader),
            std::move(transaction)};
        m_transactionLog->startDbTransaction(
            systemId.c_str(),
            [this, transactionContext = std::move(transactionContext)](
                nx::sql::QueryContext* queryContext) mutable
            {
                return processTransactionInDbConnectionThread(
                    queryContext,
                    std::move(transactionContext));
            },
            [this, handler = std::move(handler)](
                nx::sql::DBResult dbResult) mutable
            {
                dbProcessingCompleted(
                    dbResult,
                    std::move(handler));
            });
    }

    nx::sql::DBResult processTransactionInDbConnectionThread(
        nx::sql::QueryContext* queryContext,
        TransactionContext transactionContext)
    {
        NX_ASSERT(transactionContext.transaction.get().command == CommandDescriptor::code);

        auto dbResultCode =
            m_transactionLog->checkIfNeededAndSaveToLog<CommandDescriptor>(
                queryContext,
                transactionContext.transportHeader.systemId.c_str(),
                transactionContext.transaction);

        if (dbResultCode == nx::sql::DBResult::cancelled)
        {
            NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this), lm("Ec2 transaction log skipped transaction %1 received from (%2, %3)")
                .arg(CommandDescriptor::name)
                .arg(transactionContext.transportHeader.systemId)
                .arg(transactionContext.transportHeader.endpoint));
            return dbResultCode;
        }
        else if (dbResultCode != nx::sql::DBResult::ok)
        {
            NX_WARNING(QnLog::EC2_TRAN_LOG.join(this), lm("Error saving transaction %1 received from (%2, %3) to the log. %4")
                .arg(CommandDescriptor::name)
                .arg(transactionContext.transportHeader.systemId)
                .arg(transactionContext.transportHeader.endpoint)
                .arg(queryContext->connection()->lastErrorText()));
            return dbResultCode;
        }

        dbResultCode = m_processTranFunc(
            queryContext,
            transactionContext.transportHeader.systemId.c_str(),
            std::move(transactionContext.transaction.take()));
        if (dbResultCode != nx::sql::DBResult::ok)
        {
            NX_WARNING(QnLog::EC2_TRAN_LOG.join(this), lm("Error processing transaction %1 received from %2. %3")
                .arg(CommandDescriptor::name)
                .arg(transactionContext.transportHeader)
                .arg(queryContext->connection()->lastErrorText()));
        }
        return dbResultCode;
    }

    void dbProcessingCompleted(
        nx::sql::DBResult dbResult,
        TransactionProcessedHandler completionHandler)
    {
        switch (dbResult)
        {
            case nx::sql::DBResult::ok:
            case nx::sql::DBResult::cancelled:
                return completionHandler(ResultCode::ok);

            default:
                return completionHandler(
                    dbResult == nx::sql::DBResult::retryLater
                    ? ResultCode::retryLater
                    : ResultCode::dbError);
        }
    }
};

} // namespace data_sync_engine
} // namespace nx
