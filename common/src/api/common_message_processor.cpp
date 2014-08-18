#include "common_message_processor.h"

#include <api/app_server_connection.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/videowall_resource.h>

#include <business/business_event_rule.h>

#include <core/resource_management/resource_pool.h>
#include "common/common_module.h"
#include "utils/common/synctime.h"
#include "runtime_info_manager.h"

QnCommonMessageProcessor::QnCommonMessageProcessor(QObject *parent) :
    QObject(parent)
{
}

void QnCommonMessageProcessor::init(const ec2::AbstractECConnectionPtr& connection)
{
    if (m_connection) {
        m_connection->disconnect(this);
    }
    m_connection = connection;

    if (!connection)
        return;

    connect( connection.get(), &ec2::AbstractECConnection::remotePeerFound, this, &QnCommonMessageProcessor::at_remotePeerFound );
    connect( connection.get(), &ec2::AbstractECConnection::remotePeerLost, this, &QnCommonMessageProcessor::at_remotePeerLost );

    connect( connection.get(), &ec2::AbstractECConnection::initNotification,
        this, &QnCommonMessageProcessor::on_gotInitialNotification );
    connect( connection.get(), &ec2::AbstractECConnection::runtimeInfoChanged,
        this, &QnCommonMessageProcessor::runtimeInfoChanged );

    connect( connection->getResourceManager().get(), &ec2::AbstractResourceManager::statusChanged,
        this, &QnCommonMessageProcessor::on_resourceStatusChanged );
    connect( connection->getResourceManager().get(), &ec2::AbstractResourceManager::resourceChanged,
        this, [this](const QnResourcePtr &resource){updateResource(resource);});
    connect( connection->getResourceManager().get(), &ec2::AbstractResourceManager::resourceParamsChanged,
        this, &QnCommonMessageProcessor::on_resourceParamsChanged );
    connect( connection->getResourceManager().get(), &ec2::AbstractResourceManager::resourceRemoved,
        this, &QnCommonMessageProcessor::on_resourceRemoved );

    connect( connection->getMediaServerManager().get(), &ec2::AbstractMediaServerManager::addedOrUpdated,
        this, [this](const QnMediaServerResourcePtr &server){updateResource(server);});
    connect( connection->getMediaServerManager().get(), &ec2::AbstractMediaServerManager::removed,
        this, &QnCommonMessageProcessor::on_resourceRemoved );

    connect( connection->getCameraManager().get(), &ec2::AbstractCameraManager::cameraAddedOrUpdated,
        this, [this](const QnVirtualCameraResourcePtr &camera){updateResource(camera);});
    connect( connection->getCameraManager().get(), &ec2::AbstractCameraManager::cameraHistoryChanged,
        this, &QnCommonMessageProcessor::on_cameraHistoryChanged );
    connect( connection->getCameraManager().get(), &ec2::AbstractCameraManager::cameraRemoved,
        this, &QnCommonMessageProcessor::on_resourceRemoved );
    connect( connection->getCameraManager().get(), &ec2::AbstractCameraManager::cameraBookmarkTagsAdded,
        this, &QnCommonMessageProcessor::cameraBookmarkTagsAdded );
    connect( connection->getCameraManager().get(), &ec2::AbstractCameraManager::cameraBookmarkTagsRemoved,
        this, &QnCommonMessageProcessor::cameraBookmarkTagsRemoved );

    connect( connection->getLicenseManager().get(), &ec2::AbstractLicenseManager::licenseChanged,
        this, &QnCommonMessageProcessor::on_licenseChanged );
    connect( connection->getLicenseManager().get(), &ec2::AbstractLicenseManager::licenseRemoved,
        this, &QnCommonMessageProcessor::on_licenseRemoved );

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
    connect( connection->getBusinessEventManager().get(), &ec2::AbstractBusinessEventManager::execBusinessAction,
        this, &QnCommonMessageProcessor::on_execBusinessAction );

    connect( connection->getUserManager().get(), &ec2::AbstractUserManager::addedOrUpdated,
        this, [this](const QnUserResourcePtr &user){updateResource(user);});
    connect( connection->getUserManager().get(), &ec2::AbstractUserManager::removed,
        this, &QnCommonMessageProcessor::on_resourceRemoved );

    connect( connection->getLayoutManager().get(), &ec2::AbstractLayoutManager::addedOrUpdated,
        this, [this](const QnLayoutResourcePtr &layout){updateResource(layout);});
    connect( connection->getLayoutManager().get(), &ec2::AbstractLayoutManager::removed,
        this, &QnCommonMessageProcessor::on_resourceRemoved );

    connect( connection->getStoredFileManager().get(), &ec2::AbstractStoredFileManager::added,
        this, &QnCommonMessageProcessor::fileAdded );
    connect( connection->getStoredFileManager().get(), &ec2::AbstractStoredFileManager::updated,
        this, &QnCommonMessageProcessor::fileUpdated );
    connect( connection->getStoredFileManager().get(), &ec2::AbstractStoredFileManager::removed,
        this, &QnCommonMessageProcessor::fileRemoved );

    connect( connection.get(), &ec2::AbstractECConnection::panicModeChanged,
        this, &QnCommonMessageProcessor::on_panicModeChanged );

    connect( connection->getVideowallManager().get(), &ec2::AbstractVideowallManager::addedOrUpdated,
        this, [this](const QnVideoWallResourcePtr &videowall){updateResource(videowall);});
    connect( connection->getVideowallManager().get(), &ec2::AbstractVideowallManager::removed,
        this, &QnCommonMessageProcessor::on_resourceRemoved );
    connect( connection->getVideowallManager().get(), &ec2::AbstractVideowallManager::controlMessage,
        this, &QnCommonMessageProcessor::videowallControlMessageReceived );  

    connect( connection.get(), &ec2::AbstractECConnection::remotePeerFound, this, &QnCommonMessageProcessor::remotePeerFound );
    connect( connection.get(), &ec2::AbstractECConnection::remotePeerLost, this, &QnCommonMessageProcessor::remotePeerLost );

    connection->startReceivingNotifications();
}

