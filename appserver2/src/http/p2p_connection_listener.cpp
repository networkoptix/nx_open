#include "p2p_connection_listener.h"

#include <QtCore/QUrlQuery>

#include <api/global_settings.h>
#include <nx_ec/ec_proto_version.h>

#include "network/tcp_connection_priv.h"
#include "nx_ec/data/api_full_info_data.h"
#include <database/db_manager.h>
#include "common/common_module.h"
#include "transaction/transaction_transport.h"
#include <nx/network/http/custom_headers.h>
#include "audit/audit_manager.h"
#include "settings.h"
#include <core/resource/media_server_resource.h>
#include <nx/fusion/serialization/lexical.h>

#include <nx/network/websocket/websocket_handshake.h>
#include <nx/network/websocket/websocket.h>
#include <nx/network/socket_delegate.h>
#include <transaction/message_bus_adapter.h>

namespace nx {
namespace p2p {

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

// -------------------------- ConnectionProcessor ---------------------

const QString ConnectionProcessor::kUrlPath(lit("/ec2/messageBus"));

ConnectionProcessor::ConnectionProcessor(
    QSharedPointer<AbstractStreamSocket> socket,
    QnTcpListener* owner)
    :
    QnTCPConnectionProcessor(socket, owner)
{
    Q_D(QnTCPConnectionProcessor);
    d->isSocketTaken = true;
    setObjectName(::toString(this));
}

ConnectionProcessor::~ConnectionProcessor()
{
    stop();
}

QByteArray ConnectionProcessor::responseBody(Qn::SerializationFormat dataFormat)
{
    ec2::ApiPeerDataEx localPeer;
    localPeer.id = commonModule()->moduleGUID();
    localPeer.persistentId = commonModule()->dbId();
    localPeer.instanceId = commonModule()->runningInstanceGUID();
    localPeer.systemId = commonModule()->globalSettings()->localSystemId();

    localPeer.peerType = Qn::PT_Server;
    localPeer.cloudHost = nx::network::AppInfo::defaultCloudHost();
    localPeer.identityTime = commonModule()->systemIdentityTime();
    localPeer.aliveUpdateIntervalMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        commonModule()->globalSettings()->aliveUpdateInterval()).count();
    localPeer.protoVersion = nx_ec::EC2_PROTO_VERSION;

