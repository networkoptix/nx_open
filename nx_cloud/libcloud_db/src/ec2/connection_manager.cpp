#include "connection_manager.h"

#include <nx/network/http/empty_message_body_source.h>
#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx_ec/data/api_peer_data.h>
#include <nx_ec/data/api_fwd.h>
#include <nx_ec/ec_proto_version.h>

#include <cloud_db_client/src/cdb_request_path.h>
#include <http/custom_headers.h>
#include <utils/common/guard.h>

#include "access_control/authorization_manager.h"
#include "stree/cdb_ns.h"
#include "incoming_transaction_dispatcher.h"
#include "outgoing_transaction_dispatcher.h"
#include "transaction_transport.h"
#include "transaction_transport_header.h"

namespace nx {
namespace cdb {
namespace ec2 {

ConnectionManager::ConnectionManager(
    const QnUuid& moduleGuid,
    const conf::Settings& settings,
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

    for (auto& connectionContext: localConnections)
        connectionContext.connection->pleaseStopSync();

    m_startedAsyncCallsCounter.wait();
}

void ConnectionManager::createTransactionConnection(
    nx_http::HttpServerConnection* const connection,
    stree::ResourceContainer authInfo,
    nx_http::Request request,
    nx_http::Response* const response,
    nx_http::HttpRequestProcessedHandler completionHandler)
{
    // GET ec2/events/ConnectingStage2?guid=%7B8b939668-837d-4658-9d7a-e2cc6c12a38b%7D&
    //  runtime-guid=%7B0eac9718-4e37-4459-8799-c3023d4f7cb5%7D&system-identity-time=0&isClient
    // TODO: #ak

    std::string systemId;
    if (!authInfo.get(attr::authSystemID, &systemId))
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("Ignoring createTransactionConnection request without systemId from %1")
            .str(connection->socket()->getForeignAddress()), cl_logDEBUG1);
        return completionHandler(nx_http::StatusCode::badRequest);
    }

    nx::String connectionId;
    ::ec2::ApiPeerData remotePeer;
    nx::String contentEncoding;
    if (!fetchDataFromConnectRequest(request, &connectionId, &remotePeer, &contentEncoding))
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG, 
            lm("Error parsing createTransactionConnection request from (%1.%2; %3)")
            .arg(remotePeer.id).arg(systemId).str(connection->socket()->getForeignAddress()),
            cl_logDEBUG1);
        return completionHandler(nx_http::StatusCode::badRequest);
    }

    NX_LOGX(QnLog::EC2_TRAN_LOG, 
        lm("Received createTransactionConnection request from (%1.%2; %3). connectionId %4")
        .arg(remotePeer.id).arg(systemId).str(connection->socket()->getForeignAddress())
        .arg(connectionId),
        cl_logDEBUG1);

    // newTransport MUST be ready to accept connections before sending response.
    const nx::String systemIdLocal(systemId.c_str());
    auto newTransport = std::make_unique<TransactionTransport>(
        connection->getAioThread(),
        &m_connectionGuardSharedState,
        m_transactionLog,
        systemIdLocal,
        connectionId,
        m_localPeerData,
        remotePeer,
        connection->socket()->getForeignAddress(),
        request,
        contentEncoding);

    ConnectionContext context{
        std::move(newTransport),
        connectionId,
        std::make_pair(systemIdLocal, remotePeer.id.toByteArray()) };

    if (!addNewConnection(std::move(context)))
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("Failed to add new transaction connection from (%1.%2; %3). connectionId %4")
            .arg(remotePeer.id).arg(systemId).str(connection->socket()->getForeignAddress())
            .arg(connectionId),
            cl_logDEBUG1);
        return completionHandler(nx_http::StatusCode::forbidden);
    }

    auto requestResult = 
        prepareOkResponseToCreateTransactionConnection(
            connectionId,
            contentEncoding,
            response);
    completionHandler(std::move(requestResult));
}

