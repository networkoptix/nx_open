#include "client_message_processor.h"
#include "core/resource_management/resource_pool.h"
#include "api/app_server_connection.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/layout_resource.h"
#include "core/resource_management/resource_discovery_manager.h"
#include "utils/common/synctime.h"
#include "common/common_module.h"
#include "plugins/resource/server_camera/server_camera.h"
#include "utils/incompatible_server_adder.h"
#include "utils/network/global_module_finder.h"

#include "version.h"


QnClientMessageProcessor::QnClientMessageProcessor():
    base_type(),
    m_incompatibleServerAdder(NULL),
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
        qnCommon->setRemoteGUID(connection->connectionInfo().ecsGuid);
        connect( connection.get(), &ec2::AbstractECConnection::remotePeerFound, this, &QnClientMessageProcessor::at_remotePeerFound);
        connect( connection.get(), &ec2::AbstractECConnection::remotePeerLost, this, &QnClientMessageProcessor::at_remotePeerLost);
        connect( connection->getMiscManager().get(), &ec2::AbstractMiscManager::systemNameChangeRequested,
                 this, &QnClientMessageProcessor::at_systemNameChangeRequested );
    } else if (m_connected) { // double init by null is allowed
        assert(!qnCommon->remoteGUID().isNull());
        ec2::ApiPeerAliveData data;
        data.peer.id = qnCommon->remoteGUID();
        at_remotePeerLost(data, false);
        qnCommon->setRemoteGUID(QUuid());
    } else if (!qnCommon->remoteGUID().isNull()) { // we are trying to reconnect to server now
        qnCommon->setRemoteGUID(QUuid());
    }
}

void QnClientMessageProcessor::setHoldConnection(bool holdConnection) {
    if (m_holdConnection == holdConnection)
        return;

    m_holdConnection = holdConnection;

    if (!m_holdConnection && !m_connected && !qnCommon->remoteGUID().isNull())
        emit connectionClosed();
}

void QnClientMessageProcessor::onResourceStatusChanged(const QnResourcePtr &resource, QnResource::Status status) {
    resource->setStatus(status);
    checkForTmpStatus(resource);
}

void QnClientMessageProcessor::updateResource(const QnResourcePtr &resource) {
    QnResourcePtr ownResource;

    ownResource = qnResPool->getIncompatibleResourceById(resource->getId(), true);

    // Use discovery information to update offline servers. They may be just incompatible.
    if (resource->getStatus() == QnResource::Offline) {
        QnModuleInformation moduleInformation = QnGlobalModuleFinder::instance()->moduleInformation(resource->getId());
        if (!moduleInformation.id.isNull()) {
            if (QnMediaServerResourcePtr mediaServer = resource.dynamicCast<QnMediaServerResource>()) {
                mediaServer->setVersion(moduleInformation.version);
                mediaServer->setSystemInfo(moduleInformation.systemInformation);
                mediaServer->setSystemName(moduleInformation.systemName);
                if (moduleInformation.systemName != qnCommon->localSystemName() || moduleInformation.version != qnCommon->engineVersion())
                    mediaServer->setStatus(QnResource::Incompatible);
            }
        }
    }

    if (ownResource.isNull()) {
        qnResPool->addResource(resource);
        if (QnMediaServerResourcePtr mediaServer = resource.dynamicCast<QnMediaServerResource>())
            determineOptimalIF(mediaServer);
    }
    else {
        bool mserverStatusChanged = false;
        bool compatibleStatusChanged = false;
        QnMediaServerResourcePtr mediaServer = ownResource.dynamicCast<QnMediaServerResource>();
        if (mediaServer) {
            mserverStatusChanged = ownResource->getStatus() != resource->getStatus();
            compatibleStatusChanged = mserverStatusChanged && (ownResource->getStatus() == QnResource::Incompatible || resource->getStatus() == QnResource::Incompatible);
        }

        // move incompatible resource to the main pool if it became normal
        if (ownResource && ownResource->getStatus() == QnResource::Incompatible && resource->getStatus() != QnResource::Incompatible)
            qnResPool->makeResourceNormal(ownResource);

        ownResource->update(resource);

        if (mserverStatusChanged && mediaServer)
            determineOptimalIF(mediaServer);

        // move server into the other subtree if compatibility has been changed
        if (compatibleStatusChanged && mediaServer)
            mediaServer->parentIdChanged(mediaServer);
    }

    // TODO: #Elric #2.3 don't update layout if we're re-reading resources, 
    // this leads to unsaved layouts spontaneously rolling back to last saved state.

    if (QnLayoutResourcePtr layout = ownResource.dynamicCast<QnLayoutResource>())
        layout->requestStore();

    checkForTmpStatus(resource);
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
        if (mediaServer->getStatus() == QnResource::Offline)
            updateServerTmpStatus(mediaServer->getId(), QnResource::Offline);
        else
            updateServerTmpStatus(mediaServer->getId(), QnResource::NotDefined);
    }
    else if (QnServerCameraPtr serverCamera = resource.dynamicCast<QnServerCamera>()) 
    {
        QnMediaServerResourcePtr mediaServer = qnResPool->getResourceById(serverCamera->getParentId()).dynamicCast<QnMediaServerResource>();
        if (mediaServer) {
            if (mediaServer->getStatus() ==QnResource::Offline)
                serverCamera->setTmpStatus(QnResource::Offline);
            else
                serverCamera->setTmpStatus(QnResource::NotDefined);
        }
    }
}

void QnClientMessageProcessor::determineOptimalIF(const QnMediaServerResourcePtr &resource)
{
    // set proxy. If some media server IF will be found, proxy address will be cleared
    const QString& proxyAddr = QnAppServerConnectionFactory::defaultUrl().host();
    resource->apiConnection()->setProxyAddr(
        resource->getApiUrl(),
        proxyAddr,
        QnAppServerConnectionFactory::defaultUrl().port() );    //starting with 2.3 proxy embedded to EC
    disconnect(resource.data(), NULL, this, NULL);
    resource->determineOptimalNetIF();
}

void QnClientMessageProcessor::updateServerTmpStatus(const QnId& id, QnResource::Status status)
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

void QnClientMessageProcessor::at_remotePeerFound(ec2::ApiPeerAliveData data, bool isProxy)
{
    if (qnCommon->remoteGUID().isNull()) {
        qWarning() << "at_remotePeerFound received while disconnected";
        return;
    }

    if (data.peer.id != qnCommon->remoteGUID())
        return;

    assert(!isProxy);
    assert(!m_connected);
    
    m_connected = true;
    emit connectionOpened();
}

void QnClientMessageProcessor::at_remotePeerLost(ec2::ApiPeerAliveData data, bool isProxy)
{
    if (qnCommon->remoteGUID().isNull()) {
        qWarning() << "at_remotePeerLost received while disconnected";
        return;
    }

    QnMediaServerResourcePtr server = qnResPool->getResourceById(data.peer.id).staticCast<QnMediaServerResource>();
    if (server)
        server->setStatus(QnResource::Offline);

    if (data.peer.id != qnCommon->remoteGUID())
        return;


    Q_ASSERT_X(!isProxy, Q_FUNC_INFO, "!isProxy");
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
    if (m_incompatibleServerAdder) {
        delete m_incompatibleServerAdder;
        m_incompatibleServerAdder = 0;
    }

    QnCommonMessageProcessor::onGotInitialNotification(fullData);
    QnResourceDiscoveryManager::instance()->setReady(true);
    //emit connectionOpened();

    m_incompatibleServerAdder = new QnIncompatibleServerAdder(this);
}
