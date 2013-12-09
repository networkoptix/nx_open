#include <QtCore/QTimer>
#include <QtCore/QDebug>
#include <QtCore/QtGlobal>
#include <QtCore/QThread>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

#include "core/resource_managment/resource_discovery_manager.h"
#include "core/resource_managment/resource_pool.h"
#include "device_plugins/server_camera/server_camera.h"

#include <utils/common/synctime.h>

#include "licensing/license.h"

#include "client_message_processor.h"

void QnClientMessageProcessor::init()
{
    QUrl appServerEventsUrl = QnAppServerConnectionFactory::defaultUrl();
    appServerEventsUrl.setPath(QLatin1String("/events/"));

    QUrlQuery query;
    query.addQueryItem(QLatin1String("format"), QLatin1String("pb"));
    query.addQueryItem(QLatin1String("guid"), QnAppServerConnectionFactory::clientGuid());

    appServerEventsUrl.setQuery(query);

    init(appServerEventsUrl, QString());
}

void QnClientMessageProcessor::init(const QUrl &url, const QString &authKey, int reconnectTimeout) {
    base_type::init(url, authKey, reconnectTimeout);

    connect(m_source.data(), SIGNAL(messageReceived(QnMessage)), this, SLOT(at_messageReceived(QnMessage)));
    connect(m_source.data(), SIGNAL(connectionOpened(QnMessage)), this, SLOT(at_connectionOpened(QnMessage)));
    connect(m_source.data(), SIGNAL(connectionClosed(QString)), this, SLOT(at_connectionClosed(QString)));
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

void QnClientMessageProcessor::processResources(const QnResourceList& resources)
{
    QnResourceList newResources;

    foreach (const QnResourcePtr& resource, resources)
        if(!updateResource(resource, false))
            newResources.push_back(resource);

    qnResPool->addResources(newResources);
}

void QnClientMessageProcessor::processLicenses(const QnLicenseList& licenses)
{
    qnLicensePool->replaceLicenses(licenses);
}

bool QnClientMessageProcessor::updateResource(QnResourcePtr resource, bool insert) // TODO: #Elric 'insert' parameter is hacky. Get rid of it and write some nicer code.
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
            determineOptimalIF(mediaServer.data());
    } 
    else {
        bool mserverStatusChanged = false;
        QnMediaServerResourcePtr mediaServer = ownResource.dynamicCast<QnMediaServerResource>();
        if (mediaServer)
            mserverStatusChanged = ownResource->getStatus() != resource->getStatus();

        ownResource->update(resource);

        if (mserverStatusChanged && mediaServer)
            determineOptimalIF(mediaServer.data());

        result = true;
    }


    if (QnLayoutResourcePtr layout = ownResource.dynamicCast<QnLayoutResource>())
        layout->requestStore();

    return result;
}

void QnClientMessageProcessor::determineOptimalIF(QnMediaServerResource* mediaServer)
{
    // set proxy. If some media server IF will be found, proxy address will be cleared
    QString url = QnAppServerConnectionFactory::defaultUrl().host();
    if (url.isEmpty())
        url = QLatin1String("127.0.0.1");
    int port = QnAppServerConnectionFactory::defaultMediaProxyPort();
    mediaServer->apiConnection()->setProxyAddr(mediaServer->getApiUrl(), url, port);
    disconnect(mediaServer, NULL, this, NULL);
    connect(mediaServer, SIGNAL(serverIfFound(const QnMediaServerResourcePtr &, const QString &, const QString &)), this, SLOT(at_serverIfFound(const QnMediaServerResourcePtr &, const QString &, const QString &)));
    mediaServer->determineOptimalNetIF();
}

void QnClientMessageProcessor::at_serverIfFound(const QnMediaServerResourcePtr &resource, const QString & url, const QString& origApiUrl)
{
    if (url != QLatin1String("proxy"))
        resource->apiConnection()->setProxyAddr(origApiUrl, QString(), 0);
}

