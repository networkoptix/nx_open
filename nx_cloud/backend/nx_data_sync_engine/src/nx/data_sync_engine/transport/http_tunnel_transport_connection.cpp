#include "http_tunnel_transport_connection.h"

namespace nx::data_sync_engine::transport {

HttpTunnelTransportConnection::HttpTunnelTransportConnection(
    const std::string& connectionId,
    std::unique_ptr<network::AbstractStreamSocket> tunnelConnection,
    int protocolVersion)
    :
    m_connectionId(connectionId),
    m_remoteEndpoint(tunnelConnection->getForeignAddress()),
    m_commonTransportHeader(protocolVersion),
    m_messagePipeline(std::move(tunnelConnection))
{
    m_messagePipeline.setMessageHandler(
        [this](auto... args) { processMessage(std::move(args)...); });
}

void HttpTunnelTransportConnection::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_messagePipeline.bindToAioThread(aioThread);
}

network::SocketAddress HttpTunnelTransportConnection::remotePeerEndpoint() const
{
    return m_remoteEndpoint;
}

ConnectionClosedSubscription& HttpTunnelTransportConnection::connectionClosedSubscription()
{
    return m_connectionClosedSubscription;
}

void HttpTunnelTransportConnection::setOnGotTransaction(
    GotTransactionEventHandler handler)
{
    m_gotTransactionEventHandler = std::move(handler);
}

QnUuid HttpTunnelTransportConnection::connectionGuid() const
{
    return QnUuid::fromStringSafe(m_connectionId);
}

const TransactionTransportHeader& 
    HttpTunnelTransportConnection::commonTransportHeaderOfRemoteTransaction() const
{
    return m_commonTransportHeader;
}

void HttpTunnelTransportConnection::sendTransaction(
    TransactionTransportHeader /*transportHeader*/,
    const std::shared_ptr<const SerializableAbstractTransaction>& /*transactionSerializer*/)
{
    // TODO
}

void HttpTunnelTransportConnection::start()
{
    // Sending local state.
}

void HttpTunnelTransportConnection::stopWhileInAioThread()
{
    m_messagePipeline.pleaseStopSync();
}

void HttpTunnelTransportConnection::processMessage(
    nx::network::server::FixedSizeMessage /*message*/)
{
    // TODO
}

} // namespace nx::data_sync_engine::transport
