#include "common_message_processor.h"

#include <nx_ec/ec_api.h>
#include <nx_ec/managers/abstract_user_manager.h>
#include <nx_ec/managers/abstract_layout_manager.h>
#include <nx_ec/managers/abstract_videowall_manager.h>
#include <nx_ec/managers/abstract_webpage_manager.h>
#include <nx_ec/managers/abstract_camera_manager.h>
#include <nx_ec/managers/abstract_server_manager.h>

#include <nx_ec/data/api_full_info_data.h>
#include <nx_ec/data/api_discovery_data.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/data/api_resource_type_data.h>
#include <nx_ec/data/api_license_data.h>
#include <nx_ec/data/api_business_rule_data.h>
#include <nx_ec/data/api_access_rights_data.h>

#include <api/app_server_connection.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/server_additional_addresses_dictionary.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/status_dictionary.h>
#include <core/resource_management/resource_access_manager.h>
#include <core/resource/camera_history.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource/camera_user_attribute_pool.h>
#include <core/resource/media_server_user_attributes.h>
#include <core/resource/storage_resource.h>
#include <core/resource/resource_factory.h>

#include <business/business_event_rule.h>

#include "common/common_module.h"
#include "utils/common/synctime.h"
#include <nx/network/socket_common.h>
#include "runtime_info_manager.h"
#include <utils/common/app_info.h>


QnCommonMessageProcessor::QnCommonMessageProcessor(QObject *parent) :
    base_type(parent)
{
}

void QnCommonMessageProcessor::init(const ec2::AbstractECConnectionPtr& connection)
{
    if (m_connection) {
        /* Safety check in case connection will not be deleted instantly. */
        m_connection->stopReceivingNotifications();
        disconnectFromConnection(m_connection);
    }
    m_connection = connection;

    if (!connection)
        return;

    connectToConnection(connection);
    connection->startReceivingNotifications();
}


