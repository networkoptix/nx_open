// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "common_message_processor.h"

#include <unordered_map>

#include <QtCore/QElapsedTimer>

#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_factory.h>
#include <core/resource/storage_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/server_additional_addresses_dictionary.h>
#include <core/resource_management/status_dictionary.h>
#include <licensing/license.h>
#include <nx/fusion/serialization/json.h>
#include <nx/network/rest/user_access_data.h>
#include <nx/network/socket_common.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/api/data/access_rights_data_deprecated.h>
#include <nx/vms/api/data/full_info_data.h>
#include <nx/vms/api/data/runtime_data.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/showreel/showreel_manager.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/user_management/predefined_user_groups.h>
#include <nx/vms/common/user_management/user_group_manager.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_analytics_manager.h>
#include <nx_ec/managers/abstract_camera_manager.h>
#include <nx_ec/managers/abstract_discovery_manager.h>
#include <nx_ec/managers/abstract_event_rules_manager.h>
#include <nx_ec/managers/abstract_layout_manager.h>
#include <nx_ec/managers/abstract_license_manager.h>
#include <nx_ec/managers/abstract_lookup_list_manager.h>
#include <nx_ec/managers/abstract_media_server_manager.h>
#include <nx_ec/managers/abstract_misc_manager.h>
#include <nx_ec/managers/abstract_resource_manager.h>
#include <nx_ec/managers/abstract_showreel_manager.h>
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
using namespace nx::vms::common;

struct QnCommonMessageProcessor::Private
{
    std::unordered_map<nx::Uuid, CameraAttributesData> cameraUserAttributesCache;
    std::unordered_map<nx::Uuid, MediaServerUserAttributesData> serverUserAttributesCache;
};

QnCommonMessageProcessor::QnCommonMessageProcessor(
    nx::vms::common::SystemContext* context,
    QObject *parent)
    :
    base_type(parent),
    nx::vms::common::SystemContextAware(context),
    d(new Private)
{
}

QnCommonMessageProcessor::~QnCommonMessageProcessor()
{
}

void QnCommonMessageProcessor::init(const ec2::AbstractECConnectionPtr& connection)
{
    NX_VERBOSE(this, "Init with connection: %1", connection);

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

    NX_DEBUG(this, "Starting connection to %1",
        connection->moduleInformation().id.toSimpleString());

    qnSyncTime->setTimeNotificationManager(connection->timeNotificationManager());
    connectToConnection(connection);
    connection->startReceivingNotifications();
}

ec2::AbstractECConnectionPtr QnCommonMessageProcessor::connection() const
{
    return m_connection;
}

