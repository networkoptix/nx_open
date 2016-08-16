/**********************************************************
* Aug 8, 2016
* a.kolesnikov
***********************************************************/

#include "connection_manager.h"

#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx_ec/data/api_peer_data.h>
#include <nx_ec/ec_proto_version.h>

#include <http/custom_headers.h>

#include "access_control/authorization_manager.h"
#include "stree/cdb_ns.h"
#include "transaction_dispatcher.h"
#include "transaction_transport.h"
#include "transaction_transport_header.h"


namespace nx {
namespace cdb {
namespace ec2 {

static const QnUuid kCdbGuid("{A1D368E4-3D01-4B18-8642-898272D7CDA9}");

ConnectionManager::ConnectionManager(
    const conf::Settings& settings,
    TransactionDispatcher* const transactionDispatcher)
:
    m_settings(settings),
    m_transactionDispatcher(transactionDispatcher),
    m_localPeerData(kCdbGuid, QnUuid::createUuid(), Qn::PT_CloudServer, Qn::UbjsonFormat)
{
}

ConnectionManager::~ConnectionManager()
{
    m_startedAsyncCallsCounter.wait();
}

void ConnectionManager::createTransactionConnection(
    nx_http::HttpServerConnection* const /*connection*/,
    stree::ResourceContainer authInfo,
    nx_http::Request request,
    nx_http::Response* const response,
    nx_http::HttpRequestProcessedHandler completionHandler)
{
    // GET ec2/events/ConnectingStage2?guid=%7B8b939668-837d-4658-9d7a-e2cc6c12a38b%7D&runtime-guid=%7B0eac9718-4e37-4459-8799-c3023d4f7cb5%7D&system-identity-time=0&isClient
    //TODO #ak

    std::string systemId;
    if (!authInfo.get(attr::authSystemID, &systemId))
        return completionHandler(
            nx_http::StatusCode::ok,
            nullptr,
            nx_http::ConnectionEvents());

    auto connectionIdIter = request.headers.find(Qn::EC2_CONNECTION_GUID_HEADER_NAME);
    if (connectionIdIter == request.headers.end())
        return completionHandler(
            nx_http::StatusCode::badRequest,
            nullptr,
            nx_http::ConnectionEvents());
    auto connectionId = connectionIdIter->second;

    ::ec2::ApiPeerData remotePeer;
    nx::String contentEncoding;
    fetchDataFromConnectRequest(request, &remotePeer, &contentEncoding);

    nx_http::ConnectionEvents connectionEvents;
    connectionEvents.onResponseHasBeenSent = 
        [this,
            connectionId = std::move(connectionId),
            systemId = std::move(systemId),
            remotePeer = std::move(remotePeer),
            request = std::move(request),
            contentEncoding](
                nx_http::HttpServerConnection* connection)
        {
            const nx::String systemIdLocal(systemId.c_str());
            auto newTransport = std::make_unique<TransactionTransport>(
                systemIdLocal,
                connectionId,
                m_localPeerData,
                remotePeer,
                QSharedPointer<AbstractCommunicatingSocket>(connection->takeSocket().release()),
                request,
                contentEncoding);

            ConnectionContext context{
                std::move(newTransport),
                connectionId,
                std::make_pair(systemIdLocal, remotePeer.id.toByteArray()) };

            addNewConnection(std::move(context));
        };

    response->headers.emplace("Content-Type", ec2::TransactionTransport::TUNNEL_CONTENT_TYPE);
    response->headers.emplace("Content-Encoding", contentEncoding);
    response->headers.emplace(Qn::EC2_GUID_HEADER_NAME, m_localPeerData.id.toByteArray());
    response->headers.emplace(
        Qn::EC2_RUNTIME_GUID_HEADER_NAME,
        m_localPeerData.instanceId.toByteArray());
    //TODO #ak have to support different proto versions and add runtime conversion between them
    response->headers.emplace(
        Qn::EC2_PROTO_VERSION_HEADER_NAME,
        nx::String::number(nx_ec::EC2_PROTO_VERSION));
    response->headers.emplace("X-Nx-Cloud", "true");
    response->headers.emplace(Qn::EC2_BASE64_ENCODING_REQUIRED_HEADER_NAME, "true");
    completionHandler(nx_http::StatusCode::ok, nullptr, std::move(connectionEvents));
}

void ConnectionManager::pushTransaction(
    nx_http::HttpServerConnection* const /*connection*/,
    stree::ResourceContainer authInfo,
    nx_http::Request request,
    nx_http::Response* const /*response*/,
    nx_http::HttpRequestProcessedHandler completionHandler)
{
    auto connectionIdIter = request.headers.find(Qn::EC2_CONNECTION_GUID_HEADER_NAME);
    if (connectionIdIter == request.headers.end())
        return completionHandler(
            nx_http::StatusCode::badRequest,
            nullptr,
            nx_http::ConnectionEvents());
    const auto connectionId = connectionIdIter->second;

    QnMutexLocker lk(&m_mutex);
    //reporting received transaction(s) to the corresponding connection 
    const auto& connectionByIdIndex = m_connections.get<kConnectionByIdIndex>();
    auto connectionIter = connectionByIdIndex.find(connectionId);
    if (connectionIter == connectionByIdIndex.end())
        return completionHandler(
            nx_http::StatusCode::notFound,
            nullptr,
            nx_http::ConnectionEvents());

    connectionIter->connection->post(
        [connectionPtr = connectionIter->connection.get(),
            request = std::move(request)]() mutable
        {
            connectionPtr->receivedTransaction(
                std::move(request.headers),
                request.messageBody);
        });

    completionHandler(nx_http::StatusCode::ok, nullptr, nx_http::ConnectionEvents());
}

void ConnectionManager::addNewConnection(ConnectionContext context)
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

