#include <QTimer>
#include <QDebug>
#include <qglobal.h>

#include "api/AppServerConnection.h"
#include "core/resourcemanagment/resource_discovery_manager.h"
#include "core/resourcemanagment/resource_pool.h"
#include "eventmanager.h"
#include "recorder/recording_manager.h"

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
    qDebug() << "Got event: " << event.eventType << " " << event.objectName << " " << event.resourceId;

    if (event.eventType == QN_EVENT_RES_CHANGE)
    {
        QnAppServerConnectionPtr appServerConnection = QnAppServerConnectionFactory::createConnection();

        QByteArray errorString;

        QnResourceList resources;
        if (appServerConnection->getResources(resources, errorString) == 0)
        {
            foreach(const QnResourcePtr& resource, resources)
            {
                if (resource->getId() == event.resourceId)
                {
                    QnResourcePtr ownResource = qnResPool->getResourceById(resource->getId());
                    if (ownResource)
                        ownResource->update(resource);
                    else
                    {
                        qnResPool->addResource(resource);
                        ownResource = resource;
                    }

                    QnSecurityCamResourcePtr securityCamera = ownResource.dynamicCast<QnSecurityCamResource>();
                    if (!securityCamera.isNull())
                    {
                        QnRecordingManager::instance()->updateCamera(securityCamera);
                    }
                }
            }
        } else
        {
            qDebug()  << "QnEventManager::eventReceived(): Can't get resource from appserver. Reason: "
                      << errorString << ", Skipping event: " << event.eventType << " " << event.objectName << " " << event.resourceId;
        }
    }
}

void QnEventManager::connectionClosed(QString errorString)
{
    qDebug() << "QnEventManager::connectionClosed(): Connection aborted:" << errorString;
}
