// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_context.h"

#include <api/runtime_info_manager.h>
#include <camera/camera_bookmarks_manager.h>
#include <camera/camera_data_manager.h>
#include <client/client_message_processor.h>
#include <core/resource/resource.h>
#include <core/resource_management/incompatible_server_watcher.h>
#include <core/resource_management/resource_runtime_data.h>
#include <nx/branding.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/server_runtime_events/server_runtime_event_connector.h>
#include <nx/vms/client/desktop/statistics/statistics_sender.h>
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
    std::unique_ptr<QnResourceRuntimeDataManager> resourceRuntimeDataManager;
    std::unique_ptr<StatisticsSender> statisticsSender;
    std::unique_ptr<VirtualCameraManager> virtualCameraManager;
    std::unique_ptr<VideoCache> videoCache;

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
    QnUuid peerId,
    nx::core::access::Mode resourceAccessMode,
    QObject* parent)
    :
    base_type(std::move(peerId), resourceAccessMode, parent),
    d(new Private{.q = this})
{
    d->initLocalRuntimeInfo();

    d->accessController = std::make_unique<QnWorkbenchAccessController>(this);
    d->videoWallOnlineScreensWatcher = std::make_unique<VideoWallOnlineScreensWatcher>(
        this);
    d->incompatibleServerWatcher = std::make_unique<QnIncompatibleServerWatcher>(this);
    d->serverRuntimeEventConnector = std::make_unique<ServerRuntimeEventConnector>();

    // Depends on ServerRuntimeEventConnector.
    d->serverStorageManager = std::make_unique<QnServerStorageManager>(this);

    d->cameraBookmarksManager = std::make_unique<QnCameraBookmarksManager>(this);
    d->cameraDataManager = std::make_unique<QnCameraDataManager>(this);

    d->resourceRuntimeDataManager = std::make_unique<QnResourceRuntimeDataManager>(this);

    d->statisticsSender = std::make_unique<StatisticsSender>(this);
    d->virtualCameraManager = std::make_unique<VirtualCameraManager>(this);
    d->videoCache = std::make_unique<VideoCache>(this);
}

SystemContext::~SystemContext()
{
}

SystemContext* SystemContext::fromResource(const QnResourcePtr& resource)
{
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

QnResourceRuntimeDataManager* SystemContext::resourceRuntimeDataManager() const
{
    return d->resourceRuntimeDataManager.get();
}

VirtualCameraManager* SystemContext::virtualCameraManager() const
{
    return d->virtualCameraManager.get();
}

VideoCache* SystemContext::videoCache() const
{
    return d->videoCache.get();
}

void SystemContext::setMessageProcessor(QnCommonMessageProcessor* messageProcessor)
{
    base_type::setMessageProcessor(messageProcessor);

    auto clientMessageProcessor = static_cast<QnClientMessageProcessor*>(messageProcessor);
    d->incompatibleServerWatcher->setMessageProcessor(clientMessageProcessor);
    d->serverRuntimeEventConnector->setMessageProcessor(clientMessageProcessor);
}

} // namespace nx::vms::client::desktop
