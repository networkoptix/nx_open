#include "common_message_processor.h"
#include "runtime_info_manager.h"

#include <QtCore/QElapsedTimer>

#include <api/app_server_connection.h>
#include <common/common_module.h>
#include <core/resource_access/user_access_data.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource_management/server_additional_addresses_dictionary.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/status_dictionary.h>
#include <core/resource_management/layout_tour_manager.h>
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

#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>

#include <utils/common/synctime.h>
#include <utils/common/app_info.h>

#include <nx_ec/ec_api.h>
#include <nx_ec/dummy_handler.h>
#include <nx_ec/managers/abstract_user_manager.h>
#include <nx_ec/managers/abstract_layout_manager.h>
#include <nx_ec/managers/abstract_videowall_manager.h>
#include <nx_ec/managers/abstract_webpage_manager.h>
#include <nx_ec/managers/abstract_camera_manager.h>
#include <nx_ec/managers/abstract_server_manager.h>
#include <nx_ec/managers/abstract_analytics_manager.h>
#include <nx_ec/data/api_conversion_functions.h>

#include <nx/network/socket_common.h>
#include <nx/vms/time_sync/abstract_time_sync_manager.h>
#include <nx/vms/api/data/access_rights_data.h>
#include <nx/vms/api/data/discovery_data.h>
#include <nx/vms/api/data/event_rule_data.h>
#include <nx/vms/api/data/full_info_data.h>
#include <nx/vms/api/data/license_data.h>
#include <nx/vms/api/data/resource_type_data.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>
#include <nx/utils/log/log.h>
#include <core/resource_management/resource_data_pool.h>

using namespace nx;
using namespace nx::vms::api;

QnCommonMessageProcessor::QnCommonMessageProcessor(QObject *parent):
    base_type(parent),
    QnCommonModuleAware(parent)
{
}

void QnCommonMessageProcessor::init(const ec2::AbstractECConnectionPtr& connection)
{
    if (m_connection)
    {
        // Safety check in case connection will not be deleted instantly.
        m_connection->stopReceivingNotifications();
        qnSyncTime->setTimeNotificationManager(nullptr);
        disconnectFromConnection(m_connection);
    }
    m_connection = connection;

    if (!connection)
        return;

    qnSyncTime->setTimeNotificationManager(connection->timeNotificationManager());
    connectToConnection(connection);
    connection->startReceivingNotifications();
}

ec2::AbstractECConnectionPtr QnCommonMessageProcessor::connection() const
{
    return m_connection;
}

Qt::ConnectionType QnCommonMessageProcessor::handlerConnectionType() const
{
    return Qt::DirectConnection;
}