    m_connections.insert(std::move(context));
}

template<int connectionIndexNumber, typename ConnectionKeyType>
void ConnectionManager::removeExistingConnection(
    QnMutexLockerBase* const /*lock*/,
    ConnectionKeyType connectionKey)
{
    auto& connectionIndex = m_connections.get<connectionIndexNumber>();
    auto existingConnectionIter = connectionIndex.find(connectionKey);
    if (existingConnectionIter != connectionIndex.end())
    {
        //removing existing connection
        std::unique_ptr<TransactionTransport> existingConnection;
        connectionIndex.modify(
            existingConnectionIter,
            [&existingConnection](ConnectionContext& data)
            {
                existingConnection = std::move(data.connection);
            });
        TransactionTransport* connectionPtr = existingConnection.get();
        connectionPtr->pleaseStop(
            [existingConnection = std::move(existingConnection)]() mutable
            {
                existingConnection.reset();
            });
        connectionIndex.erase(existingConnectionIter);
    }
}

void ConnectionManager::removeConnection(const nx::String& connectionId)
{
    //always called within transport's aio thread

    QnMutexLocker lk(&m_mutex);
    auto& index = m_connections.get<kConnectionByIdIndex>();
    auto iter = index.find(connectionId);
    if (iter == index.end())
        return; //assert?
    ConnectionContext connectionContext;
    index.modify(
        iter,
        [&connectionContext](ConnectionContext& value){ connectionContext = std::move(value); });
    index.erase(iter);
    lk.unlock();

    //::ec2::TransactionTransportBase does not support its removal 
    //  in signal handler, so have to remove it delayed...
    //TODO #ak have to ensure somehow that no events 
    //  from connectionContext.connection are delivered
    auto connectionPtr = connectionContext.connection.get();
    connectionPtr->pleaseStop(
        [connection = std::move(connectionContext.connection)]() mutable
        {
            connection.reset();
        });
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
        //closing connection in case of failure
        QnMutexLocker lk(&m_mutex);
        removeExistingConnection<kConnectionByIdIndex, nx::String>(&lk, connectionId);
    }
}

void ConnectionManager::fetchDataFromConnectRequest(
    const nx_http::Request& request,
    ::ec2::ApiPeerData* const remotePeer,
    nx::String* const contentEncoding)
{
    QUrlQuery query = QUrlQuery(request.requestLine.url.query());
    const bool isClient = query.hasQueryItem("isClient");
    const bool isMobileClient = query.hasQueryItem("isMobile");
    QnUuid remoteGuid = QnUuid(query.queryItemValue("guid"));
    QnUuid remoteRuntimeGuid = QnUuid(query.queryItemValue("runtime-guid"));
    if (remoteGuid.isNull())
        remoteGuid = QnUuid::createUuid();

    const Qn::PeerType peerType =
    isMobileClient ? Qn::PT_MobileClient :
        isClient ? Qn::PT_DesktopClient :
        Qn::PT_Server;

    Qn::SerializationFormat dataFormat = Qn::UbjsonFormat;
    if (query.hasQueryItem("format"))
        QnLexical::deserialize(query.queryItemValue("format"), &dataFormat);

    //checking content encoding requested by remote peer
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
}

}   // namespace ec2
}   // namespace cdb
}   // namespace nx