/*
* EC2 related processing. Need move to other class
*/

void QnCommonMessageProcessor::at_remotePeerFound(ec2::ApiPeerAliveData data)
{
    QnResourcePtr res = qnResPool->getResourceById(data.peer.id);
    if (res)
        res->setStatus(Qn::Online);

}

void QnCommonMessageProcessor::at_remotePeerLost(ec2::ApiPeerAliveData data)
{
    QnResourcePtr res = qnResPool->getResourceById(data.peer.id);
    if (res) {
        res->setStatus(Qn::Offline);
        if (data.peer.peerType != Qn::PT_Server) {
            // This server hasn't own DB
            foreach(QnResourcePtr camera, qnResPool->getAllCameras(res))
                camera->setStatus(Qn::Offline);
        }
    }
}


void QnCommonMessageProcessor::on_gotInitialNotification(const ec2::QnFullResourceData &fullData)
{
    onGotInitialNotification(fullData);
    on_businessRuleReset(fullData.bRules);
}


void QnCommonMessageProcessor::on_resourceStatusChanged( const QUuid& resourceId, Qn::ResourceStatus status )
{
    QnResourcePtr resource = qnResPool->getResourceById(resourceId);
    if (resource)
        onResourceStatusChanged(resource, status);
}

void QnCommonMessageProcessor::on_resourceParamsChanged( const QUuid& resourceId, const QnKvPairList& kvPairs )
{
    QnResourcePtr resource = qnResPool->getResourceById(resourceId);
    if (!resource)
        return;

    foreach (const QnKvPair &pair, kvPairs)
        if( pair.isPredefinedParam() )
            resource->setParam(pair.name(), pair.value(), QnDomainMemory);
        else
            resource->setProperty(pair.name(), pair.value());
}

void QnCommonMessageProcessor::on_resourceRemoved( const QUuid& resourceId )
{
    if (canRemoveResource(resourceId))
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
    else
        removeResourceIgnored(resourceId);
}

