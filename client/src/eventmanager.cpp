#include <QTimer>
#include <QDebug>
#include <qglobal.h>

#include "api/AppServerConnection.h"
#include "core/resourcemanagment/resource_discovery_manager.h"
#include "core/resourcemanagment/resource_pool.h"
#include "device_plugins/server_camera/server_camera.h"
#include "eventmanager.h"

Q_GLOBAL_STATIC(QnEventManager, static_instance)

QnEventManager* QnEventManager::instance()
{
    return static_instance();
}

void QnEventManager::init(const QUrl& url, int timeout)
{
    m_source = QSharedPointer<QnEventSource>(new QnEventSource(url, timeout));

    connect(m_source.data(), SIGNAL(eventReceived(QnEvent)), this, SLOT(eventReceived(QnEvent)));
    connect(m_source.data(), SIGNAL(connectionClosed(QString)), this, SLOT(connectionClosed(QString)));
}

QnEventManager::QnEventManager()
{
}

void QnEventManager::run()
{
    m_source->startRequest();
}

void QnEventManager::stop()
{
    m_source->stop();
}

void QnEventManager::eventReceived(QnEvent event)
{
    qDebug() << "Got event: " << event.eventType << " " << event.objectName << " " << event.resourceId << event.resourceGuid;

    if (event.eventType == QN_EVENT_RES_STATUS_CHANGE)
    {
        QnResourcePtr resource;
        if (!event.resourceGuid.isEmpty())
            resource = qnResPool->getResourceByGuid(event.resourceGuid);
        else
            resource = qnResPool->getResourceById(event.resourceId);

        if (resource)
        {
            QnResource::Status status = (QnResource::Status)event.data.toInt();
            resource->setStatus(status);
        }
    }
    else if (event.eventType == QN_EVENT_RES_CHANGE)
    {
        QnAppServerConnectionPtr appServerConnection = QnAppServerConnectionFactory::createConnection();

        QByteArray errorString;
        QnResourceList resources;
        if (appServerConnection->getResources(resources, errorString) == 0)
        {
            QnResourcePtr ownResource;

            if (!event.resourceGuid.isEmpty())
                ownResource = qnResPool->getResourceByGuid(event.resourceGuid);
            else
                ownResource = qnResPool->getResourceById(event.resourceId);

            foreach(QnResourcePtr resource, resources)
            {
                if ((!event.resourceGuid.isEmpty() && resource->getGuid() == event.resourceGuid) || (event.resourceGuid.isEmpty() && resource->getId() == event.resourceId))
                {
                    if (ownResource.isNull())
                        qnResPool->addResource(resource);
                    else
                        ownResource->update(resource);
                }
            }
        } else
        {
            qDebug()  << "QnEventManager::eventReceived(): Can't get resource from appserver. Reason: "
                      << errorString << ", Skipping event: " << event.eventType << " " << event.objectName << " " << event.resourceId;
        }
    } else if (event.eventType == QN_EVENT_RES_DELETE)
    {
        QnResourcePtr ownResource = qnResPool->getResourceById(event.resourceId);
        qnResPool->removeResource(ownResource);
    }
}

void QnEventManager::connectionClosed(QString errorString)
{
    qDebug() << "Connection aborted:" << errorString;
}
