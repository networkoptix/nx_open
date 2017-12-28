#include "connection_manager.h"

#include <nx/network/http/custom_headers.h>
#include <nx/network/http/empty_message_body_source.h>
#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/cpp14.h>

#include <nx_ec/data/api_peer_data.h>
#include <nx_ec/data/api_fwd.h>
#include <nx_ec/ec_proto_version.h>

#include <nx/cloud/cdb/api/ec2_request_paths.h>

#include "compatible_ec2_protocol_version.h"
#include "incoming_transaction_dispatcher.h"
#include "outgoing_transaction_dispatcher.h"
#include "p2p_sync_settings.h"
#include "transaction_transport.h"
#include "transaction_transport_header.h"
#include "websocket_transaction_transport.h"
#include "../access_control/authorization_manager.h"
#include "../stree/cdb_ns.h"
#include <nx/network/websocket/websocket_handshake.h>

namespace nx {
namespace cdb {
namespace ec2 {

ConnectionManager::ConnectionManager(
    const QnUuid& moduleGuid,
    const Settings& settings,
    TransactionLog* const transactionLog,
    IncomingTransactionDispatcher* const transactionDispatcher,
    OutgoingTransactionDispatcher* const outgoingTransactionDispatcher)
:
    m_settings(settings),
    m_transactionLog(transactionLog),
    m_transactionDispatcher(transactionDispatcher),
    m_outgoingTransactionDispatcher(outgoingTransactionDispatcher),
    m_localPeerData(
        moduleGuid,
        QnUuid::createUuid(),
        Qn::PT_CloudServer,
        Qn::UbjsonFormat),
    m_onNewTransactionSubscriptionId(nx::utils::kInvalidSubscriptionId)
{
    using namespace std::placeholders;

    m_transactionDispatcher->registerSpecialCommandHandler
        <::ec2::ApiCommand::tranSyncRequest, ::ec2::ApiSyncRequestData>(
            std::bind(&ConnectionManager::processSpecialTransaction<::ec2::ApiSyncRequestData>,
                        this, _1, _2, _3, _4));
    m_transactionDispatcher->registerSpecialCommandHandler
        <::ec2::ApiCommand::tranSyncResponse, ::ec2::QnTranStateResponse>(
            std::bind(&ConnectionManager::processSpecialTransaction<::ec2::QnTranStateResponse>,
                        this, _1, _2, _3, _4));
    m_transactionDispatcher->registerSpecialCommandHandler
        <::ec2::ApiCommand::tranSyncDone, ::ec2::ApiTranSyncDoneData>(
            std::bind(&ConnectionManager::processSpecialTransaction<::ec2::ApiTranSyncDoneData>,
                        this, _1, _2, _3, _4));

    m_outgoingTransactionDispatcher->onNewTransactionSubscription()->subscribe(
        std::bind(&ConnectionManager::dispatchTransaction, this, _1, _2),
        &m_onNewTransactionSubscriptionId);
}

ConnectionManager::~ConnectionManager()
{
    m_outgoingTransactionDispatcher->onNewTransactionSubscription()
        ->removeSubscription(m_onNewTransactionSubscriptionId);

    ConnectionDict localConnections;
    {
        QnMutexLocker lk(&m_mutex);
        std::swap(localConnections, m_connections);
    }

    for (auto it = localConnections.begin(); it != localConnections.end(); ++it)
        it->connection->pleaseStopSync();

    m_startedAsyncCallsCounter.wait();
}

void ConnectionManager::createTransactionConnection(
    nx::network::http::HttpServerConnection* const connection,
    nx::utils::stree::ResourceContainer authInfo,
    nx::network::http::Request request,
    nx::network::http::Response* const response,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    // GET /ec2/events/ConnectingStage2?guid=%7B8b939668-837d-4658-9d7a-e2cc6c12a38b%7D&
    //  runtime-guid=%7B0eac9718-4e37-4459-8799-c3023d4f7cb5%7D&system-identity-time=0&isClient
    // TODO: #ak

    std::string systemId;
    if (!authInfo.get(attr::authSystemId, &systemId))
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("Ignoring createTransactionConnection request without systemId from %1")
            .arg(connection->socket()->getForeignAddress()), cl_logDEBUG1);
        return completionHandler(nx::network::http::StatusCode::badRequest);
    }