    if (dataFormat == Qn::JsonFormat)
        return QJson::serialized(localPeer);
    else if (dataFormat == Qn::UbjsonFormat)
        return QnUbjson::serialized(localPeer);
    else
        return QByteArray();
}

bool ConnectionProcessor::isDisabledPeer(const ec2::ApiPeerData& remotePeer) const
{
    return (!commonModule()->allowedPeers().isEmpty() &&
        !commonModule()->allowedPeers().contains(remotePeer.id) &&
        !remotePeer.isClient());
}

bool ConnectionProcessor::isPeerCompatible(const ec2::ApiPeerDataEx& remotePeer) const
{
    Q_D(const QnTCPConnectionProcessor);
    const auto& commonModule = d->owner->commonModule();

    if (remotePeer.peerType == Qn::PT_Server && commonModule->isReadOnly())
        return false;
    if (!remotePeer.systemId.isNull() &&
        remotePeer.systemId != commonModule->globalSettings()->localSystemId())
    {
        NX_WARNING(
            this,
            lm("Reject incoming P2P connection from peer %1 because of different systemId. "
                "Local peer version: %2, remote peer version: %3")
            .arg(d->socket->getForeignAddress().address.toString())
            .arg(commonModule->globalSettings()->localSystemId())
            .arg(remotePeer.systemId));
        return false;
    }
    if (remotePeer.peerType != Qn::PT_MobileClient)
    {
        if (nx_ec::EC2_PROTO_VERSION != remotePeer.protoVersion)
        {
            NX_LOG(this,
                lm("Reject incoming P2P connection from peer %1 because of different EC2 proto version. "
                    "Local peer version: %2, remote peer version: %3")
                .arg(d->socket->getForeignAddress().address.toString())
                .arg(nx_ec::EC2_PROTO_VERSION)
                .arg(remotePeer.protoVersion),
                cl_logWARNING);
            return false;
        }
    }
    if (remotePeer.peerType == Qn::PT_Server)
    {
        if (nx::network::AppInfo::defaultCloudHost() != remotePeer.cloudHost)
        {
            NX_LOG(this,
                lm("Reject incoming P2P connection from peer %1 because they have different built in cloud host setting. "
                    "Local peer host: %2, remote peer host: %3")
                .arg(d->socket->getForeignAddress().address.toString())
                .arg(nx::network::AppInfo::defaultCloudHost())
                .arg(remotePeer.cloudHost),
                cl_logWARNING);
            return false;
        }
    }
    return true;
}

ec2::ApiPeerDataEx ConnectionProcessor::deserializeRemotePeerInfo()
{
    Q_D(const QnTCPConnectionProcessor);
    ec2::ApiPeerDataEx remotePeer;
    QUrlQuery query(d->request.requestLine.url.query());

    Qn::SerializationFormat dataFormat = Qn::UbjsonFormat;
    if (query.hasQueryItem("format"))
        QnLexical::deserialize(query.queryItemValue("format"), &dataFormat);

    bool success = false;
    QByteArray peerData = nx_http::getHeaderValue(d->request.headers, Qn::EC2_PEER_DATA);
    peerData = QByteArray::fromBase64(peerData);
    if (dataFormat == Qn::JsonFormat)
        remotePeer = QJson::deserialized(peerData, ec2::ApiPeerDataEx(), &success);
    else if (dataFormat == Qn::UbjsonFormat)
        remotePeer = QnUbjson::deserialized(peerData, ec2::ApiPeerDataEx(), &success);

    if (remotePeer.id.isNull())
        remotePeer.id = QnUuid::createUuid();
    return remotePeer;
}

Qn::UserAccessData ConnectionProcessor::userAccessData(const ec2::ApiPeerDataEx& remotePeer) const
{
    Q_D(const QnTCPConnectionProcessor);
    // By default all peers have read permissions on all resources
    auto access = d->accessRights;
    if (remotePeer.peerType == Qn::PT_Server)
    {
        // Here we substitute admin user with SuperAccess user to pass by all access checks unhurt
        // since server-to-server order of transactions is unpredictable and access check for resource attribute
        // may come before resource itself is added to the resource pool and this may be restricted by the access
        // checking mechanics.
        if (access != Qn::kSystemAccess)
        {
            bool authAsOwner = d->accessRights.userId == QnUserResource::kAdminGuid;
            NX_ASSERT(authAsOwner, "Server must always be authorised as owner");
            if (authAsOwner)
                access = Qn::kSystemAccess;
        }
    }
    else
    {
        access.access = Qn::UserAccessData::Access::ReadAllResources;
    }
    return access;
}

void ConnectionProcessor::run()
{
    Q_D(QnTCPConnectionProcessor);
    initSystemThreadId();

    if (d->clientRequest.isEmpty() && !readRequest())
        return;
    parseRequest();

    ec2::ApiPeerDataEx remotePeer = deserializeRemotePeerInfo();
    if (!isPeerCompatible(remotePeer))
    {
        sendResponse(nx_http::StatusCode::forbidden, nx_http::StringType());
        return;
    }

    const auto commonModule = d->owner->commonModule();
    const auto connection = commonModule->ec2Connection();
    auto messageBus = (connection->messageBus()->dynamicCast<MessageBus*>());
    if (!messageBus)
    {
        sendResponse(
            nx_http::StatusCode::forbidden,
            nx_http::StringType());
        return;
    }
    ec2::ConnectionLockGuard connectionLockGuard(
        commonModule->moduleGUID(),
        messageBus->connectionGuardSharedState(),
        remotePeer.id,
        ec2::ConnectionLockGuard::Direction::Incoming);

    if (remotePeer.peerType == Qn::PT_Server)
    {
        // addition checking to prevent two connections (ingoing and outgoing) at once
        // 1-st stage
        bool lockOK = connectionLockGuard.tryAcquireConnecting();

        d->response.headers.insert(nx_http::HttpHeader(
            Qn::EC2_CONNECT_STAGE_1,
            nx_http::StringType()));

        sendResponse(
            lockOK ? nx_http::StatusCode::noContent : nx_http::StatusCode::forbidden,
            nx_http::StringType());
        if (!lockOK)
            return;

        // 2-nd stage
        if (!readRequest())
            return;
        parseRequest();
    }

    if(!connectionLockGuard.tryAcquireConnected() || //< lock failed
        remotePeer.id == commonModule->moduleGUID() || //< can't connect to itself
        isDisabledPeer(remotePeer)) //< allowed peers are strict
    {
        sendResponse(nx_http::StatusCode::forbidden, nx_http::StringType());
        return;
    }

    using namespace nx::network;
    auto error = websocket::validateRequest(d->request, &d->response);
    if (error != websocket::Error::noError)
    {
        NX_LOG(lm("Invalid WEB socket request. Validation failed. Error: %1").arg((int)error), cl_logERROR);
        d->socket->close();
        return;
    }

    d->response.headers.insert(nx_http::HttpHeader(
        Qn::EC2_PEER_DATA,
        responseBody(remotePeer.dataFormat).toBase64()));

    sendResponse(
        nx_http::StatusCode::switchingProtocols,
        nx_http::StringType());


    std::function<void()> onConnectionClosedCallback;
    if (remotePeer.isClient())
    {
        auto session = authSession();
        qnAuditManager->at_connectionOpened(session);
        onConnectionClosedCallback = std::bind(&QnAuditManager::at_connectionClosed, qnAuditManager, session);
    }

    std::unique_ptr<ShareSocketDelegate> socket(new ShareSocketDelegate(std::move(d->socket)));
    socket->setNonBlockingMode(true);
    auto keepAliveTimeout = std::chrono::milliseconds(remotePeer.aliveUpdateIntervalMs);
    WebSocketPtr webSocket(new websocket::WebSocket(std::move(socket)));
    webSocket->setAliveTimeoutEx(keepAliveTimeout, 2);

    messageBus->gotConnectionFromRemotePeer(
        remotePeer,
        std::move(connectionLockGuard),
        std::move(webSocket),
        userAccessData(remotePeer),
        onConnectionClosedCallback);
}

} // namespace p2p
} // namespace nx
