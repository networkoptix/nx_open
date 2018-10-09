#include "http_transport_acceptor.h"

#include <nx/fusion/serialization/lexical.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/empty_message_body_source.h>
#include <nx/network/socket_global.h>
#include <nx/vms/api/types/connection_types.h>

#include "generic_transport.h"
#include "../connection_manager.h"
#include "../compatible_ec2_protocol_version.h"

namespace nx::data_sync_engine::transport {

HttpTransportAcceptor::HttpTransportAcceptor(
    const QnUuid& peerId,
    const ProtocolVersionRange& protocolVersionRange,
    TransactionLog* transactionLog,
    ConnectionManager* connectionManager,
    const OutgoingCommandFilter& outgoingCommandFilter)
    :
    m_protocolVersionRange(protocolVersionRange),
    m_transactionLog(transactionLog),
    m_connectionManager(connectionManager),
    m_outgoingCommandFilter(outgoingCommandFilter),
    m_localPeerData(
        peerId,
        QnUuid::createUuid(),
        vms::api::PeerType::cloudServer,
        Qn::UbjsonFormat),
    m_connectionGuardSharedState(
        std::make_shared<::ec2::ConnectionGuardSharedState>())
{
}

void HttpTransportAcceptor::createConnection(
    nx::network::http::HttpServerConnection* const connection,
    const std::string& systemId,
    nx::network::http::Request request,
    nx::network::http::Response* const response,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    // GET /ec2/events/ConnectingStage2?guid=%7B8b939668-837d-4658-9d7a-e2cc6c12a38b%7D&
    //  runtime-guid=%7B0eac9718-4e37-4459-8799-c3023d4f7cb5%7D&system-identity-time=0&isClient
    // TODO: #ak

    if (systemId.empty())
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
            lm("Ignoring createTransactionConnection request without systemId from %1")
            .args(connection->socket()->getForeignAddress()));
        return completionHandler(nx::network::http::StatusCode::badRequest);
    }

    ConnectionRequestAttributes connectionRequestAttributes;
    if (!fetchDataFromConnectRequest(request, &connectionRequestAttributes))
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
            lm("Error parsing createTransactionConnection request from (%1.%2; %3)")
            .args(connectionRequestAttributes.remotePeer.id, systemId,
                connection->socket()->getForeignAddress()));
        return completionHandler(nx::network::http::StatusCode::badRequest);
    }

    if (!m_protocolVersionRange.isCompatible(
            connectionRequestAttributes.remotePeerProtocolVersion))
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
            lm("Incompatible connection request from (%1.%2; %3). Requested protocol version %4")
            .args(connectionRequestAttributes.remotePeer.id, systemId,
                connection->socket()->getForeignAddress(),
                connectionRequestAttributes.remotePeerProtocolVersion));
        return completionHandler(nx::network::http::StatusCode::badRequest);
    }

    NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
        lm("Received createTransactionConnection request from (%1.%2; %3). connectionId %4")
        .args(connectionRequestAttributes.remotePeer.id, systemId,
            connection->socket()->getForeignAddress(),
            connectionRequestAttributes.connectionId));

    auto commandPipeline = std::make_unique<TransactionTransport>(
        m_protocolVersionRange,
        connection->getAioThread(),
        m_connectionGuardSharedState,
        connectionRequestAttributes,
        systemId,
        m_localPeerData,
        connection->socket()->getForeignAddress(),
        request);

    auto newTransport = std::make_unique<GenericTransport>(
        m_protocolVersionRange,
        m_transactionLog,
        m_outgoingCommandFilter,
        systemId,
        connectionRequestAttributes,
        m_localPeerData,
        std::move(commandPipeline));

    ConnectionManager::ConnectionContext context{
        std::move(newTransport),
        connectionRequestAttributes.connectionId,
        {systemId, connectionRequestAttributes.remotePeer.id.toByteArray().toStdString()},
        network::http::getHeaderValue(request.headers, "User-Agent").toStdString()};

    if (!m_connectionManager->addNewConnection(std::move(context)))
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
            lm("Failed to add new transaction connection from (%1.%2; %3). connectionId %4")
            .args(connectionRequestAttributes.remotePeer.id, systemId,
                connection->socket()->getForeignAddress(),
                connectionRequestAttributes.connectionId));
        return completionHandler(nx::network::http::StatusCode::forbidden);
    }

    auto requestResult =
        prepareOkResponseToCreateTransactionConnection(
            connectionRequestAttributes,
            response);
    completionHandler(std::move(requestResult));
}