void ConnectionManager::pushTransaction(
    nx_http::HttpServerConnection* const connection,
    stree::ResourceContainer /*authInfo*/,
    nx_http::Request request,
    nx_http::Response* const /*response*/,
    nx_http::HttpRequestProcessedHandler completionHandler)
{
    if (!request.requestLine.url.path().startsWith(kPushEc2TransactionPath))
        return completionHandler(nx_http::StatusCode::notFound);

    auto connectionIdIter = request.headers.find(Qn::EC2_CONNECTION_GUID_HEADER_NAME);
    if (connectionIdIter == request.headers.end())
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG, 
            lm("Received %1 request from %2 without required header %3")
            .arg(request.requestLine.url.path()).str(connection->socket()->getForeignAddress())
            .arg(Qn::EC2_CONNECTION_GUID_HEADER_NAME),
            cl_logDEBUG1);
        return completionHandler(nx_http::StatusCode::badRequest);
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
            .arg(request.requestLine.url.path()).str(connection->socket()->getForeignAddress())
            .arg(connectionId),
            cl_logDEBUG1);
        return completionHandler(nx_http::StatusCode::notFound);
    }

    NX_LOGX(QnLog::EC2_TRAN_LOG, 
        lm("Received %1 request from %2 for connection %3")
        .arg(request.requestLine.url.path()).str(connection->socket()->getForeignAddress())
        .arg(connectionId),
        cl_logDEBUG2);

    connectionIter->connection->post(
        [connectionPtr = connectionIter->connection.get(),
            request = std::move(request)]() mutable
        {
            connectionPtr->receivedTransaction(
                std::move(request.headers),
                std::move(request.messageBody));
        });

    completionHandler(nx_http::StatusCode::ok);
}

void ConnectionManager::dispatchTransaction(
    const nx::String& systemId,
    std::shared_ptr<const TransactionWithSerializedPresentation> transactionSerializer)
{
    NX_LOGX(QnLog::EC2_TRAN_LOG, 
        lm("systemId %1. Dispatching transaction %2")
        .arg(systemId).str(transactionSerializer->transactionHeader()),
        cl_logDEBUG2);

    // Generating transport header.
    TransactionTransportHeader transportHeader;
    transportHeader.systemId = systemId;
    transportHeader.vmsTransportHeader.distance = 1;
    transportHeader.vmsTransportHeader.processedPeers.insert(m_localPeerData.id);
    // transportHeader.vmsTransportHeader.dstPeers.insert(); TODO:.

    QnMutexLocker lk(&m_mutex);
    const auto& connectionBySystemIdAndPeerIdIndex =
        m_connections.get<kConnectionBySystemIdAndPeerIdIndex>();

    std::size_t connectionCount = 0;
    std::array<TransactionTransport*, 7> connectionsToSendTo;
    for (auto connectionIt = connectionBySystemIdAndPeerIdIndex
            .lower_bound(std::make_pair(systemId, nx::String()));
        connectionIt != connectionBySystemIdAndPeerIdIndex.end()
            && connectionIt->systemIdAndPeerId.first == systemId;
        ++connectionIt)
    {
        if (connectionIt->systemIdAndPeerId.second ==
                transactionSerializer->transactionHeader().peerID.toByteArray())
        {
            // Not sending transaction to peer which has generated it.
            continue;
        }

        connectionsToSendTo[connectionCount++] = connectionIt->connection.get();
        if (connectionCount < connectionsToSendTo.size())
            continue;

        for (auto& connection : connectionsToSendTo)
            connection->sendTransaction(
                transportHeader,
                transactionSerializer);
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
            connectionsToSendTo[i]->sendTransaction(
                transportHeader,
                transactionSerializer);
    }
}

api::VmsConnectionDataList ConnectionManager::getVmsConnections() const
{
    QnMutexLocker lk(&m_mutex);
    api::VmsConnectionDataList result;
    for (const auto& connectionContext: m_connections)
    {
        api::VmsConnectionData connectionData;
        connectionData.systemId = connectionContext.systemIdAndPeerId.first.toStdString();
        connectionData.mediaserverEndpoint =
            connectionContext.connection->remoteSocketAddr().toString().toStdString();
        result.connections.push_back(std::move(connectionData));
    }

    return result;
}

