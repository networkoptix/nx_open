#include "client_message_processor.h"
#include "core/resource_management/resource_pool.h"
#include "api/app_server_connection.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/layout_resource.h"
#include "core/resource_management/resource_discovery_manager.h"
#include "utils/common/synctime.h"
#include "common/common_module.h"
#include "device_plugins/server_camera/server_camera.h"

QnClientMessageProcessor::QnClientMessageProcessor():
    base_type(),
    m_opened(false)
{
}

void QnClientMessageProcessor::init(const ec2::AbstractECConnectionPtr& connection)
{
    QnCommonMessageProcessor::init(connection);
    connect( connection.get(), &ec2::AbstractECConnection::remotePeerFound, this, &QnClientMessageProcessor::at_remotePeerFound);
    connect( connection.get(), &ec2::AbstractECConnection::remotePeerLost, this, &QnClientMessageProcessor::at_remotePeerLost);
}

void QnClientMessageProcessor::onResourceStatusChanged(const QnResourcePtr &resource, QnResource::Status status) {
    resource->setStatus(status);
    checkForTmpStatus(resource);
}

void QnClientMessageProcessor::updateResource(const QnResourcePtr &resource) {
    QnResourcePtr ownResource;

    ownResource = qnResPool->getResourceById(resource->getId());

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
    if (data.peer.id == qnCommon->moduleGUID())
        return;

    if (isProxy) {
        //updateTmpStatus(id, QnResource::NotDefined);
        return;
    }

    if (!m_opened) {
        m_opened = true;
        emit connectionOpened();
    }
}

void QnClientMessageProcessor::at_remotePeerLost(ec2::ApiPeerAliveData, bool isProxy)
{
    if (isProxy) {
        //updateTmpStatus(id, QnResource::Offline);
        return;
    }

    if (m_opened) {
        m_opened = false;
        emit connectionClosed();
        QString serverTypeName = lit("Server");
        foreach(QnResourcePtr res, qnResPool->getAllResourceByTypeName(serverTypeName))
            res->setStatus(QnResource::Offline);
        foreach(QnResourcePtr res, qnResPool->getAllCameras(QnResourcePtr()))
            res->setStatus(QnResource::Offline);
    }
}

void QnClientMessageProcessor::onGotInitialNotification(const ec2::QnFullResourceData& fullData)
{
    QnCommonMessageProcessor::onGotInitialNotification(fullData);
    QnResourceDiscoveryManager::instance()->setReady(true);
    //emit connectionOpened();
}
