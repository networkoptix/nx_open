// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mobile_client_module.h"

#include <QtGui/QDesktopServices>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlEngine>

#include <api/runtime_info_manager.h>
#include <camera/camera_bookmarks_manager.h>
#include <client_core/client_core_module.h>
#include <common/static_common_module.h>
#include <context/context.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/mobile_client_resource_factory.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>
#include <network/router.h>
#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/client/mobile/software_trigger/event_rules_watcher.h>
#include <nx/client/mobile/two_way_audio/server_audio_connection_watcher.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/cloud/tunnel/connector_factory.h>
#include <nx/network/cloud/tunnel/outgoing_tunnel_pool.h>
#include <nx/network/maintenance/log/collector_api_paths.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/i18n/translation_manager.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/helpers.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection_factory.h>
#include <nx/vms/client/core/network/remote_session_timeout_watcher.h>
#include <nx/vms/client/core/network/server_certificate_validation_level.h>
#include <nx/vms/client/core/ptz/client_ptz_controller_pool.h>
#include <nx/vms/client/core/resource/resource_processor.h>
#include <nx/vms/client/core/resource/screen_recording/audio_only/desktop_audio_only_resource_searcher_impl.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_resource.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_resource_searcher.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/settings/systems_visibility_manager.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/system_finder/system_finder.h>
#include <nx/vms/client/core/system_logon/remote_connection_user_interaction_delegate.h>
#include <nx/vms/client/core/watchers/known_server_connections.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/mobile/controllers/web_view_controller.h>
#include <nx/vms/client/mobile/maintenance/remote_log_manager.h>
#include <nx/vms/client/mobile/push_notification/push_notification_manager.h>
#include <nx/vms/client/mobile/session/session_manager.h>
#include <nx/vms/client/mobile/system_context.h>
#include <nx/vms/client/mobile/ui/qml_wrapper_helper.h>
#include <nx/vms/common/network/server_compatibility_validator.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/discovery/manager.h>
#include <nx/vms/time/formatter.h>
#include <ui/window_utils.h>
#include <watchers/available_cameras_watcher.h>

#include "mobile_client_message_processor.h"
#include "mobile_client_meta_types.h"
#include "mobile_client_settings.h"
#include "mobile_client_startup_parameters.h"

using namespace nx::vms::client;
using namespace nx::vms::client::mobile;

static const QString kQmlRoot = "qrc:///qml";

template<typename BaseType>
BaseType extract(const QSharedPointer<BaseType>& pointer)
{
    return pointer ? *pointer : BaseType();
}

struct QnMobileClientModule::Private
{
    std::unique_ptr<QnResourceDiscoveryManager> resourceDiscoveryManager;
    std::unique_ptr<WebViewController> webViewController;
    std::unique_ptr<core::SystemsVisibilityManager> systemsVisibilityManager;
    std::unique_ptr<QnMobileClientResourceFactory> resourceFactory;
    std::unique_ptr<QnAvailableCamerasWatcher> availableCamerasWatcher;
    std::unique_ptr<core::ResourceProcessor> resourceProcessor;
    std::unique_ptr<nx::client::mobile::EventRulesWatcher> eventRulesWatcher;
    std::unique_ptr<nx::client::mobile::ServerAudioConnectionWatcher> serverAudioConnectionWatcher;
    std::unique_ptr<core::DesktopResourceSearcher> desktopResourceSearcher;
};

template<> QnMobileClientModule* Singleton<QnMobileClientModule>::s_instance = nullptr;

