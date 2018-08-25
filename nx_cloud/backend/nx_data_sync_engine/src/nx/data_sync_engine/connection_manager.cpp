#include "connection_manager.h"

#include <nx/fusion/serialization/lexical.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/empty_message_body_source.h>
#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx/network/websocket/websocket_handshake.h>
#include <nx/p2p/p2p_serialization.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/cpp14.h>
#include <nx/vms/api/types/connection_types.h>

#include <nx_ec/data/api_fwd.h>

#include "compatible_ec2_protocol_version.h"
#include "incoming_transaction_dispatcher.h"
#include "outgoing_transaction_dispatcher.h"
#include "p2p_sync_settings.h"
#include "transaction_transport.h"
#include "transaction_transport_header.h"
#include "websocket_transaction_transport.h"

namespace nx {
namespace data_sync_engine {

ConnectionManager::ConnectionManager(
    const QnUuid& moduleGuid,
    const Settings& settings,
    const ProtocolVersionRange& protocolVersionRange,
    TransactionLog* const transactionLog,
    IncomingTransactionDispatcher* const transactionDispatcher,
    OutgoingTransactionDispatcher* const outgoingTransactionDispatcher)
:
    m_settings(settings),
    m_protocolVersionRange(protocolVersionRange),
    m_transactionLog(transactionLog),
    m_transactionDispatcher(transactionDispatcher),
    m_outgoingTransactionDispatcher(outgoingTransactionDispatcher),
    m_localPeerData(
        moduleGuid,
        QnUuid::createUuid(),
        vms::api::PeerType::cloudServer,
        Qn::UbjsonFormat),
    m_onNewTransactionSubscriptionId(nx::utils::kInvalidSubscriptionId)
{
    using namespace std::placeholders;

    m_transactionDispatcher->registerSpecialCommandHandler
        <::ec2::ApiCommand::tranSyncRequest, vms::api::SyncRequestData>(
            std::bind(&ConnectionManager::processSpecialTransaction<vms::api::SyncRequestData>,
                        this, _1, _2, _3, _4));
    m_transactionDispatcher->registerSpecialCommandHandler
        <::ec2::ApiCommand::tranSyncResponse, vms::api::TranStateResponse>(
            std::bind(&ConnectionManager::processSpecialTransaction<vms::api::TranStateResponse>,
                        this, _1, _2, _3, _4));
    m_transactionDispatcher->registerSpecialCommandHandler
        <::ec2::ApiCommand::tranSyncDone, vms::api::TranSyncDoneData>(
            std::bind(&ConnectionManager::processSpecialTransaction<vms::api::TranSyncDoneData>,
                        this, _1, _2, _3, _4));

    m_outgoingTransactionDispatcher->onNewTransactionSubscription().subscribe(
        std::bind(&ConnectionManager::dispatchTransaction, this, _1, _2),
        &m_onNewTransactionSubscriptionId);
}

ConnectionManager::~ConnectionManager()
{
    m_outgoingTransactionDispatcher->onNewTransactionSubscription()
        .removeSubscription(m_onNewTransactionSubscriptionId);

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
            .args(connectionRequestAttributes.remotePeer.id, systemId,
                connection->socket()->getForeignAddress()),
            cl_logDEBUG1);
        return completionHandler(nx::network::http::StatusCode::badRequest);
    }

