#include "client_message_processor.h"

#include <QtCore/QTimer>
#include <QtCore/QDebug>
#include <QtCore/QtGlobal>
#include <QtCore/QThread>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

#include <api/message_source.h>
#include <api/app_server_connection.h>

#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/layout_resource.h>

#include "device_plugins/server_camera/server_camera.h"

#include <utils/common/synctime.h>

void QnClientMessageProcessor::init()
{
    QUrl appServerEventsUrl = QnAppServerConnectionFactory::defaultUrl();
    appServerEventsUrl.setPath(QLatin1String("/events/"));

    QUrlQuery query;
    query.addQueryItem(QLatin1String("format"), QLatin1String("pb"));
    query.addQueryItem(QLatin1String("guid"), QnAppServerConnectionFactory::clientGuid());

    appServerEventsUrl.setQuery(query);

    base_type::init(appServerEventsUrl, QString());

    m_source->setVideoWallKey(QnAppServerConnectionFactory::videoWallKey());
}

QnClientMessageProcessor::QnClientMessageProcessor():
    base_type()
{
    QThread *thread = new QThread(); // TODO: #Elric leaking thread here.
    thread->start();

    moveToThread(thread);
}

void QnClientMessageProcessor::run() {
    init();
    base_type::run();
}

void QnClientMessageProcessor::loadRuntimeInfo(const QnMessage &message) {
    //do not call base method - we do not need session key
    if (!message.systemName.isEmpty())
        QnAppServerConnectionFactory::setSystemName(message.systemName);
    if (!message.publicIp.isEmpty())
        QnAppServerConnectionFactory::setPublicIp(message.publicIp);
}


void QnClientMessageProcessor::handleConnectionOpened(const QnMessage &message) {
    updateHardwareIds(message);
    processResources(message.resources);
    processLicenses(message.licenses);
    processCameraServerItems(message.cameraServerItems);

    QnResourceDiscoveryManager::instance()->setReady(true);
    qDebug() << "Connection opened";

    qnSyncTime->reset();
    base_type::handleConnectionOpened(message);
}

void QnClientMessageProcessor::handleMessage(const QnMessage &message) {
    base_type::handleMessage(message);

    switch(message.messageType) {
    case Qn::Message_Type_RuntimeInfoChange: {
        if (!message.mainHardwareIds.isEmpty())
            qnLicensePool->setMainHardwareIds(message.mainHardwareIds);

        if (!message.compatibleHardwareIds.isEmpty())
            qnLicensePool->setCompatibleHardwareIds(message.compatibleHardwareIds);
    }

    case Qn::Message_Type_License: {
        qnLicensePool->addLicense(message.license);
        break;
    }
    case Qn::Message_Type_ResourceDisabledChange: {
        QnResourcePtr resource;
        if (!message.resourceGuid.isEmpty())
            resource = qnResPool->getResourceByGuid(message.resourceGuid);
        else
            resource = qnResPool->getResourceById(message.resourceId);

        if (resource)
            resource->setDisabled(message.resourceDisabled);
        break;
    }
    case Qn::Message_Type_ResourceStatusChange: {
        QnResourcePtr resource;
        if (!message.resourceGuid.isEmpty())
            resource = qnResPool->getResourceByGuid(message.resourceGuid);
        else
            resource = qnResPool->getResourceById(message.resourceId);

        if (resource)
            resource->setStatus(message.resourceStatus);
        break;
    }
    case Qn::Message_Type_CameraServerItem: {
        QnCameraHistoryPool::instance()->addCameraHistoryItem(*message.cameraServerItem);
        break;
    }
    case Qn::Message_Type_ResourceChange: {
        if (!message.resource) {
            qWarning() << "Got Message_Type_ResourceChange with empty resource in it";
            return;
        }
        updateResource(message.resource, true, true);
        break;
    }
    case Qn::Message_Type_ResourceDelete: {
        if (QnResourcePtr ownResource = qnResPool->getResourceById(message.resourceId))
            qnResPool->removeResource(ownResource);
        break;
    }
    case Qn::Message_Type_VideoWallControl:
    {
        emit videoWallControlMessageReceived(message.videoWallControlMessage);
        break;
    }
    default:
        break;
    }
}

void QnClientMessageProcessor::processResources(const QnResourceList& resources)
{
    QnResourceList newResources;

    foreach (const QnResourcePtr& resource, resources)
        if(!updateResource(resource, false, false))
            newResources.push_back(resource);

    qnResPool->addResources(newResources);
}

void QnClientMessageProcessor::processLicenses(const QnLicenseList& licenses)
{
    qnLicensePool->replaceLicenses(licenses);
}

bool QnClientMessageProcessor::updateResource(QnResourcePtr resource, bool insert, bool updateLayouts)
{
    bool result = false;
    QnResourcePtr ownResource;

    QString guid = resource->getGuid();
    if (!guid.isEmpty())
        ownResource = qnResPool->getResourceByGuid(guid);
    else
        ownResource = qnResPool->getResourceById(resource->getId());

    if (ownResource.isNull()) {
        if(insert) {
            qnResPool->addResource(resource);
            result = true;
        }
        if (QnMediaServerResourcePtr mediaServer = resource.dynamicCast<QnMediaServerResource>())
            determineOptimalIF(mediaServer);
    } else if(updateLayouts || !ownResource.dynamicCast<QnLayoutResource>()) {
        bool mserverStatusChanged = false;
        QnMediaServerResourcePtr mediaServer = ownResource.dynamicCast<QnMediaServerResource>();
        if (mediaServer)
            mserverStatusChanged = ownResource->getStatus() != resource->getStatus();

        ownResource->update(resource);

        if (mserverStatusChanged && mediaServer)
            determineOptimalIF(mediaServer);

        result = true;
    }

    if (QnLayoutResourcePtr layout = ownResource.dynamicCast<QnLayoutResource>())
        layout->requestStore();

    return result;
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
    connect(resource.data(), SIGNAL(serverIfFound(const QnMediaServerResourcePtr &, const QString &, const QString &)), this, SLOT(at_serverIfFound(const QnMediaServerResourcePtr &, const QString &, const QString &)));
    resource->determineOptimalNetIF();
}

void QnClientMessageProcessor::at_serverIfFound(const QnMediaServerResourcePtr &resource, const QString & url, const QString& origApiUrl)
{
    if (url != QLatin1String("proxy"))
        resource->apiConnection()->setProxyAddr(origApiUrl, QString(), 0);
}

void QnClientMessageProcessor::processCameraServerItems(const QnCameraHistoryList& cameraHistoryList)
{
    foreach(QnCameraHistoryPtr history, cameraHistoryList)
        QnCameraHistoryPool::instance()->addCameraHistory(history);
}

void QnClientMessageProcessor::updateHardwareIds(const QnMessage& message)
{
    qnLicensePool->setMainHardwareIds(message.mainHardwareIds);
    qnLicensePool->setCompatibleHardwareIds(message.compatibleHardwareIds);
}