void HttpTransportAcceptor::pushTransaction(
    nx::network::http::HttpServerConnection* const connection,
    const std::string& /*systemId*/,
    nx::network::http::Request request,
    nx::network::http::Response* const /*response*/,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    auto connectionIdIter = request.headers.find(Qn::EC2_CONNECTION_GUID_HEADER_NAME);
    if (connectionIdIter == request.headers.end())
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this), lm("Received %1 request from %2 without required header %3")
            .arg(request.requestLine.url.path()).arg(connection->socket()->getForeignAddress())
            .arg(Qn::EC2_CONNECTION_GUID_HEADER_NAME));
        return completionHandler(nx::network::http::StatusCode::badRequest);
    }
    const auto connectionId = connectionIdIter->second.toStdString();

    bool foundConnectionOfExpectedType = true;
    const auto connectionFound = m_connectionManager->modifyConnectionByIdSafe(
        connectionId,
        std::bind(&HttpTransportAcceptor::postTransactionToTransport, this,
            std::placeholders::_1, std::move(request), &foundConnectionOfExpectedType));

    if (!connectionFound)
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
            lm("Received %1 request from %2 for unknown connection %3")
            .args(request.requestLine.url.path(),
                connection->socket()->getForeignAddress(), connectionId));
        return completionHandler(nx::network::http::StatusCode::notFound);
    }

    if (!foundConnectionOfExpectedType)
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
            lm("Received %1 request from %2 for connection %3 of unexpected type")
            .args(request.requestLine.url.path(),
                connection->socket()->getForeignAddress(), connectionId));
        return completionHandler(nx::network::http::StatusCode::badRequest);
    }

    NX_VERBOSE(QnLog::EC2_TRAN_LOG.join(this),
        lm("Received %1 request from %2 for connection %3")
        .args(request.requestLine.url.path(),
            connection->socket()->getForeignAddress(), connectionId));

    completionHandler(nx::network::http::StatusCode::ok);
}

bool HttpTransportAcceptor::fetchDataFromConnectRequest(
    const nx::network::http::Request& request,
    ConnectionRequestAttributes* connectionRequestAttributes)
{
    auto connectionIdIter = request.headers.find(Qn::EC2_CONNECTION_GUID_HEADER_NAME);
    if (connectionIdIter == request.headers.end())
        return false;
    connectionRequestAttributes->connectionId = connectionIdIter->second.toStdString();

    if (!nx::network::http::readHeader(
            request.headers,
            Qn::EC2_PROTO_VERSION_HEADER_NAME,
            &connectionRequestAttributes->remotePeerProtocolVersion))
    {
        NX_VERBOSE(QnLog::EC2_TRAN_LOG.join(this),
            lm("Missing required header %1").arg(Qn::EC2_PROTO_VERSION_HEADER_NAME));
        return false;
    }

    QUrlQuery query = QUrlQuery(request.requestLine.url.query());
    const bool isClient = query.hasQueryItem("isClient");
    const bool isMobileClient = query.hasQueryItem("isMobile");
    QnUuid remoteGuid = QnUuid::fromStringSafe(query.queryItemValue("guid"));
    QnUuid remoteRuntimeGuid = QnUuid::fromStringSafe(query.queryItemValue("runtime-guid"));
    if (remoteGuid.isNull())
        remoteGuid = QnUuid::createUuid();

    const vms::api::PeerType peerType = isMobileClient
        ? vms::api::PeerType::mobileClient
        : (isClient ? vms::api::PeerType::desktopClient : vms::api::PeerType::server);

    Qn::SerializationFormat dataFormat = Qn::UbjsonFormat;
    if (query.hasQueryItem("format"))
    {
        if (!QnLexical::deserialize(query.queryItemValue("format"), &dataFormat))
        {
            NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
                lm("Invalid value of \"format\" field: %1")
                .arg(query.queryItemValue("format")));
            return false;
        }
    }

    // Checking content encoding requested by remote peer.
    auto acceptEncodingHeaderIter = request.headers.find("Accept-Encoding");
    if (acceptEncodingHeaderIter != request.headers.end())
    {
        nx::network::http::header::AcceptEncodingHeader acceptEncodingHeader(
            acceptEncodingHeaderIter->second);
        if (acceptEncodingHeader.encodingIsAllowed("identity"))
            connectionRequestAttributes->contentEncoding = "identity";
        else if (acceptEncodingHeader.encodingIsAllowed("gzip"))
            connectionRequestAttributes->contentEncoding = "gzip";
    }

    connectionRequestAttributes->remotePeer =
        vms::api::PeerData(remoteGuid, remoteRuntimeGuid, peerType, dataFormat);

    return true;
}

