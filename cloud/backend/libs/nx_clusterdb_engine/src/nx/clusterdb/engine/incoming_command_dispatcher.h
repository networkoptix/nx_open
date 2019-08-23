#pragma once

#include <atomic>

#include <nx/network/aio/timer.h>
#include <nx/sql/async_sql_query_executor.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/log/log.h>
#include <nx/utils/subscription.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include "command_processor.h"
#include "transport/command_transport_header.h"

namespace nx::clusterdb::engine {

class CommandLog;

/**
 * Dispaches transaction received from remote peer to a corresponding processor.
 */
class NX_DATA_SYNC_ENGINE_API IncomingCommandDispatcher
{
public:
    using WatchCommandSubscription = nx::utils::Subscription<
        const CommandTransportHeader&,
        const CommandHeader&>;

    template<typename DbFunc, typename DataArg>
    using DbFuncResult = typename std::invoke_result_t<
        DbFunc,
        nx::sql::QueryContext*, std::string, DataArg>;

    IncomingCommandDispatcher(CommandLog* const transactionLog);
    virtual ~IncomingCommandDispatcher();

    /**
     * @note Method is non-blocking, result is delivered by invoking completionHandler.
     */
    void dispatchTransaction(
        CommandTransportHeader transportHeader,
        std::unique_ptr<DeserializableCommandData> commandData,
        CommandProcessedHandler completionHandler);

    /**
     * Register processor function by command type.
     */
    template<typename CommandDescriptor, typename DbFunc>
    void registerCommandHandler(
        DbFunc processTranFunc,
        typename std::enable_if_t<
            std::is_same_v<
                DbFuncResult<DbFunc, Command<typename CommandDescriptor::Data>>,
                nx::sql::DBResult>,
        DbFunc>* = nullptr)
    {
        using SpecificCommandProcessor = CommandProcessor<CommandDescriptor>;

        auto context = std::make_unique<CommandProcessorContext>();
        context->processor = std::make_unique<SpecificCommandProcessor>(
            m_commandLog,
            std::move(processTranFunc));

        m_commandProcessors.emplace(
            CommandDescriptor::code,
            std::move(context));
    }

    /**
     * DbFunc must have the signature: void(nx::sql::QueryContext*, std::string, CommandDescriptor::Data)
     *
     * @param clusterId if non-empty and not matching the clusterId supplied with the transaction,
     * dbFunc is not invoked. If empty, dbFunc is always invoked.
     */
    template<typename CommandDescriptor, typename DbFunc>
    void registerCommandHandler(
        DbFunc processTranFunc,
        typename std::enable_if_t<
            std::is_same_v<DbFuncResult<DbFunc, typename CommandDescriptor::Data>, void>,
            DbFunc>* = nullptr)
    {
        registerCommandHandler<CommandDescriptor>(
            [processTranFunc = std::move(processTranFunc)](
                auto queryContext,
                auto clusterId,
                auto command) -> nx::sql::DBResult
            {
                processTranFunc(queryContext, std::move(clusterId), std::move(command.params));
                return nx::sql::DBResult::ok;
            });
    }

    /**
     * Register processor function that does not need to save data to DB.
     */
    template<typename CommandDescriptor>
    void registerSpecialCommandHandler(
        typename SpecialCommandProcessor<CommandDescriptor>::ProcessorFunc processTranFunc)
    {
        using SpecificCommandProcessor = SpecialCommandProcessor<CommandDescriptor>;

        auto context = std::make_unique<CommandProcessorContext>();
        context->processor = std::make_unique<SpecificCommandProcessor>(
            std::move(processTranFunc));

        m_commandProcessors.emplace(
            CommandDescriptor::code,
            std::move(context));
    }

    /**
     * Waits for every running transaction of type transactionType processing to complete.
     */
    template<typename CommandDescriptor>
    void removeHandler()
    {
        removeHandler(CommandDescriptor::code);
    }

    WatchCommandSubscription& watchCommandSubscription();

private:
    struct CommandProcessorContext
    {
        std::unique_ptr<AbstractCommandProcessor> processor;
        bool markedForRemoval = false;
        std::atomic<int> usageCount;
        QnWaitCondition usageCountDecreased;

        CommandProcessorContext(): usageCount(0) {}
    };

    using CommandProcessors =
        std::map<int, std::unique_ptr<CommandProcessorContext>>;

    CommandLog* const m_commandLog;
    CommandProcessors m_commandProcessors;
    nx::network::aio::Timer m_aioTimer;
    mutable QnMutex m_mutex;
    WatchCommandSubscription m_watchTransactionSubscription;

    void removeHandler(int commandCode);
};

} // namespace nx::clusterdb::engine
