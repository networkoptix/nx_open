#include <QTimer>
#include <QDebug>
#include <qglobal.h>

#include "api/AppServerConnection.h"
#include "core/resourcemanagment/asynch_seacher.h"
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

void QnEventManager::eventReceived(QnEvent event)
{
    if (event.eventType == QN_EVENT_RES_CHANGE)
    {
        QnAppServerConnectionPtr appServerConnection = QnAppServerConnectionFactory::createConnection(QnResourceDiscoveryManager::instance());

        QnResourceList resources;
        appServerConnection->getResources(resources);

        foreach(const QnResourcePtr& resource, resources)
        {
            if (resource->getId() == event.resourceId)
            {
                qnResPool->addResource(resource);

                QnResourcePtr ownResource = qnResPool->getResourceById(resource->getId());

                QnSequrityCamResourcePtr securityCamera = ownResource.dynamicCast<QnSequrityCamResource>();
                if (!securityCamera.isNull())
                {
                    QnRecordingManager::instance()->updateSchedule(securityCamera);
                }
            }
        }
    }

    qDebug() << event.eventType << " " << event.objectName << " " << event.resourceId;
}

void QnEventManager::connectionClosed(QString errorString)
{
    qDebug() << "Connection aborted:" << errorString;
}
