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

void QnClientMessageProcessor::init(ec2::AbstractECConnectionPtr connection)
{
    QnCommonMessageProcessor::init(connection);
    connect( connection.get(), &ec2::AbstractECConnection::remotePeerFound, this, &QnClientMessageProcessor::at_remotePeerFound);
    connect( connection.get(), &ec2::AbstractECConnection::remotePeerLost, this, &QnClientMessageProcessor::at_remotePeerLost);
}

void QnClientMessageProcessor::onResourceStatusChanged(QnResourcePtr resource, QnResource::Status status)
{
    resource->setStatus(status);
}

void QnClientMessageProcessor::updateResource(QnResourcePtr resource)
{
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
}

void QnClientMessageProcessor::determineOptimalIF(const QnMediaServerResourcePtr &resource)
{
    // set proxy. If some media server IF will be found, proxy address will be cleared
    QString url = QnAppServerConnectionFactory::defaultUrl().host();
    if (url.isEmpty())
        url = QLatin1String("127.0.0.1");
    int port = QnAppServerConnectionFactory::defaultMediaProxyPort();
    resource->apiConnection()->setProxyAddr(resource->getApiUrl(), url, port);
    disconnect(resource.data(), NULL, this, NULL);
    connect(resource.data(), SIGNAL(serverIfFound(const QnMediaServerResourcePtr &, const QString &, const QString &)), 
             this, SLOT(at_serverIfFound(const QnMediaServerResourcePtr &, const QString &, const QString &)));
    resource->determineOptimalNetIF();
}

void QnClientMessageProcessor::processResources(const QnResourceList& resources)
{
    qnResPool->beginTran();
    foreach (const QnResourcePtr& resource, resources)
        updateResource(resource);
    qnResPool->commit();
}

void QnClientMessageProcessor::processLicenses(const QnLicenseList& licenses)
{
    qnLicensePool->replaceLicenses(licenses);
}

void QnClientMessageProcessor::updateHardwareIds(const ec2::QnFullResourceData& fullData)
{
    qnLicensePool->setMainHardwareIds(fullData.serverInfo.mainHardwareIds);
    qnLicensePool->setCompatibleHardwareIds(fullData.serverInfo.compatibleHardwareIds);
}

void QnClientMessageProcessor::processCameraServerItems(const QnCameraHistoryList& cameraHistoryList)
{
    foreach(QnCameraHistoryPtr history, cameraHistoryList)
        QnCameraHistoryPool::instance()->addCameraHistory(history);
}

void QnClientMessageProcessor::onGotInitialNotification(const ec2::QnFullResourceData& fullData)
{
    QnAppServerConnectionFactory::setBox(fullData.serverInfo.armBox);

    updateHardwareIds(fullData);
    processResources(fullData.resources);
    processLicenses(fullData.licenses);
    processCameraServerItems(fullData.cameraHistory);

    QnResourceDiscoveryManager::instance()->setReady(true);
    qnSyncTime->reset();
    //emit connectionOpened();
}

void QnClientMessageProcessor::updateTmpStatus(const QnId& id, QnResource::Status status)
{
    QnResourcePtr server = qnResPool->getResourceById(id);
    if (!server)
        return;
    foreach(QnResourcePtr res, qnResPool->getAllEnabledCameras(server)) {
        QnServerCameraPtr serverCamera = res.dynamicCast<QnServerCamera>();
        if (serverCamera)
            serverCamera->setTmpStatus(status);
    }
}

void QnClientMessageProcessor::at_remotePeerFound(QnId id, bool isClient, bool isProxy)
{
    if (isProxy) {
        updateTmpStatus(id, QnResource::NotDefined);
        return;
    }

    if (!m_opened) {
        m_opened = true;
        emit connectionOpened();
    }
}

void QnClientMessageProcessor::at_remotePeerLost(QnId id, bool isClient, bool isProxy)
{
    if (isProxy) {
        updateTmpStatus(id, QnResource::Offline);
        return;
    }

    if (m_opened) {
        m_opened = false;
        emit connectionClosed();
        foreach(QnResourcePtr res, qnResPool->getAllResourceByTypeName(lit("Server")))
            res->setStatus(QnResource::Offline);
        foreach(QnResourcePtr res, qnResPool->getAllEnabledCameras())
            res->setStatus(QnResource::Offline);
    }
}
