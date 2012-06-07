#include <QTimer>
#include <QDebug>
#include <qglobal.h>

#include "api/app_server_connection.h"
#include "core/resourcemanagment/resource_discovery_manager.h"
#include "core/resourcemanagment/resource_pool.h"
#include "server_message_processor.h"
#include "recorder/recording_manager.h"
#include "serverutil.h"

Q_GLOBAL_STATIC(QnServerMessageProcessor, static_instance)

QnServerMessageProcessor* QnServerMessageProcessor::instance()
{
    return static_instance();
}

void QnServerMessageProcessor::init(const QUrl& url, int timeout)
{
    m_source = QSharedPointer<QnMessageSource>(new QnMessageSource(url, timeout));

    connect(m_source.data(), SIGNAL(messageReceived(QnMessage)), this, SLOT(at_messageReceived(QnMessage)));
    connect(m_source.data(), SIGNAL(connectionClosed(QString)), this, SLOT(at_connectionClosed(QString)));
    connect(m_source.data(), SIGNAL(connectionReset()), this, SLOT(at_connectionReset()));
}

QnServerMessageProcessor::QnServerMessageProcessor()
{
}

void QnServerMessageProcessor::run()
{
    m_source->startRequest();
}

void QnServerMessageProcessor::stop()
{
    m_source->stop();
}

void QnServerMessageProcessor::at_connectionReset()
{
    emit connectionReset();
}

void QnServerMessageProcessor::at_messageReceived(QnMessage event)
{
    qDebug() << "Got event: " << event.eventType << " " << event.objectName << " " << event.objectId;

    if (event.eventType == QN_MESSAGE_LICENSE_CHANGE)
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
    else if (event.eventType == QN_MESSAGE_CAMERA_SERVER_ITEM)
    {
        QString mac = event.dict["mac"].toString();
        QString serverGuid = event.dict["server_guid"].toString();
        qint64 timestamp_ms = event.dict["timestamp"].toLongLong();

        QnCameraHistoryItem historyItem(mac, timestamp_ms, serverGuid);

        QnCameraHistoryPool::instance()->addCameraHistoryItem(historyItem);
    }
    else if (event.eventType == QN_MESSAGE_RES_CHANGE)
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
            qDebug()  << "QnEventManager::messageReceived(): Can't get resource from appserver. Reason: "
                      << errorString << ", Skipping event: " << event.eventType << " " << event.objectName << " " << event.objectId;
        }
    }
    else if (event.eventType == QN_MESSAGE_RES_STATUS_CHANGE)
    {
        QnResourcePtr resource = qnResPool->getResourceById(event.objectId);

        if (resource)
        {
            QnResource::Status status = (QnResource::Status)event.data.toInt();
            resource->setStatus(status);
        }
    } else if (event.eventType == QN_MESSAGE_RES_DISABLED_CHANGE)
	{
		QnResourcePtr resource = qnResPool->getResourceById(event.objectId);

		if (resource)
		{
			resource->setDisabled(event.data.toInt());
		}
    } else if (event.eventType == QN_MESSAGE_RES_DELETE)
    {
        QnResourcePtr resource = qnResPool->getResourceById(event.objectId);

        if (resource)
        {
            qnResPool->removeResource(resource);
        }
    }
}

void QnServerMessageProcessor::at_connectionClosed(QString errorString)
{
    qDebug() << "QnEventManager::connectionClosed(): Connection aborted:" << errorString;
}
