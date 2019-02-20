#include "acceptor.h"

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/multipart_msg_body_source.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_parse_helper.h>

#include <nx/p2p/p2p_serialization.h>
#include <nx/vms/api/types/connection_types.h>

#include "request_path.h"
#include "server_side_command_pipeline.h"
#include "../p2p_websocket/connection.h"
#include "../util.h"
#include "../../compatible_ec2_protocol_version.h"
#include "../../connection_manager.h"

namespace nx::clusterdb::engine::transport::p2p::http {

Acceptor::Acceptor(
    const std::string& nodeId,
    const ProtocolVersionRange& protocolVersionRange,
    CommandLog* commandLog,
    ConnectionManager* connectionManager,
    const OutgoingCommandFilter& commandFilter)
    :
    m_protocolVersionRange(protocolVersionRange),
    m_commandLog(commandLog),
    m_connectionManager(connectionManager),
    m_commandFilter(commandFilter),
    m_localPeer(
        QnUuid::fromStringSafe(nodeId),
        QnUuid::createUuid(),
        nx::vms::api::PeerType::cloudServer,
        Qn::UbjsonFormat)
{
}

void Acceptor::registerHandlers(
    const std::string& rootPath,
    nx::network::http::server::rest::MessageDispatcher* messageDispatcher)
{
    messageDispatcher->registerRequestProcessorFunc(
        nx::network::http::Method::get,
        nx::network::url::joinPath(rootPath, kCommandPath),
        [this](auto&&... args) { return openServerCommandChannel(std::move(args)...); });

    messageDispatcher->registerRequestProcessorFunc(
        nx::network::http::Method::post,
        nx::network::url::joinPath(rootPath, kCommandPath),
        [this](auto&&... args) { return processClientCommand(std::move(args)...); });
}

void Acceptor::openServerCommandChannel(
    nx::network::http::RequestContext requestContext,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    const auto& request = requestContext.request;
    auto response = requestContext.response;

    const auto remotePeerData = nx::p2p::deserializePeerData(request);
    const auto connectionId = nx::network::http::getHeaderValue(
        request.headers, Qn::EC2_CONNECTION_GUID_HEADER_NAME).toStdString();
    const auto systemId = extractSystemIdFromHttpRequest(requestContext);

    if (!validateRequest(requestContext, connectionId, systemId, remotePeerData))
        return completionHandler(nx::network::http::StatusCode::badRequest);

    vms::api::PeerDataEx localPeer;
    localPeer.assign(m_localPeer);
    localPeer.cloudHost = nx::network::SocketGlobals::cloud().cloudHost();
    localPeer.protoVersion = remotePeerData.protoVersion;
    nx::p2p::serializePeerData(*response, localPeer, remotePeerData.dataFormat);

    auto multipartMessageBodySource =
        std::make_unique<nx::network::http::MultipartMessageBodySource>(
            "28cc020ef6044ccaa9a524d3303a4cd2");
    auto commandPipeline = std::make_unique<ServerSideCommandPipeline>(
        multipartMessageBodySource.get(),
        "application/ubjson",
        requestContext.connection->socket()->getForeignAddress());
    commandPipeline->start();

    if (!registerNewConnection(
            requestContext,
            connectionId,
            systemId,
            std::exchange(commandPipeline, nullptr),
            localPeer,
            remotePeerData))
    {
        return completionHandler(nx::network::http::StatusCode::forbidden);
    }

    NX_DEBUG(this, "Accepted new connection from %1. systemId %2, connectionId %3",
        requestContext.connection->socket()->getForeignAddress(), systemId, connectionId);

    nx::network::http::RequestResult result(nx::network::http::StatusCode::ok);
    result.dataSource = std::move(multipartMessageBodySource);
    completionHandler(std::move(result));
}

void Acceptor::processClientCommand(
    nx::network::http::RequestContext requestContext,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    const auto connectionId = nx::network::http::getHeaderValue(
        requestContext.request.headers, Qn::EC2_CONNECTION_GUID_HEADER_NAME).toStdString();
    if (connectionId.empty())
    {
        NX_DEBUG(this, "Received request from %1 without connection id",
            requestContext.connection->socket()->getForeignAddress());
        return completionHandler(nx::network::http::StatusCode::badRequest);
    }

    const auto resultCode = forwardMessageToConnection(
        connectionId,
        std::exchange(requestContext.request.messageBody, {}));

    if (resultCode == nx::network::http::StatusCode::ok)
    {
        NX_VERBOSE(this, "Received message from %1 for connection %2",
            requestContext.connection->socket()->getForeignAddress(), connectionId);
    }
    else
    {
        NX_DEBUG(this, "Failed to process message from %1 for connection %2. %3",
            requestContext.connection->socket()->getForeignAddress(), connectionId,
                nx::network::http::StatusCode::toString(resultCode));
    }

    completionHandler(resultCode);
}

nx::network::http::StatusCode::Value Acceptor::forwardMessageToConnection(
    const std::string& connectionId,
    nx::Buffer message)
{
    bool messageProcessed = false;
    const bool connectionFound = m_connectionManager->modifyConnectionByIdSafe(
        connectionId,
        [message = std::move(message), &messageProcessed](
            AbstractConnection* connection) mutable
        {
            auto p2pConnection = dynamic_cast<websocket::Connection*>(connection);
            if (!p2pConnection)
                return;

            auto commandPipeline =
                dynamic_cast<ServerSideCommandPipeline*>(&p2pConnection->p2pTransport());
            if (!commandPipeline)
                return;

            commandPipeline->saveReceivedMessage(std::move(message));
            messageProcessed = true;
        });

    if (!connectionFound)
        return nx::network::http::StatusCode::notFound;
    if (!messageProcessed)
        return nx::network::http::StatusCode::forbidden;

    return nx::network::http::StatusCode::ok;
}

bool Acceptor::validateRequest(
    const nx::network::http::RequestContext& requestContext,
    const std::string& connectionId,
    const std::string& systemId,
    const vms::api::PeerDataEx& remotePeerInfo)
{
    auto connection = requestContext.connection;

    if (systemId.empty())
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
            "Ignoring connection request without systemId from peer %1",
            connection->socket()->getForeignAddress());
        return false;
    }

    if (connectionId.empty())
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
            "Ignoring connection request without connectionId from peer %1",
            connection->socket()->getForeignAddress());
        return false;
    }

    if (!m_protocolVersionRange.isCompatible(remotePeerInfo.protoVersion))
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
            "Incompatible connection request from (%1.%2; %3). Requested protocol version %4",
            remotePeerInfo.id, systemId, connection->socket()->getForeignAddress(),
            remotePeerInfo.protoVersion);
        return false;
    }

    return true;
}

bool Acceptor::registerNewConnection(
    const nx::network::http::RequestContext& requestContext,
    const std::string& connectionId,
    const std::string& systemId,
    std::unique_ptr<nx::p2p::IP2PTransport> commandPipeline,
    const vms::api::PeerDataEx& localPeer,
    const vms::api::PeerDataEx& remotePeer)
{
    const auto userAgent = nx::network::http::getHeaderValue(
        requestContext.request.headers, "User-Agent").toStdString();

    auto connection = std::make_unique<websocket::Connection>(
        m_protocolVersionRange,
        m_commandLog,
        systemId,
        m_commandFilter,
        connectionId,
        std::exchange(commandPipeline, nullptr),
        localPeer,
        remotePeer);

    ConnectionManager::ConnectionContext context{
        std::move(connection),
        connectionId,
        {systemId, remotePeer.id.toByteArray().toStdString()},
        userAgent};

    if (!m_connectionManager->addNewConnection(std::move(context)))
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
            "Failed to add new websocket transaction connection from (%1.%2; %3). connectionId %4",
            remotePeer.id, systemId, requestContext.connection->socket()->getForeignAddress(),
            connectionId);
        return false;
    }

    return true;
}

} // namespace nx::clusterdb::engine::transport::p2p::http
