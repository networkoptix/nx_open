#include <QTimer>
#include <QDebug>
#include <qglobal.h>

#include "api/AppServerConnection.h"
#include "core/resourcemanagment/resource_discovery_manager.h"
#include "core/resourcemanagment/resource_pool.h"
#include "eventmanager.h"
#include "recorder/recording_manager.h"
#include "serverutil.h"

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
    qDebug() << "Got event: " << event.eventType << " " << event.objectName << " " << event.objectId;

    if (event.eventType == QN_EVENT_LICENSE_CHANGE)
    {
        QnAppServerConnectionPtr appServerConnection = QnAppServerConnectionFactory::createConnection();

        QnLicenseList licenses;
        QByteArray errorString;

        if (appServerConnection->getLicenses(licenses, errorString) == 0)
        {
            qnLicensePool->replaceLicenses(licenses);
        }

    }
    else if (event.eventType == QN_EVENT_RES_CHANGE)
    {
        if (event.objectName != "Camera" && event.objectName != "Server")
            return;

        QnAppServerConnectionPtr appServerConnection = QnAppServerConnectionFactory::createConnection();

        QByteArray errorString;

        QnResourceList resources;
        if (appServerConnection->getResources(resources, errorString) == 0)
        {
            foreach(const QnResourcePtr& resource, resources)
            {
                if (resource->getId() == event.objectId)
                {
                    QnVideoServerResourcePtr ownVideoServer = qnResPool->getResourceByGuid(serverGuid()).dynamicCast<QnVideoServerResource>();

                    QnSecurityCamResourcePtr securityCamera = resource.dynamicCast<QnSecurityCamResource>();
                    if (securityCamera && securityCamera->getParentId() != ownVideoServer->getId())
                        continue;

                    QnVideoServerResourcePtr videoServer = resource.dynamicCast<QnVideoServerResource>();
                    if (videoServer && videoServer->getId() != ownVideoServer->getId())
                        continue;

                    QnResourcePtr ownResource = qnResPool->getResourceById(resource->getId());
                    if (ownResource)
                    {
                        ownResource->update(resource);
                    }
                    else
                    {
                        qnResPool->addResource(resource);
                        ownResource = resource;
                    }

                    QnSecurityCamResourcePtr ownSecurityCamera = ownResource.dynamicCast<QnSecurityCamResource>();
                    if (ownSecurityCamera)
                        QnRecordingManager::instance()->updateCamera(ownSecurityCamera);
                }
            }
        } else
        {
            qDebug()  << "QnEventManager::eventReceived(): Can't get resource from appserver. Reason: "
                      << errorString << ", Skipping event: " << event.eventType << " " << event.objectName << " " << event.objectId;
        }
    }
}

void QnEventManager::connectionClosed(QString errorString)
{
    qDebug() << "QnEventManager::connectionClosed(): Connection aborted:" << errorString;
}