void QnCommonMessageProcessor::connectToConnection(const ec2::AbstractECConnectionPtr& connection)
{
    const auto on_resourceUpdated =
        [this](const auto& resource, ec2::NotificationSource source)
        {
            updateResource(resource, source);
        };

    const auto connectionType = handlerConnectionType();

    connect(
        connection,
        &ec2::AbstractECConnection::remotePeerFound,
        this,
        &QnCommonMessageProcessor::on_remotePeerFound,
        connectionType);
    connect(
        connection,
        &ec2::AbstractECConnection::remotePeerLost,
        this,
        &QnCommonMessageProcessor::on_remotePeerLost,
        connectionType);
    connect(
        connection,
        &ec2::AbstractECConnection::initNotification,
        this,
        &QnCommonMessageProcessor::on_gotInitialNotification,
        connectionType);
    connect(
        connection,
        &ec2::AbstractECConnection::runtimeInfoChanged,
        this,
        &QnCommonMessageProcessor::runtimeInfoChanged,
        connectionType);
    connect(
        connection,
        &ec2::AbstractECConnection::runtimeInfoRemoved,
        this,
        &QnCommonMessageProcessor::runtimeInfoRemoved,
        connectionType);

    const auto resourceManager = connection->resourceNotificationManager();
    connect(
        resourceManager,
        &ec2::AbstractResourceNotificationManager::statusChanged,
        this,
        &QnCommonMessageProcessor::on_resourceStatusChanged,
        connectionType);
    connect(
        resourceManager,
        &ec2::AbstractResourceNotificationManager::resourceParamChanged,
        this,
        &QnCommonMessageProcessor::on_resourceParamChanged,
        connectionType);
    connect(
        resourceManager,
        &ec2::AbstractResourceNotificationManager::resourceParamRemoved,
        this,
        &QnCommonMessageProcessor::on_resourceParamRemoved,
        connectionType);
    connect(
        resourceManager,
        &ec2::AbstractResourceNotificationManager::resourceRemoved,
        this,
        &QnCommonMessageProcessor::on_resourceRemoved,
        connectionType);
    connect(
        resourceManager,
        &ec2::AbstractResourceNotificationManager::resourceStatusRemoved,
        this,
        &QnCommonMessageProcessor::on_resourceStatusRemoved,
        connectionType);

    const auto mediaServerManager = connection->mediaServerNotificationManager();
    connect(
        mediaServerManager,
        &ec2::AbstractMediaServerNotificationManager::addedOrUpdated,
        this,
        on_resourceUpdated,
        connectionType);
    connect(
        mediaServerManager,
        &ec2::AbstractMediaServerNotificationManager::storageChanged,
        this,
        on_resourceUpdated,
        connectionType);
    connect(
        mediaServerManager,
        &ec2::AbstractMediaServerNotificationManager::removed,
        this,
        &QnCommonMessageProcessor::on_resourceRemoved,
        connectionType);
    connect(
        mediaServerManager,
        &ec2::AbstractMediaServerNotificationManager::storageRemoved,
        this,
        &QnCommonMessageProcessor::on_resourceRemoved,
        connectionType);
    connect(
        mediaServerManager,
        &ec2::AbstractMediaServerNotificationManager::userAttributesChanged,
        this,
        &QnCommonMessageProcessor::on_mediaServerUserAttributesChanged,
        connectionType);
    connect(
        mediaServerManager,
        &ec2::AbstractMediaServerNotificationManager::userAttributesRemoved,
        this,
        &QnCommonMessageProcessor::on_mediaServerUserAttributesRemoved,
        connectionType);

    const auto cameraManager = connection->cameraNotificationManager();
    connect(
        cameraManager,
        &ec2::AbstractCameraNotificationManager::addedOrUpdated,
        this,
        on_resourceUpdated,
        connectionType);
    connect(
        cameraManager,
        &ec2::AbstractCameraNotificationManager::userAttributesChanged,
        this,
        &QnCommonMessageProcessor::on_cameraUserAttributesChanged,
        connectionType);
    connect(
        cameraManager,
        &ec2::AbstractCameraNotificationManager::userAttributesRemoved,
        this,
        &QnCommonMessageProcessor::on_cameraUserAttributesRemoved,
        connectionType);
    connect(
        cameraManager,
        &ec2::AbstractCameraNotificationManager::cameraHistoryChanged,
        this,
        &QnCommonMessageProcessor::on_cameraHistoryChanged,
        connectionType);
    connect(
        cameraManager,
        &ec2::AbstractCameraNotificationManager::removed,
        this,
        &QnCommonMessageProcessor::on_resourceRemoved,
        connectionType);

    const auto userManager = connection->userNotificationManager();
    connect(
        userManager,
        &ec2::AbstractUserNotificationManager::addedOrUpdated,
        this,
        on_resourceUpdated,
        connectionType);
    connect(
        userManager,
        &ec2::AbstractUserNotificationManager::removed,
        this,
        &QnCommonMessageProcessor::on_resourceRemoved,
        connectionType);
    connect(
        userManager,
        &ec2::AbstractUserNotificationManager::accessRightsChanged,
        this,
        &QnCommonMessageProcessor::on_accessRightsChanged,
        connectionType);
    connect(
        userManager,
        &ec2::AbstractUserNotificationManager::userRoleAddedOrUpdated,
        this,
        &QnCommonMessageProcessor::on_userRoleChanged,
        connectionType);
    connect(
        userManager,
        &ec2::AbstractUserNotificationManager::userRoleRemoved,
        this,
        &QnCommonMessageProcessor::on_userRoleRemoved,
        connectionType);

    const auto layoutManager = connection->layoutNotificationManager();
    connect(
        layoutManager,
        &ec2::AbstractLayoutNotificationManager::addedOrUpdated,
        this,
        on_resourceUpdated,
        connectionType);
    connect(
        layoutManager,
        &ec2::AbstractLayoutNotificationManager::removed,
        this,
        &QnCommonMessageProcessor::on_resourceRemoved,
        connectionType);

    const auto videowallManager = connection->videowallNotificationManager();
    connect(
        videowallManager,
        &ec2::AbstractVideowallNotificationManager::addedOrUpdated,
        this,
        on_resourceUpdated,
        connectionType);
    connect(
        videowallManager,
        &ec2::AbstractVideowallNotificationManager::removed,
        this,
        &QnCommonMessageProcessor::on_resourceRemoved,
        connectionType);
    connect(
        videowallManager,
        &ec2::AbstractVideowallNotificationManager::controlMessage,
        this,
        &QnCommonMessageProcessor::videowallControlMessageReceived,
        connectionType);

    const auto webPageManager = connection->webPageNotificationManager();
    connect(
        webPageManager,
        &ec2::AbstractWebPageNotificationManager::addedOrUpdated,
        this,
        on_resourceUpdated,
        connectionType);
    connect(
        webPageManager,
        &ec2::AbstractWebPageNotificationManager::removed,
        this,
        &QnCommonMessageProcessor::on_resourceRemoved,
        connectionType);

    const auto licenseManager = connection->licenseNotificationManager();
    connect(
        licenseManager,
        &ec2::AbstractLicenseNotificationManager::licenseChanged,
        this,
        &QnCommonMessageProcessor::on_licenseChanged,
        connectionType);
    connect(
        licenseManager,
        &ec2::AbstractLicenseNotificationManager::licenseRemoved,
        this,
        &QnCommonMessageProcessor::on_licenseRemoved,
        connectionType);

    const auto eventManager = connection->businessEventNotificationManager();
    connect(
        eventManager,
        &ec2::AbstractBusinessEventNotificationManager::addedOrUpdated,
        this,
        &QnCommonMessageProcessor::on_eventRuleAddedOrUpdated,
        connectionType);
    connect(
        eventManager,
        &ec2::AbstractBusinessEventNotificationManager::removed,
        this,
        &QnCommonMessageProcessor::on_businessEventRemoved,
        connectionType);
    connect(
        eventManager,
        &ec2::AbstractBusinessEventNotificationManager::businessActionBroadcasted,
        this,
        &QnCommonMessageProcessor::on_businessActionBroadcasted,
        connectionType);
    connect(
        eventManager,
        &ec2::AbstractBusinessEventNotificationManager::businessRuleReset,
        this,
        &QnCommonMessageProcessor::resetEventRules,
        connectionType);
    connect(
        eventManager,
        &ec2::AbstractBusinessEventNotificationManager::gotBroadcastAction,
        this,
        &QnCommonMessageProcessor::on_broadcastBusinessAction,
        connectionType);
    connect(
        eventManager,
        &ec2::AbstractBusinessEventNotificationManager::execBusinessAction,
        this,
        &QnCommonMessageProcessor::on_execBusinessAction,
        connectionType);

    const auto storedFileManager = connection->storedFileNotificationManager();
    connect(
        storedFileManager,
        &ec2::AbstractStoredFileNotificationManager::added,
        this,
        &QnCommonMessageProcessor::fileAdded,
        connectionType);
    connect(
        storedFileManager,
        &ec2::AbstractStoredFileNotificationManager::updated,
        this,
        &QnCommonMessageProcessor::fileUpdated,
        connectionType);
    connect(
        storedFileManager,
        &ec2::AbstractStoredFileNotificationManager::removed,
        this,
        &QnCommonMessageProcessor::fileRemoved,
        connectionType);

    const auto timeManager = connection->timeNotificationManager();
    connect(
        timeManager,
        &ec2::AbstractTimeNotificationManager::timeChanged,
        this,
        &QnCommonMessageProcessor::syncTimeChanged,
        connectionType);

    const auto discoveryManager = connection->discoveryNotificationManager();
    connect(
        discoveryManager,
        &ec2::AbstractDiscoveryNotificationManager::discoveryInformationChanged,
        this,
        &QnCommonMessageProcessor::on_gotDiscoveryData,
        connectionType);
    connect(
        discoveryManager,
        &ec2::AbstractDiscoveryNotificationManager::discoveredServerChanged,
        this,
        &QnCommonMessageProcessor::discoveredServerChanged,
        connectionType);

    const auto layoutTourManager = connection->layoutTourNotificationManager();
    connect(
        layoutTourManager,
        &ec2::AbstractLayoutTourNotificationManager::addedOrUpdated,
        this,
        &QnCommonMessageProcessor::handleTourAddedOrUpdated,
        connectionType);
    connect(
        layoutTourManager,
        &ec2::AbstractLayoutTourNotificationManager::removed,
        this->layoutTourManager(),
        &QnLayoutTourManager::removeTour,
        connectionType);

    const auto analyticsManager = connection->analyticsNotificationManager();
    connect(
        analyticsManager,
        &ec2::AbstractAnalyticsNotificationManager::analyticsPluginAddedOrUpdated,
        this,
        on_resourceUpdated,
        connectionType);
    connect(
        analyticsManager,
        &ec2::AbstractAnalyticsNotificationManager::analyticsPluginRemoved,
        this,
        &QnCommonMessageProcessor::on_resourceRemoved,
        connectionType);
    connect(
        analyticsManager,
        &ec2::AbstractAnalyticsNotificationManager::analyticsEngineAddedOrUpdated,
        this,
        on_resourceUpdated,
        connectionType);
    connect(
        analyticsManager,
        &ec2::AbstractAnalyticsNotificationManager::analyticsEngineRemoved,
        this,
        &QnCommonMessageProcessor::on_resourceRemoved,
        connectionType);
}

