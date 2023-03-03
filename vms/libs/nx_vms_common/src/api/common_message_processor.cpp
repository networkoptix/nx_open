// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "common_message_processor.h"

#include <QtCore/QElapsedTimer>

#include <common/common_module.h>
#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_factory.h>
#include <core/resource/storage_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_access/user_access_data.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/server_additional_addresses_dictionary.h>
#include <core/resource_management/status_dictionary.h>
#include <core/resource_management/user_roles_manager.h>
#include <licensing/license.h>
#include <nx/analytics/taxonomy/state_watcher.h>
#include <nx/fusion/serialization/json.h>
#include <nx/network/socket_common.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/api/data/access_rights_data.h>
#include <nx/vms/api/data/discovery_data.h>
#include <nx/vms/api/data/event_rule_data.h>
#include <nx/vms/api/data/full_info_data.h>
#include <nx/vms/api/data/license_data.h>
#include <nx/vms/api/data/resource_type_data.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>
#include <nx/vms/time/abstract_time_sync_manager.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_analytics_manager.h>
#include <nx_ec/managers/abstract_camera_manager.h>
#include <nx_ec/managers/abstract_discovery_manager.h>
#include <nx_ec/managers/abstract_event_rules_manager.h>
#include <nx_ec/managers/abstract_layout_manager.h>
#include <nx_ec/managers/abstract_layout_tour_manager.h>
#include <nx_ec/managers/abstract_license_manager.h>
#include <nx_ec/managers/abstract_media_server_manager.h>
#include <nx_ec/managers/abstract_misc_manager.h>
#include <nx_ec/managers/abstract_resource_manager.h>
#include <nx_ec/managers/abstract_stored_file_manager.h>
#include <nx_ec/managers/abstract_time_manager.h>
#include <nx_ec/managers/abstract_user_manager.h>
#include <nx_ec/managers/abstract_videowall_manager.h>
#include <nx_ec/managers/abstract_vms_rules_manager.h>
#include <nx_ec/managers/abstract_webpage_manager.h>
#include <utils/common/synctime.h>

#include "runtime_info_manager.h"

using namespace nx;
using namespace nx::vms::api;