QnMobileClientModule::QnMobileClientModule(
    SystemContext* context,
    const QnMobileClientStartupParameters& startupParameters,
    QObject* parent)
    :
    QObject(parent),
    base_type(context),
    d(new Private())
{
    Q_INIT_RESOURCE(mobile_client);

    nx::log::LevelSettings logUploadFilter;
    logUploadFilter.primary = nx::log::Level::verbose;
    logUploadFilter.filters.emplace(std::string("nx"), nx::log::Level::verbose);
    logUploadFilter.filters.emplace(std::string("Qn"), nx::log::Level::verbose);

    m_logUploader = std::make_unique<nx::network::maintenance::log::UploaderManager>(
        nx::network::url::Builder().setScheme(nx::network::http::kUrlSchemeName)
            .setHost(nx::branding::cloudHost()).setPath(nx::network::maintenance::log::kLogCollectorPathPrefix),
        logUploadFilter);

    m_remoteLogManager = std::make_unique<RemoteLogManager>();

    QnMobileClientMetaTypes::initialize();

    /* Init singletons. */

    const auto currentContext = systemContext();
    currentContext->startModuleDiscovery(core::appContext()->moduleDiscoveryManager());

    m_clientCoreModule.reset(new QnClientCoreModule(currentContext));

    m_pushManager.reset(new PushNotificationManager());

    if (!qnSettings->enableHolePunching())
    {
        // Disable UDP hole punching.
        nx::network::SocketGlobals::instance().cloud().applyArguments(
            nx::ArgumentParser({"--cloud-connect-disable-udp"}));
    }
    else
    {
        NX_DEBUG(this, "Hole punching is enabled");
    }

    m_clientCoreModule->initializeNetworking(
        nx::vms::api::PeerType::mobileClient,
        Qn::SerializationFormat::json);

    d->webViewController = std::make_unique<WebViewController>();
    d->systemsVisibilityManager = std::make_unique<core::SystemsVisibilityManager>();

    nx::network::SocketGlobals::cloud().outgoingTunnelPool().assignOwnPeerId("mc",
        currentContext->peerId());

    d->resourceFactory = std::make_unique<QnMobileClientResourceFactory>();

    nx::vms::api::RuntimeData runtimeData;
    runtimeData.peer.id = currentContext->peerId();
    runtimeData.peer.peerType = nx::vms::api::PeerType::mobileClient;
    runtimeData.peer.dataFormat = Qn::SerializationFormat::json;
    runtimeData.brand = nx::branding::brand();
    runtimeData.customization = nx::branding::customization();
    currentContext->runtimeInfoManager()->updateLocalItem(
        [&runtimeData](auto* data)
        {
            *data = QnPeerRuntimeInfo(runtimeData);
            return true;
        });

    d->availableCamerasWatcher = std::make_unique<QnAvailableCamerasWatcher>(currentContext);
    connect(currentContext->userWatcher(), &core::UserWatcher::userChanged,
        d->availableCamerasWatcher.get(), &QnAvailableCamerasWatcher::setUser);

    core::appContext()->moduleDiscoveryManager()->start(currentContext->resourcePool());

    connect(qApp, &QGuiApplication::applicationStateChanged, this,
        [currentContext, moduleManager = core::appContext()->moduleDiscoveryManager()]
            (Qt::ApplicationState state)
        {
            switch (state)
            {
                case Qt::ApplicationActive:
                    moduleManager->start(currentContext->resourcePool());
                    break;
                case Qt::ApplicationSuspended:
                    moduleManager->stop();
                    break;
                default:
                    break;
            }
        });

    d->resourceDiscoveryManager = std::make_unique<QnResourceDiscoveryManager>(currentContext);
    d->resourceProcessor = std::make_unique<core::ResourceProcessor>(currentContext);
    d->resourceProcessor->moveToThread(d->resourceDiscoveryManager.get());
    d->resourceDiscoveryManager->setResourceProcessor(d->resourceProcessor.get());
    core::appContext()->knownServerConnectionsWatcher()->start();

    d->resourceDiscoveryManager->setReady(true);

    auto qmlRoot = startupParameters.qmlRoot.isEmpty() ? kQmlRoot : startupParameters.qmlRoot;
    if (!qmlRoot.endsWith('/'))
        qmlRoot.append('/');
    NX_INFO(this, "Setting QML root to %1", qmlRoot);

    auto qmlEngine = core::appContext()->qmlEngine();
    qmlEngine->setBaseUrl(
        qmlRoot.startsWith("qrc:") ? QUrl(qmlRoot) : QUrl::fromLocalFile(qmlRoot));
    qmlEngine->addImportPath(qmlRoot);

    if (nx::build_info::isIos())
        qmlEngine->addImportPath("qt_qml");

    for (const QString& path: startupParameters.qmlImportPaths)
        qmlEngine->addImportPath(path);

    if (nx::build_info::isAndroid())
    {
        // We have to use android-specific code to check if we use 24-hours time format.
        nx::vms::time::Formatter::forceSystemTimeFormat(is24HoursTimeFormat());
    }
    m_context.reset(new QnContext(currentContext));
    d->eventRulesWatcher = std::make_unique<nx::client::mobile::EventRulesWatcher>();

    const auto handleConnected =
        [this]() { d->eventRulesWatcher->handleConnectionChanged(); };

    using mobile::SessionManager;
    const auto sessionManager = m_context->sessionManager();
    connect(sessionManager, &SessionManager::sessionStartedSuccessfully,
        this, handleConnected);
    connect(sessionManager, &SessionManager::sessionRestored,
        this, handleConnected);

    d->serverAudioConnectionWatcher =
        std::make_unique<nx::client::mobile::ServerAudioConnectionWatcher>(
            sessionManager,
            currentContext);

    initializePushNotifications();
    initializeSessionTimeoutWatcher();
    initializeConnectionUserInteractionDelegate();
}

QnMobileClientModule::~QnMobileClientModule()
{
    if (auto longRunnablePool = QnLongRunnablePool::instance())
        longRunnablePool->stopAll();

    const auto currentContext = systemContext();
    currentContext->sessionManager()->setSuspended(true);
    currentContext->setSession({});
    currentContext->deleteMessageProcessor();

    qApp->disconnect(this);
}