nx::Uuid QnCommonMessageProcessor::currentPeerId() const
{
    if (const auto connection = this->connection())
        return connection->moduleInformation().id;

    return {};
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
            // On the client side due to queued connections slot may be handled after disconnect.
            if (m_connection)
                updateResource(resource, source);
        };

    const auto on_hardwareIdMappingAdded =
        [this](const auto& hardwareIdMapping) { addHardwareIdMapping(hardwareIdMapping); };

    const auto on_hardwareIdMappingRemoved =
        [this](const nx::Uuid& id) { removeHardwareIdMapping(id); };

    const auto connectionType = handlerConnectionType();

    NX_VERBOSE(this, "Connecting to connection: %1, type: %2",
        connection->moduleInformation().id, connectionType);

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
        &QnCommonMessageProcessor::on_initNotification,
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
        &QnCommonMessageProcessor::on_userGroupChanged,
        connectionType);
    connect(
        userManager.get(),
        &ec2::AbstractUserNotificationManager::userRoleRemoved,
        this,
        &QnCommonMessageProcessor::on_userGroupRemoved,
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
        &QnCommonMessageProcessor::updateLicense,
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

    const auto vmsRulesManager = connection->vmsRulesNotificationManager();
    connect(
        vmsRulesManager.get(),
        &ec2::AbstractVmsRulesNotificationManager::eventReceived,
        this,
        &QnCommonMessageProcessor::vmsEventReceived,
        connectionType);
    connect(
        vmsRulesManager.get(),
        &ec2::AbstractVmsRulesNotificationManager::actionReceived,
        this,
        &QnCommonMessageProcessor::vmsActionReceived,
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

    const auto showreelNotificationManager = connection->showreelNotificationManager().get();
    connect(
        showreelNotificationManager,
        &ec2::AbstractShowreelNotificationManager::addedOrUpdated,
        this,
        &QnCommonMessageProcessor::handleTourAddedOrUpdated,
        connectionType);
    connect(
        showreelNotificationManager,
        &ec2::AbstractShowreelNotificationManager::removed,
        showreelManager(),
        &ShowreelManager::removeShowreel,
        connectionType);

    const auto lookupListNotificationManager = connection->lookupListNotificationManager().get();
    connect(lookupListNotificationManager,
        &ec2::AbstractLookupListNotificationManager::addedOrUpdated,
        lookupListManager(),
        &LookupListManager::addOrUpdate,
        connectionType);
    connect(lookupListNotificationManager,
        &ec2::AbstractLookupListNotificationManager::removed,
        lookupListManager(),
        &LookupListManager::remove,
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
    NX_VERBOSE(this, "Disconnecting from connection: %1", connection->moduleInformation().id);

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
    connection->showreelNotificationManager()->disconnect(this);
}

void QnCommonMessageProcessor::on_gotDiscoveryData(
    const nx::vms::api::DiscoveryData& data, bool addInformation)
{
    if (data.id.isNull())
        return;

    nx::Url url(data.url);

    auto server = resourcePool()->getResourceById<QnMediaServerResource>(data.id);
    if (!server)
    {
        const auto dictionary = serverAdditionalAddressesDictionary();
        if (!data.ignore)
        {
            QList<nx::Url> urls = dictionary->additionalUrls(data.id);
            urls.append(url);
            dictionary->setAdditionalUrls(data.id, urls);
        }
        else
        {
            QList<nx::Url> urls = dictionary->ignoredUrls(data.id);
            urls.append(url);
            dictionary->setIgnoredUrls(data.id, urls);
        }
        return;
    }

    QList<nx::network::SocketAddress> addresses = server->getNetAddrList();
    QList<nx::Url> additionalUrls = server->getAdditionalUrls();
    QList<nx::Url> ignoredUrls = server->getIgnoredUrls();

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

void QnCommonMessageProcessor::on_remotePeerFound(nx::Uuid data, PeerType peerType)
{
    handleRemotePeerFound(data, peerType);
    emit remotePeerFound(data, peerType);
}

void QnCommonMessageProcessor::on_remotePeerLost(nx::Uuid data, PeerType peerType)
{
    handleRemotePeerLost(data, peerType);
    emit remotePeerLost(data, peerType);
}

void QnCommonMessageProcessor::on_resourceStatusChanged(
    const nx::Uuid& resourceId,
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
        resourceStatusDictionary()->setValue(resourceId, localStatus);
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
        resourcePropertyDictionary()->setValue(
            param.resourceId,
            param.name,
            param.value,
            /*markDirty*/ false);
    }

    if (param.name == Qn::kResourceDataParamName)
    {
        const bool loaded = param.value.isEmpty()
            || systemContext()->resourceDataPool()->loadData(param.value.toUtf8());
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
        resourcePropertyDictionary()->on_resourceParamRemoved(
            param.resourceId,
            param.name);
    }
    else
    {
        refreshIgnoredResourceProperty(param.resourceId, param.name);
    }
}

void QnCommonMessageProcessor::on_resourceRemoved(
    const nx::Uuid& resourceId, ec2::NotificationSource source)
{
    if (canRemoveResource(resourceId, source))
    {
        if (QnResourcePtr ownResource = resourcePool()->getResourceById(resourceId))
            resourcePool()->removeResource(ownResource);
        resourceStatusDictionary()->remove(resourceId);
    }
    else
        removeResourceIgnored(resourceId);
}

void QnCommonMessageProcessor::on_resourceStatusRemoved(
    const nx::Uuid& resourceId, ec2::NotificationSource source)
{
    if (!canRemoveResource(resourceId, source))
    {
        if (auto res = resourcePool()->getResourceById(resourceId))
        {
            if (auto connection = messageBusConnection())
            {
                connection->getResourceManager(nx::network::rest::kSystemSession)->setResourceStatus(
                    resourceId, res->getStatus(), [](int /*requestId*/, ec2::ErrorCode) {});
            }
        }
    }
}

void QnCommonMessageProcessor::on_accessRightsChanged(
    const AccessRightsDataDeprecated& accessRightsDeprecated)
{
    using namespace nx::vms::api;
    static constexpr AccessRights kLiveViewerAccessRights = AccessRight::view | AccessRight::audio;
    static constexpr AccessRights kViewerAccessRights = kLiveViewerAccessRights
        | AccessRight::viewArchive | AccessRight::exportArchive | AccessRight::viewBookmarks;
    static constexpr AccessRights kAdvancedViewerAccessRights =
        kViewerAccessRights | AccessRight::manageBookmarks | AccessRight::userInput;

    std::map<nx::Uuid, AccessRights> resourceAccessRights;
    std::optional<UserGroupData> group;
    auto user =
        resourcePool()->getResourceById<QnUserResource>(accessRightsDeprecated.userId);
    if (user)
    {
        resourceAccessRights = user->ownResourceAccessRights();
    }
    else
    {
        group = userGroupManager()->find(accessRightsDeprecated.userId);
        if (!group)
        {
            NX_DEBUG(this, "No user or group %1 found", accessRightsDeprecated.userId);
            return;
        }

        resourceAccessRights = group->resourceAccessRights;
    }
    AccessRights accessRights{};
    for (auto it = resourceAccessRights.begin(); it != resourceAccessRights.end();)
    {
        auto groupId = it->first;
        it = PredefinedUserGroups::contains(groupId) ? std::next(it) : resourceAccessRights.erase(it);
        if (groupId == kAdministratorsGroupId || groupId == kPowerUsersGroupId)
            accessRights = kFullAccessRights;
        else if (groupId == kAdvancedViewersGroupId)
            accessRights = kAdvancedViewerAccessRights;
        else if (groupId == kViewersGroupId)
            accessRights = kViewerAccessRights;
        else if (groupId == kLiveViewersGroupId)
            accessRights = kLiveViewerAccessRights;
    }
    for (const auto& r: accessRightsDeprecated.resourceIds)
        resourceAccessRights[r] = accessRights;
    if (user)
    {
        user->setResourceAccessRights(resourceAccessRights);
    }
    else
    {
        group->resourceAccessRights = std::move(resourceAccessRights);
        on_userGroupChanged(*group);
    }
}

void QnCommonMessageProcessor::on_userGroupChanged(const UserGroupData& userGroup)
{
    userGroupManager()->addOrUpdate(userGroup);
    accessRightsManager()->setOwnResourceAccessMap(userGroup.id,
        {userGroup.resourceAccessRights.begin(), userGroup.resourceAccessRights.end()});
}

void QnCommonMessageProcessor::on_userGroupRemoved(const nx::Uuid& userGroupId)
{
    userGroupManager()->remove(userGroupId);

    for (const auto& user: resourcePool()->getResources<QnUserResource>())
    {
        const auto isGroupRemoved =
            [&userGroupId](const auto& id) { return id == userGroupId; };

        auto groupIds = user->siteGroupIds();
        if (nx::utils::erase_if(groupIds, isGroupRemoved))
            user->setSiteGroupIds(groupIds);
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
        d->cameraUserAttributesCache[userAttributes.cameraId] = userAttributes;
    }
}

void QnCommonMessageProcessor::on_cameraUserAttributesRemoved(const nx::Uuid& cameraId)
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
        d->serverUserAttributesCache[attrs.serverId] = attrs;
}

void QnCommonMessageProcessor::on_mediaServerUserAttributesRemoved(const nx::Uuid& serverId)
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
    cameraHistoryPool()->setServerFootageData(serverFootageData);
}