    ConnectionRequestAttributes connectionRequestAttributes;
    if (!fetchDataFromConnectRequest(request, &connectionRequestAttributes))
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("Error parsing createTransactionConnection request from (%1.%2; %3)")
            .arg(connectionRequestAttributes.remotePeer.id).arg(systemId)
            .arg(connection->socket()->getForeignAddress()),
            cl_logDEBUG1);
        return completionHandler(nx::network::http::StatusCode::badRequest);
    }

    if (!isProtocolVersionCompatible(connectionRequestAttributes.remotePeerProtocolVersion))
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("Incompatible connection request from (%1.%2; %3). Requested protocol version %4")
            .arg(connectionRequestAttributes.remotePeer.id).arg(systemId)
            .arg(connection->socket()->getForeignAddress())
            .arg(connectionRequestAttributes.remotePeerProtocolVersion),
            cl_logDEBUG1);
        return completionHandler(nx::network::http::StatusCode::badRequest);
    }

    NX_LOGX(QnLog::EC2_TRAN_LOG,
        lm("Received createTransactionConnection request from (%1.%2; %3). connectionId %4")
        .arg(connectionRequestAttributes.remotePeer.id).arg(systemId).arg(connection->socket()->getForeignAddress())
        .arg(connectionRequestAttributes.connectionId),
        cl_logDEBUG1);

    // newTransport MUST be ready to accept connections before sending response.
    const nx::String systemIdLocal(systemId.c_str());
    auto newTransport = std::make_unique<TransactionTransport>(
        connection->getAioThread(),
        &m_connectionGuardSharedState,
        m_transactionLog,
        connectionRequestAttributes,
        systemIdLocal,
        m_localPeerData,
        connection->socket()->getForeignAddress(),
        request);

    ConnectionContext context{
        std::move(newTransport),
        connectionRequestAttributes.connectionId,
        {systemIdLocal, connectionRequestAttributes.remotePeer.id.toByteArray()} };

    if (!addNewConnection(std::move(context)))
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("Failed to add new transaction connection from (%1.%2; %3). connectionId %4")
            .arg(connectionRequestAttributes.remotePeer.id).arg(systemId)
            .arg(connection->socket()->getForeignAddress()).arg(connectionRequestAttributes.connectionId),
            cl_logDEBUG1);
        return completionHandler(nx::network::http::StatusCode::forbidden);
    }

    auto requestResult =
        prepareOkResponseToCreateTransactionConnection(
            connectionRequestAttributes,
            response);
    completionHandler(std::move(requestResult));
}

void ConnectionManager::createWebsocketTransactionConnection(
    nx::network::http::HttpServerConnection* const connection,
    nx::utils::stree::ResourceContainer authInfo,
    nx::network::http::Request request,
    nx::network::http::Response* const response,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    using namespace std::placeholders;
    using namespace nx::network;

    auto remotePeerInfo = ::ec2::deserializeFromRequest(request);

    std::string systemId;
    if (!authInfo.get(attr::authSystemId, &systemId))
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("Ignoring createWebsocketTransactionConnection request without systemId from peer %1")
            .arg(connection->socket()->getForeignAddress()), cl_logDEBUG1);
        return completionHandler(nx::network::http::StatusCode::badRequest);
    }
    const nx::String systemIdLocal(systemId.c_str());


    ::ec2::ApiPeerDataEx localPeer(m_localPeerData);
    ::ec2::serializeToResponse(response, localPeer, remotePeerInfo.dataFormat);

    auto error = websocket::validateRequest(request, response);
    if (error != websocket::Error::noError)
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("Can't upgrade request from peer %1 to webSocket. Error: %2")
            .arg(connection->socket()->getForeignAddress())
            .arg((int) error),
            cl_logDEBUG1);
        return completionHandler(nx::network::http::StatusCode::badRequest);
    }

    nx::network::http::RequestResult result(nx::network::http::StatusCode::switchingProtocols);
    result.connectionEvents.onResponseHasBeenSent =
        std::bind(&ConnectionManager::onHttpConnectionUpgraded, this,
            _1, std::move(remotePeerInfo), std::move(authInfo), std::move(systemIdLocal));
    completionHandler(std::move(result));
}

