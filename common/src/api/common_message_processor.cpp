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

#include <core/resource_access/user_access_data.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/providers/resource_access_provider.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource_management/server_additional_addresses_dictionary.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/status_dictionary.h>
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

#include <nx/utils/log/log.h>


QnCommonMessageProcessor::QnCommonMessageProcessor(QObject *parent):
    base_type(parent),
    QnCommonModuleAware(parent)
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
    /* //TODO: #GDM #c++14 re-enable when generic lambdas will be supported
    auto on_resourceUpdated = [this](const auto& resource)
    {
        updateResource(resource);
    };
    */

    // Use direct connect for persistent transactions

#define on_resourceUpdated(Type) static_cast<void (QnCommonMessageProcessor::*)(const Type&, ec2::NotificationSource)>(&QnCommonMessageProcessor::updateResource)

    connect(connection, &ec2::AbstractECConnection::remotePeerFound,                this, &QnCommonMessageProcessor::on_remotePeerFound);
    connect(connection, &ec2::AbstractECConnection::remotePeerLost,                 this, &QnCommonMessageProcessor::on_remotePeerLost);
    connect(connection, &ec2::AbstractECConnection::initNotification,               this, &QnCommonMessageProcessor::on_gotInitialNotification);
    connect(connection, &ec2::AbstractECConnection::runtimeInfoChanged,             this, &QnCommonMessageProcessor::runtimeInfoChanged);

    auto resourceManager = connection->getResourceNotificationManager();
    connect(resourceManager, &ec2::AbstractResourceNotificationManager::statusChanged,          this, &QnCommonMessageProcessor::on_resourceStatusChanged, Qt::DirectConnection);
    connect(resourceManager, &ec2::AbstractResourceNotificationManager::resourceParamChanged,   this, &QnCommonMessageProcessor::on_resourceParamChanged, Qt::DirectConnection);
    connect(resourceManager, &ec2::AbstractResourceNotificationManager::resourceParamRemoved,   this, &QnCommonMessageProcessor::on_resourceParamRemoved, Qt::DirectConnection);
    connect(resourceManager, &ec2::AbstractResourceNotificationManager::resourceRemoved,        this, &QnCommonMessageProcessor::on_resourceRemoved, Qt::DirectConnection);

    auto mediaServerManager = connection->getMediaServerNotificationManager();
    connect(mediaServerManager, &ec2::AbstractMediaServerNotificationManager::addedOrUpdated,   this, on_resourceUpdated(ec2::ApiMediaServerData), Qt::DirectConnection);
    connect(mediaServerManager, &ec2::AbstractMediaServerNotificationManager::storageChanged,   this, on_resourceUpdated(ec2::ApiStorageData), Qt::DirectConnection);
    connect(mediaServerManager, &ec2::AbstractMediaServerNotificationManager::removed,          this, &QnCommonMessageProcessor::on_resourceRemoved, Qt::DirectConnection);
    connect(mediaServerManager, &ec2::AbstractMediaServerNotificationManager::storageRemoved,   this, &QnCommonMessageProcessor::on_resourceRemoved, Qt::DirectConnection);
    connect(mediaServerManager, &ec2::AbstractMediaServerNotificationManager::userAttributesChanged, this, &QnCommonMessageProcessor::on_mediaServerUserAttributesChanged, Qt::DirectConnection);
    connect(mediaServerManager, &ec2::AbstractMediaServerNotificationManager::userAttributesRemoved, this, &QnCommonMessageProcessor::on_mediaServerUserAttributesRemoved, Qt::DirectConnection);

    auto cameraManager = connection->getCameraNotificationManager();
    connect(cameraManager, &ec2::AbstractCameraNotificationManager::addedOrUpdated,             this, on_resourceUpdated(ec2::ApiCameraData), Qt::DirectConnection);
    connect(cameraManager, &ec2::AbstractCameraNotificationManager::userAttributesChanged,      this, &QnCommonMessageProcessor::on_cameraUserAttributesChanged, Qt::DirectConnection);
    connect(cameraManager, &ec2::AbstractCameraNotificationManager::userAttributesRemoved,      this, &QnCommonMessageProcessor::on_cameraUserAttributesRemoved, Qt::DirectConnection);
    connect(cameraManager, &ec2::AbstractCameraNotificationManager::cameraHistoryChanged,       this, &QnCommonMessageProcessor::on_cameraHistoryChanged, Qt::DirectConnection);
    connect(cameraManager, &ec2::AbstractCameraNotificationManager::removed,                    this, &QnCommonMessageProcessor::on_resourceRemoved, Qt::DirectConnection);

    auto userManager = connection->getUserNotificationManager();
    connect(userManager, &ec2::AbstractUserNotificationManager::addedOrUpdated,                 this, on_resourceUpdated(ec2::ApiUserData), Qt::DirectConnection);
    connect(userManager, &ec2::AbstractUserNotificationManager::removed,                        this, &QnCommonMessageProcessor::on_resourceRemoved, Qt::DirectConnection);
    connect(userManager, &ec2::AbstractUserNotificationManager::accessRightsChanged,            this, &QnCommonMessageProcessor::on_accessRightsChanged, Qt::DirectConnection);
    connect(userManager, &ec2::AbstractUserNotificationManager::userRoleAddedOrUpdated,         this, &QnCommonMessageProcessor::on_userRoleChanged, Qt::DirectConnection);
    connect(userManager, &ec2::AbstractUserNotificationManager::userRoleRemoved,                this, &QnCommonMessageProcessor::on_userRoleRemoved, Qt::DirectConnection);

    auto layoutManager = connection->getLayoutNotificationManager();
    connect(layoutManager, &ec2::AbstractLayoutNotificationManager::addedOrUpdated,             this, on_resourceUpdated(ec2::ApiLayoutData), Qt::DirectConnection);
    connect(layoutManager, &ec2::AbstractLayoutNotificationManager::removed,                    this, &QnCommonMessageProcessor::on_resourceRemoved, Qt::DirectConnection);

    auto videowallManager = connection->getVideowallNotificationManager();
    connect(videowallManager, &ec2::AbstractVideowallNotificationManager::addedOrUpdated,       this, on_resourceUpdated(ec2::ApiVideowallData), Qt::DirectConnection);
    connect(videowallManager, &ec2::AbstractVideowallNotificationManager::removed,              this, &QnCommonMessageProcessor::on_resourceRemoved, Qt::DirectConnection);
    connect(videowallManager, &ec2::AbstractVideowallNotificationManager::controlMessage,       this, &QnCommonMessageProcessor::videowallControlMessageReceived);

    auto webPageManager = connection->getWebPageNotificationManager();
    connect(webPageManager, &ec2::AbstractWebPageNotificationManager::addedOrUpdated,           this, on_resourceUpdated(ec2::ApiWebPageData), Qt::DirectConnection);
    connect(webPageManager, &ec2::AbstractWebPageNotificationManager::removed,                  this, &QnCommonMessageProcessor::on_resourceRemoved, Qt::DirectConnection);

    auto licenseManager = connection->getLicenseNotificationManager();
    connect(licenseManager, &ec2::AbstractLicenseNotificationManager::licenseChanged,           this, &QnCommonMessageProcessor::on_licenseChanged, Qt::DirectConnection);
    connect(licenseManager, &ec2::AbstractLicenseNotificationManager::licenseRemoved,           this, &QnCommonMessageProcessor::on_licenseRemoved, Qt::DirectConnection);

    auto eventManager = connection->getBusinessEventNotificationManager();
    connect(eventManager, &ec2::AbstractBusinessEventNotificationManager::addedOrUpdated,       this, &QnCommonMessageProcessor::on_businessEventAddedOrUpdated, Qt::DirectConnection);
    connect(eventManager, &ec2::AbstractBusinessEventNotificationManager::removed,              this, &QnCommonMessageProcessor::on_businessEventRemoved, Qt::DirectConnection);
    connect(eventManager, &ec2::AbstractBusinessEventNotificationManager::businessActionBroadcasted, this, &QnCommonMessageProcessor::on_businessActionBroadcasted);
    connect(eventManager, &ec2::AbstractBusinessEventNotificationManager::businessRuleReset,    this, &QnCommonMessageProcessor::on_businessRuleReset, Qt::DirectConnection);
    connect(eventManager, &ec2::AbstractBusinessEventNotificationManager::gotBroadcastAction,   this, &QnCommonMessageProcessor::on_broadcastBusinessAction);
    connect(eventManager, &ec2::AbstractBusinessEventNotificationManager::execBusinessAction,   this, &QnCommonMessageProcessor::on_execBusinessAction);

    auto storedFileManager = connection->getStoredFileNotificationManager();
    connect(storedFileManager, &ec2::AbstractStoredFileNotificationManager::added,              this, &QnCommonMessageProcessor::fileAdded, Qt::DirectConnection);
    connect(storedFileManager, &ec2::AbstractStoredFileNotificationManager::updated,            this, &QnCommonMessageProcessor::fileUpdated, Qt::DirectConnection);
    connect(storedFileManager, &ec2::AbstractStoredFileNotificationManager::removed,            this, &QnCommonMessageProcessor::fileRemoved, Qt::DirectConnection);

    auto timeManager = connection->getTimeNotificationManager();
    connect(timeManager, &ec2::AbstractTimeNotificationManager::timeServerSelectionRequired,    this, &QnCommonMessageProcessor::timeServerSelectionRequired);
    connect(timeManager, &ec2::AbstractTimeNotificationManager::timeChanged,                    this, &QnCommonMessageProcessor::syncTimeChanged);
    connect(timeManager, &ec2::AbstractTimeNotificationManager::peerTimeChanged,                this, &QnCommonMessageProcessor::peerTimeChanged);

    auto discoveryManager = connection->getDiscoveryNotificationManager();
    connect(discoveryManager, &ec2::AbstractDiscoveryNotificationManager::discoveryInformationChanged, this, &QnCommonMessageProcessor::on_gotDiscoveryData);
    connect(discoveryManager, &ec2::AbstractDiscoveryNotificationManager::discoveredServerChanged, this, &QnCommonMessageProcessor::discoveredServerChanged);