    if (!m_protocolVersionRange.isCompatible(
            connectionRequestAttributes.remotePeerProtocolVersion))
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("Incompatible connection request from (%1.%2; %3). Requested protocol version %4")
            .args(connectionRequestAttributes.remotePeer.id, systemId,
                connection->socket()->getForeignAddress(),
                connectionRequestAttributes.remotePeerProtocolVersion),
            cl_logDEBUG1);
        return completionHandler(nx::network::http::StatusCode::badRequest);
    }

    NX_LOGX(QnLog::EC2_TRAN_LOG,
        lm("Received createTransactionConnection request from (%1.%2; %3). connectionId %4")
            .args(connectionRequestAttributes.remotePeer.id, systemId,
                connection->socket()->getForeignAddress(),
                connectionRequestAttributes.connectionId),
        cl_logDEBUG1);

    // newTransport MUST be ready to accept connections before sending response.
    const nx::String systemIdLocal(systemId.c_str());
    auto newTransport = std::make_unique<TransactionTransport>(
        m_protocolVersionRange,
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
        {systemIdLocal, connectionRequestAttributes.remotePeer.id.toByteArray()},
        network::http::getHeaderValue(request.headers, "User-Agent").toStdString()};

    vms::api::PeerDataEx remotePeer;
    remotePeer.assign(connectionRequestAttributes.remotePeer);
    remotePeer.protoVersion = connectionRequestAttributes.remotePeerProtocolVersion;
    remotePeer.cloudHost = nx::network::SocketGlobals::cloud().cloudHost();

    if (!addNewConnection(std::move(context), remotePeer))
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("Failed to add new transaction connection from (%1.%2; %3). connectionId %4")
            .args(connectionRequestAttributes.remotePeer.id, systemId,
                connection->socket()->getForeignAddress(),
                connectionRequestAttributes.connectionId),
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
    const std::string& systemId,
    nx::network::http::Request request,
    nx::network::http::Response* const response,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    using namespace std::placeholders;
    using namespace nx::network;

    auto remotePeerInfo = p2p::deserializePeerData(request);

    if (systemId.empty())
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("Ignoring createWebsocketTransactionConnection request without systemId from peer %1")
            .arg(connection->socket()->getForeignAddress()), cl_logDEBUG1);
        return completionHandler(nx::network::http::StatusCode::badRequest);
    }

    if (!m_protocolVersionRange.isCompatible(remotePeerInfo.protoVersion))
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("Incompatible connection request from (%1.%2; %3). Requested protocol version %4")
            .args(remotePeerInfo.id, systemId, connection->socket()->getForeignAddress(),
                remotePeerInfo.protoVersion),
            cl_logDEBUG1);
        return completionHandler(nx::network::http::StatusCode::badRequest);
    }

    vms::api::PeerDataEx localPeer;
    localPeer.assign(m_localPeerData);
    localPeer.cloudHost = nx::network::SocketGlobals::cloud().cloudHost();
    localPeer.protoVersion = remotePeerInfo.protoVersion;
    p2p::serializePeerData(*response, localPeer, remotePeerInfo.dataFormat);

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

void ConnectionManager::pushTransaction(
    nx::network::http::HttpServerConnection* const connection,
    const std::string& /*systemId*/,
    nx::network::http::Request request,
    nx::network::http::Response* const /*response*/,
    nx::network::http::RequestProcessedHandler completionHandler)
{
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
    TransactionTransportHeader transportHeader(m_protocolVersionRange.currentVersion());
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

std::vector<SystemConnectionInfo> ConnectionManager::getConnections() const
{
    QnMutexLocker lk(&m_mutex);

    std::vector<SystemConnectionInfo> result;
    result.reserve(m_connections.size());
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it)
    {
        result.push_back({
            it->fullPeerName.systemId.toStdString(),
            it->connection->remoteSocketAddr(),
            it->userAgent});
    }

    return result;
}

std::size_t ConnectionManager::getConnectionCount() const
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
    NX_LOGX(QnLog::EC2_TRAN_LOG,
        lm("Closing all connections to system %1").args(systemId), cl_logDEBUG1);

    auto allConnectionsRemovedGuard =
        nx::utils::makeSharedGuard(std::move(completionHandler));

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

bool ConnectionManager::addNewConnection(
    ConnectionContext context,
    const vms::api::PeerDataEx& remotePeerInfo)
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
        m_systemStatusChangedSubscription.notify(
            systemId,
            { true /*online*/, remotePeerInfo.protoVersion });
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
            existingConnection.swap(data.connection);
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
        systemId.toStdString(), { false /*offline*/, 0 });
}

