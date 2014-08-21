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
        qnCommon->setRemoteGUID(QUuid(connection->connectionInfo().ecsGuid));
        connect( connection.get(), &ec2::AbstractECConnection::remotePeerFound, this, &QnClientMessageProcessor::at_remotePeerFound);
        connect( connection.get(), &ec2::AbstractECConnection::remotePeerLost, this, &QnClientMessageProcessor::at_remotePeerLost);
        connect( connection->getMiscManager().get(), &ec2::AbstractMiscManager::systemNameChangeRequested,
                 this, &QnClientMessageProcessor::at_systemNameChangeRequested );
    } else if (m_connected) { // double init by null is allowed
        assert(!qnCommon->remoteGUID().isNull());
        ec2::ApiPeerAliveData data;
        data.peer.id = qnCommon->remoteGUID();
        qnCommon->setRemoteGUID(QUuid());
        m_connected = false;
        emit connectionClosed();
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

    QnResourcePtr ownResource;

    ownResource = qnResPool->getIncompatibleResourceById(resource->getId(), true);

    // Use discovery information to update offline servers. They may be just incompatible.
    if (resource->getStatus() == Qn::Offline) {
        QnModuleInformation moduleInformation = QnGlobalModuleFinder::instance()->moduleInformation(resource->getId());
        if (!moduleInformation.id.isNull() && !moduleInformation.isCompatibleToCurrentSystem()) {
            if (QnMediaServerResourcePtr mediaServer = resource.dynamicCast<QnMediaServerResource>()) {
                mediaServer->setVersion(moduleInformation.version);
                mediaServer->setSystemInfo(moduleInformation.systemInformation);
                mediaServer->setSystemName(moduleInformation.systemName);
                mediaServer->setStatus(Qn::Incompatible);
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
        QnMediaServerResourcePtr mediaServer = ownResource.dynamicCast<QnMediaServerResource>();
        if (mediaServer)
            mserverStatusChanged = ownResource->getStatus() != resource->getStatus();

        // move incompatible resource to the main pool if it became normal
        if (ownResource && ownResource->getStatus() == Qn::Incompatible && resource->getStatus() != Qn::Incompatible)
            qnResPool->makeResourceNormal(ownResource);

        ownResource->update(resource);

        if (mserverStatusChanged && mediaServer)
            determineOptimalIF(mediaServer);
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
        if (mediaServer->getStatus() == Qn::Offline)
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

void QnClientMessageProcessor::determineOptimalIF(const QnMediaServerResourcePtr &resource)
{
    // set proxy. If some servers IF will be found, proxy address will be cleared
    const QString& proxyAddr = QnAppServerConnectionFactory::url().host();
    resource->apiConnection()->setProxyAddr(
        resource->getApiUrl(),
        proxyAddr,
        QnAppServerConnectionFactory::url().port() );    //starting with 2.3 proxy embedded to Server
    disconnect(resource.data(), NULL, this, NULL);
    resource->determineOptimalNetIF();
}

void QnClientMessageProcessor::updateServerTmpStatus(const QUuid& id, Qn::ResourceStatus status)
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
    if (m_incompatibleServerAdder) {
        delete m_incompatibleServerAdder;
        m_incompatibleServerAdder = 0;
    }

    QnCommonMessageProcessor::onGotInitialNotification(fullData);
    m_incompatibleServerAdder = new QnIncompatibleServerAdder(this);
}
