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
#include <nx/network/p2p_transport/p2p_http_server_transport.h>
#include <nx/network/p2p_transport/p2p_websocket_transport.h>

namespace nx {
namespace p2p {

using namespace nx::vms::api;
using namespace nx::network;

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
    if (remotePeer.dataFormat == Qn::UbjsonFormat)
    {
        if (nx_ec::EC2_PROTO_VERSION != remotePeer.protoVersion)
        {
            NX_WARNING(this,
                lm("Reject incoming P2P connection using UBJSON "
                    "from peer %1 because of different EC2 proto version. "
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
            NX_WARNING(this,
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

bool ConnectionProcessor::canAcceptConnection(const vms::api::PeerDataEx& remotePeer)
{
    Q_D(QnTCPConnectionProcessor);

    if (commonModule()->isStandAloneMode())
    {
        NX_DEBUG(this,
            "Incoming messageBus connections are temporary disabled. Ignore new incoming "
            "connection.");
        sendResponse(nx::network::http::StatusCode::forbidden,
            "The media server is running in standalone mode");
        return false;
    }

    if (!isPeerCompatible(remotePeer))
    {
        sendResponse(nx::network::http::StatusCode::forbidden, "Peer is not compatible");
        return false;
    }

    const auto commonModule = d->owner->commonModule();
    const auto connection = commonModule->ec2Connection();
    auto messageBus = (connection->messageBus()->dynamicCast<ServerMessageBus*>());
    if (!messageBus)
    {
        sendResponse(
            nx::network::http::StatusCode::forbidden, "The media server is not is in P2p mode");
        return false;
    }

    if (!messageBus->validateRemotePeerData(remotePeer))
    {
        sendResponse(nx::network::http::StatusCode::forbidden,
            "The media server is going to restart to replace its database");
        return false;
    }
    return true;
}

class SameGuidConnectionLocker
{
public:

    SameGuidConnectionLocker(const QnUuid& id): m_id(id)
    {
        {
            QnMutexLocker lock(&m_commonMutex);

            if (m_mutexList.find(id) == m_mutexList.cend())
            {
                m_mutex = std::make_shared<QnMutex>();
                m_mutexList[id] = std::weak_ptr<QnMutex>(m_mutex);
            }
            else
            {
                m_mutex = std::shared_ptr<QnMutex>(m_mutexList[id]);
            }
        }

        m_mutex->lock();
    }

    SameGuidConnectionLocker(SameGuidConnectionLocker&& other):
        m_id(other.m_id),
        m_mutex(std::move(other.m_mutex))
    {
    }

    SameGuidConnectionLocker& operator=(const SameGuidConnectionLocker& other) = delete;

    ~SameGuidConnectionLocker()
    {
        if (!m_mutex)
            return;

        m_mutex->unlock();

        QnMutexLocker lock(&m_commonMutex);

        if (m_mutex.use_count() == 1)
            m_mutexList.erase(m_id);
    }

private:
    QnUuid m_id;
    std::shared_ptr<QnMutex> m_mutex;
    static std::map<QnUuid, std::weak_ptr<QnMutex>> m_mutexList;
    static QnMutex m_commonMutex;
};

std::map<QnUuid, std::weak_ptr<QnMutex>> SameGuidConnectionLocker::m_mutexList{};
QnMutex SameGuidConnectionLocker::m_commonMutex;

bool ConnectionProcessor::tryAcquireConnecting(
    ec2::ConnectionLockGuard& connectionLockGuard,
    const vms::api::PeerDataEx& remotePeer)
{
    Q_D(QnTCPConnectionProcessor);
    // addition checking to prevent two connections (ingoing and outgoing) at once
    // 1-st stage
    bool lockOK = connectionLockGuard.tryAcquireConnecting();

    d->response.headers.insert(
        nx::network::http::HttpHeader(Qn::EC2_CONNECT_STAGE_1, nx::network::http::StringType()));

    if (!lockOK)
    {
        sendResponse(nx::network::http::StatusCode::forbidden,
            lm("Connection from peer %1 already established").arg(remotePeer.id).toUtf8());
        return false;
    }
    sendResponse(nx::network::http::StatusCode::noContent, nx::network::http::StringType());

    // 2-nd stage
    if (!readRequest())
        return false;
    parseRequest();
    return true;
}

bool ConnectionProcessor::tryAcquireConnected(
    ec2::ConnectionLockGuard& connectionLockGuard,
    const vms::api::PeerDataEx& remotePeer)
{
    if (!connectionLockGuard.tryAcquireConnected() ||    //< lock failed
        remotePeer.id == commonModule()->moduleGUID() || //< can't connect to itself
        isDisabledPeer(remotePeer))                      //< allowed peers are strict
    {
        sendResponse(nx::network::http::StatusCode::forbidden,
            lm("The connection from the peer %1 is already established")
                .arg(remotePeer.id)
                .toUtf8());
        return false;
    }
    return true;
}

void ConnectionProcessor::run()
{
    Q_D(QnTCPConnectionProcessor);
    initSystemThreadId();

    if (d->clientRequest.isEmpty() && !readRequest())
        return;
    parseRequest();

    vms::api::PeerDataEx remotePeer = deserializePeerData(d->request);
    if (!canAcceptConnection(remotePeer))
        return;

    auto connection = commonModule()->ec2Connection();
    auto messageBus = (connection->messageBus()->dynamicCast<ServerMessageBus*>());

    // Lock mutex to prevent processing GET and POST at the same time for the same connection.
    SameGuidConnectionLocker sameGuidconnectionLockGuard(remotePeer.connectionGuid);

    if (d->request.requestLine.method == "POST")
    {
        d->socket->setNonBlockingMode(true);
        messageBus->gotPostConnection(remotePeer, std::move(d->socket), std::move(d->requestBody));
        return;
    }

    // Strict ingoing and outgoing connections s1->s2 and s2->s1. Reject one of them.
    ec2::ConnectionLockGuard sameDirectionConnectionLockGuard(
        commonModule()->moduleGUID(),
        messageBus->connectionGuardSharedState(),
        remotePeer.id,
        ec2::ConnectionLockGuard::Direction::Incoming);

    if (remotePeer.isServer() && !tryAcquireConnecting(sameDirectionConnectionLockGuard, remotePeer))
        return;
    if (!tryAcquireConnected(sameDirectionConnectionLockGuard, remotePeer))
        return;

    bool useWebSocket = false;
    if (remotePeer.transport != P2pTransportMode::http)
    {
        auto error = websocket::validateRequest(d->request, &d->response);
        if (error != websocket::Error::noError && remotePeer.transport == P2pTransportMode::websocket)
        {
            auto errorMessage = lm("Invalid WEB socket request. Validation failed. Error: %1").arg((int)error);
            sendResponse(nx::network::http::StatusCode::forbidden, errorMessage.toUtf8());
            NX_ERROR(this, errorMessage);
            return;
        }
        useWebSocket = (error == websocket::Error::noError);
    }

    serializePeerData(d->response, localPeer(), remotePeer.dataFormat);
    sendResponse(
        useWebSocket ? http::StatusCode::switchingProtocols : http::StatusCode::ok,
        nx::network::http::StringType());

    std::function<void()> onConnectionClosedCallback;
    if (remotePeer.isClient())
    {
        auto session = authSession();
        commonModule()->auditManager()->at_connectionOpened(session);
        onConnectionClosedCallback = std::bind(&QnAuditManager::at_connectionClosed,
            commonModule()->auditManager(), session);
    }

    d->socket->setNonBlockingMode(true);
    auto keepAliveTimeout = std::chrono::milliseconds(remotePeer.aliveUpdateIntervalMs);

    P2pTransportPtr p2pTransport;
    const auto dataFormat = remotePeer.dataFormat == Qn::JsonFormat
        ? websocket::FrameType::text : websocket::FrameType::binary;

    if (useWebSocket)
    {
        p2pTransport = std::make_unique<P2PWebsocketTransport>(std::move(d->socket), dataFormat);
        p2pTransport->start();

        messageBus->gotConnectionFromRemotePeer(
            remotePeer,
            std::move(sameDirectionConnectionLockGuard),
            std::move(p2pTransport),
            QUrlQuery(d->request.requestLine.url.query()),
            userAccessData(remotePeer),
            onConnectionClosedCallback);
    }
    else
    {
        auto p2pHttpServerTransport = new P2PHttpServerTransport(
            std::move(d->socket),
            dataFormat);

        auto onGetReceivedCallback =
            [messageBus,
            remotePeer,
            sameDirectionConnectionLockGuard = std::move(sameDirectionConnectionLockGuard),
            sameGuidconnectionLockGuard = std::move(sameGuidconnectionLockGuard),
            p2pHttpServerTransport,
            query = QUrlQuery(d->request.requestLine.url.query()),
            accessData = userAccessData(remotePeer),
            onConnectionClosedCallback](
                SystemError::ErrorCode error) mutable
            {
                if (error == SystemError::noError)
                {
                    messageBus->gotConnectionFromRemotePeer(
                        remotePeer,
                        std::move(sameDirectionConnectionLockGuard),
                        P2pTransportPtr(p2pHttpServerTransport),
                        query,
                        accessData,
                        onConnectionClosedCallback);
                }
                else
                {
                    delete p2pHttpServerTransport;
                    // #TODO: #rvasilenko Implement this.
                }
            };

        p2pHttpServerTransport->start(std::move(onGetReceivedCallback));
    }
}

} // namespace p2p
} // namespace nx