void ConnectionManager::removeConnection(const nx::String& connectionId)
{
    NX_LOGX(QnLog::EC2_TRAN_LOG, lm("Removing connection %1").args(connectionId), cl_logDEBUG2);

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
            ResultCode resultCode)
        {
            onTransactionDone(connectionId, resultCode);
        });
}

void ConnectionManager::onTransactionDone(
    const nx::String& connectionId,
    ResultCode resultCode)
{
    if (resultCode != ResultCode::ok)
    {
        NX_LOGX(
            QnLog::EC2_TRAN_LOG,
            lm("Closing connection %1 due to failed transaction (result code %2)")
                .arg(connectionId).arg(toString(resultCode)),
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

    const vms::api::PeerType peerType = isMobileClient
        ? vms::api::PeerType::mobileClient
        : (isClient ? vms::api::PeerType::desktopClient : vms::api::PeerType::server);

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
        vms::api::PeerData(remoteGuid, remoteRuntimeGuid, peerType, dataFormat);

    return true;
}

template<typename TransactionDataType>
void ConnectionManager::processSpecialTransaction(
    const nx::String& /*systemId*/,
    const TransactionTransportHeader& transportHeader,
    Command<TransactionDataType> data,
    TransactionProcessedHandler handler)
{
    QnMutexLocker lk(&m_mutex);

    const auto& connectionByIdIndex = m_connections.get<kConnectionByIdIndex>();
    auto connectionIter = connectionByIdIndex.find(transportHeader.connectionId);
    if (connectionIter == connectionByIdIndex.end())
        return; //< This can happen since connection destruction happens with some
                //  delay after connection has been removed from m_connections.

    // TODO: #ak Get rid of dynamic_cast.
    auto transactionTransport =
        dynamic_cast<TransactionTransport*>(connectionIter->connection.get());

    // NOTE: transactionTransport variable can safely be used within its own AIO thread.
    NX_ASSERT(transactionTransport->isInSelfAioThread());

    lk.unlock();

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

    NX_ASSERT(m_protocolVersionRange.isCompatible(
        connectionRequestAttributes.remotePeerProtocolVersion));
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
            if (connectionIter == connectionByIdIndex.end())
                return; //< Connection can be removed at any moment while accepting another connection.

            auto transactionTransport =
                dynamic_cast<TransactionTransport*>(connectionIter->connection.get());
            if (transactionTransport)
            {
                transactionTransport->setOutgoingConnection(connection->takeSocket());
                transactionTransport->startOutgoingChannel();
            }
        };
    requestResult.dataSource =
        std::make_unique<nx::network::http::EmptyMessageBodySource>(
            ::ec2::QnTransactionTransportBase::TUNNEL_CONTENT_TYPE,
            boost::none);

    return requestResult;
}

void ConnectionManager::addWebSocketTransactionTransport(
    std::unique_ptr<network::AbstractStreamSocket> connection,
    vms::api::PeerDataEx localPeerInfo,
    vms::api::PeerDataEx remotePeerInfo,
    const std::string& systemId,
    const std::string& userAgent)
{
    const auto remoteAddress = connection->getForeignAddress();
    auto webSocket = std::make_unique<network::websocket::WebSocket>(
        std::move(connection));

    auto connectionId = QnUuid::createUuid();

    auto transactionTransport = std::make_unique<WebSocketTransactionTransport>(
        m_protocolVersionRange,
        m_transactionLog,
        systemId.c_str(),
        connectionId,
        std::move(webSocket),
        localPeerInfo,
        remotePeerInfo);

    ConnectionContext context{
        std::move(transactionTransport),
        connectionId.toSimpleByteArray(),
        {systemId.c_str(), remotePeerInfo.id.toByteArray()},
        userAgent};

    if (!addNewConnection(std::move(context), remotePeerInfo))
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("Failed to add new websocket transaction connection from (%1.%2; %3). connectionId %4")
            .arg(remotePeerInfo.id).arg(systemId).arg(remoteAddress).arg(connectionId),
            cl_logDEBUG1);
    }
}

} // namespace data_sync_engine
} // namespace nx