void QnCommonMessageProcessor::connectToConnection(const ec2::AbstractECConnectionPtr &connection)
{
    auto on_resourceUpdated = [this](const auto& resource)
    {
        updateResource(resource);
    };

    connect(connection, &ec2::AbstractECConnection::remotePeerFound,                this, &QnCommonMessageProcessor::on_remotePeerFound );
    connect(connection, &ec2::AbstractECConnection::remotePeerLost,                 this, &QnCommonMessageProcessor::on_remotePeerLost );
    connect(connection, &ec2::AbstractECConnection::initNotification,               this, &QnCommonMessageProcessor::on_gotInitialNotification );
    connect(connection, &ec2::AbstractECConnection::runtimeInfoChanged,             this, &QnCommonMessageProcessor::runtimeInfoChanged );

    auto resourceManager = connection->getResourceManager();
    connect(resourceManager, &ec2::AbstractResourceManager::statusChanged,          this, &QnCommonMessageProcessor::on_resourceStatusChanged );
    connect(resourceManager, &ec2::AbstractResourceManager::resourceParamChanged,   this, &QnCommonMessageProcessor::on_resourceParamChanged );
    connect(resourceManager, &ec2::AbstractResourceManager::resourceRemoved,        this, &QnCommonMessageProcessor::on_resourceRemoved );

    auto mediaServerManager = connection->getMediaServerManager();
    connect(mediaServerManager, &ec2::AbstractMediaServerManager::addedOrUpdated,   this, on_resourceUpdated);
    connect(mediaServerManager, &ec2::AbstractMediaServerManager::storageChanged,   this, on_resourceUpdated);
    connect(mediaServerManager, &ec2::AbstractMediaServerManager::removed,          this, &QnCommonMessageProcessor::on_resourceRemoved );
    connect(mediaServerManager, &ec2::AbstractMediaServerManager::storageRemoved,   this, &QnCommonMessageProcessor::on_resourceRemoved );
    connect(mediaServerManager, &ec2::AbstractMediaServerManager::userAttributesChanged, this, &QnCommonMessageProcessor::on_mediaServerUserAttributesChanged );
    connect(mediaServerManager, &ec2::AbstractMediaServerManager::userAttributesRemoved, this, &QnCommonMessageProcessor::on_mediaServerUserAttributesRemoved );

    auto cameraManager = connection->getCameraManager();
    connect(cameraManager, &ec2::AbstractCameraManager::addedOrUpdated,             this, on_resourceUpdated);
    connect(cameraManager, &ec2::AbstractCameraManager::userAttributesChanged,      this, &QnCommonMessageProcessor::on_cameraUserAttributesChanged );
    connect(cameraManager, &ec2::AbstractCameraManager::userAttributesRemoved,      this, &QnCommonMessageProcessor::on_cameraUserAttributesRemoved );
    connect(cameraManager, &ec2::AbstractCameraManager::cameraHistoryChanged,       this, &QnCommonMessageProcessor::on_cameraHistoryChanged );
    connect(cameraManager, &ec2::AbstractCameraManager::removed,                    this, &QnCommonMessageProcessor::on_resourceRemoved );

    auto userManager = connection->getUserManager();
    connect(userManager, &ec2::AbstractUserManager::addedOrUpdated,                 this, on_resourceUpdated);
    connect(userManager, &ec2::AbstractUserManager::removed,                        this, &QnCommonMessageProcessor::on_resourceRemoved );

    auto layoutManager = connection->getLayoutManager();
    connect(layoutManager, &ec2::AbstractLayoutManager::addedOrUpdated,             this, on_resourceUpdated);
    connect(layoutManager, &ec2::AbstractLayoutManager::removed,                    this, &QnCommonMessageProcessor::on_resourceRemoved );

    auto videowallManager = connection->getVideowallManager();
    connect(videowallManager, &ec2::AbstractVideowallManager::addedOrUpdated,       this, on_resourceUpdated);
    connect(videowallManager, &ec2::AbstractVideowallManager::removed,              this, &QnCommonMessageProcessor::on_resourceRemoved );
    connect(videowallManager, &ec2::AbstractVideowallManager::controlMessage,       this, &QnCommonMessageProcessor::videowallControlMessageReceived );

    auto webPageManager = connection->getWebPageManager();
    connect(webPageManager, &ec2::AbstractWebPageManager::addedOrUpdated,           this, on_resourceUpdated);
    connect(webPageManager, &ec2::AbstractWebPageManager::removed,                  this, &QnCommonMessageProcessor::on_resourceRemoved );

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

    auto discoveryManager = connection->getDiscoveryManager();
    connect(discoveryManager, &ec2::AbstractDiscoveryManager::discoveryInformationChanged, this, &QnCommonMessageProcessor::on_gotDiscoveryData );
    connect(discoveryManager, &ec2::AbstractDiscoveryManager::discoveredServerChanged, this, &QnCommonMessageProcessor::discoveredServerChanged);
}

void QnCommonMessageProcessor::disconnectFromConnection(const ec2::AbstractECConnectionPtr &connection)
{
    connection->disconnect(this);
    connection->getResourceManager()->disconnect(this);
    connection->getMediaServerManager()->disconnect(this);
    connection->getCameraManager()->disconnect(this);
    connection->getLicenseManager()->disconnect(this);
    connection->getBusinessEventManager()->disconnect(this);
    connection->getUserManager()->disconnect(this);
    connection->getLayoutManager()->disconnect(this);
    connection->getStoredFileManager()->disconnect(this);
    connection->getDiscoveryManager()->disconnect(this);
    connection->getTimeManager()->disconnect(this);
    connection->getMiscManager()->disconnect(this);
}

void QnCommonMessageProcessor::on_gotInitialNotification(const ec2::ApiFullInfoData& fullData)
{
    onGotInitialNotification(fullData);
    on_businessRuleReset(fullData.rules);
}