void QnCommonMessageProcessor::updateLicense(const QnLicensePtr& license)
{
    systemContext()->licensePool()->addLicense(license);
}

void QnCommonMessageProcessor::on_licenseRemoved(const QnLicensePtr& license)
{
    licensePool()->removeLicense(license);
}

void QnCommonMessageProcessor::on_eventRuleAddedOrUpdated(const nx::vms::api::EventRuleData& data)
{
    nx::vms::event::RulePtr eventRule(new nx::vms::event::Rule());
    ec2::fromApiToResource(data, eventRule);
    eventRuleManager()->addOrUpdateRule(eventRule);
}

void QnCommonMessageProcessor::on_businessEventRemoved(const nx::Uuid& id)
{
    eventRuleManager()->removeRule(id);
}

void QnCommonMessageProcessor::handleTourAddedOrUpdated(
    const nx::vms::api::ShowreelData& showreel)
{
    showreelManager()->addOrUpdateShowreel(showreel);
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
    QHash<nx::Uuid, QnResourcePtr> remoteResources;
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
    licensePool()->replaceLicenses(licenses);
    saasServiceManager()->updateLocalRecordingLicenseV1();
}

void QnCommonMessageProcessor::resetCamerasWithArchiveList(
    const ServerFootageDataList& cameraHistoryList)
{
    cameraHistoryPool()->resetServerFootageData(cameraHistoryList);
}

void QnCommonMessageProcessor::resetEventRules(const nx::vms::api::EventRuleDataList& eventRules)
{
    vms::event::RuleList ruleList;
    ec2::fromApiToResourceList(eventRules, ruleList);
    eventRuleManager()->resetRules(ruleList);
}

void QnCommonMessageProcessor::resetVmsRules(const nx::vms::api::rules::RuleList& vmsRules)
{
    emit vmsRulesReset(peerId(), vmsRules);
}

void QnCommonMessageProcessor::resetUserGroups(const UserGroupDataList& userGroups)
{
    userGroupManager()->resetAll(userGroups);

    // Ensure group permissions cached
    for (const auto& group: userGroups)
    {
        accessRightsManager()->setOwnResourceAccessMap(
            group.id, {group.resourceAccessRights.begin(), group.resourceAccessRights.end()});
    }
}

