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
    connect(m_source.data(), SIGNAL(connectionOpened()), this, SLOT(connectionOpened()));
    connect(m_source.data(), SIGNAL(connectionClosed(QString)), this, SLOT(at_connectionClose(QString)));
    connect(m_source.data(), SIGNAL(connectionReset()), this, SLOT(at_connectionReset()));
}

QnEventManager::QnEventManager()
    : m_seqNumber(0)
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

    if (event.eventType == QN_EVENT_EMPTY)
    {
        if (m_seqNumber == 0)
        {
            // No tracking yet. Just initialize seqNumber.
            m_seqNumber = event.seqNumber;
        }
        else if (QnEvent::nextSeqNumber(m_seqNumber) != event.seqNumber)
        {
            // Tracking is on and some events are missed and/or reconnect occured
            m_seqNumber = event.seqNumber;
            emit connectionReset();
        }
    }
    else if (event.eventType == QN_EVENT_LICENSE_CHANGE)
    {
        QnAppServerConnectionPtr appServerConnection = QnAppServerConnectionFactory::createConnection();

        QnLicenseList licenses;
        QByteArray errorString;

        if (appServerConnection->getLicenses(licenses, errorString) == 0)
        {
            foreach (QnLicensePtr license, licenses.licenses())
            {
                // Someone wants to steal our software
                if (!license->isValid())
                {
                    QnLicenseList dummy;
                    dummy.setHardwareId("invalid");
                    qnLicensePool->replaceLicenses(dummy);
                    break;
                }
            }

            qnLicensePool->replaceLicenses(licenses);
        }
    }
    else if (event.eventType == QN_CAMERA_SERVER_ITEM)
    {
        QString mac = event.dict["mac"].toString();
        QString serverGuid = event.dict["server_guid"].toString();
        qint64 timestamp_ms = event.dict["timestamp"].toLongLong();

        QnCameraHistoryItem historyItem(mac, timestamp_ms, serverGuid);

        QnCameraHistoryPool::instance()->addCameraHistoryItem(historyItem);
    }
    else if (event.eventType == QN_EVENT_RES_CHANGE)
    {
        if (event.objectName != "Camera" && event.objectName != "Server")
            return;

        QnVideoServerResourcePtr ownVideoServer = qnResPool->getResourceByGuid(serverGuid()).dynamicCast<QnVideoServerResource>();
        if (event.objectName == "Server" && event.resourceGuid != serverGuid())
            return;
        else if (event.objectName == "Camera" && event.parentId != ownVideoServer->getId())
            return;

        QnAppServerConnectionPtr appServerConnection = QnAppServerConnectionFactory::createConnection();

        QByteArray errorString;
        QnResourcePtr resource;
        if (appServerConnection->getResource(event.objectId, resource, errorString) == 0)
        {
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
        } else
        {
            qDebug()  << "QnEventManager::eventReceived(): Can't get resource from appserver. Reason: "
                      << errorString << ", Skipping event: " << event.eventType << " " << event.objectName << " " << event.objectId;
        }
    }
    else if (event.eventType == QN_EVENT_RES_STATUS_CHANGE)
    {
        QnResourcePtr resource = qnResPool->getResourceById(event.objectId);

        if (resource)
        {
            QnResource::Status status = (QnResource::Status)event.data.toInt();
            resource->setStatus(status);
        }
    } else if (event.eventType == QN_EVENT_RES_DISABLED_CHANGE)
	{
		QnResourcePtr resource = qnResPool->getResourceById(event.objectId);

		if (resource)
		{
			resource->setDisabled(event.data.toInt());
		}
    } else if (event.eventType == QN_EVENT_RES_DELETE)
    {
        QnResourcePtr resource = qnResPool->getResourceById(event.objectId);

        if (resource)
        {
            qnResPool->removeResource(resource);
        }
    }
}

void QnEventManager::at_connectionClose(QString errorString)
{
    qDebug() << "QnEventManager::connectionClosed(): Connection aborted:" << errorString;
}
