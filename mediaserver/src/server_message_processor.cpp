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
    // qDebug() << "Got event: " << event.eventType << " " << event.objectName << " " << event.objectId;

    if (event.eventType == Qn::Message_Type_License)
    {
        // New license added
        if (event.license->isValid())
            qnLicensePool->addLicense(event.license);
    }
    else if (event.eventType == Qn::Message_Type_CameraServerItem)
    {
/*        QString mac = event.dict["mac"].toString();
        QString serverGuid = event.dict["server_guid"].toString();
        qint64 timestamp_ms = event.dict["timestamp"].toLongLong();

        QnCameraHistoryItem historyItem(mac, timestamp_ms, serverGuid);*/

        QnCameraHistoryPool::instance()->addCameraHistoryItem(*event.cameraServerItem);
    }
    else if (event.eventType == Qn::Message_Type_ResourceChange)
    {
        QnResourcePtr resource = event.resource;

        QnVideoServerResourcePtr ownVideoServer = qnResPool->getResourceByGuid(serverGuid()).dynamicCast<QnVideoServerResource>();

        bool isServer = resource.dynamicCast<QnVideoServerResource>();
        bool isCamera = resource.dynamicCast<QnVirtualCameraResource>();

        if (!isServer && !isCamera)
            return;

        // If the resource is mediaServer then egnore if not this server
        if (isServer && resource->getGuid() != serverGuid())
            return;

        // If camera from other server - ignore 
        if (isCamera && resource->getParentId() != ownVideoServer->getId())
            return;

        // We are always online
        if (isServer)
            resource->setStatus(QnResource::Online);

        QByteArray errorString;
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
    } else if (event.eventType == Qn::Message_Type_ResourceDisabledChange)
    {
        QnResourcePtr resource = qnResPool->getResourceById(event.resourceId);

        if (resource)
        {
            resource->setDisabled(event.resourceDisabled);
            if (event.resourceDisabled) // we always ignore status changes 
                resource->setStatus(QnResource::Offline); 
        }
    } else if (event.eventType == Qn::Message_Type_ResourceDelete)
    {
        QnResourcePtr resource = qnResPool->getResourceById(event.resourceId);

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
