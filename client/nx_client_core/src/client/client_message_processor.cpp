#include "client_message_processor.h"

#include <api/app_server_connection.h>
#include <api/global_settings.h>

#include <client_core/client_core_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/layout_tour_state_manager.h>

#include <core/resource/media_server_resource.h>
#include <core/resource/layout_resource.h>

#include <nx_ec/ec_api.h>
#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>

#include "utils/common/synctime.h"
#include "common/common_module.h"

#include <utils/common/app_info.h>

#include <nx/utils/log/log.h>

QnClientMessageProcessor::QnClientMessageProcessor(QObject* parent):
    base_type(parent),
    m_status(),
    m_connected(false),
    m_holdConnection(false)
{
}

void QnClientMessageProcessor::init(const ec2::AbstractECConnectionPtr &connection)
{
    if (connection)
    {
        if (m_status.state() != QnConnectionState::Reconnecting)
        {
            NX_DEBUG(this, "init, state -> Connecting");
            m_status.setState(QnConnectionState::Connecting);
        }
    }
    else
    {
        NX_DEBUG(this, lit("init NULL, state -> Disconnected"));
        m_status.setState(QnConnectionState::Disconnected);
    }


    if (connection)
    {
        auto info = connection->connectionInfo();
        const auto serverId = info.serverId();
        NX_DEBUG(this, lit("Connection established to %1").arg(serverId.toString()));

        /*
         * For cloud connections we can get here url, containing only system id.
         * In that case each request may potentially be sent to another server, which may lead
         * to undefined behavior. So we are fixing server which we were connected to in the url.
         */
        auto currentUrl = info.ecUrl;
        QString host = currentUrl.host();
        if (nx::network::SocketGlobals::addressResolver().isCloudHostName(host))
        {
            const auto fullHost = serverId.toSimpleString() + L'.' + info.cloudSystemId;
            NX_EXPECT(nx::network::SocketGlobals::addressResolver().isCloudHostName(fullHost));
            if (host != fullHost)
            {
                NX_DEBUG(this, lit("Url fixed from %1 to %2").arg(host).arg(fullHost));
                NX_EXPECT(false, "Correct url must be filled in ecUrl");
                currentUrl.setHost(fullHost);
                connection->updateConnectionUrl(currentUrl);
            }
        }

        commonModule()->setRemoteGUID(serverId);
    }
    else if (m_connected)
    { // double init by null is allowed
        NX_ASSERT(!commonModule()->remoteGUID().isNull());
        ec2::ApiPeerAliveData data;
        data.peer.id = commonModule()->remoteGUID();
        commonModule()->setRemoteGUID(QnUuid());
        m_connected = false;
        NX_DEBUG(this, "Deinit while connected, connection closed");
        emit connectionClosed();
    }
    else if (!commonModule()->remoteGUID().isNull())
    { // we are trying to reconnect to server now
        commonModule()->setRemoteGUID(QnUuid());
    }

    QnCommonMessageProcessor::init(connection);
}

const QnClientConnectionStatus* QnClientMessageProcessor::connectionStatus() const
{
    return &m_status;
}

void QnClientMessageProcessor::setHoldConnection(bool holdConnection)
{
    NX_DEBUG(this, lm("Hold connection set to %1").arg(holdConnection));
    if (m_holdConnection == holdConnection)
        return;

    m_holdConnection = holdConnection;

    if (!m_holdConnection && !m_connected && !commonModule()->remoteGUID().isNull())
    {
        NX_DEBUG(this, "Unholding connection while disconnected, connection closed");
        emit connectionClosed();
    }
}

void QnClientMessageProcessor::connectToConnection(const ec2::AbstractECConnectionPtr &connection)
{
    base_type::connectToConnection(connection);
}

void QnClientMessageProcessor::disconnectFromConnection(const ec2::AbstractECConnectionPtr &connection)
{
    base_type::disconnectFromConnection(connection);
    connection->getMiscNotificationManager()->disconnect(this);
}

void QnClientMessageProcessor::handleTourAddedOrUpdated(const ec2::ApiLayoutTourData& tour)
{
    if (qnClientCoreModule->layoutTourStateManager()->isChanged(tour.id))
        return;

    base_type::handleTourAddedOrUpdated(tour);
}