nx::network::http::RequestResult
    HttpTransportAcceptor::prepareOkResponseToCreateTransactionConnection(
        const ConnectionRequestAttributes& connectionRequestAttributes,
        nx::network::http::Response* const response)
{
    response->headers.emplace(
        "Content-Type",
        ::ec2::QnTransactionTransportBase::TUNNEL_CONTENT_TYPE);
    response->headers.emplace(
        "Content-Encoding",
        connectionRequestAttributes.contentEncoding.c_str());
    response->headers.emplace(
        Qn::EC2_GUID_HEADER_NAME,
        m_localPeerData.id.toByteArray());
    response->headers.emplace(
        Qn::EC2_RUNTIME_GUID_HEADER_NAME,
        m_localPeerData.instanceId.toByteArray());

    NX_ASSERT(m_protocolVersionRange.isCompatible(
        connectionRequestAttributes.remotePeerProtocolVersion));

    response->headers.emplace(
        Qn::EC2_PROTO_VERSION_HEADER_NAME,
        nx::String::number(connectionRequestAttributes.remotePeerProtocolVersion));
    response->headers.emplace("X-Nx-Cloud", "true");
    response->headers.emplace(Qn::EC2_BASE64_ENCODING_REQUIRED_HEADER_NAME, "true");

    nx::network::http::RequestResult requestResult(nx::network::http::StatusCode::ok);

    requestResult.connectionEvents.onResponseHasBeenSent =
        std::bind(&HttpTransportAcceptor::startOutgoingChannel, this,
            connectionRequestAttributes.connectionId,
            std::placeholders::_1);

    requestResult.dataSource =
        std::make_unique<nx::network::http::EmptyMessageBodySource>(
            ::ec2::QnTransactionTransportBase::TUNNEL_CONTENT_TYPE,
            boost::none);

    return requestResult;
}

void HttpTransportAcceptor::startOutgoingChannel(
    const std::string& connectionId,
    nx::network::http::HttpServerConnection* connection)
{
    m_connectionManager->modifyConnectionByIdSafe(
        connectionId,
        [connection](AbstractTransactionTransport* transportConnection)
        {
            // TODO: #ak Get rid of type casts here.

            auto transactionTransport =
                dynamic_cast<GenericTransport*>(transportConnection);
            if (transactionTransport)
            {
                static_cast<TransactionTransport&>(transactionTransport->commandPipeline())
                    .setOutgoingConnection(connection->takeSocket());
                transactionTransport->startOutgoingChannel();
            }
        });
}

void HttpTransportAcceptor::postTransactionToTransport(
    AbstractTransactionTransport* transportConnection,
    nx::network::http::Request request,
    bool* foundConnectionOfExpectedType)
{
    auto transactionTransport =
        dynamic_cast<GenericTransport*>(transportConnection);
    if (!transactionTransport)
    {
        *foundConnectionOfExpectedType = false;
        return;
    }
    *foundConnectionOfExpectedType = true;

    transactionTransport->post(
        [transactionTransport, request = std::move(request)]() mutable
        {
            static_cast<TransactionTransport&>(transactionTransport->commandPipeline())
                .receivedTransaction(
                    std::move(request.headers),
                    std::move(request.messageBody));
        });
}

} // namespace nx::data_sync_engine::transport