void ConnectionManager::pushTransaction(
    nx::network::http::HttpServerConnection* const connection,
    nx::utils::stree::ResourceContainer /*authInfo*/,
    nx::network::http::Request request,
    nx::network::http::Response* const /*response*/,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    const auto path = request.requestLine.url.path();
    if (!path.startsWith(api::kPushEc2TransactionPath + lit("/")) &&
        // TODO: #ak remove (together with the constant) after 3.0 release.
        !path.startsWith(api::kDeprecatedPushEc2TransactionPath + lit("/")))
    {
        return completionHandler(nx::network::http::StatusCode::notFound);
    }

    auto connectionIdIter = request.headers.find(Qn::EC2_CONNECTION_GUID_HEADER_NAME);
    if (connectionIdIter == request.headers.end())
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("Received %1 request from %2 without required header %3")
            .arg(request.requestLine.url.path()).arg(connection->socket()->getForeignAddress())
            .arg(Qn::EC2_CONNECTION_GUID_HEADER_NAME),
            cl_logDEBUG1);
        return completionHandler(nx::network::http::StatusCode::badRequest);
    }
    const auto connectionId = connectionIdIter->second;

    QnMutexLocker lk(&m_mutex);
    // Reporting received transaction(s) to the corresponding connection.
    const auto& connectionByIdIndex = m_connections.get<kConnectionByIdIndex>();
    auto connectionIter = connectionByIdIndex.find(connectionId);
    if (connectionIter == connectionByIdIndex.end())
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("Received %1 request from %2 for unknown connection %3")
            .arg(request.requestLine.url.path()).arg(connection->socket()->getForeignAddress())
            .arg(connectionId),
            cl_logDEBUG1);
        return completionHandler(nx::network::http::StatusCode::notFound);
    }

    auto transactionTransport =
        dynamic_cast<TransactionTransport*>(connectionIter->connection.get());
    if (!transactionTransport)
    {
        return completionHandler(nx::network::http::StatusCode::badRequest);
    }

    NX_LOGX(QnLog::EC2_TRAN_LOG,
        lm("Received %1 request from %2 for connection %3")
        .arg(request.requestLine.url.path()).arg(connection->socket()->getForeignAddress())
        .arg(connectionId),
        cl_logDEBUG2);

    transactionTransport->post(
        [transactionTransport,
            request = std::move(request)]() mutable
        {
            transactionTransport->receivedTransaction(
                std::move(request.headers),
                std::move(request.messageBody));
        });

    completionHandler(nx::network::http::StatusCode::ok);
}

void ConnectionManager::dispatchTransaction(
    const nx::String& systemId,
    std::shared_ptr<const SerializableAbstractTransaction> transactionSerializer)
{
    NX_LOGX(QnLog::EC2_TRAN_LOG,
        lm("systemId %1. Dispatching transaction %2")
        .arg(systemId).arg(transactionSerializer->transactionHeader()),
        cl_logDEBUG2);

    // Generating transport header.
    TransactionTransportHeader transportHeader;
    transportHeader.systemId = systemId;
    transportHeader.vmsTransportHeader.distance = 1;
    transportHeader.vmsTransportHeader.processedPeers.insert(m_localPeerData.id);
    // transportHeader.vmsTransportHeader.dstPeers.insert(); TODO:.

    QnMutexLocker lk(&m_mutex);
    const auto& connectionBySystemIdAndPeerIdIndex =
        m_connections.get<kConnectionByFullPeerNameIndex>();

    std::size_t connectionCount = 0;
    std::array<AbstractTransactionTransport*, 7> connectionsToSendTo;
    for (auto connectionIt = connectionBySystemIdAndPeerIdIndex
            .lower_bound(FullPeerName{systemId, nx::String()});
        connectionIt != connectionBySystemIdAndPeerIdIndex.end()
            && connectionIt->fullPeerName.systemId == systemId;
        ++connectionIt)
    {
        if (connectionIt->fullPeerName.peerId ==
                transactionSerializer->transactionHeader().peerID.toByteArray())
        {
            // Not sending transaction to peer which has generated it.
            continue;
        }

        connectionsToSendTo[connectionCount++] = connectionIt->connection.get();
        if (connectionCount < connectionsToSendTo.size())
            continue;

        for (auto& connection : connectionsToSendTo)
        {
            connection->sendTransaction(
                transportHeader,
                transactionSerializer);
        }
        connectionCount = 0;
    }

    if (connectionCount == 1)
    {
        // Minor optimization.
        connectionsToSendTo[0]->sendTransaction(
            std::move(transportHeader),
            std::move(transactionSerializer));
    }
    else
    {
        for (std::size_t i = 0; i < connectionCount; ++i)
        {
            connectionsToSendTo[i]->sendTransaction(
                transportHeader,
                transactionSerializer);
        }
    }
}