void QnClientMessageProcessor::onResourceStatusChanged(
    const QnResourcePtr &resource,
    Qn::ResourceStatus status,
    ec2::NotificationSource /*source*/)
{
    resource->setStatus(status);
}

void QnClientMessageProcessor::updateResource(const QnResourcePtr &resource, ec2::NotificationSource source)
{
    NX_ASSERT(resource);
    /*
     * In rare cases we can receive updateResource call when the client is
     * in the reconnect process (it starts an event loop inside the main
     * thread). Populating the resource pool or changing resources is highly
     * inappropriate at that time.
     * */
    if (!m_connection || !resource)
        return;

    auto isFile = [](const QnResourcePtr &resource)
    {
        QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>();
        return layout && layout->isFile();
    };

    QnResourcePtr ownResource = resourcePool()->getResourceById(resource->getId());

    /* Security check. Local layouts must not be overridden by server's.
    * Really that means GUID conflict, caused by saving of local layouts to server. */
    if (isFile(resource) || isFile(ownResource))
        return;

    resource->addFlags(Qn::remote);
    resource->removeFlags(Qn::local);

    QnCommonMessageProcessor::updateResource(resource, source);
    if (!ownResource)
    {
        resourcePool()->addResource(resource);
    }
    else
    {
        // TODO: #GDM don't update layout if we're re-reading resources,
        // this leads to unsaved layouts spontaneously rolling back to last saved state.
        // Solution is to move 'saved/modified' flags to Qn::ResourceFlags. VMS-3180
        ownResource->update(resource);
        if (QnLayoutResourcePtr layout = ownResource.dynamicCast<QnLayoutResource>())
            layout->requestStore();
    }
}

void QnClientMessageProcessor::handleRemotePeerFound(QnUuid peer, Qn::PeerType peerType)
{
    base_type::handleRemotePeerFound(peer, peerType);

    /*
     * Avoiding multiple connectionOpened() if client was on breakpoint or
     * if ec connection settings were changed.
     */
    if (m_connected)
        return;

    if (commonModule()->remoteGUID().isNull())
    {
        NX_WARNING(this, "Remote peer found while disconnected");
        return;
    }

    if (peer != commonModule()->remoteGUID())
        return;

    NX_DEBUG(this, lit("peer found, state -> Connected"));
    m_status.setState(QnConnectionState::Connected);
    m_connected = true;
    NX_DEBUG(this, "Remote peer found, connection opened");
    emit connectionOpened();
}

void QnClientMessageProcessor::handleRemotePeerLost(QnUuid peer, Qn::PeerType peerType)
{
    base_type::handleRemotePeerLost(peer, peerType);

    if (commonModule()->remoteGUID().isNull())
    {
        NX_WARNING(this, "Remote peer lost while disconnected");
        return;
    }

    if (peer != commonModule()->remoteGUID())
        return;

    /*
        RemotePeerLost signal during connect is perfectly OK. We are receiving old notifications
        that were not sent as TransactionMessageBus was stopped. Peer id is the same if we are
        connecting to the same server we are already connected to (and just disconnected).
    */
    if (!m_connected)
        return;

    NX_DEBUG(this, lit("peer lost, state -> Reconnecting"));
    m_status.setState(QnConnectionState::Reconnecting);

    /* Mark server as offline, so user will understand why is he reconnecting. */
    if (auto server = commonModule()->currentServer())
        server->setStatus(Qn::Offline);

    m_connected = false;

    if (!m_holdConnection)
    {
        NX_DEBUG(this, "Connection closed");
        emit connectionClosed();
    }
    else
    {
        NX_DEBUG(this, "Holding connection...");
    }
}

void QnClientMessageProcessor::onGotInitialNotification(const ec2::ApiFullInfoData& fullData)
{
    NX_DEBUG(this, lit("resources received, state -> Ready"));
    QnCommonMessageProcessor::onGotInitialNotification(fullData);
    m_status.setState(QnConnectionState::Ready);
    NX_DEBUG(this, lit("Received initial notification while connected to %1")
        .arg(commonModule()->remoteGUID().toString()));
    NX_EXPECT(commonModule()->currentServer());

    /* Get server time as soon as we setup connection. */
    qnSyncTime->currentMSecsSinceEpoch();
}
