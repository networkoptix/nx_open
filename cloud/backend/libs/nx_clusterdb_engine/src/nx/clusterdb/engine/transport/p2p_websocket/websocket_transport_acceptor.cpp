#include "websocket_transport_acceptor.h"

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/network/websocket/websocket.h>
#include <nx/network/websocket/websocket_handshake.h>

#include <nx/p2p/p2p_serialization.h>
#include <nx/p2p/transport/p2p_websocket_transport.h>
#include <nx/vms/api/types/connection_types.h>

#include "../../connection_manager.h"
#include "../../compatible_ec2_protocol_version.h"
#include "../../http/sync_connection_request_handler.h"
#include "request_path.h"
#include "websocket_transaction_transport.h"

namespace nx::clusterdb::engine::transport::p2p::websocket {

WebSocketTransportAcceptor::WebSocketTransportAcceptor(
    const QnUuid& moduleGuid,
    const ProtocolVersionRange& protocolVersionRange,
    CommandLog* transactionLog,
    ConnectionManager* connectionManager,
    const OutgoingCommandFilter& outgoingCommandFilter)
    :
    m_protocolVersionRange(protocolVersionRange),
    m_commandLog(transactionLog),
    m_connectionManager(connectionManager),
    m_outgoingCommandFilter(outgoingCommandFilter),
    m_localPeerData(
        moduleGuid,
        QnUuid::createUuid(),
        vms::api::PeerType::cloudServer,
        Qn::UbjsonFormat)
{
}

void WebSocketTransportAcceptor::registerHandlers(
    const std::string& rootPath,
    nx::network::http::server::rest::MessageDispatcher* messageDispatcher)
{
    messageDispatcher->registerRequestProcessorFunc(
        nx::network::http::Method::get,
        nx::network::url::joinPath(rootPath, kCommandPath),
        [this](auto&&... args) { return createConnection(std::move(args)...); });

    // TODO: #ak Remove in ~ 3 months after 4.0 release.
    messageDispatcher->registerRequestProcessorFunc(
        nx::network::http::Method::get,
        nx::network::url::joinPath(rootPath, kDeprecatedCommandPath),
        [this](auto&&... args) { return createConnection(std::move(args)...); });
}

void WebSocketTransportAcceptor::createConnection(
    nx::network::http::RequestContext requestContext,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    using namespace nx::network;

    const auto systemId = extractSystemIdFromHttpRequest(requestContext);

    const auto& request = requestContext.request;
    auto connection = requestContext.connection;
    auto response = requestContext.response;

    auto remotePeerInfo = nx::p2p::deserializePeerData(request);

    if (systemId.empty())
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
            lm("Ignoring createWebsocketTransactionConnection request without systemId from peer %1")
            .arg(connection->socket()->getForeignAddress()));
        return completionHandler(nx::network::http::StatusCode::badRequest);
    }

    if (!m_protocolVersionRange.isCompatible(remotePeerInfo.protoVersion))
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
            lm("Incompatible connection request from (%1.%2; %3). Requested protocol version %4")
            .args(remotePeerInfo.id, systemId, connection->socket()->getForeignAddress(),
                remotePeerInfo.protoVersion));
        return completionHandler(nx::network::http::StatusCode::badRequest);
    }

    vms::api::PeerDataEx localPeer;
    localPeer.assign(m_localPeerData);
    localPeer.cloudHost = nx::network::SocketGlobals::cloud().cloudHost();
    localPeer.protoVersion = remotePeerInfo.protoVersion;
    nx::p2p::serializePeerData(*response, localPeer, remotePeerInfo.dataFormat);

    auto error = nx::network::websocket::validateRequest(request, response);
    if (error != nx::network::websocket::Error::noError)
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
            lm("Can't upgrade request from peer %1 to webSocket. Error: %2")
            .args(connection->socket()->getForeignAddress(), (int) error));
        return completionHandler(nx::network::http::StatusCode::badRequest);
    }

    nx::network::http::RequestResult result(nx::network::http::StatusCode::switchingProtocols);
    result.connectionEvents.onResponseHasBeenSent =
        [this, localPeer = std::move(localPeer), remotePeerInfo = std::move(remotePeerInfo),
            request = std::move(request), systemId](
                network::http::HttpServerConnection* connection) mutable
        {
            addWebSocketTransactionTransport(
                connection->takeSocket(),
                std::move(localPeer),
                std::move(remotePeerInfo),
                systemId,
                network::http::getHeaderValue(request.headers, "User-Agent").toStdString());
        };
    completionHandler(std::move(result));
}

void WebSocketTransportAcceptor::addWebSocketTransactionTransport(
    std::unique_ptr<network::AbstractStreamSocket> connection,
    vms::api::PeerDataEx localPeerInfo,
    vms::api::PeerDataEx remotePeerInfo,
    const std::string& systemId,
    const std::string& userAgent)
{
    const auto remoteAddress = connection->getForeignAddress();
    auto p2pTransport = std::make_unique<nx::p2p::P2PWebsocketTransport>(
        std::move(connection), network::websocket::FrameType::binary);

    p2pTransport->start();
    const auto connectionId = QnUuid::createUuid().toSimpleString().toStdString();

    auto transactionTransport = std::make_unique<WebsocketCommandTransport>(
        m_protocolVersionRange,
        m_commandLog,
        systemId.c_str(),
        m_outgoingCommandFilter,
        connectionId,
        std::move(p2pTransport),
        localPeerInfo,
        remotePeerInfo);

    ConnectionManager::ConnectionContext context{
        std::move(transactionTransport),
        connectionId,
        {systemId, remotePeerInfo.id.toByteArray().toStdString()},
        userAgent};

    if (!m_connectionManager->addNewConnection(std::move(context)))
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
            lm("Failed to add new websocket transaction connection from (%1.%2; %3). connectionId %4")
            .args(remotePeerInfo.id, systemId, remoteAddress, connectionId));
    }
}

} // namespace nx::clusterdb::engine::transport::p2p::websocket
