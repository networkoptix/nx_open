#include "client_message_processor.h"

#include <api/app_server_connection.h>
#include <api/global_settings.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_discovery_manager.h>

#include <nx_ec/ec_api.h>

#include "utils/common/synctime.h"
#include "common/common_module.h"

#include <utils/common/app_info.h>

#include <nx/utils/log/log.h>

namespace {

void trace(const QString& message)
{
    qDebug() << "QnClientMessageProcessor: " << message;
    //NX_LOG(lit("QnClientMessageProcessor: ") + message), cl_logDEBUG1);
}

}

QnClientMessageProcessor::QnClientMessageProcessor()
    :
    base_type(),

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
            trace(lit("init, state -> Connecting"));
            m_status.setState(QnConnectionState::Connecting);
        }
    }
    else
    {
        trace(lit("init NULL, state -> Disconnected"));
        m_status.setState(QnConnectionState::Disconnected);
    }


    if (connection)
    {
        trace(lit("Connection established to %1").arg(connection->connectionInfo().ecsGuid));
        qnCommon->setRemoteGUID(QnUuid(connection->connectionInfo().ecsGuid));
        //TODO: #GDM #3.0 in case of cloud sockets we need to modify QnAppServerConnectionFactory::url() - add server id before cloud id
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

const QnClientConnectionStatus* QnClientMessageProcessor::connectionStatus() const
{
    return &m_status;
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

    QnResourcePtr ownResource = qnResPool->getResourceById(resource->getId());

    /* Security check. Local layouts must not be overridden by server's.
    * Really that means GUID conflict, caused by saving of local layouts to server. */
    if (isFile(resource) || isFile(ownResource))
        return;

    resource->addFlags(Qn::remote);
    resource->removeFlags(Qn::local);

    QnCommonMessageProcessor::updateResource(resource);
    if (!ownResource)
    {
        qnResPool->addResource(resource);
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

void QnClientMessageProcessor::handleRemotePeerFound(const ec2::ApiPeerAliveData &data)
{
    base_type::handleRemotePeerFound(data);

    /*
     * Avoiding multiple connectionOpened() if client was on breakpoint or
     * if ec connection settings were changed.
     */
    if (m_connected)
        return;

    if (qnCommon->remoteGUID().isNull())
    {
        qWarning() << "at_remotePeerFound received while disconnected";
        return;
    }

    if (data.peer.id != qnCommon->remoteGUID())
        return;

    trace(lit("peer found, state -> Connected"));
    m_status.setState(QnConnectionState::Connected);
    m_connected = true;
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
    if (!m_connected)
        return;

    trace(lit("peer lost, state -> Reconnecting"));
    m_status.setState(QnConnectionState::Reconnecting);

    /* Mark server as offline, so user will understand why is he reconnecting. */
    if (auto server = qnCommon->currentServer())
        server->setStatus(Qn::Offline);

    m_connected = false;

    if (!m_holdConnection)
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
    trace(lit("resources received, state -> Ready"));
    QnCommonMessageProcessor::onGotInitialNotification(fullData);
    m_status.setState(QnConnectionState::Ready);
    trace(lit("Received initial notification while connected to %1")
        .arg(qnCommon->remoteGUID().toString()));
    NX_EXPECT(qnCommon->currentServer());

    /* Get server time as soon as we setup connection. */
    qnSyncTime->currentMSecsSinceEpoch();
}
