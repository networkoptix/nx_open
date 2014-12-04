#include "common_message_processor.h"

#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_discovery_data.h>

#include <api/app_server_connection.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/videowall_resource.h>

#include <business/business_event_rule.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_user_attribute_pool.h>
#include <core/resource/media_server_user_attributes.h>
#include "common/common_module.h"
#include "utils/common/synctime.h"
#include "runtime_info_manager.h"
#include <utils/common/app_info.h>
#include "nx_ec/data/api_resource_data.h"
#include "core/resource_management/resource_properties.h"
#include "core/resource_management/status_dictionary.h"

QnCommonMessageProcessor::QnCommonMessageProcessor(QObject *parent) :
    base_type(parent)
{
}

void QnCommonMessageProcessor::init(const ec2::AbstractECConnectionPtr& connection)
{
    if (m_connection) {
        /* Safety check in case connection will not be deleted instantly. */
        m_connection->disconnect(this);
        m_connection->getResourceManager()->disconnect(this);
        m_connection->getMediaServerManager()->disconnect(this);
        m_connection->getCameraManager()->disconnect(this);
        m_connection->getLicenseManager()->disconnect(this);
        m_connection->getBusinessEventManager()->disconnect(this);
        m_connection->getUserManager()->disconnect(this);
        m_connection->getLayoutManager()->disconnect(this);
        m_connection->getStoredFileManager()->disconnect(this);
        m_connection->getDiscoveryManager()->disconnect(this);
        m_connection->getTimeManager()->disconnect(this);
    }
    m_connection = connection;

    if (!connection)
        return;

    connect(connection, &ec2::AbstractECConnection::remotePeerFound,                this, &QnCommonMessageProcessor::remotePeerFound );
    connect(connection, &ec2::AbstractECConnection::remotePeerLost,                 this, &QnCommonMessageProcessor::remotePeerLost );
    connect(connection, &ec2::AbstractECConnection::initNotification,               this, &QnCommonMessageProcessor::on_gotInitialNotification );
    connect(connection, &ec2::AbstractECConnection::runtimeInfoChanged,             this, &QnCommonMessageProcessor::runtimeInfoChanged );

    auto resourceManager = connection->getResourceManager();
    connect(resourceManager, &ec2::AbstractResourceManager::resourceChanged,        this, [this](const QnResourcePtr &resource){updateResource(resource);});
    connect(resourceManager, &ec2::AbstractResourceManager::statusChanged,          this, &QnCommonMessageProcessor::on_resourceStatusChanged );
    connect(resourceManager, &ec2::AbstractResourceManager::resourceParamChanged,   this, &QnCommonMessageProcessor::on_resourceParamChanged );
    connect(resourceManager, &ec2::AbstractResourceManager::resourceRemoved,        this, &QnCommonMessageProcessor::on_resourceRemoved );

    auto mediaServerManager = connection->getMediaServerManager();
    connect(mediaServerManager, &ec2::AbstractMediaServerManager::addedOrUpdated,   this, [this](const QnMediaServerResourcePtr &server){updateResource(server);});
    connect(mediaServerManager, &ec2::AbstractMediaServerManager::storageChanged,   this, [this](const QnAbstractStorageResourcePtr &storage){updateResource(storage);});
    connect(mediaServerManager, &ec2::AbstractMediaServerManager::removed,          this, &QnCommonMessageProcessor::on_resourceRemoved );
    connect(mediaServerManager, &ec2::AbstractMediaServerManager::storageRemoved,   this, &QnCommonMessageProcessor::on_resourceRemoved );
    connect(mediaServerManager, &ec2::AbstractMediaServerManager::userAttributesChanged, this, &QnCommonMessageProcessor::on_mediaServerUserAttributesChanged );
    connect(mediaServerManager, &ec2::AbstractMediaServerManager::userAttributesRemoved, this, &QnCommonMessageProcessor::on_mediaServerUserAttributesRemoved );

    auto cameraManager = connection->getCameraManager();
    connect(cameraManager, &ec2::AbstractCameraManager::cameraAddedOrUpdated,       this, [this](const QnVirtualCameraResourcePtr &camera){updateResource(camera);});
    connect(cameraManager, &ec2::AbstractCameraManager::userAttributesChanged,      this, &QnCommonMessageProcessor::on_cameraUserAttributesChanged );
    connect(cameraManager, &ec2::AbstractCameraManager::userAttributesRemoved,      this, &QnCommonMessageProcessor::on_cameraUserAttributesRemoved );
    connect(cameraManager, &ec2::AbstractCameraManager::cameraHistoryChanged,       this, &QnCommonMessageProcessor::on_cameraHistoryChanged );
    connect(cameraManager, &ec2::AbstractCameraManager::cameraHistoryRemoved,       this, &QnCommonMessageProcessor::on_cameraHistoryRemoved );
    connect(cameraManager, &ec2::AbstractCameraManager::cameraRemoved,              this, &QnCommonMessageProcessor::on_resourceRemoved );
    connect(cameraManager, &ec2::AbstractCameraManager::cameraBookmarkTagsAdded,    this, &QnCommonMessageProcessor::cameraBookmarkTagsAdded );
    connect(cameraManager, &ec2::AbstractCameraManager::cameraBookmarkTagsRemoved,  this, &QnCommonMessageProcessor::cameraBookmarkTagsRemoved );

    auto userManager = connection->getUserManager();
    connect(userManager, &ec2::AbstractUserManager::addedOrUpdated,                 this, [this](const QnUserResourcePtr &user){updateResource(user);});
    connect(userManager, &ec2::AbstractUserManager::removed,                        this, &QnCommonMessageProcessor::on_resourceRemoved );

    auto layoutManager = connection->getLayoutManager();
    connect(layoutManager, &ec2::AbstractLayoutManager::addedOrUpdated,             this, [this](const QnLayoutResourcePtr &layout){updateResource(layout);});
    connect(layoutManager, &ec2::AbstractLayoutManager::removed,                    this, &QnCommonMessageProcessor::on_resourceRemoved );

    auto videowallManager = connection->getVideowallManager();
    connect(videowallManager, &ec2::AbstractVideowallManager::addedOrUpdated,       this, [this](const QnVideoWallResourcePtr &videowall){updateResource(videowall);});
    connect(videowallManager, &ec2::AbstractVideowallManager::removed,              this, &QnCommonMessageProcessor::on_resourceRemoved );
    connect(videowallManager, &ec2::AbstractVideowallManager::controlMessage,       this, &QnCommonMessageProcessor::videowallControlMessageReceived );

    auto licenseManager = connection->getLicenseManager();
    connect(licenseManager, &ec2::AbstractLicenseManager::licenseChanged,           this, &QnCommonMessageProcessor::on_licenseChanged );
    connect(licenseManager, &ec2::AbstractLicenseManager::licenseRemoved,           this, &QnCommonMessageProcessor::on_licenseRemoved );

    auto eventManager = connection->getBusinessEventManager();
    connect(eventManager, &ec2::AbstractBusinessEventManager::addedOrUpdated,       this, &QnCommonMessageProcessor::on_businessEventAddedOrUpdated );
    connect(eventManager, &ec2::AbstractBusinessEventManager::removed,              this, &QnCommonMessageProcessor::on_businessEventRemoved );
    connect(eventManager, &ec2::AbstractBusinessEventManager::businessActionBroadcasted, this, &QnCommonMessageProcessor::on_businessActionBroadcasted );
    connect(eventManager, &ec2::AbstractBusinessEventManager::businessRuleReset,    this, &QnCommonMessageProcessor::on_businessRuleReset );
    connect(eventManager, &ec2::AbstractBusinessEventManager::gotBroadcastAction,   this, &QnCommonMessageProcessor::on_broadcastBusinessAction );
    connect(eventManager, &ec2::AbstractBusinessEventManager::execBusinessAction,   this, &QnCommonMessageProcessor::on_execBusinessAction );

    auto storedFileManager = connection->getStoredFileManager();
    connect(storedFileManager, &ec2::AbstractStoredFileManager::added,              this, &QnCommonMessageProcessor::fileAdded );
    connect(storedFileManager, &ec2::AbstractStoredFileManager::updated,            this, &QnCommonMessageProcessor::fileUpdated );
    connect(storedFileManager, &ec2::AbstractStoredFileManager::removed,            this, &QnCommonMessageProcessor::fileRemoved );

    auto timeManager = connection->getTimeManager();
    connect(timeManager, &ec2::AbstractTimeManager::timeServerSelectionRequired,    this, &QnCommonMessageProcessor::timeServerSelectionRequired);
    connect(timeManager, &ec2::AbstractTimeManager::timeChanged,                    this, &QnCommonMessageProcessor::syncTimeChanged);
    connect(timeManager, &ec2::AbstractTimeManager::peerTimeChanged,                this, &QnCommonMessageProcessor::peerTimeChanged);

    connect( connection->getDiscoveryManager(), &ec2::AbstractDiscoveryManager::discoveryInformationChanged,
        this, &QnCommonMessageProcessor::on_gotDiscoveryData );

    connection->startReceivingNotifications();
}