void QnCommonMessageProcessor::disconnectFromConnection(const ec2::AbstractECConnectionPtr &connection)
{
    connection->disconnect(this);
    connection->resourceNotificationManager()->disconnect(this);
    connection->mediaServerNotificationManager()->disconnect(this);
    connection->cameraNotificationManager()->disconnect(this);
    connection->licenseNotificationManager()->disconnect(this);
    connection->businessEventNotificationManager()->disconnect(this);
    connection->userNotificationManager()->disconnect(this);
    connection->layoutNotificationManager()->disconnect(this);
    connection->storedFileNotificationManager()->disconnect(this);
    connection->discoveryNotificationManager()->disconnect(this);
    connection->timeNotificationManager()->disconnect(this);
    connection->miscNotificationManager()->disconnect(this);
    connection->layoutTourNotificationManager()->disconnect(this);

    layoutTourManager()->resetTours();
}

void QnCommonMessageProcessor::on_gotInitialNotification(const FullInfoData& fullData)
{
    onGotInitialNotification(fullData);

    // TODO: #GDM #3.1 logic is not perfect, who will clean them on disconnect?
    resetEventRules(fullData.rules);
}

void QnCommonMessageProcessor::on_gotDiscoveryData(
    const nx::vms::api::DiscoveryData& data, bool addInformation)
{
    if (data.id.isNull())
        return;

    nx::utils::Url url(data.url);

    QnMediaServerResourcePtr server = resourcePool()->getResourceById<QnMediaServerResource>(data.id);
    if (!server) {
        if (!data.ignore) {
            QList<nx::utils::Url> urls = commonModule()->serverAdditionalAddressesDictionary()->additionalUrls(data.id);
            urls.append(url);
            commonModule()->serverAdditionalAddressesDictionary()->setAdditionalUrls(data.id, urls);
        } else {
            QList<nx::utils::Url> urls = commonModule()->serverAdditionalAddressesDictionary()->ignoredUrls(data.id);
            urls.append(url);
            commonModule()->serverAdditionalAddressesDictionary()->setIgnoredUrls(data.id, urls);
        }
        return;
    }

    QList<nx::network::SocketAddress> addresses = server->getNetAddrList();
    QList<nx::utils::Url> additionalUrls = server->getAdditionalUrls();
    QList<nx::utils::Url> ignoredUrls = server->getIgnoredUrls();

    if (addInformation) {
        if (!additionalUrls.contains(url) && !addresses.contains(nx::network::SocketAddress(url.host(), url.port())))
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

void QnCommonMessageProcessor::on_remotePeerFound(QnUuid data, PeerType peerType)
{
    handleRemotePeerFound(data, peerType);
    emit remotePeerFound(data, peerType);
}

void QnCommonMessageProcessor::on_remotePeerLost(QnUuid data, PeerType peerType)
{
    handleRemotePeerLost(data, peerType);
    emit remotePeerLost(data, peerType);
}

void QnCommonMessageProcessor::on_resourceStatusChanged(
    const QnUuid& resourceId,
    ResourceStatus status,
    ec2::NotificationSource source)
{
    if (source == ec2::NotificationSource::Local)
        return; //< ignore local setStatus call. Data already in the resourcePool

    const auto localStatus = static_cast<Qn::ResourceStatus>(status);
    QnResourcePtr resource = resourcePool()->getResourceById(resourceId);
    if (resource)
        onResourceStatusChanged(resource, localStatus, source);
    else
        statusDictionary()->setValue(resourceId, localStatus);
}

void QnCommonMessageProcessor::on_resourceParamChanged(
    const ResourceParamWithRefData& param)
{
    QnResourcePtr resource = resourcePool()->getResourceById(param.resourceId);
    if (resource)
        resource->setProperty(param.name, param.value, QnResource::NO_MARK_DIRTY);
    else
        resourcePropertyDictionary()->setValue(param.resourceId, param.name, param.value, false);

    if (param.name == Qn::kResourceDataParamName)
    {
        const bool loaded = param.value.isEmpty()
            || commonModule()->resourceDataPool()->loadData(param.value.toUtf8());
        NX_ASSERT(loaded, "Invalid json received");
    }
}

void QnCommonMessageProcessor::on_resourceParamRemoved(
    const ResourceParamWithRefData& param)
{
    resourcePropertyDictionary()->on_resourceParamRemoved(param.resourceId, param.name);
}

void QnCommonMessageProcessor::on_resourceRemoved( const QnUuid& resourceId )
{
    if (canRemoveResource(resourceId))
    {
        if (QnResourcePtr ownResource = resourcePool()->getResourceById(resourceId))
        {
            resourcePool()->removeResource(ownResource);
        }
    }
    else
        removeResourceIgnored(resourceId);
}

void QnCommonMessageProcessor::on_resourceStatusRemoved(const QnUuid& resourceId)
{
    if (!canRemoveResource(resourceId))
    {
        if (auto res = resourcePool()->getResourceById(resourceId))
        {
            if (auto connection = commonModule()->ec2Connection())
            {
                connection->getResourceManager(Qn::kSystemAccess)->setResourceStatus(
                    resourceId,
                    res->getStatus(),
                    ec2::DummyHandler::instance(),
                    &ec2::DummyHandler::onRequestDone);
            }
        }
    }
}

void QnCommonMessageProcessor::on_accessRightsChanged(const AccessRightsData& accessRights)
{
    QSet<QnUuid> accessibleResources;
    for (const QnUuid& id : accessRights.resourceIds)
        accessibleResources << id;
    if (auto user = resourcePool()->getResourceById<QnUserResource>(accessRights.userId))
    {
        sharedResourcesManager()->setSharedResources(user, accessibleResources);
    }
    else
    {
        auto role = userRolesManager()->userRole(accessRights.userId);
        if (!role.isNull())
            sharedResourcesManager()->setSharedResources(role, accessibleResources);
    }
}

void QnCommonMessageProcessor::on_userRoleChanged(const UserRoleData& userRole)
{
    userRolesManager()->addOrUpdateUserRole(userRole);
}

void QnCommonMessageProcessor::on_userRoleRemoved(const QnUuid& userRoleId)
{
    userRolesManager()->removeUserRole(userRoleId);
    for (const auto& user : resourcePool()->getResources<QnUserResource>())
    {
        if (user->userRoleId() == userRoleId)
            user->setUserRoleId(QnUuid());
    }
}

void QnCommonMessageProcessor::on_cameraUserAttributesChanged(
    const CameraAttributesData& attrs)
{
    QnCameraUserAttributesPtr userAttributes(new QnCameraUserAttributes());
    ec2::fromApiToResource(attrs, userAttributes);

    QSet<QByteArray> modifiedFields;
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock(
            cameraUserAttributesPool(),
            userAttributes->cameraId);
        if ((*userAttributesLock)->licenseUsed && !attrs.scheduleEnabled)
            NX_INFO(this, "Recording was turned off for camera %1", attrs.cameraId);
        (*userAttributesLock)->assign(*userAttributes, &modifiedFields);
    }
    const QnResourcePtr& res = resourcePool()->getResourceById(userAttributes->cameraId);
    if (res) //it is OK if resource is missing
        res->emitModificationSignals(modifiedFields);
}

void QnCommonMessageProcessor::on_cameraUserAttributesRemoved(const QnUuid& cameraId)
{
    QSet<QByteArray> modifiedFields;
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock(
            cameraUserAttributesPool(),
            cameraId );
        //TODO #ak for now, never removing this structure, just resetting to empty value
        (*userAttributesLock)->assign( QnCameraUserAttributes(), &modifiedFields );
        (*userAttributesLock)->cameraId = cameraId;
    }
    const QnResourcePtr& res = resourcePool()->getResourceById(cameraId);
    if( res )
        res->emitModificationSignals( modifiedFields );
}