QnCommonMessageProcessor::QnCommonMessageProcessor(
    nx::vms::common::SystemContext* context,
    QObject *parent)
    :
    base_type(parent),
    nx::vms::common::SystemContextAware(context)
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

    const auto on_hardwareIdMappingAdded =
        [this](const auto& hardwareIdMapping) { addHardwareIdMapping(hardwareIdMapping); };

    const auto on_hardwareIdMappingRemoved =
        [this](const QnUuid& id) { removeHardwareIdMapping(id); };

    const auto connectionType = handlerConnectionType();

    connect(
        connection.get(),
        &ec2::AbstractECConnection::remotePeerFound,
        this,
        &QnCommonMessageProcessor::on_remotePeerFound,
        connectionType);
    connect(
        connection.get(),
        &ec2::AbstractECConnection::remotePeerLost,
        this,
        &QnCommonMessageProcessor::on_remotePeerLost,
        connectionType);
    connect(
        connection.get(),
        &ec2::AbstractECConnection::initNotification,
        this,
        &QnCommonMessageProcessor::on_gotInitialNotification,
        connectionType);
    connect(
        connection.get(),
        &ec2::AbstractECConnection::runtimeInfoChanged,
        this,
        &QnCommonMessageProcessor::runtimeInfoChanged,
        connectionType);
    connect(
        connection.get(),
        &ec2::AbstractECConnection::runtimeInfoRemoved,
        this,
        &QnCommonMessageProcessor::runtimeInfoRemoved,
        connectionType);
    connect(
        connection.get(),
        &ec2::AbstractECConnection::serverRuntimeEventOccurred,
        this,
        &QnCommonMessageProcessor::serverRuntimeEventOccurred,
        connectionType);

    const auto resourceManager = connection->resourceNotificationManager();
    connect(
        resourceManager.get(),
        &ec2::AbstractResourceNotificationManager::statusChanged,
        this,
        &QnCommonMessageProcessor::on_resourceStatusChanged,
        connectionType);
    connect(
        resourceManager.get(),
        &ec2::AbstractResourceNotificationManager::resourceParamChanged,
        this,
        &QnCommonMessageProcessor::on_resourceParamChanged,
        connectionType);
    connect(
        resourceManager.get(),
        &ec2::AbstractResourceNotificationManager::resourceParamRemoved,
        this,
        &QnCommonMessageProcessor::on_resourceParamRemoved,
        connectionType);
    connect(
        resourceManager.get(),
        &ec2::AbstractResourceNotificationManager::resourceRemoved,
        this,
        &QnCommonMessageProcessor::on_resourceRemoved,
        connectionType);
    connect(
        resourceManager.get(),
        &ec2::AbstractResourceNotificationManager::resourceStatusRemoved,
        this,
        &QnCommonMessageProcessor::on_resourceStatusRemoved,
        connectionType);

    const auto mediaServerManager = connection->mediaServerNotificationManager();
    connect(
        mediaServerManager.get(),
        &ec2::AbstractMediaServerNotificationManager::addedOrUpdated,
        this,
        on_resourceUpdated,
        connectionType);
    connect(
        mediaServerManager.get(),
        &ec2::AbstractMediaServerNotificationManager::storageChanged,
        this,
        on_resourceUpdated,
        connectionType);
    connect(
        mediaServerManager.get(),
        &ec2::AbstractMediaServerNotificationManager::removed,
        this,
        &QnCommonMessageProcessor::on_resourceRemoved,
        connectionType);
    connect(
        mediaServerManager.get(),
        &ec2::AbstractMediaServerNotificationManager::storageRemoved,
        this,
        &QnCommonMessageProcessor::on_resourceRemoved,
        connectionType);
    connect(
        mediaServerManager.get(),
        &ec2::AbstractMediaServerNotificationManager::userAttributesChanged,
        this,
        &QnCommonMessageProcessor::on_mediaServerUserAttributesChanged,
        connectionType);
    connect(
        mediaServerManager.get(),
        &ec2::AbstractMediaServerNotificationManager::userAttributesRemoved,
        this,
        &QnCommonMessageProcessor::on_mediaServerUserAttributesRemoved,
        connectionType);

    const auto cameraManager = connection->cameraNotificationManager();
    connect(
        cameraManager.get(),
        &ec2::AbstractCameraNotificationManager::hardwareIdMappingAdded,
        this,
        on_hardwareIdMappingAdded,
        connectionType);
    connect(
        cameraManager.get(),
        &ec2::AbstractCameraNotificationManager::hardwareIdMappingRemoved,
        this,
        on_hardwareIdMappingRemoved,
        connectionType);
    connect(
        cameraManager.get(),
        &ec2::AbstractCameraNotificationManager::addedOrUpdated,
        this,
        on_resourceUpdated,
        connectionType);
    connect(
        cameraManager.get(),
        &ec2::AbstractCameraNotificationManager::userAttributesChanged,
        this,
        &QnCommonMessageProcessor::on_cameraUserAttributesChanged,
        connectionType);
    connect(
        cameraManager.get(),
        &ec2::AbstractCameraNotificationManager::userAttributesRemoved,
        this,
        &QnCommonMessageProcessor::on_cameraUserAttributesRemoved,
        connectionType);
    connect(
        cameraManager.get(),
        &ec2::AbstractCameraNotificationManager::cameraHistoryChanged,
        this,
        &QnCommonMessageProcessor::on_cameraHistoryChanged,
        connectionType);
    connect(
        cameraManager.get(),
        &ec2::AbstractCameraNotificationManager::removed,
        this,
        &QnCommonMessageProcessor::on_resourceRemoved,
        connectionType);

    const auto userManager = connection->userNotificationManager();
    connect(
        userManager.get(),
        &ec2::AbstractUserNotificationManager::addedOrUpdated,
        this,
        on_resourceUpdated,
        connectionType);
    connect(
        userManager.get(),
        &ec2::AbstractUserNotificationManager::removed,
        this,
        &QnCommonMessageProcessor::on_resourceRemoved,
        connectionType);
    connect(
        userManager.get(),
        &ec2::AbstractUserNotificationManager::accessRightsChanged,
        this,
        &QnCommonMessageProcessor::on_accessRightsChanged,
        connectionType);
    connect(
        userManager.get(),
        &ec2::AbstractUserNotificationManager::userRoleAddedOrUpdated,
        this,
        &QnCommonMessageProcessor::on_userRoleChanged,
        connectionType);
    connect(
        userManager.get(),
        &ec2::AbstractUserNotificationManager::userRoleRemoved,
        this,
        &QnCommonMessageProcessor::on_userRoleRemoved,
        connectionType);

    const auto layoutManager = connection->layoutNotificationManager();
    connect(
        layoutManager.get(),
        &ec2::AbstractLayoutNotificationManager::addedOrUpdated,
        this,
        on_resourceUpdated,
        connectionType);
    connect(
        layoutManager.get(),
        &ec2::AbstractLayoutNotificationManager::removed,
        this,
        &QnCommonMessageProcessor::on_resourceRemoved,
        connectionType);

    const auto videowallManager = connection->videowallNotificationManager();
    connect(
        videowallManager.get(),
        &ec2::AbstractVideowallNotificationManager::addedOrUpdated,
        this,
        on_resourceUpdated,
        connectionType);
    connect(
        videowallManager.get(),
        &ec2::AbstractVideowallNotificationManager::removed,
        this,
        &QnCommonMessageProcessor::on_resourceRemoved,
        connectionType);
    connect(
        videowallManager.get(),
        &ec2::AbstractVideowallNotificationManager::controlMessage,
        this,
        &QnCommonMessageProcessor::videowallControlMessageReceived,
        connectionType);

    const auto webPageManager = connection->webPageNotificationManager();
    connect(
        webPageManager.get(),
        &ec2::AbstractWebPageNotificationManager::addedOrUpdated,
        this,
        on_resourceUpdated,
        connectionType);
    connect(
        webPageManager.get(),
        &ec2::AbstractWebPageNotificationManager::removed,
        this,
        &QnCommonMessageProcessor::on_resourceRemoved,
        connectionType);

    const auto licenseManager = connection->licenseNotificationManager();
    connect(
        licenseManager.get(),
        &ec2::AbstractLicenseNotificationManager::licenseChanged,
        this,
        &QnCommonMessageProcessor::on_licenseChanged,
        connectionType);
    connect(
        licenseManager.get(),
        &ec2::AbstractLicenseNotificationManager::licenseRemoved,
        this,
        &QnCommonMessageProcessor::on_licenseRemoved,
        connectionType);

    const auto eventManager = connection->businessEventNotificationManager();
    connect(
        eventManager.get(),
        &ec2::AbstractBusinessEventNotificationManager::addedOrUpdated,
        this,
        &QnCommonMessageProcessor::on_eventRuleAddedOrUpdated,
        connectionType);
    connect(
        eventManager.get(),
        &ec2::AbstractBusinessEventNotificationManager::removed,
        this,
        &QnCommonMessageProcessor::on_businessEventRemoved,
        connectionType);
    connect(
        eventManager.get(),
        &ec2::AbstractBusinessEventNotificationManager::businessRuleReset,
        this,
        &QnCommonMessageProcessor::resetEventRules,
        connectionType);
    connect(
        eventManager.get(),
        &ec2::AbstractBusinessEventNotificationManager::gotBroadcastAction,
        this,
        &QnCommonMessageProcessor::on_broadcastBusinessAction,
        connectionType);

    const auto vmsRulesManager = connection->vmsRulesNotificationManager();
    connect(
        vmsRulesManager.get(),
        &ec2::AbstractVmsRulesNotificationManager::eventReceived,
        this,
        &QnCommonMessageProcessor::vmsEventReceived,
        connectionType);
    connect(
        vmsRulesManager.get(),
        &ec2::AbstractVmsRulesNotificationManager::reset,
        this,
        [this]{ resetVmsRules({}); },
        connectionType);
    connect(
        vmsRulesManager.get(),
        &ec2::AbstractVmsRulesNotificationManager::ruleUpdated,
        this,
        &QnCommonMessageProcessor::vmsRuleUpdated,
        connectionType);
    connect(
        vmsRulesManager.get(),
        &ec2::AbstractVmsRulesNotificationManager::ruleRemoved,
        this,
        &QnCommonMessageProcessor::vmsRuleRemoved,
        connectionType);

    const auto storedFileManager = connection->storedFileNotificationManager();
    connect(
        storedFileManager.get(),
        &ec2::AbstractStoredFileNotificationManager::added,
        this,
        &QnCommonMessageProcessor::fileAdded,
        connectionType);
    connect(
        storedFileManager.get(),
        &ec2::AbstractStoredFileNotificationManager::updated,
        this,
        &QnCommonMessageProcessor::fileUpdated,
        connectionType);
    connect(
        storedFileManager.get(),
        &ec2::AbstractStoredFileNotificationManager::removed,
        this,
        &QnCommonMessageProcessor::fileRemoved,
        connectionType);

    const auto discoveryManager = connection->discoveryNotificationManager();
    connect(
        discoveryManager.get(),
        &ec2::AbstractDiscoveryNotificationManager::gotInitialDiscoveredServers,
        this,
        &QnCommonMessageProcessor::gotInitialDiscoveredServers,
        connectionType);
    connect(
        discoveryManager.get(),
        &ec2::AbstractDiscoveryNotificationManager::discoveryInformationChanged,
        this,
        &QnCommonMessageProcessor::on_gotDiscoveryData,
        connectionType);
    connect(
        discoveryManager.get(),
        &ec2::AbstractDiscoveryNotificationManager::discoveredServerChanged,
        this,
        &QnCommonMessageProcessor::discoveredServerChanged,
        connectionType);

    const auto layoutTourManager = connection->layoutTourNotificationManager();
    connect(
        layoutTourManager.get(),
        &ec2::AbstractLayoutTourNotificationManager::addedOrUpdated,
        this,
        &QnCommonMessageProcessor::handleTourAddedOrUpdated,
        connectionType);
    connect(
        layoutTourManager.get(),
        &ec2::AbstractLayoutTourNotificationManager::removed,
        m_context->layoutTourManager(),
        &QnLayoutTourManager::removeTour,
        connectionType);

    const auto analyticsManager = connection->analyticsNotificationManager();
    connect(
        analyticsManager.get(),
        &ec2::AbstractAnalyticsNotificationManager::analyticsPluginAddedOrUpdated,
        this,
        on_resourceUpdated,
        connectionType);
    connect(
        analyticsManager.get(),
        &ec2::AbstractAnalyticsNotificationManager::analyticsPluginRemoved,
        this,
        &QnCommonMessageProcessor::on_resourceRemoved,
        connectionType);
    connect(
        analyticsManager.get(),
        &ec2::AbstractAnalyticsNotificationManager::analyticsEngineAddedOrUpdated,
        this,
        on_resourceUpdated,
        connectionType);
    connect(
        analyticsManager.get(),
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
    connection->vmsRulesNotificationManager()->disconnect(this);
    connection->userNotificationManager()->disconnect(this);
    connection->layoutNotificationManager()->disconnect(this);
    connection->storedFileNotificationManager()->disconnect(this);
    connection->discoveryNotificationManager()->disconnect(this);
    connection->miscNotificationManager()->disconnect(this);
    connection->layoutTourNotificationManager()->disconnect(this);

    m_context->layoutTourManager()->resetTours();
}