bool ConnectionManager::isSystemConnected(const std::string& systemId) const
{
    QnMutexLocker lk(&m_mutex);

    const auto& connectionBySystemIdAndPeerIdIndex =
        m_connections.get<kConnectionBySystemIdAndPeerIdIndex>();
    const auto systemIter = connectionBySystemIdAndPeerIdIndex.lower_bound(
        std::make_pair(nx::String(systemId.c_str()), nx::String()));

    return 
        systemIter != connectionBySystemIdAndPeerIdIndex.end() &&
        systemIter->systemIdAndPeerId.first == systemId;
}

void ConnectionManager::closeConnectionsToSystem(
    const nx::String& systemId,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    auto allConnectionsRemovedGuard = makeSharedGuard(std::move(completionHandler));

    QnMutexLocker lk(&m_mutex);

    auto& connectionBySystemIdAndPeerIdIndex =
        m_connections.get<kConnectionBySystemIdAndPeerIdIndex>();
    auto it = connectionBySystemIdAndPeerIdIndex.lower_bound(
        std::make_pair(systemId, nx::String()));
    while (it != connectionBySystemIdAndPeerIdIndex.end() &&
           it->systemIdAndPeerId.first == systemId)
    {
        removeConnectionByIter(
            &lk,
            connectionBySystemIdAndPeerIdIndex,
            it++,
            [allConnectionsRemovedGuard](){});
    }
}