void QnCommonMessageProcessor::on_mediaServerUserAttributesChanged(
    const MediaServerUserAttributesData& attrs)
{
    QnMediaServerUserAttributesPtr userAttributes(new QnMediaServerUserAttributes());
    ec2::fromApiToResource(attrs, userAttributes);

    QSet<QByteArray> modifiedFields;
    {
        QnMediaServerUserAttributesPool::ScopedLock lk(
            mediaServerUserAttributesPool(),
            userAttributes->serverId );
        (*lk)->assign( *userAttributes, &modifiedFields );
    }
    const QnResourcePtr& res = resourcePool()->getResourceById(userAttributes->serverId);
    if( res )   //it is OK if resource is missing
        res->emitModificationSignals( modifiedFields );
}

void QnCommonMessageProcessor::on_mediaServerUserAttributesRemoved(const QnUuid& serverId)
{
    QSet<QByteArray> modifiedFields;
    {
        QnMediaServerUserAttributesPool::ScopedLock lk(mediaServerUserAttributesPool(), serverId );
        //TODO #ak for now, never removing this structure, just resetting to empty value
        (*lk)->assign( QnMediaServerUserAttributes(), &modifiedFields );
        (*lk)->serverId = serverId;
    }
    const QnResourcePtr& res = resourcePool()->getResourceById(serverId);
    if( res )   //it is OK if resource is missing
        res->emitModificationSignals( modifiedFields );
}

