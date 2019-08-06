#include "acceptor.h"

#include <nx/vms/api/types/connection_types.h>

#include "command_pipeline.h"
#include "../generic_transport.h"
#include "../util.h"
#include "../../connection_manager.h"

namespace nx::clusterdb::engine::transport::http_tunnel {

Acceptor::Acceptor(
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

void Acceptor::registerHandlers(
    const std::string& rootPath,
    nx::network::http::server::rest::MessageDispatcher* messageDispatcher)
{
    m_tunnelingServer.registerRequestHandlers(
        rootPath,
        messageDispatcher);
}

void Acceptor::authorize(
    const network::http::RequestContext* httpRequestContext,
    CompletionHandler completionHandler)
{
    detail::ConnectRequestContext connectRequestContext;
    connectRequestContext.userAgent = network::http::getHeaderValue(
        httpRequestContext->request.headers, "User-Agent").toStdString();

    connectRequestContext.clusterId = extractSystemIdFromHttpRequest(*httpRequestContext);
    if (connectRequestContext.clusterId.empty())
    {
        NX_DEBUG(this, "Ignoring connect request without systemId from %1",
            httpRequestContext->connection->socket()->getForeignAddress());
        return completionHandler(
            nx::network::http::StatusCode::badRequest,
            std::move(connectRequestContext));
    }

    if (!connectRequestContext.attributes.read(httpRequestContext->request.headers))
    {
        return completionHandler(
            network::http::StatusCode::badRequest,
            std::move(connectRequestContext));
    }

    ConnectionRequestAttributes responseAttributes;
    responseAttributes.remotePeer = m_localPeerData;
    responseAttributes.remotePeerProtocolVersion = m_protocolVersionRange.currentVersion();
    responseAttributes.connectionId = connectRequestContext.attributes.connectionId;
    responseAttributes.write(&httpRequestContext->response->headers);

    completionHandler(
        network::http::StatusCode::ok,
        std::move(connectRequestContext));
}

void Acceptor::saveCreatedTunnel(
    std::unique_ptr<network::AbstractStreamSocket> connection,
    detail::ConnectRequestContext requestContext)
{
    const auto remotePeerEndpoint = connection->getForeignAddress();

    auto commandPipeline = std::make_unique<CommandPipeline>(
        m_protocolVersionRange,
        requestContext.attributes,
        requestContext.clusterId,
        std::move(connection));

    auto newTransport = std::make_unique<GenericTransport>(
        m_protocolVersionRange,
        m_commandLog,
        m_outgoingCommandFilter,
        requestContext.clusterId,
        requestContext.attributes,
        m_localPeerData,
        std::move(commandPipeline));

    newTransport->start();

    const auto remoteNodeId = 
        requestContext.attributes.remotePeer.id.toSimpleByteArray().toStdString();
    ConnectionManager::ConnectionContext context{
        std::move(newTransport),
        remoteNodeId,
        requestContext.attributes.connectionId,
        {requestContext.clusterId, remoteNodeId},
        requestContext.userAgent};

    if (!m_connectionManager->addNewConnection(std::move(context)))
    {
        NX_DEBUG(this,
            "Failed to add new transaction connection from (%1.%2; %3). connectionId %4",
            requestContext.attributes.remotePeer.id, requestContext.clusterId,
            remotePeerEndpoint, requestContext.attributes.connectionId);
        return;
    }
}

} // namespace nx::clusterdb::engine::transport::http_tunnel