api::VmsConnectionDataList ConnectionManager::getVmsConnections() const
{
    QnMutexLocker lk(&m_mutex);
    api::VmsConnectionDataList result;
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it)
    {
        api::VmsConnectionData connectionData;
        connectionData.systemId = it->fullPeerName.systemId.toStdString();
        connectionData.mediaserverEndpoint =
            it->connection->remoteSocketAddr().toString().toStdString();
        result.connections.push_back(std::move(connectionData));
    }

    return result;
}

std::size_t ConnectionManager::getVmsConnectionCount() const
{
    QnMutexLocker lk(&m_mutex);
    return m_connections.size();
}

bool ConnectionManager::isSystemConnected(const std::string& systemId) const
{
    QnMutexLocker lk(&m_mutex);

    const auto& connectionBySystemIdAndPeerIdIndex =
        m_connections.get<kConnectionByFullPeerNameIndex>();
    const auto systemIter = connectionBySystemIdAndPeerIdIndex.lower_bound(
        FullPeerName{nx::String(systemId.c_str()), nx::String()});

    return
        systemIter != connectionBySystemIdAndPeerIdIndex.end() &&
        systemIter->fullPeerName.systemId == systemId;
}

unsigned int ConnectionManager::getConnectionCountBySystemId(
    const nx::String& systemId) const
{
    QnMutexLocker lock(&m_mutex);
    return getConnectionCountBySystemId(lock, systemId);
}

void ConnectionManager::closeConnectionsToSystem(
    const nx::String& systemId,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    auto allConnectionsRemovedGuard = makeSharedGuard(std::move(completionHandler));

    QnMutexLocker lock(&m_mutex);

    auto& connectionBySystemIdAndPeerIdIndex =
        m_connections.get<kConnectionByFullPeerNameIndex>();
    auto it = connectionBySystemIdAndPeerIdIndex.lower_bound(
        FullPeerName{systemId, nx::String()});
    while (it != connectionBySystemIdAndPeerIdIndex.end() &&
           it->fullPeerName.systemId == systemId)
    {
        removeConnectionByIter(
            lock,
            connectionBySystemIdAndPeerIdIndex,
            it++,
            [allConnectionsRemovedGuard](){});
    }
}

ConnectionManager::SystemStatusChangedSubscription&
    ConnectionManager::systemStatusChangedSubscription()
{
    return m_systemStatusChangedSubscription;
}

bool ConnectionManager::addNewConnection(ConnectionContext context)
{
    using namespace std::placeholders;

    QnMutexLocker lock(&m_mutex);

    const auto systemWasOffline = getConnectionCountBySystemId(
        lock, context.fullPeerName.systemId) == 0;

    removeExistingConnection<
        kConnectionByFullPeerNameIndex,
        decltype(context.fullPeerName)>(lock, context.fullPeerName);

    if (!isOneMoreConnectionFromSystemAllowed(lock, context))
        return false;

    context.connection->setOnConnectionClosed(
        std::bind(&ConnectionManager::removeConnection, this, context.connectionId));
    context.connection->setOnGotTransaction(
        std::bind(
            &ConnectionManager::onGotTransaction, this,
            context.connection->connectionGuid().toByteArray(), _1, _2, _3));

    NX_LOGX(QnLog::EC2_TRAN_LOG,
        lm("Adding new transaction connection %1 from %2")
            .arg(context.connectionId)
            .arg(context.connection->commonTransportHeaderOfRemoteTransaction()),
        cl_logDEBUG1);

    const auto systemId = context.fullPeerName.systemId.toStdString();

    if (!m_connections.insert(std::move(context)).second)
    {
        NX_ASSERT(false);
        return false;
    }

    if (systemWasOffline)
    {
        lock.unlock();
        m_systemStatusChangedSubscription.notify(systemId, api::SystemHealth::online);
    }

    return true;
}

