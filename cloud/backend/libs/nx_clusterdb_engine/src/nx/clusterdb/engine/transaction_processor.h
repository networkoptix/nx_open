#pragma once

#include <optional>
#include <tuple>

#include <nx/network/aio/timer.h>
#include <nx/sql/async_sql_query_executor.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/log/log.h>

#include "serialization/transaction_deserializer.h"
#include "transaction_log.h"
#include "transport/transaction_transport_header.h"

namespace nx::clusterdb::engine {

class CommandLog;

typedef nx::utils::MoveOnlyFunc<void(ResultCode)> CommandProcessedHandler;

class AbstractCommandProcessor
{
public:
    virtual ~AbstractCommandProcessor() {}

    /**
     * Parse and process UbJson-serialized transaction.
     */
    virtual void process(
        CommandTransportHeader transportHeader,
        std::unique_ptr<DeserializableCommandData> commandData,
        CommandProcessedHandler completionHandler) = 0;
};

/**
 * Does abstract transaction processing logic.
 * Specific transaction logic is implemented by specific manager.
 */
template<typename CommandDescriptor>
class BaseCommandProcessor:
    public AbstractCommandProcessor
{
public:
    using SpecificCommand = Command<typename CommandDescriptor::Data>;

    virtual ~BaseCommandProcessor()
    {
        m_aioTimer.pleaseStopSync();
    }

    virtual void process(
        CommandTransportHeader transportHeader,
        std::unique_ptr<DeserializableCommandData> commandData,
        CommandProcessedHandler completionHandler) override
    {
        auto commandWrapper =
            commandData->deserialize<CommandDescriptor>(
                transportHeader.transactionFormatVersion);
        if (!commandWrapper)
        {
            reportTransactionDeserializationFailure(
                transportHeader, std::move(completionHandler));
            return;
        }

        this->process(
            std::move(transportHeader),
            std::move(*commandWrapper),
            std::move(completionHandler));
    }

protected:
    nx::network::aio::Timer m_aioTimer;

    virtual void process(
        CommandTransportHeader transportHeader,
        SerializableCommand<CommandDescriptor> transaction,
        CommandProcessedHandler handler) = 0;

private:
    template<typename DeserializeTransactionDataFunc>
    void processTransactionInternal(
        DeserializeTransactionDataFunc deserializeTransactionDataFunc,
        CommandTransportHeader transportHeader,
        CommandHeader transactionHeader,
        CommandProcessedHandler completionHandler)
    {
        auto transaction = SpecificCommand(std::move(transactionHeader));
        if (!deserializeTransactionDataFunc(&transaction.params))
        {
            NX_WARNING(this,
                lm("Failed to deserialize transaction %1 received from %2")
                    .args(CommandDescriptor::name, transportHeader));
            m_aioTimer.post(
                [completionHandler = std::move(completionHandler)]
                {
                    completionHandler(ResultCode::badRequest);
                });
            return;
        }

        this->process(
            std::move(transportHeader),
            std::move(transaction),
            std::move(completionHandler));
    }

    void reportTransactionDeserializationFailure(
        const CommandTransportHeader& transportHeader,
        CommandProcessedHandler completionHandler)
    {
        NX_WARNING(this,
            lm("Failed to deserialize transaction %1 received from %2")
                .args(CommandDescriptor::name, transportHeader));
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
    public BaseCommandProcessor<CommandDescriptor>
{
    using base_type = BaseCommandProcessor<CommandDescriptor>;

public:
    typedef nx::utils::MoveOnlyFunc<void(
        const std::string& /*systemId*/,
        CommandTransportHeader /*transportHeader*/,
        Command<typename CommandDescriptor::Data> /*data*/,
        CommandProcessedHandler /*handler*/)> ProcessorFunc;

    SpecialCommandProcessor(ProcessorFunc processorFunc):
        m_processorFunc(std::move(processorFunc))
    {
    }

private:
    ProcessorFunc m_processorFunc;

    virtual void process(
        CommandTransportHeader transportHeader,
        SerializableCommand<CommandDescriptor> transaction,
        CommandProcessedHandler handler) override
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
class CommandProcessor:
    public BaseCommandProcessor<CommandDescriptor>
{
public:
    using SpecificCommand = Command<typename CommandDescriptor::Data>;

    using ProcessEc2TransactionFunc = nx::utils::MoveOnlyFunc<
        nx::sql::DBResult(
            nx::sql::QueryContext*, std::string /*systemId*/, SpecificCommand)>;

    /**
     * @param processTranFunc This function does transaction-specific logic: e.g., saves data to DB
     */
    CommandProcessor(
        CommandLog* const transactionLog,
        ProcessEc2TransactionFunc processTranFunc)
    :
        m_commandLog(transactionLog),
        m_processTranFunc(std::move(processTranFunc))
    {
    }

private:
    struct TransactionContext
    {
        CommandTransportHeader transportHeader;
        SerializableCommand<CommandDescriptor> transaction;
    };

    CommandLog* const m_commandLog;
    ProcessEc2TransactionFunc m_processTranFunc;
    nx::network::aio::Timer m_aioTimer;

    virtual void process(
        CommandTransportHeader transportHeader,
        SerializableCommand<CommandDescriptor> transaction,
        CommandProcessedHandler handler) override
    {
        using namespace std::placeholders;

        const auto systemId = transportHeader.systemId;
        TransactionContext transactionContext{
            std::move(transportHeader),
            std::move(transaction)};
        m_commandLog->startDbTransaction(
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
            m_commandLog->checkIfNeededAndSaveToLog<CommandDescriptor>(
                queryContext,
                transactionContext.transportHeader.systemId.c_str(),
                transactionContext.transaction);

        if (dbResultCode == nx::sql::DBResult::cancelled)
        {
            NX_DEBUG(this,
                lm("Ec2 transaction log skipped transaction %1 received from (%2, %3)")
                .args(CommandDescriptor::name, transactionContext.transportHeader.systemId,
                    transactionContext.transportHeader.endpoint));
            return dbResultCode;
        }
        else if (dbResultCode != nx::sql::DBResult::ok)
        {
            NX_WARNING(this,
                lm("Error saving transaction %1 received from (%2, %3) to the log. %4")
                .args(CommandDescriptor::name, transactionContext.transportHeader.systemId,
                    transactionContext.transportHeader.endpoint,
                    queryContext->connection()->lastErrorText()));
            return dbResultCode;
        }

        dbResultCode = m_processTranFunc(
            queryContext,
            transactionContext.transportHeader.systemId.c_str(),
            std::move(transactionContext.transaction.take()));
        if (dbResultCode != nx::sql::DBResult::ok)
        {
            NX_WARNING(this,
                lm("Error processing transaction %1 received from %2. %3")
                .args(CommandDescriptor::name, transactionContext.transportHeader,
                    queryContext->connection()->lastErrorText()));
        }
        return dbResultCode;
    }

    void dbProcessingCompleted(
        nx::sql::DBResult dbResult,
        CommandProcessedHandler completionHandler)
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

} // namespace nx::clusterdb::engine
