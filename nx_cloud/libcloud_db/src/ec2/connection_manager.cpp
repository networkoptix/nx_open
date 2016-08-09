/**********************************************************
* Aug 8, 2016
* a.kolesnikov
***********************************************************/

#include "connection_manager.h"

#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx_ec/data/api_peer_data.h>

#include <http/custom_headers.h>

#include "access_control/authorization_manager.h"
#include "http_handlers.h"
#include "stree/cdb_ns.h"
#include "transaction_transport.h"
#include <cloud_db_client/src/cdb_request_path.h>



namespace nx {
namespace cdb {
namespace ec2 {

ConnectionManager::ConnectionManager(const conf::Settings& settings)
:
    m_settings(settings)
{
}

ConnectionManager::~ConnectionManager()
{
}

void ConnectionManager::createTransactionConnection(
    nx_http::HttpServerConnection* const connection,
    stree::ResourceContainer authInfo,
    nx_http::Request request,
    nx_http::Response* const response,
    std::function<void(
        const nx_http::StatusCode::Value statusCode,
        std::unique_ptr<nx_http::AbstractMsgBodySource> responseMsgBodyDataSource)
    > completionHandler)
{
    // GET ec2/events/ConnectingStage2?guid=%7B8b939668-837d-4658-9d7a-e2cc6c12a38b%7D&runtime-guid=%7B0eac9718-4e37-4459-8799-c3023d4f7cb5%7D&system-identity-time=0&isClient
    //TODO #ak

    std::string systemId;
    if (!authInfo.get(attr::authSystemID, &systemId))
        return completionHandler(nx_http::StatusCode::ok, nullptr);

    auto connectionIdIter = request.headers.find(Qn::EC2_CONNECTION_GUID_HEADER_NAME);
    if (connectionIdIter == request.headers.end())
        return completionHandler(nx_http::StatusCode::badRequest, nullptr);

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
    nx::String contentEncoding;
    if (acceptEncodingHeaderIter != request.headers.end())
    {
        nx_http::header::AcceptEncodingHeader acceptEncodingHeader(acceptEncodingHeaderIter->second);
        if (acceptEncodingHeader.encodingIsAllowed("identity"))
            contentEncoding = "identity";
        else if (acceptEncodingHeader.encodingIsAllowed("gzip"))
            contentEncoding = "gzip";
    }

    ::ec2::ApiPeerData remotePeer(remoteGuid, remoteRuntimeGuid, peerType, dataFormat);

    auto newTransport = std::make_unique<TransactionTransport>(
        connectionIdIter->second,
        remotePeer,
        QSharedPointer<AbstractCommunicatingSocket>(connection->takeSocket().release()),
        request,
        contentEncoding);

    //TODO #ak take socket after sending response message

    QnMutexLocker lk(&m_mutex);
    const auto connectionInsertResult = m_connections.emplace(
        std::make_pair(QString::fromStdString(systemId), remoteGuid),
        nullptr);
    if (!connectionInsertResult.second)
    {
        TransactionTransport* connectionPtr = connectionInsertResult.first->second.get();
        m_connectionsToRemove.emplace(
            connectionPtr,
            std::move(connectionInsertResult.first->second));
        connectionInsertResult.first->second->post(
            std::bind(&ConnectionManager::removeConnection, this, connectionPtr));
    }
    connectionInsertResult.first->second = std::move(newTransport);
    lk.unlock();
}

void ConnectionManager::pushTransaction(
    nx_http::HttpServerConnection* const connection,
    stree::ResourceContainer authInfo,
    nx_http::Request request,
    nx_http::Response* const response,
    std::function<void(
        const nx_http::StatusCode::Value statusCode,
        std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource)
    > completionHandler)
{
    //TODO #ak
}

void ConnectionManager::removeConnection(TransactionTransport* transport)
{
    //always called within transport's aio thread
    //TODO #ak
}

}   // namespace ec2
}   // namespace cdb
}   // namespace nx
