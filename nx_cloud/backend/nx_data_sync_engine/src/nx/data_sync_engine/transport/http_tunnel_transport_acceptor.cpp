#include "http_tunnel_transport_acceptor.h"

namespace nx::data_sync_engine::transport {

HttpTunnelTransportAcceptor::HttpTunnelTransportAcceptor(
    const QnUuid& peerId,
    const ProtocolVersionRange& /*protocolVersionRange*/,
    TransactionLog* /*transactionLog*/,
    ConnectionManager* /*connectionManager*/,
    const OutgoingCommandFilter& /*outgoingCommandFilter*/)
    :
    m_peerId(peerId),
    //m_protocolVersionRange(protocolVersionRange),
    //m_transactionLog(transactionLog),
    //m_connectionManager(connectionManager),
    //m_outgoingCommandFilter(outgoingCommandFilter),
    m_tunnelingServer(
        [this](auto... args) { saveCreatedTunnel(std::move(args)...); },
        nullptr) // TODO Set authorizer.
{
}

void HttpTunnelTransportAcceptor::registerHandlers(
    const std::string& rootPath,
    nx::network::http::server::rest::MessageDispatcher* messageDispatcher)
{
    m_tunnelingServer.registerRequestHandlers(
        rootPath,
        messageDispatcher);
}

void HttpTunnelTransportAcceptor::saveCreatedTunnel(
    std::unique_ptr<network::AbstractStreamSocket> /*connection*/)
{
    // TODO
}

} // namespace nx::data_sync_engine::transport
