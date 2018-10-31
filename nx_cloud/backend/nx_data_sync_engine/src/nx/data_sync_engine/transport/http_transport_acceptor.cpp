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
    nx::network::http::RequestContext requestContext,
    const std::string& systemId,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    // GET /ec2/events/ConnectingStage2?guid=%7B8b939668-837d-4658-9d7a-e2cc6c12a38b%7D&
    //  runtime-guid=%7B0eac9718-4e37-4459-8799-c3023d4f7cb5%7D&system-identity-time=0&isClient
    // TODO: #ak

    auto httpConnection = requestContext.connection;

    if (systemId.empty())
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
            lm("Ignoring createTransactionConnection request without systemId from %1")
            .args(httpConnection->socket()->getForeignAddress()));
        return completionHandler(nx::network::http::StatusCode::badRequest);
    }

    ConnectionRequestAttributes connectionRequestAttributes;
    if (!fetchDataFromConnectRequest(requestContext.request, &connectionRequestAttributes))
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
            lm("Error parsing createTransactionConnection request from (%1.%2; %3)")
            .args(connectionRequestAttributes.remotePeer.id, systemId,
                httpConnection->socket()->getForeignAddress()));
        return completionHandler(nx::network::http::StatusCode::badRequest);
    }

    if (!m_protocolVersionRange.isCompatible(
            connectionRequestAttributes.remotePeerProtocolVersion))
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
            lm("Incompatible connection request from (%1.%2; %3). Requested protocol version %4")
            .args(connectionRequestAttributes.remotePeer.id, systemId,
                httpConnection->socket()->getForeignAddress(),
                connectionRequestAttributes.remotePeerProtocolVersion));
        return completionHandler(nx::network::http::StatusCode::badRequest);
    }

    NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
        lm("Received createTransactionConnection request from (%1.%2; %3). connectionId %4")
        .args(connectionRequestAttributes.remotePeer.id, systemId,
            httpConnection->socket()->getForeignAddress(),
            connectionRequestAttributes.connectionId));

    TransportConnectionContext transportConnectionContext;
    transportConnectionContext.connectionSequence = ++m_lastConnectionSequence;
    
    auto commandPipeline = std::make_unique<TransactionTransport>(
        m_protocolVersionRange,
        httpConnection->getAioThread(),
        m_connectionGuardSharedState,
        connectionRequestAttributes,
        systemId,
        m_localPeerData,
        httpConnection->socket()->getForeignAddress(),
        requestContext.request);
    transportConnectionContext.commandPipeline = commandPipeline.get();

    auto newTransport = std::make_unique<GenericTransport>(
        m_protocolVersionRange,
        m_transactionLog,
        m_outgoingCommandFilter,
        systemId,
        connectionRequestAttributes,
        m_localPeerData,
        std::move(commandPipeline));

    transportConnectionContext.transport = newTransport.get();
    newTransport->connectionClosedSubscription().subscribe(
        [this, seq = transportConnectionContext.connectionSequence](
            auto... /*args*/)
        {
            forgetConnection(seq);
        },
        &transportConnectionContext.connectionClosedSubscriptionId);

    ConnectionManager::ConnectionContext context{
        std::move(newTransport),
        connectionRequestAttributes.connectionId,
        {systemId, connectionRequestAttributes.remotePeer.id.toByteArray().toStdString()},
        network::http::getHeaderValue(requestContext.request.headers, "User-Agent").toStdString()};

    if (!m_connectionManager->addNewConnection(std::move(context)))
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
            lm("Failed to add new transaction connection from (%1.%2; %3). connectionId %4")
            .args(connectionRequestAttributes.remotePeer.id, systemId,
                httpConnection->socket()->getForeignAddress(),
                connectionRequestAttributes.connectionId));
        return completionHandler(nx::network::http::StatusCode::forbidden);
    }

    auto requestResult =
        prepareOkResponseToCreateTransactionConnection(
            connectionRequestAttributes,
            requestContext.response);

    requestResult.connectionEvents.onResponseHasBeenSent =
        std::bind(&HttpTransportAcceptor::startOutgoingChannel, this,
            transportConnectionContext.connectionSequence,
            std::placeholders::_1);

    {
        QnMutexLocker lock(&m_mutex);
        m_connections.emplace(
            transportConnectionContext.connectionSequence,
            transportConnectionContext);
    }

    completionHandler(std::move(requestResult));
}

void HttpTransportAcceptor::pushTransaction(
    nx::network::http::RequestContext requestContext,
    const std::string& /*systemId*/,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    const auto& request = requestContext.request;
    auto connection = requestContext.connection;

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

    requestResult.dataSource =
        std::make_unique<nx::network::http::EmptyMessageBodySource>(
            ::ec2::QnTransactionTransportBase::TUNNEL_CONTENT_TYPE,
            boost::none);

    return requestResult;
}

void HttpTransportAcceptor::forgetConnection(
    std::int64_t connectionSequence)
{
    QnMutexLocker lock(&m_mutex);
    m_connections.erase(connectionSequence);
}

void HttpTransportAcceptor::startOutgoingChannel(
    std::int64_t connectionSequence,
    nx::network::http::HttpServerConnection* connection)
{
    QnMutexLocker lock(&m_mutex);

    auto connectionIter = m_connections.find(connectionSequence);
    if (connectionIter == m_connections.end())
        return;
    
    TransportConnectionContext& transportContext = connectionIter->second;

    transportContext.commandPipeline->setOutgoingConnection(
        connection->takeSocket());
    transportContext.transport->start();

    transportContext.transport->connectionClosedSubscription()
        .removeSubscription(transportContext.connectionClosedSubscriptionId);
    m_connections.erase(connectionIter);
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
