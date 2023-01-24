// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_context.h"

#include <api/media_server_statistics_manager.h>
#include <api/runtime_info_manager.h>
#include <camera/camera_bookmarks_manager.h>
#include <camera/camera_data_manager.h>
#include <client/client_message_processor.h>
#include <core/resource/resource.h>
#include <core/resource_management/incompatible_server_watcher.h>
#include <nx/branding.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource/layout_snapshot_manager.h>
#include <nx/vms/client/desktop/server_runtime_events/server_runtime_event_connector.h>
#include <nx/vms/client/desktop/settings/system_specific_local_settings.h>
#include <nx/vms/client/desktop/statistics/statistics_sender.h>
#include <nx/vms/client/desktop/system_administration/watchers/logs_management_watcher.h>
#include <nx/vms/client/desktop/utils/video_cache.h>
#include <nx/vms/client/desktop/utils/virtual_camera_manager.h>
#include <nx/vms/client/desktop/videowall/videowall_online_screens_watcher.h>
#include <server/server_storage_manager.h>
#include <ui/workbench/workbench_access_controller.h>

#include "application_context.h"

namespace nx::vms::client::desktop {

namespace {

Qn::SerializationFormat serializationFormat()
{
    return ini().forceJsonConnection ? Qn::JsonFormat : Qn::UbjsonFormat;
}

} // namespace

struct SystemContext::Private
{
    SystemContext* const q;
    std::unique_ptr<QnWorkbenchAccessController> accessController;
    std::unique_ptr<VideoWallOnlineScreensWatcher> videoWallOnlineScreensWatcher;
    std::unique_ptr<QnIncompatibleServerWatcher> incompatibleServerWatcher;
    std::unique_ptr<ServerRuntimeEventConnector> serverRuntimeEventConnector;
    std::unique_ptr<QnServerStorageManager> serverStorageManager;
    std::unique_ptr<QnCameraBookmarksManager> cameraBookmarksManager;
    std::unique_ptr<QnCameraDataManager> cameraDataManager;
    std::unique_ptr<StatisticsSender> statisticsSender;
    std::unique_ptr<VirtualCameraManager> virtualCameraManager;
    std::unique_ptr<VideoCache> videoCache;
    std::unique_ptr<LayoutSnapshotManager> layoutSnapshotManager;
    std::unique_ptr<LogsManagementWatcher> logsManagementWatcher;
    std::unique_ptr<QnMediaServerStatisticsManager> mediaServerStatisticsManager;
    std::unique_ptr<SystemSpecificLocalSettings> localSettings;

    void initLocalRuntimeInfo()
    {
        nx::vms::api::RuntimeData runtimeData;
        runtimeData.peer.id = q->peerId();
        runtimeData.peer.instanceId = q->sessionId();
        runtimeData.peer.peerType = appContext()->localPeerType();
        runtimeData.peer.dataFormat = serializationFormat();
        runtimeData.brand = ini().developerMode ? QString() : nx::branding::brand();
        runtimeData.customization = ini().developerMode ? QString() : nx::branding::customization();
        runtimeData.videoWallInstanceGuid = appContext()->videoWallInstanceId();
        q->runtimeInfoManager()->updateLocalItem(runtimeData);
    }

};

SystemContext::SystemContext(
    Mode mode,
    QnUuid peerId,
    nx::core::access::Mode resourceAccessMode,
    QObject* parent)
    :
    base_type(mode, std::move(peerId), resourceAccessMode, parent),
    d(new Private{.q = this})
{
    d->accessController = std::make_unique<QnWorkbenchAccessController>(this, resourceAccessMode);

    switch (mode)
    {
        case Mode::client:
            d->initLocalRuntimeInfo();
            d->videoWallOnlineScreensWatcher = std::make_unique<VideoWallOnlineScreensWatcher>(
                this);
            d->incompatibleServerWatcher = std::make_unique<QnIncompatibleServerWatcher>(this);
            d->serverRuntimeEventConnector = std::make_unique<ServerRuntimeEventConnector>();
            // Depends on ServerRuntimeEventConnector.
            d->serverStorageManager = std::make_unique<QnServerStorageManager>(this);
            d->cameraBookmarksManager = std::make_unique<QnCameraBookmarksManager>(this);
            d->cameraDataManager = std::make_unique<QnCameraDataManager>(this);
            d->statisticsSender = std::make_unique<StatisticsSender>(this);
            d->virtualCameraManager = std::make_unique<VirtualCameraManager>(this);
            d->videoCache = std::make_unique<VideoCache>(this);
            d->layoutSnapshotManager = std::make_unique<LayoutSnapshotManager>(this);
            d->logsManagementWatcher = std::make_unique<LogsManagementWatcher>(this);
            d->mediaServerStatisticsManager = std::make_unique<QnMediaServerStatisticsManager>(
                this);
            d->localSettings = std::make_unique<SystemSpecificLocalSettings>(this);
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
            break;
    }

}

SystemContext::~SystemContext()
{
}

SystemContext* SystemContext::fromResource(const QnResourcePtr& resource)
{
    if (!resource)
        return {};

    return dynamic_cast<SystemContext*>(resource->systemContext());
}

QnWorkbenchAccessController* SystemContext::accessController() const
{
    return d->accessController.get();
}

VideoWallOnlineScreensWatcher* SystemContext::videoWallOnlineScreensWatcher() const
{
    return d->videoWallOnlineScreensWatcher.get();
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

void SystemContext::setMessageProcessor(QnCommonMessageProcessor* messageProcessor)
{
    base_type::setMessageProcessor(messageProcessor);

    auto clientMessageProcessor = static_cast<QnClientMessageProcessor*>(messageProcessor);
    d->incompatibleServerWatcher->setMessageProcessor(clientMessageProcessor);
    d->serverRuntimeEventConnector->setMessageProcessor(clientMessageProcessor);
    d->logsManagementWatcher->setMessageProcessor(clientMessageProcessor);
}

} // namespace nx::vms::client::desktop