void QnCommonMessageProcessor::on_gotInitialNotification(const ec2::QnFullResourceData &fullData)
{
    onGotInitialNotification(fullData);
    on_businessRuleReset(fullData.bRules);
}


void QnCommonMessageProcessor::on_gotDiscoveryData(const ec2::ApiDiscoveryData &data, bool addInformation)
{
    QnMediaServerResourcePtr server = qnResPool->getResourceById(data.id).dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    QUrl url(data.url);

    QList<QHostAddress> addresses = server->getNetAddrList();
    QList<QUrl> additionalUrls = server->getAdditionalUrls();
    QList<QUrl> ignoredUrls = server->getIgnoredUrls();

    if (addInformation) {
        if (!data.ignore) {
            if (!additionalUrls.contains(url) && !addresses.contains(QHostAddress(url.host())))
                additionalUrls.append(url);
            ignoredUrls.removeOne(url);
        } else {
            if (!ignoredUrls.contains(url))
                ignoredUrls.append(url);
        }
    } else {
        additionalUrls.removeOne(url);
        ignoredUrls.removeOne(url);
    }
    server->setAdditionalUrls(additionalUrls);
    server->setIgnoredUrls(ignoredUrls);
}

void QnCommonMessageProcessor::on_resourceStatusChanged( const QnUuid& resourceId, Qn::ResourceStatus status )
{
    QnResourcePtr resource = qnResPool->getResourceById(resourceId);
    if (resource)
        onResourceStatusChanged(resource, status);
    else
        qnStatusDictionary->setValue(resourceId, status);
}

