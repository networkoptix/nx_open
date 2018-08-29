#pragma once

#include <atomic>

#include <nx/network/aio/timer.h>
#include <nx/sql/async_sql_query_executor.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/log/log.h>
#include <nx/utils/subscription.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include "transaction_processor.h"
#include "transaction_transport_header.h"

namespace nx {
namespace data_sync_engine {

class TransactionLog;

/**
 * Dispaches transaction received from remote peer to a corresponding processor.
 */
class NX_DATA_SYNC_ENGINE_API IncomingTransactionDispatcher
{
public:
    using WatchTransactionSubscription = nx::utils::Subscription<
        const TransactionTransportHeader&,
        const CommandHeader&>;

    IncomingTransactionDispatcher(
        const QnUuid& moduleGuid,
        TransactionLog* const transactionLog);
    virtual ~IncomingTransactionDispatcher();

    /**
     * @note Method is non-blocking, result is delivered by invoking completionHandler.
     */
    void dispatchTransaction(
        TransactionTransportHeader transportHeader,
        Qn::SerializationFormat tranFormat,
        QByteArray data,
        TransactionProcessedHandler completionHandler);

    /**
     * Register processor function by command type.
     */
    template<int TransactionCommandValue, typename TransactionDataType>
    void registerTransactionHandler(
        typename TransactionProcessor<
            TransactionCommandValue, TransactionDataType
        >::ProcessEc2TransactionFunc processTranFunc)
    {
        using SpecificTransactionProcessor =
            TransactionProcessor<TransactionCommandValue, TransactionDataType>;

        auto context = std::make_unique<TransactionProcessorContext>();
        context->processor = std::make_unique<SpecificTransactionProcessor>(
            m_transactionLog,
            std::move(processTranFunc));

        m_transactionProcessors.emplace(
            (::ec2::ApiCommand::Value)TransactionCommandValue,
            std::move(context));
    }

    /**
     * Register processor function that does not need to save data to DB.
     */
    template<int TransactionCommandValue, typename TransactionDataType>
    void registerSpecialCommandHandler(
        typename SpecialCommandProcessor<
            TransactionCommandValue, TransactionDataType
        >::ProcessorFunc processTranFunc)
    {
        auto context = std::make_unique<TransactionProcessorContext>();
        context->processor = std::make_unique<SpecialCommandProcessor<
            TransactionCommandValue, TransactionDataType>>(
                std::move(processTranFunc));

        m_transactionProcessors.emplace(
            (::ec2::ApiCommand::Value)TransactionCommandValue,
            std::move(context));
    }

    /**
     * Waits for every running transaction of type transactionType processing to complete.
     */
    void removeHandler(::ec2::ApiCommand::Value transactionType);

    WatchTransactionSubscription& watchTransactionSubscription();

private:
    struct TransactionProcessorContext
    {
        std::unique_ptr<AbstractTransactionProcessor> processor;
        bool markedForRemoval = false;
        std::atomic<int> usageCount;
        QnWaitCondition usageCountDecreased;

        TransactionProcessorContext(): usageCount(0) {}
    };

    using TransactionProcessors =
        std::map<::ec2::ApiCommand::Value, std::unique_ptr<TransactionProcessorContext>>;

    const QnUuid m_moduleGuid;
    TransactionLog* const m_transactionLog;
    TransactionProcessors m_transactionProcessors;
    nx::network::aio::Timer m_aioTimer;
    mutable QnMutex m_mutex;
    WatchTransactionSubscription m_watchTransactionSubscription;

    void dispatchUbjsonTransaction(
        TransactionTransportHeader transportHeader,
        QByteArray serializedTransaction,
        TransactionProcessedHandler handler);

    void dispatchJsonTransaction(
        TransactionTransportHeader transportHeader,
        QByteArray serializedTransaction,
        TransactionProcessedHandler handler);

    template<typename TransactionDataSource>
    void dispatchTransaction(
        TransactionTransportHeader transportHeader,
        CommandHeader transaction,
        TransactionDataSource dataSource,
        TransactionProcessedHandler completionHandler);
};

} // namespace data_sync_engine
} // namespace nx
