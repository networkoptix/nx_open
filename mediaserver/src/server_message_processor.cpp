#include <QTimer>
#include <QDebug>
#include <qglobal.h>

#include "api/app_server_connection.h"
#include "core/resource_managment/resource_discovery_manager.h"
#include "core/resource_managment/resource_pool.h"
#include "server_message_processor.h"
#include "recorder/recording_manager.h"
#include "serverutil.h"
#include <business/business_rule_processor.h>

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

    connect(this, SIGNAL(businessRuleChanged(QnBusinessEventRulePtr)), qnBusinessRuleProcessor, SLOT(at_businessRuleChanged(QnBusinessEventRulePtr)));
    connect(this, SIGNAL(businessRuleDeleted(QnId)), qnBusinessRuleProcessor, SLOT(at_businessRuleDeleted(QnId)));
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
    NX_LOG( QString::fromLatin1("Received event message %1, resourceId %2, resource %3").
        arg(Qn::toString(event.eventType)).arg(event.resourceId.toString()).arg(event.resource ? event.resource->getName() : QString("NULL")), cl_logDEBUG1 );

    if (event.eventType == Qn::Message_Type_License)
    {
        // New license added. LicensePool verifies it.
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

        QnMediaServerResourcePtr ownMediaServer = qnResPool->getResourceByGuid(serverGuid()).dynamicCast<QnMediaServerResource>();

        bool isServer = resource.dynamicCast<QnMediaServerResource>();
        bool isCamera = resource.dynamicCast<QnVirtualCameraResource>();

        if (!isServer && !isCamera)
            return;

        // If the resource is mediaServer then ignore if not this server
        if (isServer && resource->getGuid() != serverGuid())
            return;

        //storing all servers' cameras too
        // If camera from other server - marking it
        if (isCamera && resource->getParentId() != ownMediaServer->getId())
            resource->addFlags( QnResource::foreigner );

        // We are always online
        if (isServer)
            resource->setStatus(QnResource::Online);

        QByteArray errorString;
        QnResourcePtr ownResource = qnResPool->getResourceById(resource->getId(), QnResourcePool::rfAllResources);
        if (ownResource)
        {
            ownResource->update(resource);
        }
        else
        {
            qnResPool->addResource(resource);
            ownResource = resource;
        }

        if (isServer)
            syncStoragesToSettings(ownMediaServer);

    } else if (event.eventType == Qn::Message_Type_ResourceDisabledChange)
    {
        QnResourcePtr resource = qnResPool->getResourceById(event.resourceId);
        //ignoring events for foreign resources

        if (resource)
        {
            resource->setDisabled(event.resourceDisabled);
            if (event.resourceDisabled) // we always ignore status changes 
                resource->setStatus(QnResource::Offline); 
        }
    } else if (event.eventType == Qn::Message_Type_ResourceDelete)
    {
        QnResourcePtr resource = qnResPool->getResourceById(event.resourceId, QnResourcePool::rfAllResources);

        if (resource)
        {
            qnResPool->removeResource(resource);
        }
    } else if (event.eventType == Qn::Message_Type_BusinessRuleInsertOrUpdate)
    {
       emit businessRuleChanged(event.businessRule);

    } else if (event.eventType == Qn::Message_Type_BusinessRuleDelete)
    {
        emit businessRuleDeleted(event.resourceId);
    } else if (event.eventType == Qn::Message_Type_BroadcastBusinessAction)
    {
        emit businessActionReceived(event.businessAction);
    }
}

void QnServerMessageProcessor::at_connectionClosed(QString errorString)
{
    qDebug() << "QnEventManager::connectionClosed(): Connection aborted:" << errorString;
}
