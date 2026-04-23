// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_context.h"

#include <QtGui/QGuiApplication>

#include <api/runtime_info_manager.h>
#include <camera/camera_thumbnail_cache.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>
#include <mobile_client/mobile_client_message_processor.h>
#include <mobile_client/mobile_client_settings.h>
#include <nx/branding.h>
#include <nx/client/mobile/soft_trigger/event_rules_watcher.h>
#include <nx/client/mobile/two_way_audio/server_audio_connection_watcher.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/event_search/models/analytics_search_list_model.h>
#include <nx/vms/client/core/event_search/models/bookmark_search_list_model.h>
#include <nx/vms/client/core/event_search/models/event_search_model_adapter.h>
#include <nx/vms/client/core/event_search/utils/analytics_search_setup.h>
#include <nx/vms/client/core/event_search/utils/bookmark_search_setup.h>
#include <nx/vms/client/core/resource/resource_processor.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_resource_searcher.h>
#include <nx/vms/client/core/two_way_audio/two_way_audio_controller.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/mobile/application_context.h>
#include <nx/vms/client/mobile/event_search/utils/common_object_search_setup.h>
#include <nx/vms/client/mobile/workarounds/user_rights_workaround.h>
#include <nx/vms/discovery/manager.h>
#include <ui/camera_thumbnail_provider.h>
#include <watchers/available_cameras_watcher.h>

namespace nx::vms::client::mobile {

namespace {

static const nx::utils::SoftwareVersion kObjectSearchMinimalSupportVersion(5, 0);

} // namespace

struct SystemContext::Private
{
    std::unique_ptr<QnAvailableCamerasWatcher> availableCamerasWatcher;
    std::unique_ptr<QnResourceDiscoveryManager> resourceDiscoveryManager;
    std::unique_ptr<core::ResourceProcessor> resourceProcessor;
    std::unique_ptr<core::DesktopResourceSearcher> desktopResourceSearcher;
    std::unique_ptr<nx::client::mobile::ServerAudioConnectionWatcher> serverAudioConnectionWatcher;
    std::unique_ptr<core::TwoWayAudioController> twoWayAudioController;
    std::unique_ptr<nx::client::mobile::EventRulesWatcher> eventRulesWatcher;
    std::unique_ptr<QnCameraThumbnailCache> thumbnailsCache;

