#include "generic_transport.h"

#include "../transaction_transport.h"

namespace nx::data_sync_engine::transport {

GenericTransport::GenericTransport(
    std::unique_ptr<AbstractTransactionTransport> commandPipeline)
    :
    m_commandPipeline(std::move(commandPipeline))
{
    bindToAioThread(m_commandPipeline->getAioThread());

    m_commandPipeline->setOnGotTransaction(
        [this](auto... args) { handleHandshakeCommands(std::move(args)...); });
}

void GenericTransport::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_commandPipeline)
        m_commandPipeline->bindToAioThread(aioThread);
}

network::SocketAddress GenericTransport::remoteSocketAddr() const
{
    return m_commandPipeline->remoteSocketAddr();
}

void GenericTransport::setOnConnectionClosed(
    ConnectionClosedEventHandler handler)
{
    m_commandPipeline->setOnConnectionClosed(std::move(handler));
}

void GenericTransport::setOnGotTransaction(
    GotTransactionEventHandler handler)
{
    m_commandHandler = std::move(handler);
}

QnUuid GenericTransport::connectionGuid() const
{
    return m_commandPipeline->connectionGuid();
}

const TransactionTransportHeader& 
    GenericTransport::commonTransportHeaderOfRemoteTransaction() const
{
    return m_commandPipeline->commonTransportHeaderOfRemoteTransaction();
}

void GenericTransport::sendTransaction(
    TransactionTransportHeader transportHeader,
    const std::shared_ptr<const SerializableAbstractTransaction>& transactionSerializer)
{
    m_commandPipeline->sendTransaction(
        std::move(transportHeader),
        transactionSerializer);
}

void GenericTransport::processSpecialTransaction(
    const TransactionTransportHeader& transportHeader,
    Command<vms::api::SyncRequestData> data,
    TransactionProcessedHandler handler)
{
    static_cast<TransactionTransport*>(m_commandPipeline.get())->processSpecialTransaction(
        transportHeader,
        std::move(data),
        std::move(handler));
}

void GenericTransport::processSpecialTransaction(
    const TransactionTransportHeader& transportHeader,
    Command<vms::api::TranStateResponse> data,
    TransactionProcessedHandler handler)
{
    static_cast<TransactionTransport*>(m_commandPipeline.get())->processSpecialTransaction(
        transportHeader,
        std::move(data),
        std::move(handler));
}

void GenericTransport::processSpecialTransaction(
    const TransactionTransportHeader& transportHeader,
    Command<vms::api::TranSyncDoneData> data,
    TransactionProcessedHandler handler)
{
    static_cast<TransactionTransport*>(m_commandPipeline.get())->processSpecialTransaction(
        transportHeader,
        std::move(data),
        std::move(handler));
}

AbstractTransactionTransport& GenericTransport::commandPipeline()
{
    return *m_commandPipeline;
}

void GenericTransport::stopWhileInAioThread()
{
    m_commandPipeline.reset();
}

void GenericTransport::handleHandshakeCommands(
    Qn::SerializationFormat serializationFormat,
    const QByteArray& serializedData,
    TransactionTransportHeader transportHeader)
{
    // TODO: Deserializing header and processing handshake locally.

    if (m_commandHandler)
    {
        m_commandHandler(
            serializationFormat,
            serializedData,
            std::move(transportHeader));
    }
}

} // namespace nx::data_sync_engine::transport