void QnCommonMessageProcessor::on_cameraHistoryChanged(
    const ServerFootageData& serverFootageData)
{
    cameraHistoryPool()->setServerFootageData(serverFootageData);
}

void QnCommonMessageProcessor::on_licenseChanged(const QnLicensePtr &license) {
    licensePool()->addLicense(license);
}

void QnCommonMessageProcessor::on_licenseRemoved(const QnLicensePtr &license) {
    licensePool()->removeLicense(license);
}

void QnCommonMessageProcessor::on_eventRuleAddedOrUpdated(const nx::vms::api::EventRuleData& data)
{
    nx::vms::event::RulePtr eventRule(new nx::vms::event::Rule());
    ec2::fromApiToResource(data, eventRule);
    eventRuleManager()->addOrUpdateRule(eventRule);
}

void QnCommonMessageProcessor::on_businessEventRemoved(const QnUuid& id)
{
    eventRuleManager()->removeRule(id);
}

void QnCommonMessageProcessor::on_businessActionBroadcasted(
    const vms::event::AbstractActionPtr& /*businessAction*/)
{
    // Nothing to do for a while.
}

void QnCommonMessageProcessor::on_broadcastBusinessAction( const vms::event::AbstractActionPtr& action )
{
    emit businessActionReceived(action);
}