void QnCommonMessageProcessor::on_gotInitialNotification(const FullInfoData& fullData)
{
    onGotInitialNotification(fullData);

    // TODO: #sivanov Logic is not perfect, who will clean them on disconnect?
    resetEventRules(fullData.rules);
    resetVmsRules(fullData.vmsRules);
}

void QnCommonMessageProcessor::on_gotDiscoveryData(
    const nx::vms::api::DiscoveryData& data, bool addInformation)
{
    if (data.id.isNull())
        return;

    nx::utils::Url url(data.url);

    auto server = resourcePool()->getResourceById<QnMediaServerResource>(data.id);
    if (!server)
    {
        if (!data.ignore)
        {
            QList<nx::utils::Url> urls =
                m_context->serverAdditionalAddressesDictionary()->additionalUrls(data.id);
            urls.append(url);
            m_context->serverAdditionalAddressesDictionary()->setAdditionalUrls(data.id, urls);
        }
        else
        {
            QList<nx::utils::Url> urls =
                m_context->serverAdditionalAddressesDictionary()->ignoredUrls(data.id);
            urls.append(url);
            m_context->serverAdditionalAddressesDictionary()->setIgnoredUrls(data.id, urls);
        }
        return;
    }

    QList<nx::network::SocketAddress> addresses = server->getNetAddrList();
    QList<nx::utils::Url> additionalUrls = server->getAdditionalUrls();
    QList<nx::utils::Url> ignoredUrls = server->getIgnoredUrls();

    if (addInformation) {
        if (!additionalUrls.contains(url) && !addresses.contains(nx::network::url::getEndpoint(url)))
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

    const auto localStatus = static_cast<nx::vms::api::ResourceStatus>(status);
    QnResourcePtr resource = resourcePool()->getResourceById(resourceId);
    if (resource)
        onResourceStatusChanged(resource, localStatus, source);
    else
        m_context->resourceStatusDictionary()->setValue(resourceId, localStatus);
}

void QnCommonMessageProcessor::on_resourceParamChanged(
    const nx::vms::api::ResourceParamWithRefData& param,
    ec2::NotificationSource source)
{
    if (handleRemoteAnalyticsNotification(param, source))
        return;

    QnResourcePtr resource = resourcePool()->getResourceById(param.resourceId);
    if (resource)
    {
        resource->setProperty(param.name, param.value, /*markDirty*/ false);
    }
    else
    {
        m_context->resourcePropertyDictionary()->setValue(
            param.resourceId,
            param.name,
            param.value,
            /*markDirty*/ false);
    }

    if (param.name == Qn::kResourceDataParamName)
    {
        const bool loaded = param.value.isEmpty()
            || m_context->resourceDataPool()->loadData(param.value.toUtf8());
        NX_ASSERT(loaded, "Invalid json received");
    }
}

bool QnCommonMessageProcessor::handleRemoteAnalyticsNotification(
    const nx::vms::api::ResourceParamWithRefData& /*param*/,
    ec2::NotificationSource /*source*/)
{
    return false;
}

void QnCommonMessageProcessor::on_resourceParamRemoved(
    const ResourceParamWithRefData& param)
{
    if (canRemoveResourceProperty(param.resourceId, param.name))
    {
        m_context->resourcePropertyDictionary()->on_resourceParamRemoved(
            param.resourceId,
            param.name);
    }
    else
    {
        refreshIgnoredResourceProperty(param.resourceId, param.name);
    }
}

void QnCommonMessageProcessor::on_resourceRemoved(
    const QnUuid& resourceId, ec2::NotificationSource source)
{
    if (canRemoveResource(resourceId, source))
    {
        if (QnResourcePtr ownResource = resourcePool()->getResourceById(resourceId))
            resourcePool()->removeResource(ownResource);
        m_context->resourceStatusDictionary()->remove(resourceId);
    }
    else
        removeResourceIgnored(resourceId);
}

void QnCommonMessageProcessor::on_resourceStatusRemoved(
    const QnUuid& resourceId, ec2::NotificationSource source)
{
    if (!canRemoveResource(resourceId, source))
    {
        if (auto res = resourcePool()->getResourceById(resourceId))
        {
            if (auto connection = messageBusConnection())
            {
                connection->getResourceManager(Qn::kSystemAccess)->setResourceStatus(
                    resourceId, res->getStatus(), [](int /*requestId*/, ec2::ErrorCode) {});
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
        m_context->sharedResourcesManager()->setSharedResources(user, accessibleResources);
    }
    else if (auto role = m_context->userRolesManager()->userRole(accessRights.userId);
             !role.id.isNull())
    {
        m_context->sharedResourcesManager()->setSharedResources(role, accessibleResources);
    }
    else
    {
        m_context->sharedResourcesManager()->setSharedResourcesById(
            accessRights.userId, accessibleResources);
    }
}

void QnCommonMessageProcessor::on_userRoleChanged(const UserRoleData& userRole)
{
    m_context->userRolesManager()->addOrUpdateUserRole(userRole);
}

void QnCommonMessageProcessor::on_userRoleRemoved(const QnUuid& userRoleId)
{
    m_context->userRolesManager()->removeUserRole(userRoleId);
    for (const auto& user: resourcePool()->getResources<QnUserResource>())
    {
        auto roles = user->userRoleIds();
        if (nx::utils::erase_if(roles, [&userRoleId](const auto& id) { return id == userRoleId; }))
            user->setUserRoleIds(roles);
    }
}

void QnCommonMessageProcessor::on_cameraUserAttributesChanged(
    const CameraAttributesData& userAttributes)
{
    if (auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(
        userAttributes.cameraId))
    {
        if (camera->isScheduleEnabled() && !userAttributes.scheduleEnabled)
            NX_INFO(this, "Recording was turned off for camera %1", userAttributes.cameraId);

        camera->setUserAttributesAndNotify(userAttributes);
    }
    else
    {
        // It is possible that user attributes will be passed before Camera Resource is created,
        // so we need to cache them and assign to the Camera when it is created.
        m_cameraUserAttributesCache[userAttributes.cameraId] = userAttributes;
    }
}

void QnCommonMessageProcessor::on_cameraUserAttributesRemoved(const QnUuid& cameraId)
{
    // It is OK if the Camera is missing.
    if (auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(cameraId))
    {
        nx::vms::api::CameraAttributesData emptyAttributes;
        emptyAttributes.cameraId = cameraId;
        camera->setUserAttributesAndNotify(emptyAttributes);
    }
}

void QnCommonMessageProcessor::on_mediaServerUserAttributesChanged(
    const MediaServerUserAttributesData& attrs)
{
    auto server = resourcePool()->getResourceById<QnMediaServerResource>(attrs.serverId);
    if (server)
        server->setUserAttributesAndNotify(attrs);
    else
        m_serverUserAttributesCache[attrs.serverId] = attrs;
}

void QnCommonMessageProcessor::on_mediaServerUserAttributesRemoved(const QnUuid& serverId)
{
    // It is OK if the Server is missing.
    if (auto server = resourcePool()->getResourceById<QnMediaServerResource>(serverId))
    {
        nx::vms::api::MediaServerUserAttributesData emptyAttributes;
        emptyAttributes.serverId = serverId;
        server->setUserAttributesAndNotify(emptyAttributes);
    }
}

void QnCommonMessageProcessor::on_cameraHistoryChanged(
    const ServerFootageData& serverFootageData)
{
    m_context->cameraHistoryPool()->setServerFootageData(serverFootageData);
}

void QnCommonMessageProcessor::on_licenseChanged(const QnLicensePtr& license)
{
    m_context->licensePool()->addLicense(license);
}

void QnCommonMessageProcessor::on_licenseRemoved(const QnLicensePtr& license)
{
    m_context->licensePool()->removeLicense(license);
}

void QnCommonMessageProcessor::on_eventRuleAddedOrUpdated(const nx::vms::api::EventRuleData& data)
{
    nx::vms::event::RulePtr eventRule(new nx::vms::event::Rule());
    ec2::fromApiToResource(data, eventRule);
    m_context->eventRuleManager()->addOrUpdateRule(eventRule);
}

void QnCommonMessageProcessor::on_businessEventRemoved(const QnUuid& id)
{
    m_context->eventRuleManager()->removeRule(id);
}

void QnCommonMessageProcessor::on_broadcastBusinessAction(const vms::event::AbstractActionPtr& action)
{
    emit businessActionReceived(action);
}

void QnCommonMessageProcessor::handleTourAddedOrUpdated(const nx::vms::api::LayoutTourData& tour)
{
    m_context->layoutTourManager()->addOrUpdateTour(tour);
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
    for (const QnResourcePtr& resource: resourcePool()->getResourcesWithFlag(Qn::remote))
    {
        remoteResources.insert(resource->getId(), resource);
    }

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
    m_context->licensePool()->replaceLicenses(licenses);
}

void QnCommonMessageProcessor::resetCamerasWithArchiveList(
    const ServerFootageDataList& cameraHistoryList)
{
    m_context->cameraHistoryPool()->resetServerFootageData(cameraHistoryList);
}

void QnCommonMessageProcessor::resetEventRules(const nx::vms::api::EventRuleDataList& eventRules)
{
    vms::event::RuleList ruleList;
    ec2::fromApiToResourceList(eventRules, ruleList);
    m_context->eventRuleManager()->resetRules(ruleList);
}

void QnCommonMessageProcessor::resetVmsRules(const nx::vms::api::rules::RuleList& vmsRules)
{
    emit vmsRulesReset(m_context->peerId(), vmsRules);
}

void QnCommonMessageProcessor::resetAccessRights(const AccessRightsDataList& accessRights)
{
    m_context->sharedResourcesManager()->reset(accessRights);
}

void QnCommonMessageProcessor::resetUserRoles(const UserRoleDataList& roles)
{
    m_context->userRolesManager()->resetUserRoles(roles);
}

bool QnCommonMessageProcessor::canRemoveResource(const QnUuid &, ec2::NotificationSource source)
{
    return true;
}

void QnCommonMessageProcessor::removeResourceIgnored(const QnUuid &)
{
}

bool QnCommonMessageProcessor::canRemoveResourceProperty(
    const QnUuid& /*resourceId*/, const QString& /*propertyName*/)
{
    return true;
}

void QnCommonMessageProcessor::refreshIgnoredResourceProperty(
    const QnUuid& /*resourceId*/, const QString& /*propertyName*/)
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
    auto pool = resourcePool();
    for (const auto& serverAttrs: serverUserAttributesList)
    {
        auto server = pool->getResourceById<QnMediaServerResource>(serverAttrs.serverId);
        if (server)
            server->setUserAttributes(serverAttrs);
        else
            m_serverUserAttributesCache[serverAttrs.serverId] = serverAttrs;
    }
}

void QnCommonMessageProcessor::resetCameraUserAttributesList(
    const CameraAttributesDataList& cameraUserAttributesList)
{
    auto pool = resourcePool();
    for (const auto& cameraAttrs: cameraUserAttributesList)
    {
        if (auto camera = pool->getResourceById<QnVirtualCameraResource>(cameraAttrs.cameraId))
            camera->setUserAttributes(cameraAttrs);
        else
            m_cameraUserAttributesCache[cameraAttrs.cameraId] = cameraAttrs;
    }
}

void QnCommonMessageProcessor::resetPropertyList(const ResourceParamWithRefDataList& params)
{
    // Store existing parameter keys.
    auto existingProperties = m_context->resourcePropertyDictionary()->allPropertyNamesByResource();

    // Update changed values.
    for (const auto& param: params)
    {
        on_resourceParamChanged(param, ec2::NotificationSource::Remote);
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
                ResourceParamWithRefData(resourceId, paramName, QString()),
                ec2::NotificationSource::Remote);
        }
    }
}

