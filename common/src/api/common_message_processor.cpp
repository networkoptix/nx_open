#include "common_message_processor.h"

#include <api/app_server_connection.h>
#include "core/resource/media_server_resource.h"
#include "core/resource/user_resource.h"
#include "core/resource/layout_resource.h"
#include "core/resource/camera_resource.h"

#include <business/business_event_rule.h>

#include <core/resource_management/resource_pool.h>
#include "common/common_module.h"

QnCommonMessageProcessor::QnCommonMessageProcessor(QObject *parent) :
    QObject(parent)
{
}

void QnCommonMessageProcessor::init(ec2::AbstractECConnectionPtr connection)
{
    if (m_connection) {
        m_connection->disconnect(this);
        //emit connectionClosed();
    }
    m_connection = connection;

    if (!connection)
        return;

    connect( connection.get(), &ec2::AbstractECConnection::initNotification,
        this, &QnCommonMessageProcessor::on_gotInitialNotification );
    connect( connection.get(), &ec2::AbstractECConnection::runtimeInfoChanged,
        this, &QnCommonMessageProcessor::on_runtimeInfoChanged );

    connect( connection->getResourceManager().get(), &ec2::AbstractResourceManager::statusChanged,
        this, &QnCommonMessageProcessor::on_resourceStatusChanged );
    connect( connection->getResourceManager().get(), &ec2::AbstractResourceManager::disabledChanged,
        this, &QnCommonMessageProcessor::on_resourceDisabledChanged );
    connect( connection->getResourceManager().get(), &ec2::AbstractResourceManager::resourceChanged,
        this, &QnCommonMessageProcessor::on_resourceChanged );
    connect( connection->getResourceManager().get(), &ec2::AbstractResourceManager::resourceParamsChanged,
        this, &QnCommonMessageProcessor::on_resourceParamsChanged );
    connect( connection->getResourceManager().get(), &ec2::AbstractResourceManager::resourceRemoved,
        this, &QnCommonMessageProcessor::on_resourceRemoved );

    connect( connection->getMediaServerManager().get(), &ec2::AbstractMediaServerManager::addedOrUpdated,
        this, &QnCommonMessageProcessor::on_mediaServerAddedOrUpdated );
    connect( connection->getMediaServerManager().get(), &ec2::AbstractMediaServerManager::removed,
        this, &QnCommonMessageProcessor::on_mediaServerRemoved );

    connect( connection->getCameraManager().get(), &ec2::AbstractCameraManager::cameraAddedOrUpdated,
        this, &QnCommonMessageProcessor::on_cameraAddedOrUpdated );
    connect( connection->getCameraManager().get(), &ec2::AbstractCameraManager::cameraHistoryChanged,
        this, &QnCommonMessageProcessor::on_cameraHistoryChanged );
    connect( connection->getCameraManager().get(), &ec2::AbstractCameraManager::cameraRemoved,
        this, &QnCommonMessageProcessor::on_cameraRemoved );

    connect( connection->getLicenseManager().get(), &ec2::AbstractLicenseManager::licenseChanged,
        this, &QnCommonMessageProcessor::on_licenseChanged );

    connect( connection->getBusinessEventManager().get(), &ec2::AbstractBusinessEventManager::addedOrUpdated,
        this, &QnCommonMessageProcessor::on_businessEventAddedOrUpdated );
    connect( connection->getBusinessEventManager().get(), &ec2::AbstractBusinessEventManager::removed,
        this, &QnCommonMessageProcessor::on_businessEventRemoved );
    connect( connection->getBusinessEventManager().get(), &ec2::AbstractBusinessEventManager::businessActionBroadcasted,
        this, &QnCommonMessageProcessor::on_businessActionBroadcasted );
    connect( connection->getBusinessEventManager().get(), &ec2::AbstractBusinessEventManager::businessRuleReset,
        this, &QnCommonMessageProcessor::on_businessRuleReset );
    connect( connection->getBusinessEventManager().get(), &ec2::AbstractBusinessEventManager::gotBroadcastAction,
        this, &QnCommonMessageProcessor::on_broadcastBusinessAction );

    connect( connection->getUserManager().get(), &ec2::AbstractUserManager::addedOrUpdated,
        this, &QnCommonMessageProcessor::on_userAddedOrUpdated );
    connect( connection->getUserManager().get(), &ec2::AbstractUserManager::removed,
        this, &QnCommonMessageProcessor::on_userRemoved );

    connect( connection->getLayoutManager().get(), &ec2::AbstractLayoutManager::addedOrUpdated,
        this, &QnCommonMessageProcessor::on_layoutAddedOrUpdated );
    connect( connection->getLayoutManager().get(), &ec2::AbstractLayoutManager::removed,
        this, &QnCommonMessageProcessor::on_layoutRemoved );

    connect( connection->getStoredFileManager().get(), &ec2::AbstractStoredFileManager::added,
        this, &QnCommonMessageProcessor::on_storedFileAdded );
    connect( connection->getStoredFileManager().get(), &ec2::AbstractStoredFileManager::updated,
        this, &QnCommonMessageProcessor::on_storedFileUpdated );
    connect( connection->getStoredFileManager().get(), &ec2::AbstractStoredFileManager::removed,
        this, &QnCommonMessageProcessor::on_storedFileRemoved );

    connect( connection.get(), &ec2::AbstractECConnection::panicModeChanged,
        this, &QnCommonMessageProcessor::on_panicModeChanged );

    connection->startReceivingNotifications(true);
}


void QnCommonMessageProcessor::on_gotInitialNotification(ec2::QnFullResourceData fullData)
{
    m_rules.clear();
    foreach(QnBusinessEventRulePtr bRule, fullData.bRules)
        m_rules[bRule->id()] = bRule;

    onGotInitialNotification(fullData);
}

