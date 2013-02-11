#include <QtCore/QTimer>
#include <QtCore/QDebug>
#include <QtCore/QtGlobal>
#include <QtCore/QThread>

#include "core/resource_managment/resource_discovery_manager.h"
#include "core/resource_managment/resource_pool.h"
#include "device_plugins/server_camera/server_camera.h"
#include "utils/common/synctime.h"

#include "client_message_processor.h"

class QnClientMessageProcessorInstance: public QnClientMessageProcessor {
public:
    QnClientMessageProcessorInstance() {
        QThread *thread = new QThread(); // TODO: #Elric leaking thread here.
        thread->start();

        moveToThread(thread);
    }
};
Q_GLOBAL_STATIC(QnClientMessageProcessorInstance, qn_clientMessageProcessor_instance);

QnClientMessageProcessor* QnClientMessageProcessor::instance()
{
    return qn_clientMessageProcessor_instance();
}

void QnClientMessageProcessor::init()
{
    QUrl appServerEventsUrl = QnAppServerConnectionFactory::defaultUrl();
    appServerEventsUrl.setPath(QLatin1String("/events/"));
    appServerEventsUrl.addQueryItem(QLatin1String("format"), QLatin1String("pb"));
    appServerEventsUrl.addQueryItem(QLatin1String("guid"), QnAppServerConnectionFactory::clientGuid());
    init(appServerEventsUrl, EVENT_RECONNECT_TIMEOUT);
}

void QnClientMessageProcessor::init(const QUrl& url, int timeout)
{
    m_source = QSharedPointer<QnMessageSource>(new QnMessageSource(url, timeout));

    connect(m_source.data(), SIGNAL(messageReceived(QnMessage)), this, SLOT(at_messageReceived(QnMessage)));
    connect(m_source.data(), SIGNAL(connectionOpened()), this, SLOT(at_connectionOpened()));
    connect(m_source.data(), SIGNAL(connectionClosed(QString)), this, SLOT(at_connectionClosed(QString)));
    connect(m_source.data(), SIGNAL(connectionReset()), this, SLOT(at_connectionReset()));
}

QnClientMessageProcessor::QnClientMessageProcessor()
{
}

void QnClientMessageProcessor::run()
{
    init();

    m_source->startRequest();
}

void QnClientMessageProcessor::stop()
{
    if (m_source)
        m_source->stop();
}

void QnClientMessageProcessor::at_resourcesReceived(int status, const QByteArray& errorString, QnResourceList resources, int handle)
{
    Q_UNUSED(handle)
    if (status != 0)
    {
        qDebug() << "QnEventManager::resourcesReceived(): Can't get resource from appserver. Reason: " << errorString;
        return;
    }

    foreach (const QnResourcePtr& resource, resources)
    {
        QnResourcePtr ownResource;
    
        QString guid = resource->getGuid();
        if (!guid.isEmpty())
            ownResource = qnResPool->getResourceByGuid(guid);
        else
            ownResource = qnResPool->getResourceById(resource->getId());

        if (ownResource.isNull())
            qnResPool->addResource(resource);
        else
            ownResource->update(resource);
    }
}

void QnClientMessageProcessor::at_licensesReceived(int status, const QByteArray &errorString, QnLicenseList licenses, int handle)
{
    Q_UNUSED(handle)
    Q_UNUSED(errorString)
    Q_UNUSED(status)

    // licenses are already verified
    qnLicensePool->replaceLicenses(licenses);
}

void QnClientMessageProcessor::at_messageReceived(QnMessage message)
{
    if (message.eventType == Qn::Message_Type_License)
    {
        qnLicensePool->addLicense(message.license);
    }
    else if (message.eventType == Qn::Message_Type_ResourceDisabledChange)
    {
        QnResourcePtr resource;
        if (!message.resourceGuid.isEmpty())
            resource = qnResPool->getResourceByGuid(message.resourceGuid);
        else
            resource = qnResPool->getResourceById(message.resourceId);

        if (resource)
        {
            resource->setDisabled(message.resourceDisabled);
        }
    }
    else if (message.eventType == Qn::Message_Type_ResourceStatusChange)
    {
        QnResourcePtr resource;
        if (!message.resourceGuid.isEmpty())
            resource = qnResPool->getResourceByGuid(message.resourceGuid);
        else
            resource = qnResPool->getResourceById(message.resourceId);

        if (resource)
        {
            resource->setStatus(message.resourceStatus);
        }
    }
    else if (message.eventType == Qn::Message_Type_CameraServerItem)
    {
        QnCameraHistoryPool::instance()->addCameraHistoryItem(*message.cameraServerItem);
    }
    else if (message.eventType == Qn::Message_Type_ResourceChange)
    {
        if (!message.resource)
        {
            qWarning() << "Got Message_Type_ResourceChange with empty resource in it";
            return;
        }

        QnResourcePtr ownResource;
    
        QString guid = message.resource->getGuid();
        if (!guid.isEmpty())
            ownResource = qnResPool->getResourceByGuid(guid);
        else
            ownResource = qnResPool->getResourceById(message.resource->getId());

        if (ownResource.isNull())
            qnResPool->addResource(message.resource); // TODO: #Ivan
        else
            ownResource->update(message.resource);

        if(QnLayoutResourcePtr layout = ownResource.dynamicCast<QnLayoutResource>())
            layout->requestStore();

    }
    else if (message.eventType == Qn::Message_Type_ResourceDelete)
    {
        QnResourcePtr ownResource = qnResPool->getResourceById(message.resourceId);
        qnResPool->removeResource(ownResource);
    }
    else if (message.eventType == Qn::Message_Type_BusinessRuleInsertOrUpdate)
    {
        emit businessRuleChanged(message.businessRule);
    }
    else if (message.eventType == Qn::Message_Type_BusinessRuleDelete)
    {
        emit businessRuleDeleted(message.resourceId);
    }
    else if (message.eventType == Qn::Message_Type_BroadcastBusinessAction)
    {
        emit businessActionReceived(message.businessAction);
    }

}

void QnClientMessageProcessor::at_connectionClosed(QString errorString)
{
    qDebug() << "Connection aborted:" << errorString;

    emit connectionClosed();
}

void QnClientMessageProcessor::at_connectionOpened()
{
    qDebug() << "Connection opened";

    qnSyncTime->reset();
    emit connectionOpened();
}

void QnClientMessageProcessor::at_connectionReset()
{
    QnAppServerConnectionFactory::createConnection()->
            getResourcesAsync(QLatin1String(""), QLatin1String("resource"), this, SLOT(at_resourcesReceived(int,QByteArray,QnResourceList,int)));
}