bool ConnectionManager::isOneMoreConnectionFromSystemAllowed(
    const QnMutexLockerBase& lk,
    const ConnectionContext& context) const
{
    const auto existingConnectionCount =
        getConnectionCountBySystemId(lk, context.fullPeerName.systemId);

    if (existingConnectionCount >= m_settings.maxConcurrentConnectionsFromSystem)
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("Refusing connection %1 from %2 since "
                "there are already %3 connections from that system")
            .arg(context.connectionId)
            .arg(context.connection->commonTransportHeaderOfRemoteTransaction())
            .arg(existingConnectionCount),
            cl_logDEBUG2);
        return false;
    }

    return true;
}

unsigned int ConnectionManager::getConnectionCountBySystemId(
    const QnMutexLockerBase& /*lk*/,
    const nx::String& systemId) const
{
    const auto& connectionByFullPeerName =
        m_connections.get<kConnectionByFullPeerNameIndex>();

    auto it = connectionByFullPeerName.lower_bound(
        FullPeerName{systemId, nx::String()});
    unsigned int activeConnections = 0;
    for (; it != connectionByFullPeerName.end(); ++it)
    {
         if (it->fullPeerName.systemId != systemId)
             break;
         ++activeConnections;
    }

    return activeConnections;
}

template<int connectionIndexNumber, typename ConnectionKeyType>
void ConnectionManager::removeExistingConnection(
    const QnMutexLockerBase& lock,
    ConnectionKeyType connectionKey)
{
    auto& connectionIndex = m_connections.get<connectionIndexNumber>();
    auto existingConnectionIter = connectionIndex.find(connectionKey);
    if (existingConnectionIter == connectionIndex.end())
        return;

    removeConnectionByIter(
        lock,
        connectionIndex,
        existingConnectionIter,
        [](){});
}

template<typename ConnectionIndex, typename Iterator, typename CompletionHandler>
void ConnectionManager::removeConnectionByIter(
    const QnMutexLockerBase& /*lock*/,
    ConnectionIndex& connectionIndex,
    Iterator connectionIterator,
    CompletionHandler completionHandler)
{
    std::unique_ptr<AbstractTransactionTransport> existingConnection;
    connectionIndex.modify(
        connectionIterator,
        [&existingConnection](ConnectionContext& data)
        {
            existingConnection = std::move(data.connection);
        });
    connectionIndex.erase(connectionIterator);

    AbstractTransactionTransport* existingConnectionPtr = existingConnection.get();

    NX_LOGX(QnLog::EC2_TRAN_LOG,
        lm("Removing transaction connection %1 from %2")
            .arg(existingConnectionPtr->connectionGuid())
            .arg(existingConnectionPtr->commonTransportHeaderOfRemoteTransaction()),
        cl_logDEBUG1);

    // ::ec2::TransactionTransportBase does not support its removal
    //  in signal handler, so have to remove it delayed.
    // TODO: #ak have to ensure somehow that no events from
    //  connectionContext.connection are delivered.

    existingConnectionPtr->post(
        [this, existingConnection = std::move(existingConnection),
            locker = m_startedAsyncCallsCounter.getScopedIncrement(),
            completionHandler = std::move(completionHandler)]() mutable
        {
            sendSystemOfflineNotificationIfNeeded(
                existingConnection->commonTransportHeaderOfRemoteTransaction().systemId);
            existingConnection.reset();
            completionHandler();
        });
}