void QnClientMessageProcessor::at_messageReceived(QnMessage message)
{
    switch(message.messageType) {
    case Qn::Message_Type_Initial:
        {
            QnAppServerConnectionFactory::setPublicIp(message.publicIp);
            break;
        }
    case Qn::Message_Type_Ping:
        {
            break;
        }
    case Qn::Message_Type_License:
        {
            qnLicensePool->addLicense(message.license);
            break;
        }
    case Qn::Message_Type_ResourceDisabledChange:
        {
            QnResourcePtr resource;
            if (!message.resourceGuid.isEmpty())
                resource = qnResPool->getResourceByGuid(message.resourceGuid);
            else
                resource = qnResPool->getResourceById(message.resourceId);

            if (resource)
                resource->setDisabled(message.resourceDisabled);
            break;
        }
    case Qn::Message_Type_ResourceStatusChange:
        {
            QnResourcePtr resource;
            if (!message.resourceGuid.isEmpty())
                resource = qnResPool->getResourceByGuid(message.resourceGuid);
            else
                resource = qnResPool->getResourceById(message.resourceId);

            if (resource)
                resource->setStatus(message.resourceStatus);
            break;
        }
    case Qn::Message_Type_CameraServerItem:
        {
            QnCameraHistoryPool::instance()->addCameraHistoryItem(*message.cameraServerItem);
            break;
        }
    case Qn::Message_Type_ResourceChange:
        {
            if (!message.resource)
            {
                qWarning() << "Got Message_Type_ResourceChange with empty resource in it";
                return;
            }
            updateResource(message.resource);
            break;
        }
    case Qn::Message_Type_ResourceDelete:
        {
            QnResourcePtr ownResource = qnResPool->getResourceById(message.resourceId);
            qnResPool->removeResource(ownResource);
            break;
        }
    case Qn::Message_Type_BusinessRuleInsertOrUpdate:
        {
            emit businessRuleChanged(message.businessRule);
            break;
        }
    case Qn::Message_Type_BusinessRuleReset:
        {
            emit businessRuleReset(message.businessRules);
            break;
        }
    case Qn::Message_Type_BusinessRuleDelete:
        {
            emit businessRuleDeleted(message.resourceId.toInt());
            break;
        }
    case Qn::Message_Type_BroadcastBusinessAction:
        {
            emit businessActionReceived(message.businessAction);
            break;
        }
    case Qn::Message_Type_FileAdd:
        {
            emit fileAdded(message.filename);
            break;
        }
    case Qn::Message_Type_FileRemove:
        {
            emit fileRemoved(message.filename);
            break;
        }
    case Qn::Message_Type_FileUpdate:
        {
            emit fileUpdated(message.filename);
            break;
        }
    case Qn::Message_Type_RuntimeInfoChange:
        break; //TODO: #ivigasin what means this message for the client?
    }
    // default-case is not used for a reason

}

void QnClientMessageProcessor::at_connectionClosed(QString errorString)
{
    qDebug() << "Connection aborted:" << errorString;

    emit connectionClosed();
}

void QnClientMessageProcessor::processCameraServerItems(const QnCameraHistoryList& cameraHistoryList)
{
    foreach(QnCameraHistoryPtr history, cameraHistoryList)
        QnCameraHistoryPool::instance()->addCameraHistory(history);
}

void QnClientMessageProcessor::updateHardwareIds(const QnMessage& message)
{
    qnLicensePool->setOldHardwareId(message.oldHardwareId);
    qnLicensePool->setHardwareId1(message.hardwareId1);
    qnLicensePool->setHardwareId2(message.hardwareId2);
    qnLicensePool->setHardwareId3(message.hardwareId3);
}

void QnClientMessageProcessor::at_connectionOpened(QnMessage message)
{
    QnAppServerConnectionFactory::setSystemName(message.systemName);

    updateHardwareIds(message);
    processResources(message.resources);
    processLicenses(message.licenses);
    processCameraServerItems(message.cameraServerItems);

    QnResourceDiscoveryManager::instance()->setReady(true);
    qDebug() << "Connection opened";

    qnSyncTime->reset();
    emit connectionOpened();
}