void QnMobileClientModule::initializeSessionTimeoutWatcher()
{
    const QPointer<SessionManager> sessionManager = m_context->sessionManager();

    const auto interruptionHandler = nx::utils::guarded(this,
        [this, sessionManager](core::RemoteSessionTimeoutWatcher::SessionExpirationReason)
        {
            if (!sessionManager)
                return;

            sessionManager->stopSession();
            emit m_context->showMessage(
                tr("Your session has expired"),
                tr("Session duration limit can be changed by the site administrators"));
        });

    auto sessionTimeoutWatcher = qnClientCoreModule->networkModule()->sessionTimeoutWatcher();
    connect(sessionTimeoutWatcher,
        &core::RemoteSessionTimeoutWatcher::sessionExpired,
        this,
        interruptionHandler);
}

QnContext* QnMobileClientModule::context() const
{
    return m_context.data();
}

void QnMobileClientModule::initDesktopCamera()
{
    // Initialize desktop camera searcher.
    d->desktopResourceSearcher = std::make_unique<core::DesktopResourceSearcher>(
        new core::DesktopAudioOnlyResourceSearcherImpl(),
        systemContext());
    d->desktopResourceSearcher->setLocal(true);
    d->resourceDiscoveryManager->addDeviceSearcher(d->desktopResourceSearcher.get());
}

void QnMobileClientModule::startLocalSearches()
{
    d->resourceDiscoveryManager->start();
}

PushNotificationManager* QnMobileClientModule::pushManager()
{
    return m_pushManager.data();
}

QQuickWindow* QnMobileClientModule::mainWindow() const
{
    return m_mainWindow;
}

void QnMobileClientModule::setMainWindow(QQuickWindow* window)
{
    m_mainWindow = window;
}

nx::network::maintenance::log::UploaderManager* QnMobileClientModule::remoteLogsUploader()
{
    return m_logUploader.get();
}

QnAvailableCamerasWatcher* QnMobileClientModule::availableCamerasWatcher() const
{
    return d->availableCamerasWatcher.get();
}

WebViewController* QnMobileClientModule::webViewController() const
{
    return d->webViewController.get();
}

QnResourceDiscoveryManager* QnMobileClientModule::resourceDiscoveryManager() const
{
    return d->resourceDiscoveryManager.get();
}

void QnMobileClientModule::initializePushNotifications()
{
    using nx::vms::client::core::CloudStatusWatcher;

    const auto cloudWatcher = core::appContext()->cloudStatusWatcher();
    const auto updateFirebaseCredentials = nx::utils::guarded(this,
        [this, cloudWatcher]()
        {
            m_pushManager->setCredentials(cloudWatcher->status() == CloudStatusWatcher::LoggedOut
                ? nx::network::http::Credentials()
                : cloudWatcher->credentials());
        });

    updateFirebaseCredentials();
    connect(cloudWatcher, &CloudStatusWatcher::credentialsChanged,
        this, updateFirebaseCredentials);
    connect(cloudWatcher, &CloudStatusWatcher::statusChanged, this,
        [updateFirebaseCredentials](CloudStatusWatcher::Status) { updateFirebaseCredentials(); });

    const auto updateSourceSystemsForPushNotifications = nx::utils::guarded(this,
        [this, cloudWatcher]()
        {
            QStringList systems;
            for (const auto& system: cloudWatcher->recentCloudSystems())
                 systems.append(system.cloudId);
            m_pushManager->setSystems(systems);
        });
    updateSourceSystemsForPushNotifications();
    connect(cloudWatcher, &CloudStatusWatcher::recentCloudSystemsChanged,
        this, updateSourceSystemsForPushNotifications);
    connect(m_pushManager.data(), &PushNotificationManager::showPushErrorMessage,
        m_context.data(), &QnContext::showMessage);
}

void QnMobileClientModule::initializeConnectionUserInteractionDelegate()
{
    using namespace nx::vms::client::core;
    using Delegate = nx::vms::client::core::RemoteConnectionUserInteractionDelegate;

    const auto validateToken =
        [this](const nx::network::http::Credentials& credentials)
        {
            return m_context->show2faValidationScreen(credentials);
        };

    const auto askToAcceptCertificates =
        [](const QList<core::TargetCertificateInfo>& certificatesInfo,
            CertificateWarning::Reason reason)
        {
            if (!NX_ASSERT(!certificatesInfo.isEmpty()))
                return false;

            const auto& info = certificatesInfo.first();

            const auto url = QUrl("Nx/Web/SslCertificateDialog.qml");
            QVariantMap props;
            props["headerText"] = CertificateWarning::header(
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

            return QmlWrapperHelper::showPopup(url, props) == "connect";
        };

    const auto showCertificateError =
        [](const core::TargetCertificateInfo& certificateInfo)
        {
            // Mobile client shows connection error itself.
        };

    auto delegate = std::make_unique<Delegate>(
        validateToken, askToAcceptCertificates, showCertificateError);

    qnClientCoreModule->networkModule()->connectionFactory()->setUserInteractionDelegate(
        std::move(delegate));
}
