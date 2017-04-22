#include "p2p_connection_listener.h"

#include <QtCore/QUrlQuery>

#include <api/global_settings.h>
#include <nx_ec/ec_proto_version.h>

#include "network/tcp_connection_priv.h"
#include "nx_ec/data/api_full_info_data.h"
#include "database/db_manager.h"
#include "common/common_module.h"
#include "transaction/transaction_transport.h"
#include "http/custom_headers.h"
#include "audit/audit_manager.h"
#include "settings.h"
#include <core/resource/media_server_resource.h>
#include <nx/fusion/serialization/lexical.h>

#include <nx/network/websocket/websocket_handshake.h>
#include <nx/network/websocket/websocket.h>
#include <nx/network/socket_delegate.h>
#include <transaction/p2p_message_bus.h>

class ShareSocketDelegate: public nx::network::StreamSocketDelegate
{
    using base_type = nx::network::StreamSocketDelegate;
public:
    ShareSocketDelegate(QSharedPointer<AbstractStreamSocket> socket):
        base_type(socket.data()),
        m_socket(std::move(socket))
    {
    }
    ~ShareSocketDelegate()
    {

    }
private:
    QSharedPointer<AbstractStreamSocket> m_socket;
};

namespace ec2
{

// -------------------------- P2pConnectionProcessor ---------------------


class P2pConnectionProcessorPrivate : public QnTCPConnectionProcessorPrivate
{
public:

    P2pConnectionProcessorPrivate() :
        QnTCPConnectionProcessorPrivate(),
        messageBus(nullptr)
    {
    }