#undef on_resourceUpdated
}

void QnCommonMessageProcessor::disconnectFromConnection(const ec2::AbstractECConnectionPtr &connection)
{
    connection->disconnect(this);
    connection->getResourceNotificationManager()->disconnect(this);
    connection->getMediaServerNotificationManager()->disconnect(this);
    connection->getCameraNotificationManager()->disconnect(this);
    connection->getLicenseNotificationManager()->disconnect(this);
    connection->getBusinessEventNotificationManager()->disconnect(this);
    connection->getUserNotificationManager()->disconnect(this);
    connection->getLayoutNotificationManager()->disconnect(this);
    connection->getStoredFileNotificationManager()->disconnect(this);
    connection->getDiscoveryNotificationManager()->disconnect(this);
    connection->getTimeNotificationManager()->disconnect(this);
    connection->getMiscNotificationManager()->disconnect(this);
}

void QnCommonMessageProcessor::on_gotInitialNotification(const ec2::ApiFullInfoData& fullData)
{
    onGotInitialNotification(fullData);
    on_businessRuleReset(fullData.rules);
}

void QnCommonMessageProcessor::on_gotDiscoveryData(const ec2::ApiDiscoveryData &data, bool addInformation)
{
    if (data.id.isNull())
        return;

    QUrl url(data.url);

    QnMediaServerResourcePtr server = commonModule()->resourcePool()->getResourceById<QnMediaServerResource>(data.id);
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


void QnCommonMessageProcessor::on_resourceStatusChanged(
    const QnUuid& resourceId,
    Qn::ResourceStatus status,
    ec2::NotificationSource source)
{
    if (source == ec2::NotificationSource::Local)
        return; //< ignore local setStatus call. Data already in the resourcePool

    QnResourcePtr resource = commonModule()->resourcePool()->getResourceById(resourceId);
    if (resource)
        onResourceStatusChanged(resource, status, source);
    else
        qnStatusDictionary->setValue(resourceId, status);
}

void QnCommonMessageProcessor::on_resourceParamChanged(const ec2::ApiResourceParamWithRefData& param )
{
    QnResourcePtr resource = commonModule()->resourcePool()->getResourceById(param.resourceId);
    if (resource)
        resource->setProperty(param.name, param.value, QnResource::NO_MARK_DIRTY);
    else
        propertyDictionary->setValue(param.resourceId, param.name, param.value, false);
}

void QnCommonMessageProcessor::on_resourceParamRemoved(const ec2::ApiResourceParamWithRefData& param )
{
    QnResourcePtr resource = commonModule()->resourcePool()->getResourceById(param.resourceId);
    if (resource)
        resource->removeProperty(param.name);
    else
        propertyDictionary->removeProperty(param.resourceId, param.name);
}

void QnCommonMessageProcessor::on_resourceRemoved( const QnUuid& resourceId )
{
    if (canRemoveResource(resourceId))
    {
        if (QnResourcePtr ownResource = commonModule()->resourcePool()->getResourceById(resourceId))
        {
            // delete dependent objects
            for(const QnResourcePtr& subRes: commonModule()->resourcePool()->getResourcesByParentId(resourceId))
                commonModule()->resourcePool()->removeResource(subRes);
            commonModule()->resourcePool()->removeResource(ownResource);
        }
    }
    else
        removeResourceIgnored(resourceId);
}

void QnCommonMessageProcessor::on_accessRightsChanged(const ec2::ApiAccessRightsData& accessRights)
{
    QSet<QnUuid> accessibleResources;
    for (const QnUuid& id : accessRights.resourceIds)
        accessibleResources << id;
    if (auto user = commonModule()->resourcePool()->getResourceById<QnUserResource>(accessRights.userId))
    {
        qnSharedResourcesManager->setSharedResources(user, accessibleResources);
    }
    else
    {
        auto role = qnUserRolesManager->userRole(accessRights.userId);
        if (!role.isNull())
            qnSharedResourcesManager->setSharedResources(role, accessibleResources);
    }
}

void QnCommonMessageProcessor::on_userRoleChanged(const ec2::ApiUserRoleData& userRole)
{
    qnUserRolesManager->addOrUpdateUserRole(userRole);
}

void QnCommonMessageProcessor::on_userRoleRemoved(const QnUuid& userRoleId)
{
    qnUserRolesManager->removeUserRole(userRoleId);
    for (const auto& user : commonModule()->resourcePool()->getResources<QnUserResource>())
    {
        if (user->userRoleId() == userRoleId)
            user->setUserRoleId(QnUuid());
    }
}

void QnCommonMessageProcessor::on_cameraUserAttributesChanged(const ec2::ApiCameraAttributesData& attrs)
{
    QnCameraUserAttributesPtr userAttributes(new QnCameraUserAttributes());
    fromApiToResource(attrs, userAttributes);

    QSet<QByteArray> modifiedFields;
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), userAttributes->cameraId );
        (*userAttributesLock)->assign( *userAttributes, &modifiedFields );
    }
    const QnResourcePtr& res = commonModule()->resourcePool()->getResourceById(userAttributes->cameraId);
    if( res )   //it is OK if resource is missing
        res->emitModificationSignals( modifiedFields );
}

