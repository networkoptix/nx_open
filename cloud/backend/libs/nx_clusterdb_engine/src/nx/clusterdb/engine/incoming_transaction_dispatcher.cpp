#include "incoming_transaction_dispatcher.h"

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/ubjson.h>
#include <nx/utils/log/log.h>

#include "command_descriptor.h"
#include "serialization/transaction_deserializer.h"

namespace nx::clusterdb::engine {

IncomingCommandDispatcher::IncomingCommandDispatcher(
    CommandLog* const transactionLog)
    :
    m_commandLog(transactionLog)
{
}

IncomingCommandDispatcher::~IncomingCommandDispatcher()
{
    m_aioTimer.pleaseStopSync();
}

IncomingCommandDispatcher::WatchCommandSubscription&
    IncomingCommandDispatcher::watchCommandSubscription()
{
    return m_watchTransactionSubscription;
}

void IncomingCommandDispatcher::dispatchTransaction(
    CommandTransportHeader transportHeader,
    std::unique_ptr<DeserializableCommandData> commandData,
    CommandProcessedHandler completionHandler)
{
    m_watchTransactionSubscription.notify(
        transportHeader,
        commandData->header());

    QnMutexLocker lock(&m_mutex);

    auto it = m_commandProcessors.find(commandData->header().command);
    if (commandData->header().command == command::UpdatePersistentSequence::code)
        return; // TODO: #ak Do something.

    if (it == m_commandProcessors.end() || it->second->markedForRemoval)
    {
        NX_VERBOSE(this, "Received unsupported transaction %1",
            engine::toString(commandData->header()));
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
    return it->second->processor->process(
        std::move(transportHeader),
        std::move(commandData),
        [this, it, completionHandler = std::move(completionHandler)](
            ResultCode resultCode)
            {
                {
                    QnMutexLocker lock(&m_mutex);
                    --it->second->usageCount;
                    it->second->usageCountDecreased.wakeAll();
                }
                completionHandler(resultCode);
            });
}

void IncomingCommandDispatcher::removeHandler(int commandCode)
{
    std::unique_ptr<CommandProcessorContext> processor;

    {
        QnMutexLocker lock(&m_mutex);

        auto processorIter = m_commandProcessors.find(commandCode);
        if (processorIter == m_commandProcessors.end())
            return;
        processorIter->second->markedForRemoval = true;

        while (processorIter->second->usageCount.load() > 0)
            processorIter->second->usageCountDecreased.wait(lock.mutex());

        processor.swap(processorIter->second);
        m_commandProcessors.erase(processorIter);
    }
}

} // namespace nx::clusterdb::engine
