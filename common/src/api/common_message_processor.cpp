#include "common_message_processor.h"

#include <nx_ec/data/api_discovery_data.h>
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

#include "version.h"

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

    connect( connection->getDiscoveryManager().get(), &ec2::AbstractDiscoveryManager::discoveryInformationChanged,
        this, &QnCommonMessageProcessor::on_gotDiscoveryData );

    connect( connection.get(), &ec2::AbstractECConnection::remotePeerFound, this, &QnCommonMessageProcessor::at_remotePeerFound );
    connect( connection.get(), &ec2::AbstractECConnection::remotePeerLost, this, &QnCommonMessageProcessor::at_remotePeerLost );

    connection->startReceivingNotifications();
}

void QnCommonMessageProcessor::at_remotePeerFound(ec2::ApiPeerAliveData data, bool /*isProxy*/)
{
    if (data.peerType == static_cast<int>(ec2::QnPeerInfo::Server))   //TODO: #Elric #ec2 get rid of the serialization hell
        qnLicensePool->addRemoteHardwareIds(data.peerId, data.hardwareIds);
}

void QnCommonMessageProcessor::at_remotePeerLost(ec2::ApiPeerAliveData data, bool /*isProxy*/)
{
    qnLicensePool->removeRemoteHardwareIds(data.peerId);
}

void QnCommonMessageProcessor::on_gotInitialNotification(const ec2::QnFullResourceData &fullData)
{
    m_rules.clear();
    foreach(QnBusinessEventRulePtr bRule, fullData.bRules)
        m_rules[bRule->id()] = bRule;

    onGotInitialNotification(fullData);
}

void QnCommonMessageProcessor::on_runtimeInfoChanged( const ec2::ApiServerInfoData& runtimeInfo )
{
    QnAppServerConnectionFactory::setPublicIp(runtimeInfo.publicIp);
    QnAppServerConnectionFactory::setSessionKey(runtimeInfo.sessionKey);
    QnAppServerConnectionFactory::setPrematureLicenseExperationDate(runtimeInfo.prematureLicenseExperationDate);
}

void QnCommonMessageProcessor::on_gotDiscoveryData(const ec2::ApiDiscoveryDataList &discoveryData, bool addInformation)
{
    QMultiHash<QnId, QUrl> m_additionalUrls;
    QMultiHash<QnId, QUrl> m_ignoredUrls;

    foreach (const ec2::ApiDiscoveryData &data, discoveryData) {
        if (data.ignore)
            m_ignoredUrls.insert(data.id, data.url);
        else
            m_additionalUrls.insert(data.id, data.url);
    }

    foreach (const QnId &id, m_additionalUrls.uniqueKeys()) {
        QnMediaServerResourcePtr server = qnResPool->getResourceById(id).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        if (addInformation) {
            server->setAdditionalUrls(m_additionalUrls.values(id));
        } else {
            QList<QUrl> urlsToRemove = m_additionalUrls.values(id);
            QList<QUrl> urls;
            foreach (const QUrl &url, server->getAdditionalUrls()) {
                if (!urlsToRemove.contains(url))
                    urls.append(url);
            }
            server->setAdditionalUrls(urls);
        }
    }

    foreach (const QnId &id, m_ignoredUrls.uniqueKeys()) {
        QnMediaServerResourcePtr server = qnResPool->getResourceById(id).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        if (addInformation) {
            server->setIgnoredUrls(m_ignoredUrls.values(id));
        } else {
            QList<QUrl> urlsToRemove = m_ignoredUrls.values(id);
            QList<QUrl> urls;
            foreach (const QUrl &url, server->getIgnoredUrls()) {
                if (!urlsToRemove.contains(url))
                    urls.append(url);
            }
            server->setIgnoredUrls(urls);
        }
    }
}

void QnCommonMessageProcessor::on_resourceStatusChanged( const QnId& resourceId, QnResource::Status status )
{
    QnResourcePtr resource = qnResPool->getResourceById(resourceId);
    if (resource)
        onResourceStatusChanged(resource, status);
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

void QnCommonMessageProcessor::on_cameraHistoryChanged(const QnCameraHistoryItemPtr &cameraHistory) {
    QnCameraHistoryPool::instance()->addCameraHistoryItem(*cameraHistory.data());
}

void QnCommonMessageProcessor::on_licenseChanged(const QnLicensePtr &license) {
    qnLicensePool->addLicense(license);
}

void QnCommonMessageProcessor::on_businessEventAddedOrUpdated(const QnBusinessEventRulePtr &businessRule){
    m_rules[businessRule->id()] = businessRule;
    emit businessRuleChanged(businessRule);
}

void QnCommonMessageProcessor::on_businessEventRemoved(const QnId &id) {
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
    foreach(QnBusinessEventRulePtr bRule, rules)
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
void QnCommonMessageProcessor::afterRemovingResource(const QnId& id) {
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

void QnCommonMessageProcessor::updateHardwareIds(const ec2::QnFullResourceData& fullData)
{
    qnLicensePool->setMainHardwareIds(fullData.serverInfo.mainHardwareIds);
    qnLicensePool->setCompatibleHardwareIds(fullData.serverInfo.compatibleHardwareIds);
    qnLicensePool->setRemoteHardwareIds( fullData.serverInfo.remoteHardwareIds);
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


void QnCommonMessageProcessor::onGotInitialNotification(const ec2::QnFullResourceData& fullData)
{
    QnAppServerConnectionFactory::setBox(fullData.serverInfo.platform);

    updateHardwareIds(fullData);
    processResources(fullData.resources);
    processLicenses(fullData.licenses);
    processCameraServerItems(fullData.cameraHistory);
    on_runtimeInfoChanged(fullData.serverInfo);
    qnSyncTime->reset();
}