void QnCommonMessageProcessor::on_runtimeInfoChanged( const ec2::QnRuntimeInfo& runtimeInfo )
{
    QnAppServerConnectionFactory::setSystemName(runtimeInfo.systemName);
    QnAppServerConnectionFactory::setPublicIp(runtimeInfo.publicIp);
    QnAppServerConnectionFactory::setSessionKey(runtimeInfo.sessionKey);
}

void QnCommonMessageProcessor::on_resourceStatusChanged( const QnId& resourceId, QnResource::Status status )
{
    QnResourcePtr resource = qnResPool->getResourceById(resourceId);
    if (resource)
        onResourceStatusChanged(resource, status);
}

void QnCommonMessageProcessor::on_resourceDisabledChanged( const QnId& resourceId, bool disabled )
{
    if (QnResourcePtr resource = qnResPool->getResourceById(resourceId)) {
        resource->setDisabled(disabled);
    }
}

void QnCommonMessageProcessor::on_resourceChanged( QnResourcePtr resource )
{
    updateResource(resource);
}

void QnCommonMessageProcessor::on_resourceParamsChanged( const QnId& resourceId, const QnKvPairList& kvPairs )
{
    QnResourcePtr resource = qnResPool->getResourceById(resourceId);
    if (!resource)
        return;

    foreach (const QnKvPair &pair, kvPairs)
        resource->setProperty(pair.name(), pair.value());
}

void QnCommonMessageProcessor::on_resourceRemoved( const QnId& resourceId )
{
    //beforeRemovingResource(resourceId);

    if (QnResourcePtr ownResource = qnResPool->getResourceById(resourceId)) 
    {
        // delete dependent objects
        foreach(QnResourcePtr subRes, qnResPool->getResourcesByParentId(resourceId))
            qnResPool->removeResource(subRes);
        qnResPool->removeResource(ownResource);
    }
    
    afterRemovingResource(resourceId);
}

void QnCommonMessageProcessor::on_mediaServerAddedOrUpdated( QnMediaServerResourcePtr mediaServer )
{
    on_resourceChanged( mediaServer );
}

void QnCommonMessageProcessor::on_mediaServerRemoved( QnId id )
{
    on_resourceRemoved( id );
}

void QnCommonMessageProcessor::on_cameraAddedOrUpdated( QnVirtualCameraResourcePtr camera )
{
    on_resourceChanged( camera );
}

void QnCommonMessageProcessor::on_cameraHistoryChanged( QnCameraHistoryItemPtr cameraHistory )
{
    QnCameraHistoryPool::instance()->addCameraHistoryItem(*cameraHistory.data());
}

void QnCommonMessageProcessor::on_cameraRemoved( QnId id )
{
    on_resourceRemoved( id );
}

void QnCommonMessageProcessor::on_licenseChanged(QnLicensePtr license)
{
    qnLicensePool->addLicense(license);
}

void QnCommonMessageProcessor::on_businessEventAddedOrUpdated( QnBusinessEventRulePtr businessRule )
{
    m_rules[businessRule->id()] = businessRule;
    emit businessRuleChanged(businessRule);
}

void QnCommonMessageProcessor::on_businessEventRemoved( QnId id )
{
    m_rules.remove(id);
    emit businessRuleDeleted(id);
}

void QnCommonMessageProcessor::on_businessActionBroadcasted( const QnAbstractBusinessActionPtr& businessAction )
{
    // nothing to do for a while
}

void QnCommonMessageProcessor::on_businessRuleReset( const QnBusinessEventRuleList& rules )
{
    m_rules.clear();
    foreach(QnBusinessEventRulePtr bRule, QnBusinessEventRule::getDefaultRules())
        m_rules[bRule->id()] = bRule;

    emit businessRuleReset(rules);
}

void QnCommonMessageProcessor::on_broadcastBusinessAction( const QnAbstractBusinessActionPtr& action )
{
    emit businessActionReceived(action);
}

void QnCommonMessageProcessor::on_userAddedOrUpdated( QnUserResourcePtr user )
{
    on_resourceChanged( user );
}

void QnCommonMessageProcessor::on_userRemoved( QnId id )
{
    on_resourceRemoved( id );
}

void QnCommonMessageProcessor::on_layoutAddedOrUpdated( QnLayoutResourcePtr layout )
{
    on_resourceChanged( layout );
}

void QnCommonMessageProcessor::on_layoutRemoved( QnId id )
{
    on_resourceRemoved( id );
}

void QnCommonMessageProcessor::on_storedFileAdded( QString filename )
{
    emit fileAdded(filename);
}

void QnCommonMessageProcessor::on_storedFileUpdated( QString filename )
{
    emit fileUpdated(filename);
}

void QnCommonMessageProcessor::on_storedFileRemoved( QString filename )
{
    emit fileRemoved(filename);
}

void QnCommonMessageProcessor::on_panicModeChanged(Qn::PanicMode mode)
{
    QnResourceList resList = qnResPool->getAllResourceByTypeName(lit("Server"));
    foreach(QnResourcePtr res, resList) {
        QnMediaServerResourcePtr mServer = res.dynamicCast<QnMediaServerResource>();
        if (mServer)
            mServer->setPanicMode(mode);
    }
}

// todo: ec2 relate logic. remove from this class

void QnCommonMessageProcessor::afterRemovingResource(const QnId& id)
{
    foreach(QnBusinessEventRulePtr bRule, m_rules.values())
    {
        if (bRule->eventResources().contains(id) || bRule->actionResources().contains(id))
        {
            QnBusinessEventRulePtr updatedRule(bRule->clone());
            updatedRule->removeResource(id);
            emit businessRuleChanged(updatedRule);
        }
    }
}
