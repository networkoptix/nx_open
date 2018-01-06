#pragma once

#include <atomic>

#include <nx/network/aio/timer.h>
#include <nx/utils/db/async_sql_query_executor.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include <nx/cloud/cdb/api/result_code.h>
#include <transaction/transaction_transport_header.h>
#include <transaction/transaction.h>

#include "transaction_processor.h"
#include "transaction_transport_header.h"

namespace nx {
namespace cdb {
namespace ec2 {

class TransactionLog;

/**
 * Dispaches transaction received from remote peer to a corresponding processor.
 */
class IncomingTransactionDispatcher
{
public:
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
    template<int TransactionCommandValue, typename TransactionDataType, typename AuxiliaryArgType>
    void registerTransactionHandler(
        typename TransactionProcessor<
            TransactionCommandValue, TransactionDataType, AuxiliaryArgType
        >::ProcessEc2TransactionFunc processTranFunc,
        typename TransactionProcessor<
            TransactionCommandValue, TransactionDataType, AuxiliaryArgType
        >::OnTranProcessedFunc onTranProcessedFunc)
    {
        auto context = std::make_unique<TransactionProcessorContext>();
        context->processor = std::make_unique<TransactionProcessor<
            TransactionCommandValue, TransactionDataType, AuxiliaryArgType>>(
                m_transactionLog,
                std::move(processTranFunc),
                std::move(onTranProcessedFunc));

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
    void removeHandler(::ec2::ApiCommand::Value transactionType)
    {
        std::unique_ptr<TransactionProcessorContext> processor;

        {
            QnMutexLocker lock(&m_mutex);

            auto processorIter = m_transactionProcessors.find(transactionType);
            if (processorIter == m_transactionProcessors.end())
                return;
            processorIter->second->markedForRemoval = true;

            while (processorIter->second->usageCount.load() > 0)
                processorIter->second->usageCountDecreased.wait(lock.mutex());

            processor.swap(processorIter->second);
            m_transactionProcessors.erase(processorIter);
        }
    }

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
        ::ec2::QnAbstractTransaction transaction,
        TransactionDataSource dataSource,
        TransactionProcessedHandler completionHandler);
};

} // namespace ec2
} // namespace cdb
} // namespace nx