void QnCommonMessageProcessor::on_cameraUserAttributesRemoved(const QnUuid& cameraId)
{
    QSet<QByteArray> modifiedFields;
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), cameraId );
        //TODO #ak for now, never removing this structure, just resetting to empty value
        (*userAttributesLock)->assign( QnCameraUserAttributes(), &modifiedFields );
        (*userAttributesLock)->cameraId = cameraId;
    }
    const QnResourcePtr& res = commonModule()->resourcePool()->getResourceById(cameraId);
    if( res )
        res->emitModificationSignals( modifiedFields );
}

void QnCommonMessageProcessor::on_mediaServerUserAttributesChanged(const ec2::ApiMediaServerUserAttributesData& attrs)
{
    QnMediaServerUserAttributesPtr userAttributes(new QnMediaServerUserAttributes());
    fromApiToResource(attrs, userAttributes);

    QSet<QByteArray> modifiedFields;
    {
        QnMediaServerUserAttributesPool::ScopedLock lk( QnMediaServerUserAttributesPool::instance(), userAttributes->serverId );
        (*lk)->assign( *userAttributes, &modifiedFields );
    }
    const QnResourcePtr& res = commonModule()->resourcePool()->getResourceById(userAttributes->serverId);
    if( res )   //it is OK if resource is missing
        res->emitModificationSignals( modifiedFields );
}