bool QnCommonMessageProcessor::canRemoveResource(const nx::Uuid&, ec2::NotificationSource)
{
    return true;
}

void QnCommonMessageProcessor::removeResourceIgnored(const nx::Uuid&)
{
}

bool QnCommonMessageProcessor::canRemoveResourceProperty(
    const nx::Uuid& /*resourceId*/, const QString& /*propertyName*/)
{
    return true;
}

void QnCommonMessageProcessor::refreshIgnoredResourceProperty(
    const nx::Uuid& /*resourceId*/, const QString& /*propertyName*/)
{
}

void QnCommonMessageProcessor::handleRemotePeerFound(nx::Uuid id, PeerType peerType)
{
    NX_VERBOSE(this, "Remote peer found, id: %1, type: %2", id, peerType);
}

void QnCommonMessageProcessor::handleRemotePeerLost(nx::Uuid id, PeerType peerType)
{
    NX_VERBOSE(this, "Remote peer lost, id: %1, type: %2", id, peerType);
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
            d->serverUserAttributesCache[serverAttrs.serverId] = serverAttrs;
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
            d->cameraUserAttributesCache[cameraAttrs.cameraId] = cameraAttrs;
    }
}

void QnCommonMessageProcessor::resetPropertyList(const ResourceParamWithRefDataList& params)
{
    // Store existing parameter keys.
    auto existingProperties = resourcePropertyDictionary()->allPropertyNamesByResource();

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
        nx::Uuid resourceId = iter.key();
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
    const auto dictionary = resourceStatusDictionary();
    auto keys = dictionary->values().keys();
    dictionary->clear();
    for(const nx::Uuid& id: keys)
    {
        if (QnResourcePtr resource = resourcePool()->getResourceById(id))
        {
            NX_VERBOSE(this,
                "%1 Emit statusChanged signal for resource %2, %3, %4",
                Q_FUNC_INFO, resource->getId(), resource->getName(), resource->getUrl());
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

void QnCommonMessageProcessor::on_initNotification(const FullInfoData& fullData)
{
    NX_DEBUG(this, "Received initial notification while connecting to %1", currentPeerId());

    nx::utils::ElapsedTimer timer(nx::utils::ElapsedTimerState::started);

    onGotInitialNotification(fullData);

    NX_DEBUG(this, "Initial notification processed in %1", timer.elapsed());

    emit initialResourcesReceived();
}

void QnCommonMessageProcessor::onGotInitialNotification(const FullInfoData& fullData)
{
    resourceAccessManager()->beginUpdate();

    serverAdditionalAddressesDictionary()->clear();

    resetResourceTypes(fullData.resourceTypes);
    resetServerUserAttributesList(fullData.serversUserAttributesList);
    resetCameraUserAttributesList(fullData.cameraUserAttributesList);
    resetPropertyList(fullData.allProperties);
    resetUserGroups(fullData.userGroups);
    resetResources(fullData);
    resetCamerasWithArchiveList(fullData.cameraHistory);
    resetStatusList(fullData.resStatusList);
    resetLicenses(fullData.licenses);

    // TODO: #sivanov Logic is not perfect, who will clean them on disconnect?
    resetEventRules(fullData.rules);
    resetVmsRules(fullData.vmsRules);

    showreelManager()->resetShowreels(fullData.showreels);

    resourceAccessManager()->endUpdate();
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
    QnUserResourcePtr qnUser = getResourceFactory()->createUser(systemContext(), user);
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

    if (NX_ASSERT(qnCamera, "Unknown resource type: %1", camera.typeId))
    {
        ec2::fromApiToResource(camera, qnCamera);
        NX_ASSERT(camera.id == QnVirtualCameraResource::physicalIdToId(qnCamera->getPhysicalId()),
            "You must fill camera ID as md5 hash of unique id");

        auto iter = d->cameraUserAttributesCache.find(camera.id);
        if (iter != d->cameraUserAttributesCache.cend())
        {
            qnCamera->setUserAttributes(iter->second);
            d->cameraUserAttributesCache.erase(iter);
        }

        updateResource(qnCamera, source);
    }
}

void QnCommonMessageProcessor::updateResource(
    const MediaServerData& serverData, ec2::NotificationSource source)
{
    QnMediaServerResourcePtr server = getResourceFactory()->createServer();
    ec2::fromApiToResource(serverData, server);
    auto iter = d->serverUserAttributesCache.find(serverData.id);
    if (iter != d->serverUserAttributesCache.cend())
    {
        server->setUserAttributes(iter->second);
        d->serverUserAttributesCache.erase(iter);
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