void QnCommonMessageProcessor::on_resourceParamChanged(const ec2::ApiResourceParamWithRefData& param )
{
    QnResourcePtr resource = qnResPool->getResourceById(param.resourceId);
    if (resource)
        resource->setProperty(param.name, param.value, false);
    else
        propertyDictionary->setValue(param.resourceId, param.name, param.value, false);
}

void QnCommonMessageProcessor::on_resourceRemoved( const QnUuid& resourceId )
{
    if (canRemoveResource(resourceId))
    {
        //beforeRemovingResource(resourceId);

        if (QnResourcePtr ownResource = qnResPool->getResourceById(resourceId)) 
        {
            // delete dependent objects
            for(const QnResourcePtr& subRes: qnResPool->getResourcesByParentId(resourceId))
                qnResPool->removeResource(subRes);
            qnResPool->removeResource(ownResource);
        }
    
        afterRemovingResource(resourceId);
    }
    else
        removeResourceIgnored(resourceId);
}

void QnCommonMessageProcessor::on_cameraUserAttributesChanged(const QnCameraUserAttributesPtr& userAttributes)
{
    QSet<QByteArray> modifiedFields;
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), userAttributes->cameraID );
        (*userAttributesLock)->assign( *userAttributes, &modifiedFields );
    }
    const QnResourcePtr& res = qnResPool->getResourceById(userAttributes->cameraID);
    if( res )   //it is OK if resource is missing
        res->emitModificationSignals( modifiedFields );
}

void QnCommonMessageProcessor::on_cameraUserAttributesRemoved(const QnUuid& cameraID)
{
    QSet<QByteArray> modifiedFields;
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), cameraID );
        //TODO #ak for now, never removing this structure, just resetting to empty value
        (*userAttributesLock)->assign( QnCameraUserAttributes(), &modifiedFields );
        (*userAttributesLock)->cameraID = cameraID;
    }
    const QnResourcePtr& res = qnResPool->getResourceById(cameraID);
    if( res )
        res->emitModificationSignals( modifiedFields );
}