void QnCommonMessageProcessor::on_execBusinessAction( const vms::event::AbstractActionPtr& action )
{
    execBusinessActionInternal(action);
}

void QnCommonMessageProcessor::handleTourAddedOrUpdated(const nx::vms::api::LayoutTourData& tour)
{
    layoutTourManager()->addOrUpdateTour(tour);
}

void QnCommonMessageProcessor::resetResourceTypes(const ResourceTypeDataList& resTypes)
{
    QnResourceTypeList qnResTypes;
    ec2::fromApiToResourceList(resTypes, qnResTypes);
    qnResTypePool->replaceResourceTypeList(qnResTypes);
}

void QnCommonMessageProcessor::resetResources(const FullInfoData& fullData)
{
    // Store all remote resources id to clean them if they are not in the list anymore.
    QHash<QnUuid, QnResourcePtr> remoteResources;
    for (const QnResourcePtr &resource: resourcePool()->getResourcesWithFlag(Qn::remote))
        remoteResources.insert(resource->getId(), resource);

    const auto updateResources = [this, &remoteResources](const auto& source)
        {
            for (const auto& resource: source)
            {
                updateResource(resource, ec2::NotificationSource::Remote);
                remoteResources.remove(resource.id);
            }
        };

    // Packet adding.
    resourcePool()->beginTran();

    updateResources(fullData.users);
    updateResources(fullData.cameras);
    updateResources(fullData.layouts);
    updateResources(fullData.videowalls);
    updateResources(fullData.webPages);
    updateResources(fullData.servers);
    updateResources(fullData.storages);
    updateResources(fullData.analyticsPlugins);
    updateResources(fullData.analyticsEngines);

    resourcePool()->commit();

    // Remove absent resources.
    for (const QnResourcePtr& resource: remoteResources)
        resourcePool()->removeResource(resource);
}

void QnCommonMessageProcessor::resetLicenses(const LicenseDataList& licenses)
{
    licensePool()->replaceLicenses(licenses);
}

void QnCommonMessageProcessor::resetCamerasWithArchiveList(
    const ServerFootageDataList& cameraHistoryList)
{
    cameraHistoryPool()->resetServerFootageData(cameraHistoryList);
}

void QnCommonMessageProcessor::resetTime()
{
    if (!m_connection)
        return;

    auto timeManager = m_connection->timeSyncManager();
    emit syncTimeChanged(timeManager->getSyncTime().count());
}

void QnCommonMessageProcessor::resetEventRules(const nx::vms::api::EventRuleDataList& eventRules)
{
    vms::event::RuleList ruleList;
    ec2::fromApiToResourceList(eventRules, ruleList);
    eventRuleManager()->resetRules(ruleList);
}

