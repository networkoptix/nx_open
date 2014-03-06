#include "client_message_processor.h"
#include "core/resource_management/resource_pool.h"
#include "api/app_server_connection.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/layout_resource.h"
#include "core/resource_management/resource_discovery_manager.h"
#include "utils/common/synctime.h"

QnClientMessageProcessor::QnClientMessageProcessor():
    base_type()
{
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
    qnLicensePool->setMainHardwareIds( QList<QByteArray>() << fullData.serverInfo.hardwareId1 << fullData.serverInfo.hardwareId2 << fullData.serverInfo.hardwareId3 );
    //qnLicensePool->setCompatibleHardwareIds(  );
}

void QnClientMessageProcessor::processCameraServerItems(const QnCameraHistoryList& cameraHistoryList)
{
    foreach(QnCameraHistoryPtr history, cameraHistoryList)
        QnCameraHistoryPool::instance()->addCameraHistory(history);
}

void QnClientMessageProcessor::onGotInitialNotification(const ec2::QnFullResourceData& fullData)
{
    updateHardwareIds(fullData);
    processResources(fullData.resources);
    processLicenses(fullData.licenses);
    processCameraServerItems(fullData.cameraHistory);

    QnResourceDiscoveryManager::instance()->setReady(true);
    qnSyncTime->reset();
    emit connectionOpened();
}
