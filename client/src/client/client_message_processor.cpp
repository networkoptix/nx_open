#include "client_message_processor.h"

#include "core/resource_management/resource_pool.h"
#include "api/app_server_connection.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/layout_resource.h"
#include "core/resource_management/resource_discovery_manager.h"
#include <core/resource_management/incompatible_server_watcher.h>

#include <nx_ec/ec_api.h>

#include "utils/common/synctime.h"
#include "common/common_module.h"
#include "plugins/resource/server_camera/server_camera.h"
#include "utils/network/global_module_finder.h"

#include <utils/common/app_info.h>


QnClientMessageProcessor::QnClientMessageProcessor():
    base_type(),
    m_incompatibleServerWatcher(new QnIncompatibleServerWatcher(this)),
    m_connected(false),
    m_holdConnection(false)
{
    m_status.setState(QnConnectionState::Disconnected);
}

void QnClientMessageProcessor::init(const ec2::AbstractECConnectionPtr &connection) {

    m_status.setState(connection
        ? QnConnectionState::Connecting
        : QnConnectionState::Disconnected);

    if (connection) {
        qnCommon->setRemoteGUID(QnUuid(connection->connectionInfo().ecsGuid));

    } else if (m_connected) { // double init by null is allowed
        Q_ASSERT(!qnCommon->remoteGUID().isNull());
        ec2::ApiPeerAliveData data;
        data.peer.id = qnCommon->remoteGUID();
        qnCommon->setRemoteGUID(QnUuid());
        m_connected = false;
        m_incompatibleServerWatcher->stop();
        emit connectionClosed();
    } else if (!qnCommon->remoteGUID().isNull()) { // we are trying to reconnect to server now
        qnCommon->setRemoteGUID(QnUuid());
    }

    QnCommonMessageProcessor::init(connection);
}

QnConnectionState QnClientMessageProcessor::connectionState() const {
    return m_status.state();
}

void QnClientMessageProcessor::setHoldConnection(bool holdConnection) {
    if (m_holdConnection == holdConnection)
        return;

    m_holdConnection = holdConnection;

    if (!m_holdConnection && !m_connected && !qnCommon->remoteGUID().isNull()) {
        m_incompatibleServerWatcher->stop();
        emit connectionClosed();
    }
}

void QnClientMessageProcessor::connectToConnection(const ec2::AbstractECConnectionPtr &connection) {
    base_type::connectToConnection(connection);
    connect( connection->getMiscManager(), &ec2::AbstractMiscManager::systemNameChangeRequested,
        this, &QnClientMessageProcessor::at_systemNameChangeRequested );
}

void QnClientMessageProcessor::disconnectFromConnection(const ec2::AbstractECConnectionPtr &connection) {
    base_type::disconnectFromConnection(connection);
    connection->getMiscManager()->disconnect(this);
}

void QnClientMessageProcessor::onResourceStatusChanged(const QnResourcePtr &resource, Qn::ResourceStatus status) {
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

    QnCommonMessageProcessor::updateResource(resource);

    QnResourcePtr ownResource = qnResPool->getResourceById(resource->getId());

    if (ownResource.isNull())
        qnResPool->addResource(resource);
    else
        ownResource->update(resource);

    // TODO: #Elric #2.3 don't update layout if we're re-reading resources, 
    // this leads to unsaved layouts spontaneously rolling back to last saved state.

    if (QnLayoutResourcePtr layout = ownResource.dynamicCast<QnLayoutResource>())
        layout->requestStore();
}

void QnClientMessageProcessor::resetResources(const QnResourceList& resources) {
    QnCommonMessageProcessor::resetResources(resources);
}

void QnClientMessageProcessor::handleRemotePeerFound(const ec2::ApiPeerAliveData &data) {
    base_type::handleRemotePeerFound(data);
    if (qnCommon->remoteGUID().isNull()) {
        qWarning() << "at_remotePeerFound received while disconnected";
        return;
    }

    if (data.peer.id != qnCommon->remoteGUID())
        return;

    m_status.setState(QnConnectionState::Connected);
    m_connected = true;
    emit connectionOpened();
}

void QnClientMessageProcessor::handleRemotePeerLost(const ec2::ApiPeerAliveData &data) {
    base_type::handleRemotePeerLost(data);

    if (qnCommon->remoteGUID().isNull()) {
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
    if (m_status.state() == QnConnectionState::Connecting)
        return;

    m_status.setState(QnConnectionState::Reconnecting);

    /* Mark server as offline, so user will understand why is he reconnecting. */
    QnMediaServerResourcePtr server = qnResPool->getResourceById(data.peer.id).staticCast<QnMediaServerResource>();
    if (server)
        server->setStatus(Qn::Offline);

    m_connected = false;

    if (!m_holdConnection) {
        emit connectionClosed();
        m_incompatibleServerWatcher->stop();
    }
}

void QnClientMessageProcessor::at_systemNameChangeRequested(const QString &systemName) {
    if (qnCommon->localSystemName() == systemName)
        return;

    qnCommon->setLocalSystemName(systemName);
}

void QnClientMessageProcessor::onGotInitialNotification(const ec2::QnFullResourceData& fullData)
{
    QnCommonMessageProcessor::onGotInitialNotification(fullData);
    m_incompatibleServerWatcher->stop();
    m_incompatibleServerWatcher->start();
    m_status.setState(QnConnectionState::Ready);
}
