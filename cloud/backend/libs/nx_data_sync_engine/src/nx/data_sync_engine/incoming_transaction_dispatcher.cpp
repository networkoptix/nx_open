#include "incoming_transaction_dispatcher.h"

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/ubjson.h>
#include <nx/utils/log/log.h>

#include "command_descriptor.h"
#include "serialization/transaction_deserializer.h"

namespace nx::clusterdb::engine {

IncomingTransactionDispatcher::IncomingTransactionDispatcher(
    TransactionLog* const transactionLog)
    :
    m_transactionLog(transactionLog)
{
}

IncomingTransactionDispatcher::~IncomingTransactionDispatcher()
{
    m_aioTimer.pleaseStopSync();
}

IncomingTransactionDispatcher::WatchTransactionSubscription&
    IncomingTransactionDispatcher::watchTransactionSubscription()
{
    return m_watchTransactionSubscription;
}

void IncomingTransactionDispatcher::dispatchTransaction(
    TransactionTransportHeader transportHeader,
    std::unique_ptr<DeserializableCommandData> commandData,
    TransactionProcessedHandler completionHandler)
{
    m_watchTransactionSubscription.notify(
        transportHeader,
        commandData->header());

    QnMutexLocker lock(&m_mutex);

    auto it = m_transactionProcessors.find(commandData->header().command);
    if (commandData->header().command == command::UpdatePersistentSequence::code)
        return; // TODO: #ak Do something.

    if (it == m_transactionProcessors.end() || it->second->markedForRemoval)
    {
        NX_VERBOSE(this, lm("Received unsupported transaction %1")
            .arg(commandData->header().command));
        // No handler registered for transaction type.
        m_aioTimer.post(
            [completionHandler = std::move(completionHandler)]
            {
                completionHandler(ResultCode::notFound);
            });
        return;
    }

    ++it->second->usageCount;

    lock.unlock();

    // TODO: should we always call completionHandler in the same thread?
    return it->second->processor->processTransaction(
        std::move(transportHeader),
        std::move(commandData),
        [it, completionHandler = std::move(completionHandler)](
            ResultCode resultCode)
        {
            --it->second->usageCount;
            it->second->usageCountDecreased.wakeAll();
            completionHandler(resultCode);
        });
}

void IncomingTransactionDispatcher::removeHandler(int commandCode)
{
    std::unique_ptr<TransactionProcessorContext> processor;

    {
        QnMutexLocker lock(&m_mutex);

        auto processorIter = m_transactionProcessors.find(commandCode);
        if (processorIter == m_transactionProcessors.end())
            return;
        processorIter->second->markedForRemoval = true;

        while (processorIter->second->usageCount.load() > 0)
            processorIter->second->usageCountDecreased.wait(lock.mutex());

        processor.swap(processorIter->second);
        m_transactionProcessors.erase(processorIter);
    }
}

} // namespace nx::clusterdb::engine
