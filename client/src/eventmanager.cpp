#include <QTimer>
#include <QDebug>
#include <qglobal.h>

#include "core/resourcemanagment/resource_discovery_manager.h"
#include "core/resourcemanagment/resource_pool.h"
#include "device_plugins/server_camera/server_camera.h"
#include "eventmanager.h"

Q_GLOBAL_STATIC(QnEventManager, static_instance)

QnEventManager* QnEventManager::instance()
{
    return static_instance();
}

void QnEventManager::init()
{
    QUrl appServerEventsUrl = QnAppServerConnectionFactory::defaultUrl();
    appServerEventsUrl.setPath("/events");
    init(appServerEventsUrl, EVENT_RECONNECT_TIMEOUT);
}

void QnEventManager::init(const QUrl& url, int timeout)
{
    m_source = QSharedPointer<QnEventSource>(new QnEventSource(url, timeout));

    connect(m_source.data(), SIGNAL(eventReceived(QnEvent)), this, SLOT(at_eventReceived(QnEvent)));
    connect(m_source.data(), SIGNAL(connectionOpened()), this, SLOT(at_connectionOpened()));
    connect(m_source.data(), SIGNAL(connectionClosed(QString)), this, SLOT(at_connectionClosed(QString)));
    connect(m_source.data(), SIGNAL(connectionReset()), this, SLOT(at_connectionReset()));
}

QnEventManager::QnEventManager()
{
}

void QnEventManager::run()
{
    init();

    m_source->startRequest();
}

void QnEventManager::stop()
{
    if (m_source)
        m_source->stop();
}

void QnEventManager::at_resourcesReceived(int status, const QByteArray& errorString, QnResourceList resources, int handle)
{
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

void QnEventManager::at_licensesReceived(int status, const QByteArray &errorString, QnLicenseList licenses, int handle)
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

void QnEventManager::at_eventReceived(QnEvent event)
{
    QByteArray debugStr;
    QTextStream stream(&debugStr);

    stream << "Got event: " << event.eventType << " " << event.objectName << " " << event.objectId << event.resourceGuid;

    if (event.eventType == QN_EVENT_RES_DISABLED_CHANGE || event.eventType == QN_EVENT_RES_STATUS_CHANGE)
        stream << "data: " << event.data.toInt();

    qDebug() << debugStr;

    if (event.eventType == QN_EVENT_LICENSE_CHANGE)
    {
        QnAppServerConnectionFactory::createConnection()->getLicensesAsync(this, SLOT(at_licensesReceived(int,QByteArray,QnLicenseList,int)));
    }
	else if (event.eventType == QN_EVENT_RES_DISABLED_CHANGE)
	{
		QnResourcePtr resource;
		if (!event.resourceGuid.isEmpty())
			resource = qnResPool->getResourceByGuid(event.resourceGuid);
		else
			resource = qnResPool->getResourceById(event.objectId);

		if (resource)
		{
			resource->setDisabled(event.data.toInt());
		}
	}
    else if (event.eventType == QN_EVENT_RES_STATUS_CHANGE)
    {
        QnResourcePtr resource;
        if (!event.resourceGuid.isEmpty())
            resource = qnResPool->getResourceByGuid(event.resourceGuid);
        else
            resource = qnResPool->getResourceById(event.objectId);

        if (resource)
        {
            QnResource::Status status = (QnResource::Status)event.data.toInt();
            resource->setStatus(status);
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
        QnAppServerConnectionFactory::createConnection()->
                getResourcesAsync(QString("id=%1").arg(event.objectId), event.objectNameLower(), this, SLOT(at_resourcesReceived(int,QByteArray,QnResourceList,int)));
    } else if (event.eventType == QN_EVENT_RES_DELETE)
    {
        QnResourcePtr ownResource = qnResPool->getResourceById(event.objectId);
        qnResPool->removeResource(ownResource);
    }
}

void QnEventManager::at_connectionClosed(QString errorString)
{
    qDebug() << "Connection aborted:" << errorString;

    emit connectionClosed();
}

void QnEventManager::at_connectionReset()
{
    QnAppServerConnectionFactory::createConnection()->
            getResourcesAsync("", "resourceEx", this, SLOT(at_resourcesReceived(int,QByteArray,QnResourceList,int)));
}