void QnCommonMessageProcessor::on_mediaServerUserAttributesChanged(const QnMediaServerUserAttributesPtr& userAttributes)
{
    QSet<QByteArray> modifiedFields;
    {
        QnMediaServerUserAttributesPool::ScopedLock lk( QnMediaServerUserAttributesPool::instance(), userAttributes->serverID );
        (*lk)->assign( *userAttributes, &modifiedFields );
    }
    const QnResourcePtr& res = qnResPool->getResourceById(userAttributes->serverID);
    if( res )   //it is OK if resource is missing
        res->emitModificationSignals( modifiedFields );
}

void QnCommonMessageProcessor::on_mediaServerUserAttributesRemoved(const QnUuid& serverID)
{
    QSet<QByteArray> modifiedFields;
    {
        QnMediaServerUserAttributesPool::ScopedLock lk( QnMediaServerUserAttributesPool::instance(), serverID );
        //TODO #ak for now, never removing this structure, just resetting to empty value
        (*lk)->assign( QnMediaServerUserAttributes(), &modifiedFields );
    }
    const QnResourcePtr& res = qnResPool->getResourceById(serverID);
    if( res )   //it is OK if resource is missing
        res->emitModificationSignals( modifiedFields );
}

void QnCommonMessageProcessor::on_cameraHistoryChanged(const QnCameraHistoryItemPtr &cameraHistory) {
    QnCameraHistoryPool::instance()->addCameraHistoryItem(*cameraHistory.data());
}

void QnCommonMessageProcessor::on_cameraHistoryRemoved(const QnCameraHistoryItemPtr &cameraHistory) {
    QnCameraHistoryPool::instance()->removeCameraHistoryItem(*cameraHistory.data());
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

void QnCommonMessageProcessor::on_businessEventRemoved(const QnUuid &id) {
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
    for(const QnBusinessEventRulePtr &bRule: rules)
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

// todo: ec2 relate logic. remove from this class
void QnCommonMessageProcessor::afterRemovingResource(const QnUuid& id) {
    for(const QnBusinessEventRulePtr& bRule: m_rules.values())
    {
        if (bRule->eventResources().contains(id) || bRule->actionResources().contains(id))
        {
            QnBusinessEventRulePtr updatedRule(bRule->clone());
            updatedRule->removeResource(id);
            emit businessRuleChanged(updatedRule);
        }
    }
}


void QnCommonMessageProcessor::resetResources(const QnResourceList& resources) {
    /* Store all remote resources id to clean them if they are not in the list anymore. */
    QHash<QnUuid, QnResourcePtr> remoteResources;
    for (const QnResourcePtr &resource: qnResPool->getResourcesWithFlag(Qn::remote))
        remoteResources.insert(resource->getId(), resource);
    
    /* Remove all incompatible resources - they will be added if exist. */
    qnResPool->removeResources(qnResPool->getAllIncompatibleResources());

    /* Packet adding. */
    qnResPool->beginTran();
    for (const QnResourcePtr& resource: resources) {
        /* Update existing. */
        updateResource(resource);

        /* And keep them from removing. */
        remoteResources.remove(resource->getId());
    }
    qnResPool->commit();

    /* Remove absent resources. */
    for (const QnResourcePtr& resource: remoteResources)
        qnResPool->removeResource(resource);
}

void QnCommonMessageProcessor::resetLicenses(const QnLicenseList& licenses) {
    qnLicensePool->replaceLicenses(licenses);
}

void QnCommonMessageProcessor::resetCameraServerItems(const QnCameraHistoryList& cameraHistoryList) {
    QnCameraHistoryPool::instance()->resetCameraHistory(cameraHistoryList);
}

bool QnCommonMessageProcessor::canRemoveResource(const QnUuid &) 
{ 
    return true; 
}

void QnCommonMessageProcessor::removeResourceIgnored(const QnUuid &) 
{
}

void QnCommonMessageProcessor::resetServerUserAttributesList( const QnMediaServerUserAttributesList& serverUserAttributesList )
{
    for( const QnMediaServerUserAttributesPtr& serverAttrs: serverUserAttributesList )
    {
        QnMediaServerUserAttributesPool::ScopedLock userAttributesLock( QnMediaServerUserAttributesPool::instance(), serverAttrs->serverID );
        *(*userAttributesLock) = *serverAttrs;
    }
}

void QnCommonMessageProcessor::resetCameraUserAttributesList( const QnCameraUserAttributesList& cameraUserAttributesList )
{
    for( const QnCameraUserAttributesPtr& cameraAttrs: cameraUserAttributesList )
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), cameraAttrs->cameraID );
        *(*userAttributesLock) = *cameraAttrs;
    }
}