void QnCommonMessageProcessor::on_mediaServerUserAttributesRemoved(const QnUuid& serverId)
{
    QSet<QByteArray> modifiedFields;
    {
        QnMediaServerUserAttributesPool::ScopedLock lk( QnMediaServerUserAttributesPool::instance(), serverId );
        //TODO #ak for now, never removing this structure, just resetting to empty value
        (*lk)->assign( QnMediaServerUserAttributes(), &modifiedFields );
    }
    const QnResourcePtr& res = commonModule()->resourcePool()->getResourceById(serverId);
    if( res )   //it is OK if resource is missing
        res->emitModificationSignals( modifiedFields );
}

void QnCommonMessageProcessor::on_cameraHistoryChanged(const ec2::ApiServerFootageData &serverFootageData)
{
    commonModule()->cameraHistoryPool()->setServerFootageData(serverFootageData);
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

template <class Datatype>
void QnCommonMessageProcessor::updateResources(
    const std::vector<Datatype>& resList,
    QHash<QnUuid, QnResourcePtr>& remoteResources)
{
    for (const auto& resource: resList)
    {
        updateResource(resource, ec2::NotificationSource::Remote);
        remoteResources.remove(resource.id);
    }
}

void QnCommonMessageProcessor::resetResources(const ec2::ApiFullInfoData& fullData)
{
    /* Store all remote resources id to clean them if they are not in the list anymore. */
    QHash<QnUuid, QnResourcePtr> remoteResources;
    for (const QnResourcePtr &resource: commonModule()->resourcePool()->getResourcesWithFlag(Qn::remote))
        remoteResources.insert(resource->getId(), resource);

    /* //TODO: #GDM #c++14 re-enable when generic lambdas will be supported
    auto updateResources = [this, &remoteResources](const auto& source)
    {
        for (const auto& resource : source)
        {
            updateResource(resource);
            remoteResources.remove(resource.id);
        }
    };
    */


    /* Packet adding. */
    commonModule()->resourcePool()->beginTran();

    updateResources(fullData.users, remoteResources);
    updateResources(fullData.cameras, remoteResources);
    updateResources(fullData.layouts, remoteResources);
    updateResources(fullData.videowalls, remoteResources);
    updateResources(fullData.webPages, remoteResources);
    updateResources(fullData.servers, remoteResources);
    updateResources(fullData.storages, remoteResources);

    commonModule()->resourcePool()->commit();

#undef updateResources

    /* Remove absent resources. */
    for (const QnResourcePtr& resource: remoteResources)
        commonModule()->resourcePool()->removeResource(resource);
}

void QnCommonMessageProcessor::resetLicenses(const ec2::ApiLicenseDataList& licenses)
{
    qnLicensePool->replaceLicenses(licenses);
}

void QnCommonMessageProcessor::resetCamerasWithArchiveList(const ec2::ApiServerFootageDataList& cameraHistoryList)
{
    commonModule()->cameraHistoryPool()->resetServerFootageData(cameraHistoryList);
}

void QnCommonMessageProcessor::resetTime()
{
    qnSyncTime->reset();

    if (!m_connection)
        return;

    auto timeManager = m_connection->getTimeManager(Qn::kSystemAccess);
    timeManager->getCurrentTime(this, [this](int handle, ec2::ErrorCode errCode, qint64 syncTime)
    {
        const auto& runtimeManager = commonModule()->runtimeInfoManager();
        Q_UNUSED(handle);
        if (errCode != ec2::ErrorCode::ok || !m_connection)
            return;

        emit syncTimeChanged(syncTime);

        ec2::QnPeerTimeInfoList peers = m_connection->getTimeManager(Qn::kSystemAccess)->getPeerTimeInfoList();
        for (const ec2::QnPeerTimeInfo &info : peers)
        {
            if (!runtimeManager->hasItem(info.peerId))
            {
                qWarning() << "Time for peer" << info.peerId << "received before peer was found";
                continue;
            }
            NX_ASSERT(ec2::ApiPeerData::isServer(runtimeManager->item(info.peerId).data.peer.peerType));
            emit peerTimeChanged(info.peerId, syncTime, info.time);
        }
    });
}

void QnCommonMessageProcessor::resetAccessRights(const ec2::ApiAccessRightsDataList& accessRights)
{
    qnSharedResourcesManager->reset(accessRights);
}

void QnCommonMessageProcessor::resetUserRoles(const ec2::ApiUserRoleDataList& roles)
{
    qnUserRolesManager->resetUserRoles(roles);
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

        QnMediaServerUserAttributesPool::ScopedLock userAttributesLock( QnMediaServerUserAttributesPool::instance(), serverAttrs.serverId );
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

        QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), cameraAttrs.cameraId );
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
        if (QnResourcePtr resource = commonModule()->resourcePool()->getResourceById(id))
        {
            NX_LOG(lit("%1 Emit statusChanged signal for resource %2, %3, %4")
                    .arg(QString::fromLatin1(Q_FUNC_INFO))
                    .arg(resource->getId().toString())
                    .arg(resource->getName())
                    .arg(resource->getUrl()), cl_logDEBUG2);
            emit resource->statusChanged(resource, Qn::StatusChangeReason::Local);
        }
    }

    for(const ec2::ApiResourceStatusData& statusData: params)
        on_resourceStatusChanged(statusData.id, statusData.status, ec2::NotificationSource::Remote);
}