void QnCommonMessageProcessor::resetStatusList(const ResourceStatusDataList& params)
{
    auto keys = m_context->resourceStatusDictionary()->values().keys();
    m_context->resourceStatusDictionary()->clear();
    for(const QnUuid& id: keys)
    {
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
    m_context->resourceAccessManager()->beginUpdate();
    m_context->resourceAccessProvider()->beginUpdate();
    m_context->analyticsTaxonomyStateWatcher()->beginUpdate();

    m_context->serverAdditionalAddressesDictionary()->clear();

    resetResourceTypes(fullData.resourceTypes);
    resetServerUserAttributesList(fullData.serversUserAttributesList);
    resetCameraUserAttributesList(fullData.cameraUserAttributesList);
    resetPropertyList(fullData.allProperties);
    resetResources(fullData);
    resetCamerasWithArchiveList(fullData.cameraHistory);
    resetStatusList(fullData.resStatusList);
    resetAccessRights(fullData.accessRights);
    resetUserRoles(fullData.userRoles);
    resetLicenses(fullData.licenses);
    m_context->layoutTourManager()->resetTours(fullData.layoutTours);

    m_context->analyticsTaxonomyStateWatcher()->endUpdate();
    m_context->resourceAccessProvider()->endUpdate();
    m_context->resourceAccessManager()->endUpdate();

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
    QnUserResourcePtr qnUser(ec2::fromApiToResource(user));
    updateResource(qnUser, source);
}

void QnCommonMessageProcessor::updateResource(
    const LayoutData& layout,
    ec2::NotificationSource source)
{
    QnLayoutResourcePtr qnLayout = getResourceFactory()->createLayout();
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
    QnVideoWallResourcePtr qnVideowall(new QnVideoWallResource());
    ec2::fromApiToResource(videowall, qnVideowall);
    updateResource(qnVideowall, source);
}

void QnCommonMessageProcessor::updateResource(
    const WebPageData& webpage,
    ec2::NotificationSource source)
{
    QnWebPageResourcePtr qnWebpage(new QnWebPageResource());
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
        ec2::fromApiToResource(camera, qnCamera);
        NX_ASSERT(camera.id == QnVirtualCameraResource::physicalIdToId(qnCamera->getPhysicalId()),
            "You must fill camera ID as md5 hash of unique id");

        auto iter = m_cameraUserAttributesCache.find(camera.id);
        if (iter != m_cameraUserAttributesCache.cend())
        {
            qnCamera->setUserAttributes(iter->second);
            m_cameraUserAttributesCache.erase(iter);
        }

        updateResource(qnCamera, source);
    }
}

void QnCommonMessageProcessor::updateResource(
    const MediaServerData& serverData, ec2::NotificationSource source)
{
    QnMediaServerResourcePtr server = getResourceFactory()->createServer();
    ec2::fromApiToResource(serverData, server);
    auto iter = m_serverUserAttributesCache.find(serverData.id);
    if (iter != m_serverUserAttributesCache.cend())
    {
        server->setUserAttributes(iter->second);
        m_serverUserAttributesCache.erase(iter);
    }
    updateResource(server, source);
}

void QnCommonMessageProcessor::updateResource(
    const StorageData& storage, ec2::NotificationSource source)
{
    NX_VERBOSE(
        this,"updateResource: Updating/creating resource %1",
        nx::utils::url::hidePassword(storage.url));

    QnStorageResourcePtr qnStorage = getResourceFactory()->createResource(
        StorageData::kResourceTypeId,
        QnResourceParams(storage.id, storage.url, QString())).dynamicCast<QnStorageResource>();
    if (qnStorage)
    {
        ec2::fromApiToResource(storage, qnStorage);
        updateResource(qnStorage, source);
    }
    else
    {
        NX_WARNING(
            this, "Failed to create storage resource from data: '%1'",
            QJson::serialized(storage));
    }
}
