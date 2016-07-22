#include "client_message_processor.h"

#include <api/app_server_connection.h>
#include <api/global_settings.h>

#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/user_resource.h"
#include "core/resource/layout_resource.h"
#include "core/resource_management/resource_discovery_manager.h"

#include <nx_ec/ec_api.h>

#include "utils/common/synctime.h"
#include "common/common_module.h"

#include <utils/common/app_info.h>


QnClientMessageProcessor::QnClientMessageProcessor()
    :
    base_type(),

    m_status(),
    m_connected(false),
    m_holdConnection(false),
    m_waitingForPeerReconnect(false)
{
    /*
     * On changing ec2 settings qnTransactionMessageBus reconnects all peers, therefore
     * disconnecting us from server. After that 'Reconnect' dialog appears and reconnects us.
     * This leads to unnecessary connectionClosed/connectionOpened sequence and closing
     * all state-dependent dialogs and notifications.
     * This workaround depends on fact that qnTransactionMessageBus works in own thread and
     * connects to ec2ConnectionSettingsChanged via queued connection.
     */
    connect(qnGlobalSettings, &QnGlobalSettings::ec2ConnectionSettingsChanged, this, [this]()
    {
        //TODO: #gdm #3.0 improve dependency logic
        if (!m_connected)
            return;

        if (m_waitingForPeerReconnect)
            return;

        m_waitingForPeerReconnect = true;
    });


}

void QnClientMessageProcessor::init(const ec2::AbstractECConnectionPtr &connection)
{
    setConnectionState(connection
        ? QnConnectionState::Connecting
        : QnConnectionState::Disconnected);

    if (connection)
    {
        qnCommon->setRemoteGUID(QnUuid(connection->connectionInfo().ecsGuid));
        //TODO: #GDM in case of cloud sockets we need to modify QnAppServerConnectionFactory::url() - add server id before cloud id
    }
    else if (m_connected)
    { // double init by null is allowed
        NX_ASSERT(!qnCommon->remoteGUID().isNull());
        ec2::ApiPeerAliveData data;
        data.peer.id = qnCommon->remoteGUID();
        qnCommon->setRemoteGUID(QnUuid());
        m_connected = false;
        emit connectionClosed();
    }
    else if (!qnCommon->remoteGUID().isNull())
    { // we are trying to reconnect to server now
        qnCommon->setRemoteGUID(QnUuid());
    }

    QnCommonMessageProcessor::init(connection);
}

QnConnectionState QnClientMessageProcessor::connectionState() const
{
    return m_status.state();
}

void QnClientMessageProcessor::setConnectionState(QnConnectionState state)
{
    if (m_status.state() == state)
        return;

    m_status.setState(state);
    emit connectionStateChanged();
}

void QnClientMessageProcessor::setHoldConnection(bool holdConnection)
{
    if (m_holdConnection == holdConnection)
        return;

    m_holdConnection = holdConnection;

    if (!m_holdConnection && !m_connected && !qnCommon->remoteGUID().isNull())
        emit connectionClosed();
}

void QnClientMessageProcessor::connectToConnection(const ec2::AbstractECConnectionPtr &connection)
{
    base_type::connectToConnection(connection);
    connect(connection->getMiscNotificationManager(), &ec2::AbstractMiscNotificationManager::systemNameChangeRequested,
        this, &QnClientMessageProcessor::at_systemNameChangeRequested);
}

void QnClientMessageProcessor::disconnectFromConnection(const ec2::AbstractECConnectionPtr &connection)
{
    base_type::disconnectFromConnection(connection);
    connection->getMiscNotificationManager()->disconnect(this);
}

void QnClientMessageProcessor::onResourceStatusChanged(const QnResourcePtr &resource, Qn::ResourceStatus status)
{
    resource->setStatus(status);
}

void QnClientMessageProcessor::updateResource(const QnResourcePtr &resource)
{
    /*
     * In rare cases we can receive updateResource call when the client is
     * in the reconnect process (it starts an event loop inside the main
     * thread). Populating the resource pool or changing resources is highly
     * inappropriate at that time.
     * */
    if (!m_connection)
        return;

    resource->addFlags(Qn::remote);
    resource->removeFlags(Qn::local);
    QnCommonMessageProcessor::updateResource(resource);

    QnResourcePtr ownResource = qnResPool->getResourceById(resource->getId());

    if (!ownResource)
    {
        qnResPool->addResource(resource);
    }
    else
    {
        // TODO: #GDM #high #3.0 don't update layout if we're re-reading resources,
        // this leads to unsaved layouts spontaneously rolling back to last saved state.
        // This will make work very unstable when admin modifies our access rights.
        // Solution is to move 'saved/modified' flags to Qn::ResourceFlags. VMS-3180
        QnLayoutResourcePtr layout = ownResource.dynamicCast<QnLayoutResource>();
        if (layout && layout->isFile())
        {
            /* Intentionally do nothing. Local layouts must not be overridden by server's.
             * Really that means GUID conflict, caused by saving of local layouts to server. */
        }
        else
        {
            ownResource->update(resource);
            if (layout)
                layout->requestStore();
        }
    }

    if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
    {
        if (user->isOwner())
            qnCommon->updateModuleInformation();
    }
}

void QnClientMessageProcessor::handleRemotePeerFound(const ec2::ApiPeerAliveData &data)
{
    base_type::handleRemotePeerFound(data);
    if (qnCommon->remoteGUID().isNull())
    {
        qWarning() << "at_remotePeerFound received while disconnected";
        return;
    }

    if (data.peer.id != qnCommon->remoteGUID())
        return;

    setConnectionState(QnConnectionState::Connected);
    m_connected = true;

    if (m_waitingForPeerReconnect)
        m_waitingForPeerReconnect = false;
    else
        emit connectionOpened();
}

void QnClientMessageProcessor::handleRemotePeerLost(const ec2::ApiPeerAliveData &data)
{
    base_type::handleRemotePeerLost(data);

    if (qnCommon->remoteGUID().isNull())
    {
        qWarning() << "at_remotePeerLost received while disconnected";
        return;
    }

    if (data.peer.id != qnCommon->remoteGUID())
        return;

    /*
        RemotePeerLost signal during connect is perfectly OK. We are receiving old notifications
        that were not sent as TransactionMessageBus was stopped. Peer id is the same if we are
        connecting to the same server we are already connected to (and just disconnected).
    */
    if (connectionState() == QnConnectionState::Connecting)
        return;

    setConnectionState(QnConnectionState::Reconnecting);

    /* Mark server as offline, so user will understand why is he reconnecting. */
    QnMediaServerResourcePtr server = qnResPool->getResourceById(data.peer.id).staticCast<QnMediaServerResource>();
    if (server)
        server->setStatus(Qn::Offline);

    m_connected = false;

    if (!m_holdConnection && !m_waitingForPeerReconnect)
        emit connectionClosed();
}

void QnClientMessageProcessor::at_systemNameChangeRequested(const QString &systemName)
{
    if (qnCommon->localSystemName() == systemName)
        return;

    qnCommon->setLocalSystemName(systemName);
}

void QnClientMessageProcessor::onGotInitialNotification(const ec2::ApiFullInfoData& fullData)
{
    QnCommonMessageProcessor::onGotInitialNotification(fullData);
    setConnectionState(QnConnectionState::Ready);

    /* Get server time as soon as we setup connection. */
    qnSyncTime->currentMSecsSinceEpoch();

    qnCommon->updateModuleInformation();
}