void QnCommonMessageProcessor::resetAccessRights(const AccessRightsDataList& accessRights)
{
    sharedResourcesManager()->reset(accessRights);
}

void QnCommonMessageProcessor::resetUserRoles(const UserRoleDataList& roles)
{
    userRolesManager()->resetUserRoles(roles);
}

bool QnCommonMessageProcessor::canRemoveResource(const QnUuid &)
{
    return true;
}

void QnCommonMessageProcessor::removeResourceIgnored(const QnUuid &)
{
}

void QnCommonMessageProcessor::handleRemotePeerFound(QnUuid /*data*/, PeerType /*peerType*/)
{
}

void QnCommonMessageProcessor::handleRemotePeerLost(QnUuid /*data*/, PeerType /*peerType*/)
{
}

void QnCommonMessageProcessor::resetServerUserAttributesList(
    const MediaServerUserAttributesDataList& serverUserAttributesList)
{
    mediaServerUserAttributesPool()->clear();
    for( const auto& serverAttrs: serverUserAttributesList )
    {
        QnMediaServerUserAttributesPtr dstElement(new QnMediaServerUserAttributes());
        ec2::fromApiToResource(serverAttrs, dstElement);

        QnMediaServerUserAttributesPool::ScopedLock userAttributesLock(
            mediaServerUserAttributesPool(), serverAttrs.serverId);
        *(*userAttributesLock) = *dstElement;
    }
}

void QnCommonMessageProcessor::resetCameraUserAttributesList(
    const CameraAttributesDataList& cameraUserAttributesList)
{
    cameraUserAttributesPool()->clear();
    for (const auto& cameraAttrs: cameraUserAttributesList)
    {
        QnCameraUserAttributesPtr dstElement(new QnCameraUserAttributes());
        ec2::fromApiToResource(cameraAttrs, dstElement);

        QnCameraUserAttributePool::ScopedLock userAttributesLock(
            cameraUserAttributesPool(),
            cameraAttrs.cameraId);
        *(*userAttributesLock) = *dstElement;
    }
}

void QnCommonMessageProcessor::resetPropertyList(
    const ResourceParamWithRefDataList& params)
{
    // Store existing parameter keys.
    auto existingProperties = resourcePropertyDictionary()->allPropertyNamesByResource();

    // Update changed values.
    for (const auto& param: params)
    {
        on_resourceParamChanged(param);
        if (existingProperties.contains(param.resourceId))
            existingProperties[param.resourceId].remove(param.name);
    }

    // Clean values that are not in the list anymore.
    for (auto iter = existingProperties.constBegin(); iter != existingProperties.constEnd(); ++iter)
    {
        QnUuid resourceId = iter.key();
        for (auto paramName: iter.value())
        {
            on_resourceParamChanged(
                ResourceParamWithRefData(resourceId, paramName, QString()));
        }
    }
}

void QnCommonMessageProcessor::resetStatusList(const ResourceStatusDataList& params)
{
    auto keys = statusDictionary()->values().keys();
    statusDictionary()->clear();
    for(const QnUuid& id: keys) {
        if (QnResourcePtr resource = resourcePool()->getResourceById(id))
        {
            NX_VERBOSE(this, lit("%1 Emit statusChanged signal for resource %2, %3, %4")
                    .arg(QString::fromLatin1(Q_FUNC_INFO))
                    .arg(resource->getId().toString())
                    .arg(resource->getName())
                    .arg(resource->getUrl()));
            emit resource->statusChanged(resource, Qn::StatusChangeReason::Local);
        }
    }

    for (const ResourceStatusData& statusData: params)
    {
        on_resourceStatusChanged(
            statusData.id,
            statusData.status,
            ec2::NotificationSource::Remote);
    }
}

void QnCommonMessageProcessor::onGotInitialNotification(const FullInfoData& fullData)
{
    resourceAccessManager()->beginUpdate();
    resourceAccessProvider()->beginUpdate();

    commonModule()->serverAdditionalAddressesDictionary()->clear();

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
    layoutTourManager()->resetTours(fullData.layoutTours);

    resourceAccessProvider()->endUpdate();
    resourceAccessManager()->endUpdate();

    emit initialResourcesReceived();
}

void QnCommonMessageProcessor::updateResource(
    const QnResourcePtr&,
    ec2::NotificationSource /*source*/)
{
}

void QnCommonMessageProcessor::updateResource(
    const nx::vms::api::UserData& user,
    ec2::NotificationSource source)
{
    QnUserResourcePtr qnUser(ec2::fromApiToResource(user, commonModule()));
    updateResource(qnUser, source);
}

