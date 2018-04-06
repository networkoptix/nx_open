#include "incoming_transaction_dispatcher.h"

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/ubjson.h>
#include <nx/utils/log/log.h>
#include <transaction/transaction.h>

#include "serialization/transaction_deserializer.h"

namespace nx {
namespace cdb {
namespace ec2 {

using namespace ::ec2;

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
            [handler = std::move(handler)]{ handler(api::ResultCode::badRequest); });
    }
}

void IncomingTransactionDispatcher::dispatchUbjsonTransaction(
    TransactionTransportHeader transportHeader,
    QByteArray serializedTransaction,
    TransactionProcessedHandler handler)
{
    QnAbstractTransaction transactionHeader(m_moduleGuid);
    auto dataSource =
        std::make_unique<TransactionUbjsonDataSource>(std::move(serializedTransaction));
    if (!TransactionDeserializer::deserialize(
            &dataSource->stream,
            &transactionHeader,
            transportHeader.transactionFormatVersion))
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("Failed to deserialized ubjson transaction received from (%1, %2). size %3")
            .arg(transportHeader.systemId).arg(transportHeader.endpoint.toString())
            .arg(dataSource->serializedTransaction.size()), cl_logDEBUG1);
        m_aioTimer.post(
            [handler = std::move(handler)]{ handler(api::ResultCode::badRequest); });
        return;
    }

    return dispatchTransaction(
        std::move(transportHeader),
        std::move(transactionHeader),
        std::move(dataSource),
        std::move(handler));
}

void IncomingTransactionDispatcher::dispatchJsonTransaction(
    TransactionTransportHeader transportHeader,
    QByteArray serializedTransaction,
    TransactionProcessedHandler handler)
{
    QnAbstractTransaction transactionHeader(m_moduleGuid);
    QJsonObject tranObject;
    // TODO: #ak put tranObject to some cache for later use
    if (!QJson::deserialize(serializedTransaction, &tranObject))
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("Failed to parse json transaction received from (%1, %2). size %3")
            .arg(transportHeader.systemId).arg(transportHeader.endpoint.toString())
            .arg(serializedTransaction.size()), cl_logDEBUG1);
        m_aioTimer.post(
            [handler = std::move(handler)]{ handler(api::ResultCode::badRequest); });
        return;
    }
    if (!QJson::deserialize(tranObject["tran"], &transactionHeader))
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("Failed to deserialize json transaction received from (%1, %2). size %3")
            .arg(transportHeader.systemId).arg(transportHeader.endpoint.toString())
            .arg(serializedTransaction.size()), cl_logDEBUG1);
        m_aioTimer.post(
            [handler = std::move(handler)]{ handler(api::ResultCode::badRequest); });
        return;
    }

    return dispatchTransaction(
        std::move(transportHeader),
        std::move(transactionHeader),
        tranObject["tran"].toObject(),
        std::move(handler));
}

template<typename TransactionDataSource>
void IncomingTransactionDispatcher::dispatchTransaction(
    TransactionTransportHeader transportHeader,
    ::ec2::QnAbstractTransaction transactionHeader,
    TransactionDataSource dataSource,
    TransactionProcessedHandler completionHandler)
{
    QnMutexLocker lock(&m_mutex);

    auto it = m_transactionProcessors.find(transactionHeader.command);
    if (transactionHeader.command == ::ec2::ApiCommand::updatePersistentSequence)
        return; // TODO: #ak Do something.
    if (it == m_transactionProcessors.end() || it->second->markedForRemoval)
    {
        NX_VERBOSE(this, lm("Received unsupported transaction %1")
            .arg(::ec2::ApiCommand::toString(transactionHeader.command)));
        // No handler registered for transaction type.
        m_aioTimer.post(
            [completionHandler = std::move(completionHandler)]
            {
                completionHandler(api::ResultCode::notFound);
            });
        return;
    }

    ++it->second->usageCount;

    lock.unlock();

    // TODO: should we always call completionHandler in the same thread?
    return it->second->processor->processTransaction(
        std::move(transportHeader),
        std::move(transactionHeader),
        std::move(dataSource),
        [it, completionHandler = std::move(completionHandler)](
            api::ResultCode resultCode)
        {
            --it->second->usageCount;
            it->second->usageCountDecreased.wakeAll();
            completionHandler(resultCode);
        });
}

} // namespace ec2
} // namespace cdb
} // namespace nx
