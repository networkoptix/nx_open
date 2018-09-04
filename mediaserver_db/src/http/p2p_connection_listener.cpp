#include "p2p_connection_listener.h"

#include <QtCore/QUrlQuery>

#include <api/global_settings.h>
#include <nx_ec/ec_proto_version.h>

#include "network/tcp_connection_priv.h"
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
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_delegate.h>
#include <transaction/message_bus_adapter.h>
#include <nx/p2p/p2p_server_message_bus.h>

namespace nx {
namespace p2p {

// -------------------------- ConnectionProcessor ---------------------

ConnectionProcessor::ConnectionProcessor(
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    QnTcpListener* owner)
    :
    QnTCPConnectionProcessor(std::move(socket), owner)
{
    Q_D(QnTCPConnectionProcessor);
    setObjectName(::toString(this));
}

ConnectionProcessor::~ConnectionProcessor()
{
    stop();
}

vms::api::PeerDataEx ConnectionProcessor::localPeer() const
{
    vms::api::PeerDataEx localPeer;
    localPeer.id = commonModule()->moduleGUID();
    localPeer.persistentId = commonModule()->dbId();
    localPeer.instanceId = commonModule()->runningInstanceGUID();
    localPeer.systemId = commonModule()->globalSettings()->localSystemId();
    localPeer.peerType = vms::api::PeerType::server;
    localPeer.cloudHost = nx::network::SocketGlobals::cloud().cloudHost();
    localPeer.identityTime = commonModule()->systemIdentityTime();
    localPeer.aliveUpdateIntervalMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        commonModule()->globalSettings()->aliveUpdateInterval()).count();
    localPeer.protoVersion = nx_ec::EC2_PROTO_VERSION;
    return localPeer;
}

bool ConnectionProcessor::isDisabledPeer(const vms::api::PeerData& remotePeer) const
{
    return (!commonModule()->allowedPeers().isEmpty() &&
        !commonModule()->allowedPeers().contains(remotePeer.id) &&
        !remotePeer.isClient());
}

bool ConnectionProcessor::isPeerCompatible(const vms::api::PeerDataEx& remotePeer) const
{
    Q_D(const QnTCPConnectionProcessor);
    const auto& commonModule = d->owner->commonModule();

    if (remotePeer.peerType == vms::api::PeerType::server && commonModule->isReadOnly())
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
    if (remotePeer.peerType != vms::api::PeerType::mobileClient)
    {
        if (nx_ec::EC2_PROTO_VERSION != remotePeer.protoVersion)
        {
            NX_WARNING(this, this,
                lm("Reject incoming P2P connection from peer %1 because of different EC2 proto version. "
                    "Local peer version: %2, remote peer version: %3")
                .arg(d->socket->getForeignAddress().address.toString())
                .arg(nx_ec::EC2_PROTO_VERSION)
                .arg(remotePeer.protoVersion));
            return false;
        }
    }
    if (remotePeer.peerType == vms::api::PeerType::server)
    {
        if (nx::network::SocketGlobals::cloud().cloudHost() != remotePeer.cloudHost)
        {
            NX_WARNING(this, this,
                lm("Reject incoming P2P connection from peer %1 because they have different built in cloud host setting. "
                    "Local peer host: %2, remote peer host: %3")
                .arg(d->socket->getForeignAddress().address.toString())
                .arg(nx::network::SocketGlobals::cloud().cloudHost())
                .arg(remotePeer.cloudHost));
            return false;
        }
    }
    return true;
}

Qn::UserAccessData ConnectionProcessor::userAccessData(const vms::api::PeerDataEx& remotePeer) const
{
    Q_D(const QnTCPConnectionProcessor);
    // By default all peers have read permissions on all resources
    auto access = d->accessRights;
    if (remotePeer.peerType == vms::api::PeerType::server)
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

    if (commonModule()->isStandAloneMode())
    {
        NX_DEBUG(this, "Incoming messageBus connections are temporary disabled. Ignore new incoming connection.");
        sendResponse(nx::network::http::StatusCode::forbidden, nx::network::http::StringType());
        return;
    }

    vms::api::PeerDataEx remotePeer = deserializePeerData(d->request);
    if (!isPeerCompatible(remotePeer))
    {
        sendResponse(nx::network::http::StatusCode::forbidden, nx::network::http::StringType());
        return;
    }

    const auto commonModule = d->owner->commonModule();
    const auto connection = commonModule->ec2Connection();
    auto messageBus = (connection->messageBus()->dynamicCast<ServerMessageBus*>());
    if (!messageBus)
    {
        sendResponse(
            nx::network::http::StatusCode::forbidden,
            nx::network::http::StringType());
        return;
    }

    if (Connection::checkAndSetSystemIdentityTime(remotePeer, commonModule))
    {
        sendResponse(nx::network::http::StatusCode::forbidden, nx::network::http::StringType());
        return;
    }

    ec2::ConnectionLockGuard connectionLockGuard(
        commonModule->moduleGUID(),
        messageBus->connectionGuardSharedState(),
        remotePeer.id,
        ec2::ConnectionLockGuard::Direction::Incoming);

    if (remotePeer.peerType == vms::api::PeerType::server)
    {
        // addition checking to prevent two connections (ingoing and outgoing) at once
        // 1-st stage
        bool lockOK = connectionLockGuard.tryAcquireConnecting();

        d->response.headers.insert(nx::network::http::HttpHeader(
            Qn::EC2_CONNECT_STAGE_1,
            nx::network::http::StringType()));

        sendResponse(
            lockOK ? nx::network::http::StatusCode::noContent : nx::network::http::StatusCode::forbidden,
            nx::network::http::StringType());
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
        sendResponse(nx::network::http::StatusCode::forbidden, nx::network::http::StringType());
        return;
    }

    using namespace nx::network;
    auto error = websocket::validateRequest(d->request, &d->response);
    if (error != websocket::Error::noError)
    {
        NX_ERROR(this, lm("Invalid WEB socket request. Validation failed. Error: %1").arg((int)error));
        d->socket->close();
        return;
    }

    serializePeerData(d->response, localPeer(), remotePeer.dataFormat);

    sendResponse(
        nx::network::http::StatusCode::switchingProtocols,
        nx::network::http::StringType());


    std::function<void()> onConnectionClosedCallback;
    if (remotePeer.isClient())
    {
        auto session = authSession();
        commonModule->auditManager()->at_connectionOpened(session);
        onConnectionClosedCallback = std::bind(&QnAuditManager::at_connectionClosed, commonModule->auditManager(), session);
    }

    d->socket->setNonBlockingMode(true);
    auto keepAliveTimeout = std::chrono::milliseconds(remotePeer.aliveUpdateIntervalMs);
    WebSocketPtr webSocket(new websocket::WebSocket(std::move(d->socket)));
    if (keepAliveTimeout > std::chrono::milliseconds::zero())
        webSocket->setAliveTimeout(keepAliveTimeout);
    webSocket->start();

    messageBus->gotConnectionFromRemotePeer(
        remotePeer,
        std::move(connectionLockGuard),
        std::move(webSocket),
        QUrlQuery(d->request.requestLine.url.query()),
        userAccessData(remotePeer),
        onConnectionClosedCallback);
}

} // namespace p2p
} // namespace nx