    QnCameraThumbnailProvider* thumbnailProvider() const;
};

QnCameraThumbnailProvider* SystemContext::Private::thumbnailProvider() const
{
    return appContext()->cameraThumbnailProvider();
}

SystemContext* SystemContext::fromResource(const QnResource* resource)
{
    if (!resource)
        return {};

    return dynamic_cast<SystemContext*>(resource->systemContext());
}

SystemContext* SystemContext::fromResource(const QnResourcePtr& resource)
{
    if (!resource)
        return {};

    return dynamic_cast<SystemContext*>(resource->systemContext());
}

SystemContext::SystemContext(Mode mode,
    nx::Uuid peerId,
    QObject* parent)
    :
    base_type(mode, peerId, parent),
    d{new Private{}}
{
    if (mode == Mode::cloudLayouts)
        return;

    if (mode == Mode::unitTests)
        return;

    d->thumbnailsCache = std::make_unique<QnCameraThumbnailCache>(this);

    if (d->thumbnailProvider())
        d->thumbnailProvider()->addThumbnailCache(d->thumbnailsCache.get());

    if (mode == Mode::crossSystem)
        return;

    createMessageProcessor<QnMobileClientMessageProcessor>(this);
    UserRightsWorkaround::install(this);

    startModuleDiscovery(core::appContext()->moduleDiscoveryManager());

    // TODO: Move all system context related initializations to the common and core system
    // contexts. Now we can't do it because of desktop client initialization.

    nx::vms::api::RuntimeData runtimeData;
    runtimeData.peer.id = peerId;
    runtimeData.peer.peerType = nx::vms::api::PeerType::mobileClient;
    runtimeData.peer.dataFormat = Qn::SerializationFormat::json;
    runtimeData.brand = nx::branding::brand();
    runtimeData.customization = nx::branding::customization();
    runtimeInfoManager()->updateLocalItem(
        [&runtimeData](auto* data)
        {
            *data = QnPeerRuntimeInfo(runtimeData);
            return true;
        });

    const auto updateDiscoveryState =
        [this]()
        {
            const auto moduleManager = core::appContext()->moduleDiscoveryManager();
            switch (qApp->applicationState())
            {
                case Qt::ApplicationActive:
                    moduleManager->start(resourcePool());
                    break;
                case Qt::ApplicationSuspended:
                    moduleManager->stop();
                    break;
                default:
                    break;
            }
        };

    updateDiscoveryState();
    connect(qApp, &QGuiApplication::applicationStateChanged, this, updateDiscoveryState);

    d->availableCamerasWatcher = std::make_unique<QnAvailableCamerasWatcher>(this);
    d->resourceDiscoveryManager = std::make_unique<QnResourceDiscoveryManager>(this);
    d->resourceProcessor = std::make_unique<core::ResourceProcessor>(this);
    d->resourceProcessor->moveToThread(d->resourceDiscoveryManager.get());
    d->resourceDiscoveryManager->setResourceProcessor(d->resourceProcessor.get());
    d->resourceDiscoveryManager->setReady(true);
    d->resourceDiscoveryManager->start();

    d->serverAudioConnectionWatcher =
        std::make_unique<nx::client::mobile::ServerAudioConnectionWatcher>(this);

    d->twoWayAudioController = std::make_unique<core::TwoWayAudioController>(this);

    const auto updateTimeMode =
        [this]()
        {
            serverTimeWatcher()->setTimeMode(qnSettings->serverTimeMode()
                ? core::ServerTimeWatcher::serverTimeMode
                : core::ServerTimeWatcher::clientTimeMode);
        };
    const auto notifier = qnSettings->notifier(QnMobileClientSettings::ServerTimeMode);
    connect(notifier, &QnPropertyNotifier::valueChanged, this, updateTimeMode);
    updateTimeMode();

    // Workaround for supporting audio-only devices when connecting to server versions earlier than
    // 6.1, which do not send resource type information for mobile client.
    connect(resourcePool(), &QnResourcePool::resourcesAdded, this,
        [this](const QnResourceList& resources)
        {
            if (moduleInformation().version >= utils::SoftwareVersion{6, 1}
                || !qnResTypePool->isEmpty())
            {
                return;
            }

            for (const auto& resource: resources)
            {
                auto camera = resource.dynamicCast<QnVirtualCameraResource>();
                if (camera && !camera->isAudioEnabled())
                    camera->setAudioEnabled(true);
            }
        });

    connect(accessController(), &core::AccessController::permissionsMaybeChanged,
        this, &SystemContext::hasSearchObjectsPermissionChanged);

    connect(accessController(), &core::AccessController::permissionsMaybeChanged,
        this, &SystemContext::hasViewBookmarksPermissionChanged);

    d->eventRulesWatcher = std::make_unique<nx::client::mobile::EventRulesWatcher>(this);
}

SystemContext::~SystemContext()
{
    if (d->thumbnailProvider())
        d->thumbnailProvider()->removeThumbnailCache(d->thumbnailsCache.get());

    if (messageProcessor())
        deleteMessageProcessor();
}

QnAvailableCamerasWatcher* SystemContext::availableCamerasWatcher() const
{
    return d->availableCamerasWatcher.get();
}

QnResourceDiscoveryManager* SystemContext::resourceDiscoveryManager() const
{
    return d->resourceDiscoveryManager.get();
}

core::TwoWayAudioController* SystemContext::twoWayAudioController() const
{
    return d->twoWayAudioController.get();
}

QnCameraThumbnailCache* SystemContext::cameraThumbnailCache() const
{
    return d->thumbnailsCache.get();
}

core::EventSearchModelAdapter* SystemContext::createSearchModel(
    bool analyticsMode)
{
    using namespace nx::vms::client::core;
    const auto adapter = new EventSearchModelAdapter();
    adapter->setSearchModel(analyticsMode
        ? static_cast<AbstractAsyncSearchListModel*>(new AnalyticsSearchListModel(this, adapter))
        : static_cast<AbstractAsyncSearchListModel*>(new BookmarkSearchListModel(this, adapter)));
    return adapter;
}

core::AnalyticsSearchSetup* SystemContext::createAnalyticsSearchSetup(
    core::EventSearchModelAdapter* model)
{
    if (!model)
        return nullptr;

    using namespace nx::vms::client::core;
    if (const auto analyticsModel = qobject_cast<AnalyticsSearchListModel*>(model->searchModel()))
        return new AnalyticsSearchSetup(analyticsModel);

    return nullptr;
}

core::BookmarkSearchSetup* SystemContext::createBookmarkSearchSetup(
    core::EventSearchModelAdapter* model)
{
    if (!model)
        return nullptr;

    using namespace nx::vms::client::core;
    if (const auto bookmarksModel = qobject_cast<BookmarkSearchListModel*>(model->searchModel()))
        return new core::BookmarkSearchSetup(bookmarksModel);

    return nullptr;
}

nx::vms::client::core::CommonObjectSearchSetup* SystemContext::createSearchSetup(
    core::EventSearchModelAdapter* model)
{
    const auto result = new CommonObjectSearchSetup(this);
    if (model)
        result->setModel(model->searchModel());
    return result;
}

bool SystemContext::hasViewBookmarksPermission()
{
    return accessController()->isDeviceAccessRelevant(
        nx::vms::api::AccessRight::viewBookmarks);
}

bool SystemContext::hasSearchObjectsPermission()
{
    const auto server = currentServer();
    return accessController()->isDeviceAccessRelevant(nx::vms::api::AccessRight::viewArchive)
        && (!server || server->getVersion() >= kObjectSearchMinimalSupportVersion);
}

nx::client::mobile::EventRulesWatcher* SystemContext::eventRulesWatcher() const
{
    return d->eventRulesWatcher.get();
}

} // namespace nx::vms::client::mobile
