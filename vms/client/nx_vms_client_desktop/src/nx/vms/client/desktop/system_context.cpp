// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_context.h"

#include <QtQml/QQmlEngine> //< For registering types.

#include <api/runtime_info_manager.h>
#include <client/client_message_processor.h>
#include <client/client_runtime_settings.h>
#include <core/resource/resource.h>
#include <nx/branding.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/qml/qml_ownership.h>
#include <nx/vms/client/desktop/access/access_controller.h>
#include <nx/vms/client/desktop/access/caching_access_controller.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/system_logon/logic/remote_session.h>
#include <nx/vms/common/private/system_context_data_p.h>

#include "application_context.h"
#include "private/system_context_data_p.h"

namespace nx::vms::client::desktop {

namespace {

Qn::SerializationFormat serializationFormat()
{
    return ini().forceJsonConnection ? Qn::SerializationFormat::json : Qn::SerializationFormat::ubjson;
}

nx::vms::api::RuntimeData createLocalRuntimeInfo(SystemContext* q)
{
    nx::vms::api::RuntimeData runtimeData;
    runtimeData.peer.id = q->peerId();
    runtimeData.peer.peerType = appContext()->localPeerType();
    runtimeData.peer.dataFormat = serializationFormat();
    runtimeData.brand = ini().developerMode ? QString() : nx::branding::brand();
    runtimeData.customization = ini().developerMode ? QString() : nx::branding::customization();
    runtimeData.videoWallInstanceGuid = appContext()->videoWallInstanceId();
    return runtimeData;
}

} // namespace

SystemContext::SystemContext(Mode mode, nx::Uuid peerId, QObject* parent):
    base_type(mode, peerId, parent),
    d(new Private)
{
    resetAccessController(mode == Mode::client || mode == Mode::unitTests
        ? new CachingAccessController(this)
        : new AccessController(this));

    d->otherServersManager = std::make_unique<OtherServersManager>(this);

    switch (mode)
    {
        case Mode::client:
            runtimeInfoManager()->updateLocalItem(createLocalRuntimeInfo(this));
            d->videoWallOnlineScreensWatcher = std::make_unique<VideoWallOnlineScreensWatcher>(
                this);
            d->serverRuntimeEventConnector = std::make_unique<ServerRuntimeEventConnector>();
            // Depends on ServerRuntimeEventConnector.
            d->serverStorageManager = std::make_unique<QnServerStorageManager>(this);
            d->cameraBookmarksManager = std::make_unique<QnCameraBookmarksManager>(this);
            d->cameraDataManager = std::make_unique<QnCameraDataManager>(this);
            d->statisticsSender = std::make_unique<StatisticsSender>(this);
            d->virtualCameraManager = std::make_unique<VirtualCameraManager>(this);
            d->videoCache = std::make_unique<VideoCache>(this);
            // LocalResourcesInitializer must be created before LayoutSnapshotManager to avoid
            // modifying layouts after they are opened.
            d->localResourcesInitializer = std::make_unique<LocalResourcesInitializer>(this);
            d->layoutSnapshotManager = std::make_unique<LayoutSnapshotManager>(this);
            d->showreelStateManager = std::make_unique<ShowreelStateManager>(this);
            d->logsManagementWatcher = std::make_unique<LogsManagementWatcher>(this);
            d->mediaServerStatisticsManager = std::make_unique<QnMediaServerStatisticsManager>(
                this);
            d->localSettings = std::make_unique<SystemSpecificLocalSettings>(this);
            d->restApiHelper = std::make_unique<RestApiHelper>(this);
            d->delayedDataLoader = std::make_unique<DelayedDataLoader>(this);
            d->taxonomyManager = std::make_unique<analytics::TaxonomyManager>(this);
            d->ldapStatusWatcher = std::make_unique<LdapStatusWatcher>(this);
            d->nonEditableUsersAndGroups = std::make_unique<NonEditableUsersAndGroups>(this);
            d->defaultPasswordCamerasWatcher = std::make_unique<DefaultPasswordCamerasWatcher>(
                this);
            d->trafficRelayUrlWatcher = std::make_unique<TrafficRelayUrlWatcher>(this);
            d->localFileCache = std::make_unique<LocalFileCache>(this);
            d->serverImageCache = std::make_unique<ServerImageCache>(this);
            d->serverNotificationCache = std::make_unique<ServerNotificationCache>(this);
            d->serverRemoteAccessWatcher = std::make_unique<ServerRemoteAccessWatcher>(this);
            break;

        case Mode::crossSystem:
            d->cameraBookmarksManager = std::make_unique<QnCameraBookmarksManager>(this);
            d->cameraDataManager = std::make_unique<QnCameraDataManager>(this);
            d->videoCache = std::make_unique<VideoCache>(this);
            d->mediaServerStatisticsManager = std::make_unique<QnMediaServerStatisticsManager>(
                this);
            break;

        case Mode::cloudLayouts:
            d->layoutSnapshotManager = std::make_unique<LayoutSnapshotManager>(this);
            break;

        case Mode::unitTests:
            d->nonEditableUsersAndGroups = std::make_unique<NonEditableUsersAndGroups>(this);
            break;
    }

    connect(this, &SystemContext::userChanged, this,
        [this](const QnUserResourcePtr& user)
        {
            if (d->virtualCameraManager)
                d->virtualCameraManager->setCurrentUser(user);
        });
}

SystemContext::~SystemContext()
{
}

void SystemContext::registerQmlType()
{
    qmlRegisterUncreatableType<SystemContext>("nx.vms.client.desktop", 1, 0, "SystemContext",
        "Cannot create instance of SystemContext.");
}

SystemContext* SystemContext::fromResource(const QnResourcePtr& resource)
{
    if (!resource)
        return {};

    return dynamic_cast<SystemContext*>(resource->systemContext());
}

void SystemContext::initializeDesktopCamera()
{
    d->desktopCameraInitializer = std::make_unique<DesktopCameraInitializer>(this);
}

std::shared_ptr<RemoteSession> SystemContext::session() const
{
    return std::dynamic_pointer_cast<RemoteSession>(base_type::session());
}

AnalyticsEventsSearchTreeBuilder* SystemContext::analyticsEventsSearchTreeBuilder() const
{
    return d->analyticsEventsSearchTreeBuilder.get();
}

VideoWallOnlineScreensWatcher* SystemContext::videoWallOnlineScreensWatcher() const
{
    return d->videoWallOnlineScreensWatcher.get();
}

LdapStatusWatcher* SystemContext::ldapStatusWatcher() const
{
    return d->ldapStatusWatcher.get();
}

NonEditableUsersAndGroups* SystemContext::nonEditableUsersAndGroups() const
{
    return d->nonEditableUsersAndGroups.get();
}

ServerRuntimeEventConnector* SystemContext::serverRuntimeEventConnector() const
{
    return d->serverRuntimeEventConnector.get();
}

QnServerStorageManager* SystemContext::serverStorageManager() const
{
    return d->serverStorageManager.get();
}

QnCameraBookmarksManager* SystemContext::cameraBookmarksManager() const
{
    return d->cameraBookmarksManager.get();
}

QnCameraDataManager* SystemContext::cameraDataManager() const
{
    return d->cameraDataManager.get();
}

VirtualCameraManager* SystemContext::virtualCameraManager() const
{
    return d->virtualCameraManager.get();
}

VideoCache* SystemContext::videoCache() const
{
    return d->videoCache.get();
}

LayoutSnapshotManager* SystemContext::layoutSnapshotManager() const
{
    return d->layoutSnapshotManager.get();
}

ShowreelStateManager* SystemContext::showreelStateManager() const
{
    return d->showreelStateManager.get();
}

LogsManagementWatcher* SystemContext::logsManagementWatcher() const
{
    return d->logsManagementWatcher.get();
}

QnMediaServerStatisticsManager* SystemContext::mediaServerStatisticsManager() const
{
    return d->mediaServerStatisticsManager.get();
}

SystemSpecificLocalSettings* SystemContext::localSettings() const
{
    return d->localSettings.get();
}

RestApiHelper* SystemContext::restApiHelper() const
{
    return d->restApiHelper.get();
}

nx::Uuid SystemContext::localSystemId() const
{
    const auto& currentConnection = connection();
    return currentConnection ? currentConnection->moduleInformation().localSystemId : nx::Uuid();
}

analytics::TaxonomyManager* SystemContext::taxonomyManager() const
{
    // TODO: #sivanov Replace with Q_PROPERTY.
    return core::withCppOwnership(d->taxonomyManager.get());
}

OtherServersManager* SystemContext::otherServersManager() const
{
    return d->otherServersManager.get();
}

common::SessionTokenHelperPtr SystemContext::getSessionTokenHelper() const
{
    return d->restApiHelper->getSessionTokenHelper();
}

DefaultPasswordCamerasWatcher* SystemContext::defaultPasswordCamerasWatcher() const
{
    return d->defaultPasswordCamerasWatcher.get();
}

SystemHealthState* SystemContext::systemHealthState() const
{
    return d->systemHealthState.get();
}

TrafficRelayUrlWatcher* SystemContext::trafficRelayUrlWatcher() const
{
    return d->trafficRelayUrlWatcher.get();
}

LocalFileCache* SystemContext::localFileCache() const
{
    return d->localFileCache.get();
}

ServerImageCache* SystemContext::serverImageCache() const
{
    return d->serverImageCache.get();
}

ServerNotificationCache* SystemContext::serverNotificationCache() const
{
    return d->serverNotificationCache.get();
}

void SystemContext::setMessageProcessor(QnCommonMessageProcessor* messageProcessor)
{
    base_type::setMessageProcessor(messageProcessor);

    const auto mode = common::SystemContext::d->mode;
    if (mode != Mode::client)
        return;

    auto clientMessageProcessor = qobject_cast<QnClientMessageProcessor*>(messageProcessor);
    if (!NX_ASSERT(clientMessageProcessor, "Invalid message processor type"))
        return;

    d->otherServersManager->setMessageProcessor(clientMessageProcessor);
    d->serverRuntimeEventConnector->setMessageProcessor(clientMessageProcessor);
    d->logsManagementWatcher->setMessageProcessor(clientMessageProcessor);
    d->serverNotificationCache->setMessageProcessor(clientMessageProcessor);
    d->intercomManager = std::make_unique<IntercomManager>(this);
    d->analyticsEventsSearchTreeBuilder = std::make_unique<AnalyticsEventsSearchTreeBuilder>(this);
    d->systemHealthState = std::make_unique<SystemHealthState>(this);

    // Desktop camera must work in the normal mode only.
    if (appContext()->runtimeSettings()->isDesktopMode())
        initializeDesktopCamera();
}

} // namespace nx::vms::client::desktop