void QnCommonMessageProcessor::on_gotDiscoveryData(const ec2::ApiDiscoveryData &data, bool addInformation)
{
    QUrl url(data.url);

    QnMediaServerResourcePtr server = qnResPool->getResourceById<QnMediaServerResource>(data.id);
    if (!server) {
        if (!data.ignore) {
            QList<QUrl> urls = QnServerAdditionalAddressesDictionary::instance()->additionalUrls(data.id);
            urls.append(url);
            QnServerAdditionalAddressesDictionary::instance()->setAdditionalUrls(data.id, urls);
        } else {
            QList<QUrl> urls = QnServerAdditionalAddressesDictionary::instance()->ignoredUrls(data.id);
            urls.append(url);
            QnServerAdditionalAddressesDictionary::instance()->setIgnoredUrls(data.id, urls);
        }
        return;
    }

    QList<SocketAddress> addresses = server->getNetAddrList();
    QList<QUrl> additionalUrls = server->getAdditionalUrls();
    QList<QUrl> ignoredUrls = server->getIgnoredUrls();

    if (addInformation) {
        if (!additionalUrls.contains(url) && !addresses.contains(SocketAddress(url.host(), url.port())))
            additionalUrls.append(url);

        if (data.ignore) {
            if (!ignoredUrls.contains(url))
                ignoredUrls.append(url);
        } else {
            ignoredUrls.removeOne(url);
        }
    } else {
        additionalUrls.removeOne(url);
        ignoredUrls.removeOne(url);
    }
    server->setAdditionalUrls(additionalUrls);
    server->setIgnoredUrls(ignoredUrls);
}

void QnCommonMessageProcessor::on_remotePeerFound(const ec2::ApiPeerAliveData &data) {
    handleRemotePeerFound(data);
    emit remotePeerFound(data);
}

void QnCommonMessageProcessor::on_remotePeerLost(const ec2::ApiPeerAliveData &data) {
    handleRemotePeerLost(data);
    emit remotePeerLost(data);
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
        resource->setProperty(param.name, param.value, QnResource::NO_MARK_DIRTY);
    else
        propertyDictionary->setValue(param.resourceId, param.name, param.value, false);
}

void QnCommonMessageProcessor::on_resourceRemoved( const QnUuid& resourceId )
{
    if (canRemoveResource(resourceId))
    {
        if (QnResourcePtr ownResource = qnResPool->getResourceById(resourceId))
        {
            // delete dependent objects
            for(const QnResourcePtr& subRes: qnResPool->getResourcesByParentId(resourceId))
                qnResPool->removeResource(subRes);
            qnResPool->removeResource(ownResource);
        }
    }
    else
        removeResourceIgnored(resourceId);
}

