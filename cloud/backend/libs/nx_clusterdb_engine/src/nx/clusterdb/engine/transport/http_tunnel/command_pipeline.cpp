#include "command_pipeline.h"

namespace nx::clusterdb::engine::transport::http_tunnel {

CommandPipeline::CommandPipeline(
    const ProtocolVersionRange& protocolVersionRange,
    const ConnectionRequestAttributes& connectionRequestAttributes,
    const std::string& clusterId,
    std::unique_ptr<network::AbstractStreamSocket> tunnelConnection)
    :
    m_protocolVersionRange(protocolVersionRange),
    m_connectionRequestAttributes(connectionRequestAttributes),
    m_clusterId(clusterId),
    m_remoteEndpoint(tunnelConnection->getForeignAddress()),
    m_messagePipeline(std::move(tunnelConnection))
{
    m_messagePipeline.setMessageHandler(
        [this](auto... args) { processMessage(std::move(args)...); });

    m_messagePipeline.registerCloseHandler(
        [this](auto errCode) { onConnectionClosed(errCode); });
}

CommandPipeline::~CommandPipeline()
{
    if (isInSelfAioThread())
        stopWhileInAioThread();
}

void CommandPipeline::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_messagePipeline.bindToAioThread(aioThread);
}

void CommandPipeline::start()
{
    m_messagePipeline.startReadingConnection();
}

network::SocketAddress CommandPipeline::remotePeerEndpoint() const
{
    return m_remoteEndpoint;
}

void CommandPipeline::setOnConnectionClosed(
    ConnectionClosedEventHandler handler)
{
    m_connectionClosedEventHandler = std::move(handler);
}

void CommandPipeline::setOnGotTransaction(
    CommandDataHandler handler)
{
    m_gotTransactionEventHandler = std::move(handler);
}

std::string CommandPipeline::connectionGuid() const
{
    return m_connectionRequestAttributes.connectionId;
}

void CommandPipeline::sendTransaction(
    CommandTransportHeader transportHeader,
    const std::shared_ptr<const CommandSerializer>& transactionSerializer)
{
    // TODO: #ak Not serialize the transportHeader here.

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

void CommandPipeline::closeConnection()
{
    m_messagePipeline.closeConnection(SystemError::interrupted);
}

void CommandPipeline::stopWhileInAioThread()
{
    m_messagePipeline.pleaseStopSync();

    if (!m_closed)
        onConnectionClosed(SystemError::connectionAbort);
}

void CommandPipeline::processMessage(
    nx::network::server::FixedSizeMessage message)
{
    CommandTransportHeader cdbTransportHeader(m_protocolVersionRange.currentVersion());
    cdbTransportHeader.endpoint = m_remoteEndpoint;
    cdbTransportHeader.clusterId = m_clusterId;
    cdbTransportHeader.connectionId = m_connectionRequestAttributes.connectionId;
    cdbTransportHeader.transactionFormatVersion =
        highestProtocolVersionCompatibleWithRemotePeer();

    QnUbjsonReader<QByteArray> stream(&message.data);
    if (!QnUbjson::deserialize(&stream, &cdbTransportHeader.vmsTransportHeader))
    {
        NX_DEBUG(this, "systemId %1, remote node %2. Failed to deserialize transport header",
            m_clusterId, m_remoteEndpoint);
        closeConnection();
        return;
    }

    m_gotTransactionEventHandler(
        m_connectionRequestAttributes.remotePeer.dataFormat,
        message.data.mid(stream.pos()),
        std::move(cdbTransportHeader));
}

int CommandPipeline::highestProtocolVersionCompatibleWithRemotePeer() const
{
    return m_connectionRequestAttributes.remotePeerProtocolVersion >= m_protocolVersionRange.begin()
        ? m_protocolVersionRange.currentVersion()
        : m_connectionRequestAttributes.remotePeerProtocolVersion;
}

void CommandPipeline::onConnectionClosed(SystemError::ErrorCode errCode)
{
    m_closed = true;

    if (m_connectionClosedEventHandler)
        nx::utils::swapAndCall(m_connectionClosedEventHandler, errCode);
}

} // namespace nx::clusterdb::engine::transport::http_tunnel