bool ConnectionManager::addNewConnection(ConnectionContext context)
{
    QnMutexLocker lk(&m_mutex);

    removeExistingConnection<
        kConnectionBySystemIdAndPeerIdIndex,
        decltype(context.systemIdAndPeerId)>(&lk, context.systemIdAndPeerId);

    context.connection->setOnConnectionClosed(
        std::bind(&ConnectionManager::removeConnection, this, context.connectionId));
    context.connection->setOnGotTransaction(
        std::bind(
            &ConnectionManager::onGotTransaction,
            this, context.connection->connectionGuid().toByteArray(),
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    NX_LOGX(QnLog::EC2_TRAN_LOG, 
        lm("Adding new transaction connection %1 from %2")
        .arg(context.connectionId)
        .str(context.connection->commonTransportHeaderOfRemoteTransaction()),
        cl_logDEBUG1);

    if (!m_connections.insert(std::move(context)).second)
    {
        NX_ASSERT(false);
        return false;
    }

    // TODO: later this method will return false in real cases

    return true;
}

template<int connectionIndexNumber, typename ConnectionKeyType>
void ConnectionManager::removeExistingConnection(
    QnMutexLockerBase* const lock,
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
    QnMutexLockerBase* const /*lock*/,
    ConnectionIndex& connectionIndex,
    Iterator connectionIterator,
    CompletionHandler completionHandler)
{
    std::unique_ptr<TransactionTransport> existingConnection;
    connectionIndex.modify(
        connectionIterator,
        [&existingConnection](ConnectionContext& data)
        {
            existingConnection = std::move(data.connection);
        });
    connectionIndex.erase(connectionIterator);

    TransactionTransport* existingConnectionPtr = existingConnection.get();
    
    NX_LOGX(QnLog::EC2_TRAN_LOG,
        lm("Removing transaction connection %1 from %2")
            .arg(existingConnectionPtr->connectionGuid())
            .str(existingConnectionPtr->commonTransportHeaderOfRemoteTransaction()),
        cl_logDEBUG1);

    // ::ec2::TransactionTransportBase does not support its removal 
    //  in signal handler, so have to remove it delayed.
    // TODO: #ak have to ensure somehow that no events from 
    //  connectionContext.connection are delivered.

    existingConnectionPtr->post(
        [existingConnection = std::move(existingConnection),
            locker = m_startedAsyncCallsCounter.getScopedIncrement(),
            completionHandler = std::move(completionHandler)]() mutable
        {
            existingConnection.reset();
            completionHandler();
        });
}

void ConnectionManager::removeConnection(const nx::String& connectionId)
{
    QnMutexLocker lk(&m_mutex);
    removeExistingConnection<kConnectionByIdIndex>(&lk, connectionId);
}

void ConnectionManager::onGotTransaction(
    const nx::String& connectionId,
    Qn::SerializationFormat tranFormat,
    const QByteArray& data,
    TransactionTransportHeader transportHeader)
{
    m_transactionDispatcher->dispatchTransaction(
        std::move(transportHeader),
        tranFormat,
        std::move(data),
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
        QnMutexLocker lk(&m_mutex);
        removeExistingConnection<kConnectionByIdIndex, nx::String>(&lk, connectionId);
    }
}

bool ConnectionManager::fetchDataFromConnectRequest(
    const nx_http::Request& request,
    nx::String* const connectionId,
    ::ec2::ApiPeerData* const remotePeer,
    nx::String* const contentEncoding)
{
    auto connectionIdIter = request.headers.find(Qn::EC2_CONNECTION_GUID_HEADER_NAME);
    if (connectionIdIter == request.headers.end())
        return false;
    *connectionId = connectionIdIter->second;

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
        nx_http::header::AcceptEncodingHeader acceptEncodingHeader(
            acceptEncodingHeaderIter->second);
        if (acceptEncodingHeader.encodingIsAllowed("identity"))
            *contentEncoding = "identity";
        else if (acceptEncodingHeader.encodingIsAllowed("gzip"))
            *contentEncoding = "gzip";
    }

    *remotePeer = ::ec2::ApiPeerData(remoteGuid, remoteRuntimeGuid, peerType, dataFormat);
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

    connectionIter->connection->processSpecialTransaction(
        transportHeader,
        std::move(data),
        std::move(handler));
}

nx_http::RequestResult 
    ConnectionManager::prepareOkResponseToCreateTransactionConnection(
        const nx::String& connectionId,
        const nx::String& contentEncoding,
        nx_http::Response* const response)
{
    response->headers.emplace("Content-Type", ec2::TransactionTransport::TUNNEL_CONTENT_TYPE);
    response->headers.emplace("Content-Encoding", contentEncoding);
    response->headers.emplace(Qn::EC2_GUID_HEADER_NAME, m_localPeerData.id.toByteArray());
    response->headers.emplace(
        Qn::EC2_RUNTIME_GUID_HEADER_NAME,
        m_localPeerData.instanceId.toByteArray());
    // TODO: #ak have to support different proto versions and add runtime conversion between them.
    response->headers.emplace(
        Qn::EC2_PROTO_VERSION_HEADER_NAME,
        nx::String::number(nx_ec::EC2_PROTO_VERSION));
    response->headers.emplace("X-Nx-Cloud", "true");
    response->headers.emplace(Qn::EC2_BASE64_ENCODING_REQUIRED_HEADER_NAME, "true");

    nx_http::RequestResult requestResult;

    requestResult.statusCode = nx_http::StatusCode::ok;
    requestResult.connectionEvents.onResponseHasBeenSent =
        [this, connectionId](
            nx_http::HttpServerConnection* connection)
        {
            QnMutexLocker lk(&m_mutex);

            const auto& connectionByIdIndex = m_connections.get<kConnectionByIdIndex>();
            auto connectionIter = connectionByIdIndex.find(connectionId);
            NX_ASSERT(connectionIter != connectionByIdIndex.end());

            connectionIter->connection->setOutgoingConnection(
                QSharedPointer<AbstractCommunicatingSocket>(connection->takeSocket().release()));
            connectionIter->connection->startOutgoingChannel();
        };
    requestResult.dataSource = 
        std::make_unique<nx_http::EmptyMessageBodySource>(
            ec2::TransactionTransport::TUNNEL_CONTENT_TYPE,
            boost::none);

    return requestResult;
}

} // namespace ec2
} // namespace cdb
} // namespace nx