void QnCommonMessageProcessor::updateResource(
    const LayoutData& layout,
    ec2::NotificationSource source)
{
    QnLayoutResourcePtr qnLayout(new QnLayoutResource(commonModule()));
    if (!layout.url.isEmpty())
    {
        NX_WARNING(this, lit("Invalid server layout with url %1").arg(layout.url));
        auto fixed = layout;
        fixed.url = QString();
        ec2::fromApiToResource(fixed, qnLayout);
    }
    else
    {
        ec2::fromApiToResource(layout, qnLayout);
    }
    updateResource(qnLayout, source);
}

void QnCommonMessageProcessor::updateResource(
    const nx::vms::api::VideowallData& videowall,
    ec2::NotificationSource source)
{
    QnVideoWallResourcePtr qnVideowall(new QnVideoWallResource(commonModule()));
    ec2::fromApiToResource(videowall, qnVideowall);
    updateResource(qnVideowall, source);
}

void QnCommonMessageProcessor::updateResource(
    const WebPageData& webpage,
    ec2::NotificationSource source)
{
    QnWebPageResourcePtr qnWebpage(new QnWebPageResource(commonModule()));
    ec2::fromApiToResource(webpage, qnWebpage);
    updateResource(qnWebpage, source);
}

void QnCommonMessageProcessor::updateResource(
    const nx::vms::api::AnalyticsPluginData& analyticsPluginData,
    ec2::NotificationSource source)
{
    const QnResourceParams parameters(
        analyticsPluginData.id,
        /*url*/ QString(),
        /*vendor*/ QString());

    nx::vms::common::AnalyticsPluginResourcePtr pluginResource =
        getResourceFactory()->createResource(
            nx::vms::api::AnalyticsPluginData::kResourceTypeId,
            parameters).dynamicCast<nx::vms::common::AnalyticsPluginResource>();

    if (!pluginResource)
    {
        NX_DEBUG(this, "Unable to create plugin resource.");
        return;
    }

    ec2::fromApiToResource(analyticsPluginData, pluginResource);
    updateResource(pluginResource, source);
}

void QnCommonMessageProcessor::updateResource(
    const nx::vms::api::AnalyticsEngineData& analyticsEngineData,
    ec2::NotificationSource source)
{
    const QnResourceParams parameters(
        analyticsEngineData.id,
        /*url*/ QString(),
        /*vendor*/ QString());

    nx::vms::common::AnalyticsEngineResourcePtr engineResource = getResourceFactory()
        ->createResource(
            nx::vms::api::AnalyticsEngineData::kResourceTypeId,
            parameters).dynamicCast<nx::vms::common::AnalyticsEngineResource>();

    if (!engineResource)
    {
        NX_DEBUG(this, "Unable to create engine resource.");
        return;
    }

    ec2::fromApiToResource(analyticsEngineData, engineResource);
    updateResource(engineResource, source);
}

void QnCommonMessageProcessor::updateResource(const CameraData& camera, ec2::NotificationSource source)
{
    QnVirtualCameraResourcePtr qnCamera = getResourceFactory()->createResource(camera.typeId,
            QnResourceParams(camera.id, camera.url, camera.vendor))
        .dynamicCast<QnVirtualCameraResource>();

    NX_ASSERT(qnCamera, QByteArray("Unknown resource type:") + camera.typeId.toByteArray());
    if (qnCamera)
    {
        qnCamera->setCommonModule(commonModule());
        ec2::fromApiToResource(camera, qnCamera);
        NX_ASSERT(camera.id == QnVirtualCameraResource::physicalIdToId(qnCamera->getUniqueId()),
            "You must fill camera ID as md5 hash of unique id");

        updateResource(qnCamera, source);
    }
}

void QnCommonMessageProcessor::updateResource(
    const MediaServerData& server, ec2::NotificationSource source)
{
    QnMediaServerResourcePtr qnServer(new QnMediaServerResource(commonModule()));
    ec2::fromApiToResource(server, qnServer);
    updateResource(qnServer, source);
}

void QnCommonMessageProcessor::updateResource(
    const StorageData& storage, ec2::NotificationSource source)
{
    QnStorageResourcePtr qnStorage = getResourceFactory()->createResource(
        StorageData::kResourceTypeId, QnResourceParams(storage.id, storage.url, QString()))
            .dynamicCast<QnStorageResource>();
    qnStorage->setCommonModule(commonModule());
    NX_ASSERT(qnStorage, "Invalid resource type pool state");
    if (qnStorage)
    {
        ec2::fromApiToResource(storage, qnStorage);
        updateResource(qnStorage, source);
    }
}