void QnCommonMessageProcessor::on_cameraUserAttributesChanged(const ec2::ApiCameraAttributesData& attrs)
{
    QnCameraUserAttributesPtr userAttributes(new QnCameraUserAttributes());
    fromApiToResource(attrs, userAttributes);

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

void QnCommonMessageProcessor::on_mediaServerUserAttributesChanged(const ec2::ApiMediaServerUserAttributesData& attrs)
{
    QnMediaServerUserAttributesPtr userAttributes(new QnMediaServerUserAttributes());
    fromApiToResource(attrs, userAttributes);

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

void QnCommonMessageProcessor::on_cameraHistoryChanged(const ec2::ApiServerFootageData &serverFootageData)
{
    qnCameraHistoryPool->setServerFootageData(serverFootageData);
}

void QnCommonMessageProcessor::on_licenseChanged(const QnLicensePtr &license) {
    qnLicensePool->addLicense(license);
}

void QnCommonMessageProcessor::on_licenseRemoved(const QnLicensePtr &license) {
    qnLicensePool->removeLicense(license);
}

void QnCommonMessageProcessor::on_businessEventAddedOrUpdated(const QnBusinessEventRulePtr &businessRule){
    m_rules.insert(businessRule->id(), businessRule);
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

void QnCommonMessageProcessor::on_businessRuleReset( const ec2::ApiBusinessRuleDataList& rules )
{
    QnBusinessEventRuleList qnRules;
    fromApiToResourceList(rules, qnRules);

    m_rules.clear();
    for(const QnBusinessEventRulePtr &bRule: qnRules)
        m_rules[bRule->id()] = bRule;

    emit businessRuleReset(qnRules);
}

void QnCommonMessageProcessor::on_broadcastBusinessAction( const QnAbstractBusinessActionPtr& action )
{
    emit businessActionReceived(action);
}

void QnCommonMessageProcessor::on_execBusinessAction( const QnAbstractBusinessActionPtr& action )
{
    execBusinessActionInternal(action);
}

void QnCommonMessageProcessor::resetResourceTypes(const ec2::ApiResourceTypeDataList& resTypes)
{
    QnResourceTypeList qnResTypes;
    fromApiToResourceList(resTypes, qnResTypes);
    qnResTypePool->replaceResourceTypeList(qnResTypes);
}

void QnCommonMessageProcessor::resetResources(const ec2::ApiFullInfoData& fullData)
{
    /* Store all remote resources id to clean them if they are not in the list anymore. */
    QHash<QnUuid, QnResourcePtr> remoteResources;
    for (const QnResourcePtr &resource: qnResPool->getResourcesWithFlag(Qn::remote))
        remoteResources.insert(resource->getId(), resource);

    auto updateResources = [this, &remoteResources](const auto& source)
    {
        for (const auto& resource : source)
        {
            updateResource(resource);
            remoteResources.remove(resource.id);
        }
    };

    /* Packet adding. */
    qnResPool->beginTran();
    updateResources(fullData.users);
    updateResources(fullData.cameras);
    updateResources(fullData.layouts);
    updateResources(fullData.videowalls);
    updateResources(fullData.webPages);
    updateResources(fullData.servers);
    updateResources(fullData.storages);

    qnResPool->commit();

    /* Remove absent resources. */
    for (const QnResourcePtr& resource: remoteResources)
        qnResPool->removeResource(resource);
}

void QnCommonMessageProcessor::resetLicenses(const ec2::ApiLicenseDataList& licenses)
{
    qnLicensePool->replaceLicenses(licenses);
}

void QnCommonMessageProcessor::resetCamerasWithArchiveList(const ec2::ApiServerFootageDataList& cameraHistoryList) {
    qnCameraHistoryPool->resetServerFootageData(cameraHistoryList);
}

void QnCommonMessageProcessor::resetTime()
{
    qnSyncTime->reset();

    if (!m_connection)
        return;

    auto timeManager = m_connection->getTimeManager();
    timeManager->getCurrentTime(this, [this](int handle, ec2::ErrorCode errCode, qint64 syncTime)
    {
        Q_UNUSED(handle);
        if (errCode != ec2::ErrorCode::ok || !m_connection)
            return;

        emit syncTimeChanged(syncTime);

        ec2::QnPeerTimeInfoList peers = m_connection->getTimeManager()->getPeerTimeInfoList();
        for (const ec2::QnPeerTimeInfo &info : peers)
        {
            if (!QnRuntimeInfoManager::instance()->hasItem(info.peerId))
            {
                qWarning() << "Time for peer" << info.peerId << "received before peer was found";
                continue;
            }
            Q_ASSERT(QnRuntimeInfoManager::instance()->item(info.peerId).data.peer.peerType == Qn::PT_Server);
            emit peerTimeChanged(info.peerId, syncTime, info.time);
        }
    });
}

void QnCommonMessageProcessor::resetAccessRights(const ec2::ApiAccessRightsDataList& accessRights)
{
    qnResourceAccessManager->reset(accessRights);
}

bool QnCommonMessageProcessor::canRemoveResource(const QnUuid &)
{
    return true;
}

void QnCommonMessageProcessor::removeResourceIgnored(const QnUuid &)
{
}

void QnCommonMessageProcessor::handleRemotePeerFound(const ec2::ApiPeerAliveData &data) {
    Q_UNUSED(data)
}

void QnCommonMessageProcessor::handleRemotePeerLost(const ec2::ApiPeerAliveData &data) {
    Q_UNUSED(data)
}


void QnCommonMessageProcessor::resetServerUserAttributesList( const ec2::ApiMediaServerUserAttributesDataList& serverUserAttributesList )
{
    QnMediaServerUserAttributesPool::instance()->clear();
    for( const auto& serverAttrs: serverUserAttributesList )
    {
        QnMediaServerUserAttributesPtr dstElement(new QnMediaServerUserAttributes());
        fromApiToResource(serverAttrs, dstElement);

        QnMediaServerUserAttributesPool::ScopedLock userAttributesLock( QnMediaServerUserAttributesPool::instance(), serverAttrs.serverID );
        *(*userAttributesLock) = *dstElement;
    }
}

void QnCommonMessageProcessor::resetCameraUserAttributesList( const ec2::ApiCameraAttributesDataList& cameraUserAttributesList )
{
    QnCameraUserAttributePool::instance()->clear();
    for( const auto & cameraAttrs: cameraUserAttributesList )
    {
        QnCameraUserAttributesPtr dstElement(new QnCameraUserAttributes());
        fromApiToResource(cameraAttrs, dstElement);

        QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), cameraAttrs.cameraID );
        *(*userAttributesLock) = *dstElement;
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

void QnCommonMessageProcessor::onGotInitialNotification(const ec2::ApiFullInfoData& fullData)
{
    QnServerAdditionalAddressesDictionary::instance()->clear();

    resetResourceTypes(fullData.resourceTypes);
    resetResources(fullData);
    resetServerUserAttributesList(fullData.serversUserAttributesList);
    resetCameraUserAttributesList(fullData.cameraUserAttributesList);
    resetPropertyList(fullData.allProperties);
    resetCamerasWithArchiveList(fullData.cameraHistory);
    resetStatusList(fullData.resStatusList);
    resetAccessRights(fullData.accessRights);

    resetLicenses(fullData.licenses);
    resetTime();

    emit initialResourcesReceived();
}

QMap<QnUuid, QnBusinessEventRulePtr> QnCommonMessageProcessor::businessRules() const {
    return m_rules;
}

void QnCommonMessageProcessor::updateResource(const QnResourcePtr& )
{
}

void QnCommonMessageProcessor::updateResource(const ec2::ApiUserData& user)
{
    QnUserResourcePtr qnUser(new QnUserResource());
    fromApiToResource(user, qnUser);
    updateResource(qnUser);
}

void QnCommonMessageProcessor::updateResource(const ec2::ApiLayoutData& layout)
{
    QnLayoutResourcePtr qnLayout(new QnLayoutResource());
    fromApiToResource(layout, qnLayout);
    updateResource(qnLayout);
}

void QnCommonMessageProcessor::updateResource(const ec2::ApiVideowallData& videowall)
{
    QnVideoWallResourcePtr qnVideowall(new QnVideoWallResource());
    fromApiToResource(videowall, qnVideowall);
    updateResource(qnVideowall);
}

void QnCommonMessageProcessor::updateResource(const ec2::ApiWebPageData& webpage)
{
    QnWebPageResourcePtr qnWebpage(new QnWebPageResource());
    fromApiToResource(webpage, qnWebpage);
    updateResource(qnWebpage);
}

void QnCommonMessageProcessor::updateResource(const ec2::ApiCameraData& camera)
{
    QnVirtualCameraResourcePtr qnCamera = getResourceFactory()->createResource(camera.typeId,
            QnResourceParams(camera.id, camera.url, camera.vendor))
        .dynamicCast<QnVirtualCameraResource>();

    NX_ASSERT(qnCamera, Q_FUNC_INFO, QByteArray("Unknown resource type:") + camera.typeId.toByteArray());
    if (qnCamera)
    {
        fromApiToResource(camera, qnCamera);
        NX_ASSERT(camera.id == QnVirtualCameraResource::uniqueIdToId(qnCamera->getUniqueId()),
            Q_FUNC_INFO,
            "You must fill camera ID as md5 hash of unique id");

        updateResource(qnCamera);
    }
}

void QnCommonMessageProcessor::updateResource(const ec2::ApiMediaServerData& server)
{
    QnMediaServerResourcePtr qnServer(new QnMediaServerResource());
    fromApiToResource(server, qnServer);
    updateResource(qnServer);
}

void QnCommonMessageProcessor::updateResource(const ec2::ApiStorageData& storage)
{
    auto resTypeId = qnResTypePool->getFixedResourceTypeId(QnResourceTypePool::kStorageTypeId);
    NX_ASSERT(!resTypeId.isNull(), Q_FUNC_INFO, "Invalid resource type pool state");
    if (resTypeId.isNull())
        return;

    QnStorageResourcePtr qnStorage = getResourceFactory()->createResource(resTypeId,
            QnResourceParams(storage.id, storage.url, QString()))
        .dynamicCast<QnStorageResource>();
    NX_ASSERT(qnStorage, Q_FUNC_INFO, "Invalid resource type pool state");
    if (qnStorage)
    {
        fromApiToResource(storage, qnStorage);
        updateResource(qnStorage);
    }
}
