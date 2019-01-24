#include "command_pipeline.h"

namespace nx::clusterdb::engine::transport {

HttpTunnelTransportConnection::HttpTunnelTransportConnection(
    const ProtocolVersionRange& protocolVersionRange,
    const ConnectionRequestAttributes& connectionRequestAttributes,
    std::unique_ptr<network::AbstractStreamSocket> tunnelConnection)
    :
    m_protocolVersionRange(protocolVersionRange),
    m_connectionRequestAttributes(connectionRequestAttributes),
    m_remoteEndpoint(tunnelConnection->getForeignAddress()),
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

void HttpTunnelTransportConnection::start()
{
    // TODO
}

network::SocketAddress HttpTunnelTransportConnection::remotePeerEndpoint() const
{
    return m_remoteEndpoint;
}

void HttpTunnelTransportConnection::setOnConnectionClosed(
    ConnectionClosedEventHandler handler)
{
    m_connectionClosedEventHandler = std::move(handler);
}

void HttpTunnelTransportConnection::setOnGotTransaction(
    CommandDataHandler handler)
{
    m_gotTransactionEventHandler = std::move(handler);
}

std::string HttpTunnelTransportConnection::connectionGuid() const
{
    return m_connectionRequestAttributes.connectionId;
}

void HttpTunnelTransportConnection::sendTransaction(
    CommandTransportHeader transportHeader,
    const std::shared_ptr<const CommandSerializer>& transactionSerializer)
{
    auto serializedTransaction = transactionSerializer->serialize(
        m_connectionRequestAttributes.remotePeer.dataFormat,
        std::move(transportHeader),
        highestProtocolVersionCompatibleWithRemotePeer());

    dispatch(
        [this, serializedTransaction = std::move(serializedTransaction)]()
        {
            m_messagePipeline.sendMessage({std::move(serializedTransaction)});
        });
}

void HttpTunnelTransportConnection::closeConnection()
{
    // TODO
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

int HttpTunnelTransportConnection::highestProtocolVersionCompatibleWithRemotePeer() const
{
    return m_connectionRequestAttributes.remotePeerProtocolVersion >= m_protocolVersionRange.begin()
        ? m_protocolVersionRange.currentVersion()
        : m_connectionRequestAttributes.remotePeerProtocolVersion;
}

} // namespace nx::clusterdb::engine::transport