void QnCommonMessageProcessor::onGotInitialNotification(const ec2::ApiFullInfoData& fullData)
{
    qDebug() << "start loading resources";
    QElapsedTimer tt;
    tt.start();

    commonModule()->resourceAccessManager()->beginUpdate();
    commonModule()->resourceAccessProvider()->beginUpdate();

    QnServerAdditionalAddressesDictionary::instance()->clear();

    resetResourceTypes(fullData.resourceTypes);
    resetResources(fullData);
    resetServerUserAttributesList(fullData.serversUserAttributesList);
    resetCameraUserAttributesList(fullData.cameraUserAttributesList);
    resetPropertyList(fullData.allProperties);
    resetCamerasWithArchiveList(fullData.cameraHistory);
    resetStatusList(fullData.resStatusList);
    resetAccessRights(fullData.accessRights);
    resetUserRoles(fullData.userRoles);
    resetLicenses(fullData.licenses);
    resetTime();

    qDebug() << "resources loaded for" << tt.elapsed();
    commonModule()->resourceAccessProvider()->endUpdate();
    qDebug() << "access ready" << tt.elapsed();
    commonModule()->resourceAccessManager()->endUpdate();
    qDebug() << "permissions ready" << tt.elapsed();

    emit initialResourcesReceived();
}

