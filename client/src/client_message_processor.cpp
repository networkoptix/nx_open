#include <QtCore/QTimer>
#include <QtCore/QDebug>
#include <QtCore/QtGlobal>
#include <QtCore/QThread>

#include "core/resourcemanagment/resource_discovery_manager.h"
#include "core/resourcemanagment/resource_pool.h"
#include "device_plugins/server_camera/server_camera.h"

#include "client_message_processor.h"

Q_GLOBAL_STATIC_WITH_INITIALIZER(QnClientMessageProcessor, qn_eventManager_instance, {
    QThread *thread = new QThread(); // TODO: leaking thread here.
    thread->start();

    x->moveToThread(thread);
})

QnClientMessageProcessor* QnClientMessageProcessor::instance()
{
    return qn_eventManager_instance();
}

void QnClientMessageProcessor::init()
{
    QUrl appServerEventsUrl = QnAppServerConnectionFactory::defaultUrl();
    appServerEventsUrl.setPath("/events");
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

void QnClientMessageProcessor::at_messageReceived(QnMessage event)
{
    QByteArray debugStr;
    QTextStream stream(&debugStr);

    stream << "Got event: " << event.eventType << " " << event.objectName << " " << event.objectId << event.resourceGuid;

    if (event.eventType == QN_MESSAGE_RES_DISABLED_CHANGE)
        stream << "disabled: " << event.disabled.toInt();

    if(event.eventType == QN_MESSAGE_RES_STATUS_CHANGE)
        stream << "status: " << event.status.toInt();

    qDebug() << debugStr;

    if (event.eventType == QN_MESSAGE_LICENSE_CHANGE)
    {
        QnAppServerConnectionFactory::createConnection()->getLicensesAsync(this, SLOT(at_licensesReceived(int,QByteArray,QnLicenseList,int)));
    }
	else if (event.eventType == QN_MESSAGE_RES_DISABLED_CHANGE)
	{
		QnResourcePtr resource;
		if (!event.resourceGuid.isEmpty())
			resource = qnResPool->getResourceByGuid(event.resourceGuid);
		else
			resource = qnResPool->getResourceById(event.objectId);

		if (resource)
		{
            resource->setDisabled(event.disabled.toInt());
		}
	}
    else if (event.eventType == QN_MESSAGE_RES_STATUS_CHANGE)
    {
        QnResourcePtr resource;
        if (!event.resourceGuid.isEmpty())
            resource = qnResPool->getResourceByGuid(event.resourceGuid);
        else
            resource = qnResPool->getResourceById(event.objectId);

        if (resource)
        {
            QnResource::Status status = (QnResource::Status)event.status.toInt();
            resource->setStatus(status);
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
        QnAppServerConnectionFactory::createConnection()->
                getResourcesAsync(QString("id=%1").arg(event.objectId), event.objectNameLower(), this, SLOT(at_resourcesReceived(int,QByteArray,QnResourceList,int)));
    } else if (event.eventType == QN_MESSAGE_RES_DELETE)
    {
        QnResourcePtr ownResource = qnResPool->getResourceById(event.objectId);
        qnResPool->removeResource(ownResource);
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

    emit connectionOpened();
}

void QnClientMessageProcessor::at_connectionReset()
{
    QnAppServerConnectionFactory::createConnection()->
            getResourcesAsync("", "resourceEx", this, SLOT(at_resourcesReceived(int,QByteArray,QnResourceList,int)));
}
