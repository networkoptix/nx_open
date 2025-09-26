// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_context.h"

#include <QtGui/QGuiApplication>
#include <QtQml/QQmlEngine>

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
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/event_search/models/analytics_search_list_model.h>
#include <nx/vms/client/core/event_search/models/bookmark_search_list_model.h>
#include <nx/vms/client/core/event_search/models/event_search_model_adapter.h>
#include <nx/vms/client/core/event_search/utils/analytics_search_setup.h>
#include <nx/vms/client/core/event_search/utils/bookmark_search_setup.h>
#include <nx/vms/client/core/network/helpers.h>
#include <nx/vms/client/core/network/network_manager.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection_factory.h>
#include <nx/vms/client/core/network/remote_session_timeout_watcher.h>
#include <nx/vms/client/core/resource/resource_processor.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_resource_searcher.h>
#include <nx/vms/client/core/system_logon/remote_connection_user_interaction_delegate.h>
#include <nx/vms/client/core/two_way_audio/two_way_audio_controller.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/mobile/application_context.h>
#include <nx/vms/client/mobile/event_search/utils/common_object_search_setup.h>
#include <nx/vms/client/mobile/session/session_manager.h>
#include <nx/vms/client/mobile/system_context.h>
#include <nx/vms/client/mobile/ui/detail/screens.h>
#include <nx/vms/client/mobile/ui/qml_wrapper_helper.h>
#include <nx/vms/client/mobile/ui/ui_controller.h>
#include <nx/vms/client/mobile/window_context.h>
#include <nx/vms/discovery/manager.h>
#include <ui/camera_thumbnail_provider.h>
#include <watchers/available_cameras_watcher.h>

namespace nx::vms::client::mobile {

namespace {

static const nx::utils::SoftwareVersion kObjectSearchMinimalSupportVersion(5, 0);

void initializeConnectionUserInteractionDelegate(SystemContext* systemContext)
{
    using namespace nx::vms::client::core;
    using Delegate = nx::vms::client::core::RemoteConnectionUserInteractionDelegate;

    const auto validateToken =
        [windowContext = systemContext->windowContext()](
            const nx::network::http::Credentials& credentials)
        {
            return windowContext->uiController()->screens()->show2faValidationScreen(credentials);
        };

    const auto askToAcceptCertificates =
        [systemContext](const QList<core::TargetCertificateInfo>& certificatesInfo,
            CertificateWarning::Reason reason)
        {
            if (!NX_ASSERT(!certificatesInfo.isEmpty()))
                return false;

            const auto& info = certificatesInfo.first();

            const auto url = QUrl(reason == CertificateWarning::Reason::unknownServer
                ? "Nx/Web/UnknownSslCertificateDialog.qml"
                : "Nx/Web/InvalidOrChangedCertificateDialog.qml");
            QVariantMap props;
            props["title"] = CertificateWarning::header(
                reason, info.target, certificatesInfo.size());
            props["messageText"] = CertificateWarning::details(reason, certificatesInfo.size());
            props["adviceText"] = CertificateWarning::advice(
                reason, CertificateWarning::ClientType::mobile);

                   // TODO: Show all certificates.
            if (const auto& chain = info.chain;
                !chain.empty())
            {
                const auto& certificate = *chain.begin();
                const auto& expiresAt = duration_cast<std::chrono::seconds>(
                    certificate.notAfter().time_since_epoch());
                props["certificateExpires"] =
                    QDateTime::fromSecsSinceEpoch(expiresAt.count()).toString();
                props["certificateCommonName"] = QString::fromStdString(
                    certificateName(certificate.subject()).value_or(std::string()));
                props["certificateIssuedBy"] = QString::fromStdString(
                    certificateName(certificate.issuer()).value_or(std::string()));
                props["certificateSHA256"] = formattedFingerprint(certificate.sha256());
                props["certificateSHA1"] = formattedFingerprint(certificate.sha1());
            }

            return QmlWrapperHelper::showPopup(
                systemContext->windowContext(), url, props) == "connect";
        };

    const auto showCertificateError =
        [](const core::TargetCertificateInfo& /*certificateInfo*/)
        {
          // Mobile client shows connection error itself.
        };

    auto delegate = std::make_unique<Delegate>(
        systemContext, validateToken, askToAcceptCertificates, showCertificateError);
    appContext()->networkModule()->connectionFactory()->setUserInteractionDelegate(std::move(delegate));
}

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

    const QPointer<QnCameraThumbnailProvider> thumbnailProvider{
        appContext()->cameraThumbnailProvider()};
};

SystemContext::SystemContext(WindowContext* context,
    Mode mode,
    nx::Uuid peerId,
    QObject* parent)
    :
    base_type(mode, peerId, parent),
    WindowContextAware(context),
    d{new Private{}}
{
    if (mode == Mode::cloudLayouts)
        return;

    d->thumbnailsCache = std::make_unique<QnCameraThumbnailCache>(this);

    connect(sessionManager(), &SessionManager::stateChanged, this,
        [this]()
        {
            if (sessionManager()->hasConnectedSession())
                d->thumbnailsCache->start();
            else
                d->thumbnailsCache->stop();
        });

    if (d->thumbnailProvider)
        d->thumbnailProvider->addThumbnailCache(d->thumbnailsCache.get());

    if (mode == Mode::crossSystem)
        return;

    createMessageProcessor<QnMobileClientMessageProcessor>(this);

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
    connect(userWatcher(), &core::UserWatcher::userChanged,
        d->availableCamerasWatcher.get(), &QnAvailableCamerasWatcher::setUser);

    d->resourceDiscoveryManager = std::make_unique<QnResourceDiscoveryManager>(this);
    d->resourceProcessor = std::make_unique<core::ResourceProcessor>(this);
    d->resourceProcessor->moveToThread(d->resourceDiscoveryManager.get());
    d->resourceDiscoveryManager->setResourceProcessor(d->resourceProcessor.get());
    d->resourceDiscoveryManager->setReady(true);
    d->resourceDiscoveryManager->start();

    d->serverAudioConnectionWatcher =
        std::make_unique<nx::client::mobile::ServerAudioConnectionWatcher>(
            windowContext()->sessionManager(), this);

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

    connect(sessionManager(), &SessionManager::connectedServerVersionChanged, this,
        [this]()
        {
            static const nx::utils::SoftwareVersion kUserRightsRefactoredVersion(3, 0);

            const bool compatibilityMode =
                sessionManager()->connectedServerVersion() < kUserRightsRefactoredVersion;
            availableCamerasWatcher()->setCompatiblityMode(compatibilityMode);
        });

    const auto watcher = userWatcher();
    connect(watcher, &core::UserWatcher::userNameChanged, this,
        [this, watcher]()
        {
            if (watcher->userName().isEmpty())
            {
                NX_DEBUG(this, "User name changed to empty, stopping session");
                if (sessionManager()->hasSession())
                    sessionManager()->stopSessionByUser();
            }
        });

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

    initializeConnectionUserInteractionDelegate(this);
}

SystemContext::~SystemContext()
{
    if (d->thumbnailProvider)
        d->thumbnailProvider->removeThumbnailCache(d->thumbnailsCache.get());

    if (mode() == Mode::client)
        sessionManager()->resetSession();

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