void ConnectionManager::sendSystemOfflineNotificationIfNeeded(
    const nx::String systemId)
{
    if (getConnectionCountBySystemId(systemId) > 0)
        return;

    m_systemStatusChangedSubscription.notify(
        systemId.toStdString(), api::SystemHealth::offline);
}

void ConnectionManager::removeConnection(const nx::String& connectionId)
{
    QnMutexLocker lock(&m_mutex);
    removeExistingConnection<kConnectionByIdIndex>(lock, connectionId);
}

void ConnectionManager::onGotTransaction(
    const nx::String& connectionId,
    Qn::SerializationFormat tranFormat,
    QByteArray serializedTransaction,
    TransactionTransportHeader transportHeader)
{
    m_transactionDispatcher->dispatchTransaction(
        std::move(transportHeader),
        tranFormat,
        std::move(serializedTransaction),
        [this, locker = m_startedAsyncCallsCounter.getScopedIncrement(), connectionId](
            api::ResultCode resultCode)
        {
            onTransactionDone(connectionId, resultCode);
        });
}

void ConnectionManager::onTransactionDone(
    const nx::String& connectionId,
    api::ResultCode resultCode)
{
    if (resultCode != api::ResultCode::ok)
    {
        NX_LOGX(
            QnLog::EC2_TRAN_LOG,
            lm("Closing connection %1 due to failed transaction (result code %2)")
                .arg(connectionId).arg(api::toString(resultCode)),
            cl_logDEBUG1);

        // Closing connection in case of failure.
        QnMutexLocker lock(&m_mutex);
        removeExistingConnection<kConnectionByIdIndex, nx::String>(lock, connectionId);
    }
}

bool ConnectionManager::fetchDataFromConnectRequest(
    const nx::network::http::Request& request,
    ConnectionRequestAttributes* connectionRequestAttributes)
{
    auto connectionIdIter = request.headers.find(Qn::EC2_CONNECTION_GUID_HEADER_NAME);
    if (connectionIdIter == request.headers.end())
        return false;
    connectionRequestAttributes->connectionId = connectionIdIter->second;

    if (!nx::network::http::readHeader(
            request.headers,
            Qn::EC2_PROTO_VERSION_HEADER_NAME,
            &connectionRequestAttributes->remotePeerProtocolVersion))
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("Missing required header %1").arg(Qn::EC2_PROTO_VERSION_HEADER_NAME),
            cl_logDEBUG2);
        return false;
    }

    QUrlQuery query = QUrlQuery(request.requestLine.url.query());
    const bool isClient = query.hasQueryItem("isClient");
    const bool isMobileClient = query.hasQueryItem("isMobile");
    QnUuid remoteGuid = QnUuid::fromStringSafe(query.queryItemValue("guid"));
    QnUuid remoteRuntimeGuid = QnUuid::fromStringSafe(query.queryItemValue("runtime-guid"));
    if (remoteGuid.isNull())
        remoteGuid = QnUuid::createUuid();

    const Qn::PeerType peerType =
    isMobileClient ? Qn::PT_MobileClient :
        isClient ? Qn::PT_DesktopClient :
        Qn::PT_Server;

    Qn::SerializationFormat dataFormat = Qn::UbjsonFormat;
    if (query.hasQueryItem("format"))
        if (!QnLexical::deserialize(query.queryItemValue("format"), &dataFormat))
        {
            NX_LOGX(QnLog::EC2_TRAN_LOG,
                lm("Invalid value of \"format\" field: %1")
                .arg(query.queryItemValue("format")), cl_logDEBUG1);
            return false;
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
        ::ec2::ApiPeerData(remoteGuid, remoteRuntimeGuid, peerType, dataFormat);
    return true;
}

template<typename TransactionDataType>
void ConnectionManager::processSpecialTransaction(
    const nx::String& /*systemId*/,
    const TransactionTransportHeader& transportHeader,
    ::ec2::QnTransaction<TransactionDataType> data,
    TransactionProcessedHandler handler)
{
    QnMutexLocker lk(&m_mutex);
    const auto& connectionByIdIndex = m_connections.get<kConnectionByIdIndex>();
    auto connectionIter = connectionByIdIndex.find(transportHeader.connectionId);
    if (connectionIter == connectionByIdIndex.end())
        return; //< This can happen since connection destruction happens with some
                //  delay after connection has been removed from m_connections.
    lk.unlock();

    // TODO: #ak Get rid of dynamic_cast.
    auto transactionTransport =
        dynamic_cast<TransactionTransport*>(connectionIter->connection.get());
    if (transactionTransport)
    {
        transactionTransport->processSpecialTransaction(
            transportHeader,
            std::move(data),
            std::move(handler));
    }
}