    P2pMessageBus* messageBus;
};

P2pConnectionProcessor::P2pConnectionProcessor(
    P2pMessageBus* messageBus,
    QSharedPointer<AbstractStreamSocket> socket,
    QnTcpListener* owner)
:
    QnTCPConnectionProcessor(new P2pConnectionProcessorPrivate, socket, owner->commonModule())
{
    Q_D(P2pConnectionProcessor);

    d->messageBus = messageBus;
    setObjectName("P2pConnectionProcessor");
}

P2pConnectionProcessor::~P2pConnectionProcessor()
{
    stop();
}

void P2pConnectionProcessor::run()
{
    Q_D(P2pConnectionProcessor);
    initSystemThreadId();

    if (d->clientRequest.isEmpty())
    {
        if (!readRequest())
            return;
    }
    parseRequest();

    using namespace nx::network;

    auto error = websocket::validateRequest(d->request, &d->response);
    if (error != websocket::Error::noError)
    {
        NX_LOG(lm("Invalid WEB socket request. Validation failed. Error: %1").arg((int)error), cl_logERROR);
        d->socket->close();
        return;
    }

    QUrlQuery query = QUrlQuery(d->request.requestLine.url.query());

    QnUuid remoteGuid = nx_http::getHeaderValue(d->request.headers, Qn::EC2_GUID_HEADER_NAME);
    if (remoteGuid.isNull())
        remoteGuid = QnUuid::createUuid();
    QnUuid remoteRuntimeGuid = nx_http::getHeaderValue(d->request.headers, Qn::EC2_RUNTIME_GUID_HEADER_NAME);
    QnUuid remoteDbGuid = nx_http::getHeaderValue(d->request.headers, Qn::EC2_DB_GUID_HEADER_NAME);
    qint64 remoteSystemIdentityTime = nx_http::getHeaderValue(d->request.headers, Qn::EC2_SYSTEM_IDENTITY_HEADER_NAME).toLongLong();

    bool deserialized = false;
    Qn::PeerType peerType = QnLexical::deserialized<Qn::PeerType>(
        query.queryItemValue("peerType"),
        Qn::PT_NotDefined,
        &deserialized);
    if (!deserialized || peerType == Qn::PT_NotDefined)
    {
        QnUuid videowallGuid = QnUuid(query.queryItemValue("videowallGuid"));
        const bool isVideowall = !videowallGuid.isNull();
        const bool isClient = query.hasQueryItem("isClient");
        const bool isMobileClient = query.hasQueryItem("isMobile");

        peerType = isMobileClient ? Qn::PT_OldMobileClient
            : isVideowall ? Qn::PT_VideowallClient
            : isClient ? Qn::PT_DesktopClient
            : Qn::PT_Server;
    }

    Qn::SerializationFormat dataFormat = Qn::UbjsonFormat;
    if (query.hasQueryItem("format"))
        QnLexical::deserialize(query.queryItemValue("format"), &dataFormat);

    ApiPeerData remotePeer(remoteGuid, remoteRuntimeGuid, peerType, dataFormat);

    if (peerType == Qn::PT_Server && ec2::Settings::instance()->dbReadOnly())
    {
        sendResponse(nx_http::StatusCode::forbidden, nx_http::StringType());
        return;
    }

    const auto& commonModule = d->messageBus->commonModule();
    auto systemNameHeaderIter = d->request.headers.find(Qn::EC2_SYSTEM_ID_HEADER_NAME);
    if ((systemNameHeaderIter != d->request.headers.end()) &&
        (nx_http::getHeaderValue(d->request.headers, Qn::EC2_SYSTEM_ID_HEADER_NAME) !=
            commonModule->globalSettings()->localSystemId().toByteArray()))
    {
        sendResponse(nx_http::StatusCode::forbidden, nx_http::StringType());
        return;
    }

    auto keepAliveTimeout = commonModule->globalSettings()->connectionKeepAliveTimeout();
    d->response.headers.emplace(
        Qn::EC2_CONNECTION_TIMEOUT_HEADER_NAME,
        nx_http::header::KeepAlive(
            keepAliveTimeout).toString());

    d->response.headers.insert(nx_http::HttpHeader(
        Qn::EC2_GUID_HEADER_NAME,
        commonModule->moduleGUID().toByteArray()));
    d->response.headers.insert(nx_http::HttpHeader(
        Qn::EC2_RUNTIME_GUID_HEADER_NAME,
        commonModule->runningInstanceGUID().toByteArray()));
    d->response.headers.insert(nx_http::HttpHeader(
        Qn::EC2_DB_GUID_HEADER_NAME,
        commonModule->dbId().toByteArray()));

    d->response.headers.insert(nx_http::HttpHeader(
        Qn::EC2_SYSTEM_IDENTITY_HEADER_NAME,
        QByteArray::number(commonModule->systemIdentityTime())));
    d->response.headers.insert(nx_http::HttpHeader(
        Qn::EC2_PROTO_VERSION_HEADER_NAME,
        nx_http::StringType::number(nx_ec::EC2_PROTO_VERSION)));
    d->response.headers.insert(nx_http::HttpHeader(
        Qn::EC2_CLOUD_HOST_HEADER_NAME,
        nx::network::AppInfo::defaultCloudHost().toUtf8()));
    d->response.headers.insert(nx_http::HttpHeader(
        Qn::EC2_SYSTEM_ID_HEADER_NAME,
        commonModule->globalSettings()->localSystemId().toByteArray()));

    sendResponse(nx_http::StatusCode::upgrade, nx_http::StringType());

    std::unique_ptr<ShareSocketDelegate> socket(new ShareSocketDelegate(std::move(d->socket)));
    socket->setNonBlockingMode(true);
    socket->setRecvTimeout(std::chrono::milliseconds(keepAliveTimeout * 2).count());
    socket->setSendTimeout(std::chrono::milliseconds(keepAliveTimeout * 2).count());

    /*
    WebSocketPtr webSocket(new websocket::Websocket(
        std::move(socket),
        d->request.messageBody));
    */
    P2pConnectionPtr connection(new P2pConnection(
        commonModule,
        remotePeer,
        d->messageBus->localPeer(),
        std::move(socket)));
    d->messageBus->gotConnectionFromRemotePeer(connection);

}
}