void QnCommonMessageProcessor::resetPropertyList(const ec2::ApiResourceParamWithRefDataList& params) {
    /* Store existing parameter keys. */
    auto existingProperties = propertyDictionary->allPropertyNamesByResource();

    /* Update changed values. */
    for(const ec2::ApiResourceParamWithRefData& param: params) {
        on_resourceParamChanged(param);
        if (existingProperties.contains(param.resourceId))
            existingProperties[param.resourceId].remove(param.name);
    }

    /* Clean values that are not in the list anymore. */
    for (auto iter = existingProperties.constBegin(); iter != existingProperties.constEnd(); ++iter) {
        QnUuid resourceId = iter.key();
        for (auto paramName: iter.value())
            on_resourceParamChanged(ec2::ApiResourceParamWithRefData(resourceId, paramName, QString()));
    }
}

void QnCommonMessageProcessor::resetStatusList(const ec2::ApiResourceStatusDataList& params) 
{
    auto keys = qnStatusDictionary->values().keys();
    qnStatusDictionary->clear();
    for(const QnUuid& id: keys) {
        if (QnResourcePtr resource = qnResPool->getResourceById(id))
            emit resource->statusChanged(resource);
    }

    for(const ec2::ApiResourceStatusData& statusData: params)
        on_resourceStatusChanged(statusData.id , statusData.status);
}

void QnCommonMessageProcessor::onGotInitialNotification(const ec2::QnFullResourceData& fullData)
{
    //QnAppServerConnectionFactory::setBox(fullData.serverInfo.platform);
    resetResources(fullData.resources);
    resetPropertyList(fullData.allProperties);
    resetServerUserAttributesList( fullData.serverUserAttributesList );
    resetCameraUserAttributesList( fullData.cameraUserAttributesList );
    resetLicenses(fullData.licenses);
    resetCameraServerItems(fullData.cameraHistory);
    resetStatusList(fullData.resStatusList);

    //on_runtimeInfoChanged(fullData.serverInfo);
    qnSyncTime->reset();   

    if (!m_connection)
        return;

    auto timeManager = m_connection->getTimeManager();
    timeManager->getCurrentTime(this, [this](int handle, ec2::ErrorCode errCode, qint64 syncTime) {
        Q_UNUSED(handle);
        if (errCode != ec2::ErrorCode::ok || !m_connection)
            return;

        emit syncTimeChanged(syncTime);

        ec2::QnPeerTimeInfoList peers = m_connection->getTimeManager()->getPeerTimeInfoList();
        for(const ec2::QnPeerTimeInfo &info: peers) {
            if( !QnRuntimeInfoManager::instance()->hasItem(info.peerId) ) {
                qWarning() << "Time for peer" << info.peerId << "received before peer was found";
                continue;
            }
            Q_ASSERT(QnRuntimeInfoManager::instance()->item(info.peerId).data.peer.peerType == Qn::PT_Server);
            emit peerTimeChanged(info.peerId, syncTime, info.time);
        }
    });

    emit initialResourcesReceived();
}

QMap<QnUuid, QnBusinessEventRulePtr> QnCommonMessageProcessor::businessRules() const {
    return m_rules;
}

void QnCommonMessageProcessor::updateResource(const QnResourcePtr &resource) {
    if (resource->getId() == qnCommon->moduleGUID()) {
        QnModuleInformation moduleInformation = qnCommon->moduleInformation();
        moduleInformation.name = resource->getName();
        qnCommon->setModuleInformation(moduleInformation);
    }
}
