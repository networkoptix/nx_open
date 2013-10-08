#include <QTimer>
#include <QDebug>
#include <qglobal.h>

#include "api/app_server_connection.h"
#include "core/resource_managment/resource_discovery_manager.h"
#include "core/resource_managment/resource_pool.h"
#include "server_message_processor.h"
#include "recorder/recording_manager.h"
#include "serverutil.h"
#include "settings.h"
#include "business/business_rule_processor.h"
#include "business/business_event_connector.h"

Q_GLOBAL_STATIC(QnServerMessageProcessor, QnServerMessageProcessor_instance)

QnServerMessageProcessor* QnServerMessageProcessor::instance()
{
    return QnServerMessageProcessor_instance();
}

void QnServerMessageProcessor::init(const QUrl& url, const QByteArray& authKey, int timeout)
{
    m_source = QSharedPointer<QnMessageSource>(new QnMessageSource(url, timeout));
	m_source->setAuthKey(authKey);

    connect(m_source.data(), SIGNAL(messageReceived(QnMessage)), this, SLOT(at_messageReceived(QnMessage)));
    connect(m_source.data(), SIGNAL(connectionOpened(QnMessage)), this, SLOT(at_connectionOpened(QnMessage)));
    connect(m_source.data(), SIGNAL(connectionClosed(QString)), this, SLOT(at_connectionClosed(QString)));
    connect(m_source.data(), SIGNAL(connectionReset()), this, SLOT(at_connectionReset()));

    connect(this, SIGNAL(businessRuleChanged(QnBusinessEventRulePtr)), qnBusinessRuleProcessor, SLOT(at_businessRuleChanged(QnBusinessEventRulePtr)));
    connect(this, SIGNAL(businessRuleDeleted(int)), qnBusinessRuleProcessor, SLOT(at_businessRuleDeleted(int)));
    connect(this, SIGNAL(businessRuleReset(QnBusinessEventRuleList)), qnBusinessRuleProcessor, SLOT(at_businessRuleReset(QnBusinessEventRuleList)));
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

void QnServerMessageProcessor::at_connectionOpened(QnMessage message)
{
    QnAppServerConnectionFactory::setSystemName(message.systemName);
    QnAppServerConnectionFactory::setPublicIp(message.publicIp);

    emit connectionOpened();
}

void QnServerMessageProcessor::at_messageReceived(QnMessage message)
{
    NX_LOG( QString::fromLatin1("Received message %1, resourceId %2, resource %3").
        arg(Qn::toString(message.messageType)).arg(message.resourceId.toString()).arg(message.resource ? message.resource->getName() : QString("NULL")), cl_logDEBUG1 );

    if (message.messageType == Qn::Message_Type_RuntimeInfoChange)
    {
        if (!message.publicIp.isNull())
            QnAppServerConnectionFactory::setPublicIp(message.publicIp);

        if (!message.systemName.isNull())
            QnAppServerConnectionFactory::setSystemName(message.systemName);
    }
    else if (message.messageType == Qn::Message_Type_License)
    {
        // New license added. LicensePool verifies it.
        qnLicensePool->addLicense(message.license);
    }
    else if (message.messageType == Qn::Message_Type_CameraServerItem)
    {
        QnCameraHistoryPool::instance()->addCameraHistoryItem(*message.cameraServerItem);
    }
    else if (message.messageType == Qn::Message_Type_ResourceChange)
    {
        QnResourcePtr resource = message.resource;

        QnMediaServerResourcePtr ownMediaServer = qnResPool->getResourceByGuid(serverGuid()).dynamicCast<QnMediaServerResource>();

        bool isServer = resource.dynamicCast<QnMediaServerResource>();
        bool isCamera = resource.dynamicCast<QnVirtualCameraResource>();
        bool isUser = resource.dynamicCast<QnUserResource>();

        if (!isServer && !isCamera && !isUser)
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

        QnResourcePtr ownResource = qnResPool->getResourceById(resource->getId(), QnResourcePool::AllResources);
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

    } else if (message.messageType == Qn::Message_Type_ResourceDisabledChange)
    {
        QnResourcePtr resource = qnResPool->getResourceById(message.resourceId);
        //ignoring messages for foreign resources

        if (resource)
        {
            resource->setDisabled(message.resourceDisabled);
            if (message.resourceDisabled) // we always ignore status changes 
                resource->setStatus(QnResource::Offline); 
        }
    } else if (message.messageType == Qn::Message_Type_ResourceDelete)
    {
        QnResourcePtr resource = qnResPool->getResourceById(message.resourceId, QnResourcePool::AllResources);

        if (resource)
        {
            qnResPool->removeResource(resource);
        }
    } else if (message.messageType == Qn::Message_Type_BusinessRuleInsertOrUpdate)
    {
       emit businessRuleChanged(message.businessRule);

    } else if (message.messageType == Qn::Message_Type_BusinessRuleDelete)
    {
        emit businessRuleDeleted(message.resourceId.toInt());
    } else if (message.messageType == Qn::Message_Type_BusinessRuleReset)
    {
       emit businessRuleReset(message.businessRules);

    } else if (message.messageType == Qn::Message_Type_BroadcastBusinessAction)
    {
        emit businessActionReceived(message.businessAction);
    }
}

void QnServerMessageProcessor::at_connectionClosed(QString errorString)
{
    qDebug() << "QnEventManager::connectionClosed(): Connection aborted:" << errorString;
}
