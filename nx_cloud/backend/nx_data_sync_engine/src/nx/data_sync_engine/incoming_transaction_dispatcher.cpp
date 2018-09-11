#include "incoming_transaction_dispatcher.h"

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/ubjson.h>
#include <nx/utils/log/log.h>

#include "command_descriptor.h"
#include "serialization/transaction_deserializer.h"

namespace nx {
namespace data_sync_engine {

IncomingTransactionDispatcher::IncomingTransactionDispatcher(
    const QnUuid& moduleGuid,
    TransactionLog* const transactionLog)
:
    m_moduleGuid(moduleGuid),
    m_transactionLog(transactionLog)
{
}

IncomingTransactionDispatcher::~IncomingTransactionDispatcher()
{
    m_aioTimer.pleaseStopSync();
}

void IncomingTransactionDispatcher::dispatchTransaction(
    TransactionTransportHeader transportHeader,
    Qn::SerializationFormat tranFormat,
    QByteArray serializedTransaction,
    TransactionProcessedHandler handler)
{
    if (tranFormat == Qn::UbjsonFormat)
    {
        dispatchUbjsonTransaction(
            std::move(transportHeader),
            std::move(serializedTransaction),
            std::move(handler));
    }
    else if (tranFormat == Qn::JsonFormat)
    {
        dispatchJsonTransaction(
            std::move(transportHeader),
            std::move(serializedTransaction),
            std::move(handler));
    }
    else
    {
        m_aioTimer.post(
            [handler = std::move(handler)]{ handler(ResultCode::badRequest); });
    }
}

IncomingTransactionDispatcher::WatchTransactionSubscription&
    IncomingTransactionDispatcher::watchTransactionSubscription()
{
    return m_watchTransactionSubscription;
}

void IncomingTransactionDispatcher::dispatchUbjsonTransaction(
    TransactionTransportHeader transportHeader,
    QByteArray serializedTransaction,
    TransactionProcessedHandler handler)
{
    CommandHeader commandHeader(m_moduleGuid);
    auto dataSource =
        std::make_unique<TransactionUbjsonDataSource>(std::move(serializedTransaction));
    if (!TransactionDeserializer::deserialize(
            &dataSource->stream,
            &commandHeader,
            transportHeader.transactionFormatVersion))
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this), lm("Failed to deserialized ubjson transaction received from (%1, %2). size %3")
            .arg(transportHeader.systemId).arg(transportHeader.endpoint.toString())
            .arg(dataSource->serializedTransaction.size()));
        m_aioTimer.post(
            [handler = std::move(handler)]{ handler(ResultCode::badRequest); });
        return;
    }

    return dispatchTransaction(
        std::move(transportHeader),
        std::move(commandHeader),
        std::move(dataSource),
        std::move(handler));
}

void IncomingTransactionDispatcher::dispatchJsonTransaction(
    TransactionTransportHeader transportHeader,
    QByteArray serializedTransaction,
    TransactionProcessedHandler handler)
{
    CommandHeader commandHeader(m_moduleGuid);
    QJsonObject tranObject;
    // TODO: #ak put tranObject to some cache for later use
    if (!QJson::deserialize(serializedTransaction, &tranObject))
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this), lm("Failed to parse json transaction received from (%1, %2). size %3")
            .arg(transportHeader.systemId).arg(transportHeader.endpoint.toString())
            .arg(serializedTransaction.size()));
        m_aioTimer.post(
            [handler = std::move(handler)]{ handler(ResultCode::badRequest); });
        return;
    }
    if (!QJson::deserialize(tranObject["tran"], &commandHeader))
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this), lm("Failed to deserialize json transaction received from (%1, %2). size %3")
            .arg(transportHeader.systemId).arg(transportHeader.endpoint.toString())
            .arg(serializedTransaction.size()));
        m_aioTimer.post(
            [handler = std::move(handler)]{ handler(ResultCode::badRequest); });
        return;
    }

    return dispatchTransaction(
        std::move(transportHeader),
        std::move(commandHeader),
        tranObject["tran"].toObject(),
        std::move(handler));
}

template<typename TransactionDataSource>
void IncomingTransactionDispatcher::dispatchTransaction(
    TransactionTransportHeader transportHeader,
    CommandHeader commandHeader,
    TransactionDataSource dataSource,
    TransactionProcessedHandler completionHandler)
{
    m_watchTransactionSubscription.notify(transportHeader, commandHeader);

    QnMutexLocker lock(&m_mutex);

    auto it = m_transactionProcessors.find(commandHeader.command);
    if (commandHeader.command == command::UpdatePersistentSequence::code)
        return; // TODO: #ak Do something.

    if (it == m_transactionProcessors.end() || it->second->markedForRemoval)
    {
        NX_VERBOSE(this, lm("Received unsupported transaction %1")
            .arg(commandHeader.command));
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
        std::move(commandHeader),
        std::move(dataSource),
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

} // namespace data_sync_engine
} // namespace nx