void QnCommonMessageProcessor::on_cameraHistoryChanged(const QnCameraHistoryItemPtr &cameraHistory) {
    QnCameraHistoryPool::instance()->addCameraHistoryItem(*cameraHistory.data());
}

void QnCommonMessageProcessor::on_licenseChanged(const QnLicensePtr &license) {
    qnLicensePool->addLicense(license);
}

void QnCommonMessageProcessor::on_licenseRemoved(const QnLicensePtr &license) {
    qnLicensePool->removeLicense(license);
}

void QnCommonMessageProcessor::on_businessEventAddedOrUpdated(const QnBusinessEventRulePtr &businessRule){
    m_rules[businessRule->id()] = businessRule;
    emit businessRuleChanged(businessRule);
}

void QnCommonMessageProcessor::on_businessEventRemoved(const QUuid &id) {
    m_rules.remove(id);
    emit businessRuleDeleted(id);
}

void QnCommonMessageProcessor::on_businessActionBroadcasted( const QnAbstractBusinessActionPtr& /* businessAction */ )
{
    // nothing to do for a while
}

void QnCommonMessageProcessor::on_businessRuleReset( const QnBusinessEventRuleList& rules )
{
    m_rules.clear();
    foreach(const QnBusinessEventRulePtr &bRule, rules)
        m_rules[bRule->id()] = bRule;

    emit businessRuleReset(rules);
}

void QnCommonMessageProcessor::on_broadcastBusinessAction( const QnAbstractBusinessActionPtr& action )
{
    emit businessActionReceived(action);
}

void QnCommonMessageProcessor::on_execBusinessAction( const QnAbstractBusinessActionPtr& action )
{
    execBusinessActionInternal(action);
}

void QnCommonMessageProcessor::on_panicModeChanged(Qn::PanicMode mode) {
    QnResourceList resList = qnResPool->getAllResourceByTypeName(lit("Server"));
    foreach(QnResourcePtr res, resList) {
        QnMediaServerResourcePtr mServer = res.dynamicCast<QnMediaServerResource>();
        if (mServer)
            mServer->setPanicMode(mode);
    }
}

// todo: ec2 relate logic. remove from this class
void QnCommonMessageProcessor::afterRemovingResource(const QUuid& id) {
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


void QnCommonMessageProcessor::processResources(const QnResourceList& resources)
{
    qnResPool->beginTran();
    foreach (const QnResourcePtr& resource, resources)
        updateResource(resource);
    qnResPool->commit();
}

void QnCommonMessageProcessor::processLicenses(const QnLicenseList& licenses)
{
    qnLicensePool->replaceLicenses(licenses);
}

void QnCommonMessageProcessor::processCameraServerItems(const QnCameraHistoryList& cameraHistoryList)
{
    foreach(QnCameraHistoryPtr history, cameraHistoryList)
        QnCameraHistoryPool::instance()->addCameraHistory(history);
}

bool QnCommonMessageProcessor::canRemoveResource(const QUuid &) 
{ 
    return true; 
}

void QnCommonMessageProcessor::removeResourceIgnored(const QUuid &) 
{
}

void QnCommonMessageProcessor::onGotInitialNotification(const ec2::QnFullResourceData& fullData)
{
    //QnAppServerConnectionFactory::setBox(fullData.serverInfo.platform);

    processResources(fullData.resources);
    processLicenses(fullData.licenses);
    processCameraServerItems(fullData.cameraHistory);
    //on_runtimeInfoChanged(fullData.serverInfo);
    qnSyncTime->reset();
}

QMap<QUuid, QnBusinessEventRulePtr> QnCommonMessageProcessor::businessRules() const {
    return m_rules;
}

void QnCommonMessageProcessor::updateResource(const QnResourcePtr &resource) 
{
    if (dynamic_cast<const QnMediaServerResource*>(resource.data()))
    {
        if (QnRuntimeInfoManager::instance()->hasItem(resource->getId()))
            resource->setStatus(Qn::Online);
        else
            resource->setStatus(Qn::Offline);
    }

}