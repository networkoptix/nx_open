#include "client_message_processor.h"
#include "core/resource_management/resource_pool.h"
#include "api/app_server_connection.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/layout_resource.h"
#include "core/resource_management/resource_discovery_manager.h"
#include <core/resource_management/incompatible_server_watcher.h>
#include "utils/common/synctime.h"
#include "common/common_module.h"
#include "plugins/resource/server_camera/server_camera.h"
#include "utils/network/global_module_finder.h"

#include <utils/common/app_info.h>


QnClientMessageProcessor::QnClientMessageProcessor():
    base_type(),
    m_connected(false),
    m_holdConnection(false)
{
}

void QnClientMessageProcessor::init(const ec2::AbstractECConnectionPtr& connection)
{
    QnCommonMessageProcessor::init(connection);
    if (connection) {
        assert(!m_connected);
        assert(qnCommon->remoteGUID().isNull());
        qnCommon->setRemoteGUID(QnUuid(connection->connectionInfo().ecsGuid));
        connect( connection, &ec2::AbstractECConnection::remotePeerFound, this, &QnClientMessageProcessor::at_remotePeerFound);
        connect( connection, &ec2::AbstractECConnection::remotePeerLost, this, &QnClientMessageProcessor::at_remotePeerLost);
        connect( connection->getMiscManager(), &ec2::AbstractMiscManager::systemNameChangeRequested,
                 this, &QnClientMessageProcessor::at_systemNameChangeRequested );
    } else if (m_connected) { // double init by null is allowed
        assert(!qnCommon->remoteGUID().isNull());
        ec2::ApiPeerAliveData data;
        data.peer.id = qnCommon->remoteGUID();
        qnCommon->setRemoteGUID(QnUuid());
        m_connected = false;
        emit connectionClosed();
    } else if (!qnCommon->remoteGUID().isNull()) { // we are trying to reconnect to server now
        qnCommon->setRemoteGUID(QnUuid());
    }
}

void QnClientMessageProcessor::setHoldConnection(bool holdConnection) {
    if (m_holdConnection == holdConnection)
        return;

    m_holdConnection = holdConnection;

    if (!m_holdConnection && !m_connected && !qnCommon->remoteGUID().isNull())
        emit connectionClosed();
}

void QnClientMessageProcessor::onResourceStatusChanged(const QnResourcePtr &resource, Qn::ResourceStatus status) {
    resource->setStatus(status);
    checkForTmpStatus(resource);
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

    if (ownResource.isNull()) {
        qnResPool->addResource(resource);
    } else {
        ownResource->update(resource);

        if (QnMediaServerResourcePtr mediaServer = ownResource.dynamicCast<QnMediaServerResource>()) {
            /* Handling a case when an incompatible server is changing its systemName at runtime and becoming compatible. */
            if (resource->getStatus() == Qn::NotDefined && mediaServer->getStatus() == Qn::Incompatible) {
                if (QnMediaServerResourcePtr updatedServer = resource.dynamicCast<QnMediaServerResource>()) {
                    if (isCompatible(qnCommon->engineVersion(), updatedServer->getVersion()))
                        mediaServer->setStatus(Qn::Online);
                }
            }
        }
    }

    // TODO: #Elric #2.3 don't update layout if we're re-reading resources, 
    // this leads to unsaved layouts spontaneously rolling back to last saved state.

    if (QnLayoutResourcePtr layout = ownResource.dynamicCast<QnLayoutResource>())
        layout->requestStore();

    checkForTmpStatus(ownResource);
}

void QnClientMessageProcessor::processResources(const QnResourceList& resources)
{
    QnCommonMessageProcessor::processResources(resources);
    foreach(const QnResourcePtr& resource, resources)
        checkForTmpStatus(resource);
}

void QnClientMessageProcessor::checkForTmpStatus(const QnResourcePtr& resource)
{
    // process tmp status
    
    if (QnMediaServerResourcePtr mediaServer = resource.dynamicCast<QnMediaServerResource>()) 
    {
        if (mediaServer->getStatus() == Qn::NotDefined)
            return;
        else if (mediaServer->getStatus() == Qn::Offline)
            updateServerTmpStatus(mediaServer->getId(), Qn::Offline);
        else
            updateServerTmpStatus(mediaServer->getId(), Qn::NotDefined);
    }
    else if (QnServerCameraPtr serverCamera = resource.dynamicCast<QnServerCamera>()) 
    {
        QnMediaServerResourcePtr mediaServer = qnResPool->getResourceById(serverCamera->getParentId()).dynamicCast<QnMediaServerResource>();
        if (mediaServer) {
            if (mediaServer->getStatus() ==Qn::Offline)
                serverCamera->setTmpStatus(Qn::Offline);
            else
                serverCamera->setTmpStatus(Qn::NotDefined);
        }
    }
}

void QnClientMessageProcessor::updateServerTmpStatus(const QnUuid& id, Qn::ResourceStatus status)
{
    QnResourcePtr server = qnResPool->getResourceById(id);
    if (!server)
        return;
    foreach(QnResourcePtr res, qnResPool->getAllCameras(server)) {
        QnServerCameraPtr serverCamera = res.dynamicCast<QnServerCamera>();
        if (serverCamera)
            serverCamera->setTmpStatus(status);
    }
}

void QnClientMessageProcessor::at_remotePeerFound(ec2::ApiPeerAliveData data)
{
    if (qnCommon->remoteGUID().isNull()) {
        qWarning() << "at_remotePeerFound received while disconnected";
        return;
    }

    if (data.peer.id != qnCommon->remoteGUID())
        return;

    assert(!m_connected);
    
    m_connected = true;
    emit connectionOpened();
}

void QnClientMessageProcessor::at_remotePeerLost(ec2::ApiPeerAliveData data)
{
    if (qnCommon->remoteGUID().isNull()) {
        qWarning() << "at_remotePeerLost received while disconnected";
        return;
    }

    QnMediaServerResourcePtr server = qnResPool->getResourceById(data.peer.id).staticCast<QnMediaServerResource>();
    if (server)
        server->setStatus(Qn::Offline);

    if (data.peer.id != qnCommon->remoteGUID())
        return;


    Q_ASSERT_X(m_connected, Q_FUNC_INFO, "m_connected");

    m_connected = false;

    if (!m_holdConnection)
        emit connectionClosed();
}

void QnClientMessageProcessor::at_systemNameChangeRequested(const QString &systemName) {
    if (qnCommon->localSystemName() == systemName)
        return;

    qnCommon->setLocalSystemName(systemName);
}

void QnClientMessageProcessor::onGotInitialNotification(const ec2::QnFullResourceData& fullData)
{
    QnCommonMessageProcessor::onGotInitialNotification(fullData);
    m_incompatibleServerWatcher.reset(new QnIncompatibleServerWatcher(this));
}