QMap<QnUuid, QnBusinessEventRulePtr> QnCommonMessageProcessor::businessRules() const {
    return m_rules;
}

void QnCommonMessageProcessor::updateResource(const QnResourcePtr&, ec2::NotificationSource /*source*/)
{
}

void QnCommonMessageProcessor::updateResource(const ec2::ApiUserData& user, ec2::NotificationSource source)
{
    QnUserResourcePtr qnUser(fromApiToResource(user));
    updateResource(qnUser, source);
}

void QnCommonMessageProcessor::updateResource(const ec2::ApiLayoutData& layout, ec2::NotificationSource source)
{
    QnLayoutResourcePtr qnLayout(new QnLayoutResource());
    if (!layout.url.isEmpty())
    {
        NX_LOG(lit("Invalid server layout with url %1").arg(layout.url), cl_logWARNING);
        auto fixed = layout;
        fixed.url = QString();
        fromApiToResource(fixed, qnLayout);
    }
    else
    {
        fromApiToResource(layout, qnLayout);
    }
    updateResource(qnLayout, source);
}

void QnCommonMessageProcessor::updateResource(const ec2::ApiVideowallData& videowall, ec2::NotificationSource source)
{
    QnVideoWallResourcePtr qnVideowall(new QnVideoWallResource());
    fromApiToResource(videowall, qnVideowall);
    updateResource(qnVideowall, source);
}

void QnCommonMessageProcessor::updateResource(const ec2::ApiWebPageData& webpage, ec2::NotificationSource source)
{
    QnWebPageResourcePtr qnWebpage(new QnWebPageResource());
    fromApiToResource(webpage, qnWebpage);
    updateResource(qnWebpage, source);
}

void QnCommonMessageProcessor::updateResource(const ec2::ApiCameraData& camera, ec2::NotificationSource source)
{
    QnVirtualCameraResourcePtr qnCamera = getResourceFactory()->createResource(camera.typeId,
            QnResourceParams(camera.id, camera.url, camera.vendor))
        .dynamicCast<QnVirtualCameraResource>();

    NX_ASSERT(qnCamera, Q_FUNC_INFO, QByteArray("Unknown resource type:") + camera.typeId.toByteArray());
    if (qnCamera)
    {
        fromApiToResource(camera, qnCamera);
        NX_ASSERT(camera.id == QnVirtualCameraResource::physicalIdToId(qnCamera->getUniqueId()),
            Q_FUNC_INFO,
            "You must fill camera ID as md5 hash of unique id");

        updateResource(qnCamera, source);
    }
}

void QnCommonMessageProcessor::updateResource(const ec2::ApiMediaServerData& server, ec2::NotificationSource source)
{
    QnMediaServerResourcePtr qnServer(new QnMediaServerResource());
    fromApiToResource(server, qnServer);
    updateResource(qnServer, source);
}

void QnCommonMessageProcessor::updateResource(const ec2::ApiStorageData& storage, ec2::NotificationSource source)
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
        updateResource(qnStorage, source);
    }
}
