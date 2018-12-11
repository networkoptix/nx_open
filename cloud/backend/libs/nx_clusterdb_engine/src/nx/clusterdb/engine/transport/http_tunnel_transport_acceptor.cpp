#include "http_tunnel_transport_acceptor.h"

#include <nx/vms/api/types/connection_types.h>

#include "generic_transport.h"
#include "http_tunnel_transport_connection.h"
#include "../connection_manager.h"

namespace nx::clusterdb::engine::transport {

HttpTunnelTransportAcceptor::HttpTunnelTransportAcceptor(
    const QnUuid& peerId,
    const ProtocolVersionRange& protocolVersionRange,
    CommandLog* transactionLog,
    ConnectionManager* connectionManager,
    const OutgoingCommandFilter& outgoingCommandFilter)
    :
    m_peerId(peerId),
    m_protocolVersionRange(protocolVersionRange),
    m_commandLog(transactionLog),
    m_connectionManager(connectionManager),
    m_outgoingCommandFilter(outgoingCommandFilter),
    m_localPeerData(
        peerId,
        QnUuid::createUuid(), //< Instance id. Whatever it is.
        vms::api::PeerType::cloudServer,
        Qn::UbjsonFormat),
    m_tunnelingServer(
        [this](auto... args) { saveCreatedTunnel(std::move(args)...); },
        this)
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

void HttpTunnelTransportAcceptor::authorize(
    const network::http::RequestContext* httpRequestContext,
    CompletionHandler completionHandler)
{
    detail::ConnectRequestContext connectRequestContext;
    connectRequestContext.userAgent = network::http::getHeaderValue(
        httpRequestContext->request.headers, "User-Agent").toStdString();

    if (!fetchDataFromConnectRequest(
            httpRequestContext->request,
            &connectRequestContext.attributes))
    {
        return completionHandler(
            network::http::StatusCode::badRequest,
            std::move(connectRequestContext));
    }
    // TODO Reading systemId.

    completionHandler(
        network::http::StatusCode::ok,
        std::move(connectRequestContext));
}

void HttpTunnelTransportAcceptor::saveCreatedTunnel(
    std::unique_ptr<network::AbstractStreamSocket> connection,
    detail::ConnectRequestContext requestContext)
{
    const auto remotePeerEndpoint = connection->getForeignAddress();

    auto commandPipeline = std::make_unique<HttpTunnelTransportConnection>(
        m_protocolVersionRange,
        requestContext.attributes,
        std::move(connection));

    auto newTransport = std::make_unique<GenericTransport>(
        m_protocolVersionRange,
        m_commandLog,
        m_outgoingCommandFilter,
        requestContext.systemId,
        requestContext.attributes,
        m_localPeerData,
        std::move(commandPipeline));

    ConnectionManager::ConnectionContext context{
        std::move(newTransport),
        requestContext.attributes.connectionId,
        {requestContext.systemId,
            requestContext.attributes.remotePeer.id.toByteArray().toStdString()},
        requestContext.userAgent};

    if (!m_connectionManager->addNewConnection(std::move(context)))
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
            lm("Failed to add new transaction connection from (%1.%2; %3). connectionId %4")
            .args(requestContext.attributes.remotePeer.id,
                requestContext.systemId, remotePeerEndpoint,
                requestContext.attributes.connectionId));
        return;
    }
}

} // namespace nx::clusterdb::engine::transport