nx::network::http::RequestResult ConnectionManager::prepareOkResponseToCreateTransactionConnection(
    const ConnectionRequestAttributes& connectionRequestAttributes,
    nx::network::http::Response* const response)
{
    response->headers.emplace("Content-Type", ::ec2::QnTransactionTransportBase::TUNNEL_CONTENT_TYPE);
    response->headers.emplace("Content-Encoding", connectionRequestAttributes.contentEncoding);
    response->headers.emplace(Qn::EC2_GUID_HEADER_NAME, m_localPeerData.id.toByteArray());
    response->headers.emplace(
        Qn::EC2_RUNTIME_GUID_HEADER_NAME,
        m_localPeerData.instanceId.toByteArray());

    NX_ASSERT(isProtocolVersionCompatible(connectionRequestAttributes.remotePeerProtocolVersion));
    response->headers.emplace(
        Qn::EC2_PROTO_VERSION_HEADER_NAME,
        nx::String::number(connectionRequestAttributes.remotePeerProtocolVersion));
    response->headers.emplace("X-Nx-Cloud", "true");
    response->headers.emplace(Qn::EC2_BASE64_ENCODING_REQUIRED_HEADER_NAME, "true");

    nx::network::http::RequestResult requestResult(nx::network::http::StatusCode::ok);

    requestResult.connectionEvents.onResponseHasBeenSent =
        [this, connectionId = connectionRequestAttributes.connectionId](
            nx::network::http::HttpServerConnection* connection)
        {
            QnMutexLocker lk(&m_mutex);

            const auto& connectionByIdIndex = m_connections.get<kConnectionByIdIndex>();
            auto connectionIter = connectionByIdIndex.find(connectionId);
            NX_ASSERT(connectionIter != connectionByIdIndex.end());

            auto transactionTransport =
                dynamic_cast<TransactionTransport*>(connectionIter->connection.get());
            if (transactionTransport)
            {
                transactionTransport->setOutgoingConnection(
                    QSharedPointer<network::AbstractCommunicatingSocket>(\
                        connection->takeSocket().release()));
                transactionTransport->startOutgoingChannel();
            }
        };
    requestResult.dataSource =
        std::make_unique<nx::network::http::EmptyMessageBodySource>(
            ::ec2::QnTransactionTransportBase::TUNNEL_CONTENT_TYPE,
            boost::none);

    return requestResult;
}

void ConnectionManager::onHttpConnectionUpgraded(
    nx::network::http::HttpServerConnection* connection,
    ::ec2::ApiPeerDataEx remotePeerInfo,
    nx::utils::stree::ResourceContainer /*authInfo*/,
    const nx::String systemId)
{
    const auto remoteAddress = connection->socket()->getForeignAddress();
    auto webSocket = std::make_unique<network::websocket::WebSocket>(
        connection->takeSocket());

    auto connectionId = QnUuid::createUuid();

    ::ec2::ApiPeerDataEx localPeerData(m_localPeerData);
    auto transactionTransport = std::make_unique<WebSocketTransactionTransport>(
        connection->getAioThread(),
        m_transactionLog,
        systemId,
        connectionId,
        std::move(webSocket),
        localPeerData,
        remotePeerInfo);

    ConnectionContext context{
        std::move(transactionTransport),
        connectionId.toSimpleByteArray(),
        { systemId, remotePeerInfo.id.toByteArray() } };

    if (!addNewConnection(std::move(context)))
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("Failed to add new websocket transaction connection from (%1.%2; %3). connectionId %4")
            .arg(remotePeerInfo.id).arg(systemId).arg(remoteAddress).arg(connectionId),
            cl_logDEBUG1);
    }
}

} // namespace ec2
} // namespace cdb
} // namespace nx
