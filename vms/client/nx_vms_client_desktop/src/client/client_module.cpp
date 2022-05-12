// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_module.h"

#include <memory>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QResource>
#include <QtGui/QPalette>
#include <QtGui/QSurfaceFormat>
#include <QtQml/QQmlEngine>
#include <QtWebEngine/QtWebEngine>
#include <QtWebEngineWidgets/QWebEngineSettings>
#include <QtWidgets/QApplication>
#include <QtWidgets/QToolTip>

#include <api/network_proxy_factory.h>
#include <api/runtime_info_manager.h>
#include <api/server_rest_connection.h>
#include <camera/camera_bookmarks_manager.h>
#include <camera/camera_data_manager.h>
#include <client_core/client_core_module.h>
#include <client_core/client_core_settings.h>
#include <client/client_autorun_watcher.h>
#include <client/client_meta_types.h>
#include <client/client_resource_processor.h>
#include <client/client_runtime_settings.h>
#include <client/client_settings.h>
#include <client/client_show_once_settings.h>
#include <client/desktop_client_message_processor.h>
#include <client/forgotten_systems_manager.h>
#include <client/system_weights_manager.h>
#include <common/common_module.h>
#include <common/static_common_module.h>
#include <core/dataprovider/data_provider_factory.h>
#include <core/resource_management/incompatible_server_watcher.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_runtime_data.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/client_camera_factory.h>
#include <core/resource/client_camera.h>
#include <core/resource/local_resource_status_watcher.h>
#include <core/resource/resource_directory_browser.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/storage/file_storage/layout_storage_resource.h>
#include <core/storage/file_storage/qtfile_storage_resource.h>
#include <decoders/video/abstract_video_decoder.h>
#include <finders/systems_finder.h>
#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/cloud/vms_gateway/vms_gateway_embeddable.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/cloud/tunnel/outgoing_tunnel_pool.h>
#include <nx/network/http/http_mod_manager.h>
#include <nx/network/socket_global.h>
#include <nx/utils/crash_dump/systemexcept.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/vms/api/protocol_version.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/settings/systems_visibility_manager.h>
#include <nx/vms/client/core/utils/font_loader.h>
#include <nx/vms/client/core/watchers/known_server_connections.h>
#include <nx/vms/client/desktop/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/desktop/analytics/analytics_icon_manager.h>
#include <nx/vms/client/desktop/analytics/analytics_metadata_provider_factory.h>
#include <nx/vms/client/desktop/analytics/analytics_settings_manager_factory.h>
#include <nx/vms/client/desktop/analytics/analytics_settings_manager.h>
#include <nx/vms/client/desktop/analytics/analytics_taxonomy_manager.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/debug_utils/components/debug_info_storage.h>
#include <nx/vms/client/desktop/debug_utils/utils/performance_monitor.h>
#include <nx/vms/client/desktop/director/director.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/integrations/integrations.h>
#include <nx/vms/client/desktop/license/videowall_license_validator.h>
#include <nx/vms/client/desktop/radass/radass_controller.h>
#include <nx/vms/client/desktop/server_runtime_events/server_runtime_event_connector.h>
#include <nx/vms/client/desktop/session_manager/default_process_interface.h>
#include <nx/vms/client/desktop/session_manager/session_manager.h>
#include <nx/vms/client/desktop/settings/ipc_settings_synchronizer.h>
#include <nx/vms/client/desktop/state/client_process_runner.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/state/fallback_shared_memory.h>
#include <nx/vms/client/desktop/state/qt_based_shared_memory.h>
#include <nx/vms/client/desktop/state/running_instances_manager.h>
#include <nx/vms/client/desktop/state/shared_memory_manager.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/style/svg_icon_provider.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_health/license_health_watcher.h>
#include <nx/vms/client/desktop/system_health/system_internet_access_watcher.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/ui/common/custom_cursors.h>
#include <nx/vms/client/desktop/ui/image_providers/resource_icon_provider.h>
#include <nx/vms/client/desktop/ui/right_panel/models/right_panel_models_adapter.h>
#include <nx/vms/client/desktop/utils/applauncher_guard.h>
#include <nx/vms/client/desktop/utils/local_proxy_server.h>
#include <nx/vms/client/desktop/utils/performance_test.h>
#include <nx/vms/client/desktop/utils/resource_widget_pixmap_cache.h>
#include <nx/vms/client/desktop/utils/upload_manager.h>
#include <nx/vms/client/desktop/utils/video_cache.h>
#include <nx/vms/client/desktop/utils/virtual_camera_manager.h>
#include <nx/vms/common/network/server_compatibility_validator.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/discovery/manager.h>
#include <nx/vms/license/usage_helper.h>
#include <nx/vms/time/formatter.h>
#include <nx/vms/utils/external_resources.h>
#include <nx/vms/utils/translation/translation_manager.h>
#include <platform/platform_abstraction.h>
#include <plugins/resource/desktop_audio_only/desktop_audio_only_resource.h>
#include <plugins/resource/desktop_camera/desktop_resource_searcher.h>
#include <server/server_storage_manager.h>
#include <statistics/statistics_manager.h>
#include <statistics/storage/statistics_file_storage.h>
#include <ui/workaround/qtbug_workaround.h>
#include <utils/common/command_line_parser.h>
#include <utils/math/color_transformations.h>
#include <utils/media/voice_spectrum_analyzer.h>
#include <watchers/cloud_status_watcher.h>
#include <watchers/server_interface_watcher.h>

#if defined(Q_OS_WIN)
    #include <ui/workaround/iexplore_url_handler.h>
    #include <plugins/resource/desktop_win/desktop_resource.h>
    #include <plugins/resource/desktop_win/desktop_resource_searcher_impl.h>
#else
    #include <plugins/resource/desktop_audio_only/desktop_audio_only_resource_searcher_impl.h>
#endif
#if defined(Q_OS_MAC)
    #include <ui/workaround/mac_utils.h>
#endif

using namespace nx;
using namespace nx::vms::client::desktop;

static const QString kQmlRoot = QStringLiteral("qrc:///qml");

namespace {

void initExternalResources()
{
    using namespace nx::vms::utils;

    nx::vms::client::core::FontLoader::loadFonts(
        externalResourcesDirectory().absoluteFilePath("fonts"));

    using nx::vms::client::desktop::analytics::IconManager;

    registerExternalResource("client_external.dat");
    registerExternalResource("bytedance_iconpark.dat",
        IconManager::librariesRoot() + "bytedance.iconpark/");
}

void initializeStatisticsManager(QnCommonModule* commonModule)
{
    const auto statManager = commonModule->instance<QnStatisticsManager>();

    statManager->setClientId(qnSettings->pcUuid());
    statManager->setStorage(QnStatisticsStoragePtr(new QnStatisticsFileStorage()));
}

QString calculateLogNameSuffix(
    SharedMemoryManager* sharedMemoryManager,
    const QnStartupParameters& startupParams)
{
    if (!startupParams.videoWallGuid.isNull())
    {
        QString result = startupParams.videoWallItemGuid.isNull()
            ? startupParams.videoWallGuid.toString()
            : startupParams.videoWallItemGuid.toString();
        result.replace(QRegularExpression(QLatin1String("[{}]")), QLatin1String("_"));
        return result;
    }

    auto idxSuffix = QString::fromStdString("_" + nx::utils::generateRandomName(4));
    if (sharedMemoryManager)
    {
        const int idx = sharedMemoryManager->currentInstanceIndex();
        if (idx == 0)
            idxSuffix = QString();
        else if (idx > 0)
            idxSuffix = "_" + QString::number(idx);
    }

    if (startupParams.selfUpdateMode)
        return "self_update" + idxSuffix;

    if (startupParams.acsMode)
        return "ax" + idxSuffix;

    return idxSuffix;
}

bool isAutoCloudHostDeductionMode()
{
    return std::string(ini().cloudHost) == "auto";
}

void initializeServerCompatibilityValidator()
{
    using namespace nx::vms::common;
    using Protocol = ServerCompatibilityValidator::Protocol;

    ServerCompatibilityValidator::DeveloperFlags developerFlags;
    if (ini().developerMode)
        developerFlags.setFlag(ServerCompatibilityValidator::DeveloperFlag::ignoreCustomization);

    if (isAutoCloudHostDeductionMode())
        developerFlags.setFlag(ServerCompatibilityValidator::DeveloperFlag::ignoreCloudHost);

    ServerCompatibilityValidator::initialize(
        ServerCompatibilityValidator::Peer::desktopClient,
        ini().forceJsonConnection ? Protocol::json : Protocol::ubjson,
        developerFlags);
}

Qn::SerializationFormat serializationFormat()
{
    return ini().forceJsonConnection ? Qn::JsonFormat : Qn::UbjsonFormat;
}

QPalette makeApplicationPalette()
{
    QPalette result(QApplication::palette());
    result.setColor(QPalette::WindowText, colorTheme()->color("light16"));
    result.setColor(QPalette::Button, colorTheme()->color("dark11"));
    result.setColor(QPalette::Light, colorTheme()->color("light10"));
    result.setColor(QPalette::Midlight, colorTheme()->color("dark13"));
    result.setColor(QPalette::Dark, colorTheme()->color("dark9"));
    result.setColor(QPalette::Mid, colorTheme()->color("dark10"));
    result.setColor(QPalette::Text, colorTheme()->color("light4"));
    result.setColor(QPalette::BrightText, colorTheme()->color("light1"));
    result.setColor(QPalette::ButtonText, colorTheme()->color("light4"));
    result.setColor(QPalette::Base, colorTheme()->color("dark7"));
    result.setColor(QPalette::Window, colorTheme()->color("dark7"));
    result.setColor(QPalette::Shadow, colorTheme()->color("dark5"));
    result.setColor(QPalette::Highlight, colorTheme()->color("brand_core"));
    result.setColor(QPalette::HighlightedText, colorTheme()->color("brand_contrast"));
    result.setColor(QPalette::Link, colorTheme()->color("brand_d2"));
    result.setColor(QPalette::LinkVisited, colorTheme()->color("brand_core"));
    result.setColor(QPalette::AlternateBase, colorTheme()->color("dark7"));
    result.setColor(QPalette::ToolTipBase, colorTheme()->color("light4"));
    result.setColor(QPalette::ToolTipText, colorTheme()->color("dark4"));
    result.setColor(QPalette::PlaceholderText, colorTheme()->color("light16"));

    static const auto kDisabledAlpha = 77;
    static const QList<QPalette::ColorRole> kDimmerRoles{{
        QPalette::WindowText, QPalette::Button, QPalette::Light,
        QPalette::Midlight, QPalette::Dark, QPalette::Mid,
        QPalette::Text, QPalette::BrightText, QPalette::ButtonText,
        QPalette::Base, QPalette::Shadow, QPalette::HighlightedText,
        QPalette::Link, QPalette::LinkVisited, QPalette::AlternateBase}};

    for (const QPalette::ColorRole role: kDimmerRoles)
        result.setColor(QPalette::Disabled, role, withAlpha(result.color(role), kDisabledAlpha));

    return result;
}

} // namespace

template<> QnClientModule* Singleton<QnClientModule>::s_instance = nullptr;

struct QnClientModule::Private
{
    Private(const QnStartupParameters& startupParameters):
        startupParameters(startupParameters)
    {
    }

    void initSettings()
    {
        NX_ASSERT(!QApplication::applicationName().isEmpty(), "Application must be initialized");
        clientSettings = std::make_unique<QnClientSettings>(startupParameters);
    }

    void initializeTranslations()
    {
        translationManager = std::make_unique<nx::vms::utils::TranslationManager>();
        bool loaded = translationManager->installTranslation(clientSettings->locale());
        if (!loaded)
            loaded = translationManager->installTranslation(nx::branding::defaultLocale());
        NX_ASSERT(loaded, "Translations could not be initialized");
    }

    void initStateModule()
    {
        const QString dataLocation = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        const QString storageDirectory = QDir(dataLocation).absoluteFilePath("state");
        clientStateHandler = std::make_unique<ClientStateHandler>(storageDirectory);

        clientStateHandler->setClientProcessExecutionInterface(
            std::make_unique<ClientProcessRunner>());

        SharedMemoryInterfacePtr sharedMemoryInterface = std::make_unique<QtBasedSharedMemory>();
        if (!sharedMemoryInterface->initialize())
            sharedMemoryInterface = std::make_unique<FallbackSharedMemory>();

        sharedMemoryManager = std::make_unique<SharedMemoryManager>(
            std::move(sharedMemoryInterface),
            std::make_unique<ClientProcessRunner>());

        clientStateHandler->setSharedMemoryManager(sharedMemoryManager.get());

        runningInstancesManager = std::make_unique<RunningInstancesManager>(
            clientSettings.get(),
            sharedMemoryManager.get());
    }

    void initLicensesModule()
    {
        using namespace nx::vms::license;

        videoWallLicenseUsageHelper =
            std::make_unique<VideoWallLicenseUsageHelper>(systemContext.get());
        videoWallLicenseUsageHelper->setCustomValidator(
            std::make_unique<license::VideoWallLicenseValidator>(systemContext.get()));
    }

    const QnStartupParameters startupParameters;
    std::unique_ptr<SystemContext> systemContext;
    std::unique_ptr<ClientStateHandler> clientStateHandler;
    std::unique_ptr<RunningInstancesManager> runningInstancesManager;
    std::unique_ptr<SharedMemoryManager> sharedMemoryManager;
    std::unique_ptr<QnClientSettings> clientSettings;
    std::unique_ptr<QnClientShowOnceSettings> clientShowOnceSettings;
    std::unique_ptr<AnalyticsSettingsManager> analyticsSettingsManager;
    std::unique_ptr<nx::vms::client::desktop::analytics::AttributeHelper> analyticsAttributeHelper;
    std::unique_ptr<nx::vms::utils::TranslationManager> translationManager;
    std::unique_ptr<nx::vms::license::VideoWallLicenseUsageHelper> videoWallLicenseUsageHelper;
    std::unique_ptr<DebugInfoStorage> debugInfoStorage;
};

QnClientModule::QnClientModule(const QnStartupParameters& startupParameters, QObject* parent):
    QObject(parent),
    d(new Private(startupParameters))
{
#if defined(Q_OS_WIN)
    // Enable full crash dumps if needed. Do not disable here as it can be enabled elsewhere.
    if (ini().profilerMode)
        win32_exception::setCreateFullCrashDump(true);
#endif

    initThread();
    initMetaInfo();
    initApplication();
    initExternalResources();

    // Depends on resources.
    d->initSettings();

    if (!d->startupParameters.selfUpdateMode)
        d->initStateModule();

    // Log initialization depends on shared memory manager.
    initLog();

    initSingletons();

    // Do not initialize anything else because we must exit immediately if run in self-update mode.
    if (d->startupParameters.selfUpdateMode)
        return;

    d->initLicensesModule();
    initNetwork();

    // Initialize application UI.
    initSkin();

    initLocalResources();
    initSurfaceFormat();
}

QnClientModule::~QnClientModule()
{
    // Discovery manager may not exist in self-update mode.
    if (auto discoveryManager = m_clientCoreModule->commonModule()->resourceDiscoveryManager())
        discoveryManager->stop();

    // Stop all long runnables before deinitializing singletons. Pool may not exist in update mode.
    if (auto longRunnablePool = QnLongRunnablePool::instance())
        longRunnablePool->stopAll();

    if (m_resourceDirectoryBrowser)
        m_resourceDirectoryBrowser->stop();

    d->systemContext->resourcePool()->threadPool()->waitForDone();

    if (d->systemContext->messageProcessor())
        d->systemContext->deleteMessageProcessor();

    // Restoring default message handler.
    nx::utils::disableQtMessageAsserts();

    // First delete clientCore module and commonModule()
    m_clientCoreModule.reset();

    // Then delete static common.
    m_staticCommon.reset();
}

void QnClientModule::initThread()
{
    QThread::currentThread()->setPriority(QThread::HighestPriority);
}

void QnClientModule::initApplication()
{
    /* Set up application parameters so that QSettings know where to look for settings. */
    QApplication::setOrganizationName(nx::branding::company());
    QApplication::setApplicationName(nx::branding::desktopClientInternalName());
    QApplication::setApplicationDisplayName(nx::branding::desktopClientDisplayName());

    const QString applicationVersion = nx::build_info::usedMetaVersion().isEmpty()
        ? nx::build_info::vmsVersion()
        : QString("%1 %2").arg(nx::build_info::vmsVersion(), nx::build_info::usedMetaVersion());

    QApplication::setApplicationVersion(applicationVersion);
    QApplication::setStartDragDistance(20);

    /* We don't want changes in desktop color settings to clash with our custom style. */
    QApplication::setDesktopSettingsAware(false);
    QApplication::setQuitOnLastWindowClosed(true);
}

void QnClientModule::initDesktopCamera([[maybe_unused]] QOpenGLWidget* window)
{
    /* Initialize desktop camera searcher. */
    auto commonModule = m_clientCoreModule->commonModule();
#if defined(Q_OS_WIN)
    auto impl = new QnDesktopResourceSearcherImpl(window);
#else
    auto impl = new QnDesktopAudioOnlyResourceSearcherImpl();
#endif
    auto desktopSearcher = commonModule->store(new QnDesktopResourceSearcher(impl));
    desktopSearcher->setLocal(true);
    commonModule->resourceDiscoveryManager()->addDeviceSearcher(desktopSearcher);
}

void QnClientModule::startLocalSearchers()
{
    auto commonModule = m_clientCoreModule->commonModule();
    commonModule->resourceDiscoveryManager()->start();
}

void QnClientModule::initMetaInfo()
{
    Q_INIT_RESOURCE(nx_vms_client_desktop);
    QnClientMetaTypes::initialize();
}

void QnClientModule::initSurfaceFormat()
{
    // Warning: do not set version or profile here.

    auto format = QSurfaceFormat::defaultFormat();

    if (qnRuntime->lightMode().testFlag(Qn::LightModeNoMultisampling))
        format.setSamples(2);
    format.setSwapBehavior(qnSettings->isGlDoubleBuffer()
        ? QSurfaceFormat::DoubleBuffer
        : QSurfaceFormat::SingleBuffer);
    format.setSwapInterval(ini().limitFrameRate ? 1 : 0);

    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);

    QSurfaceFormat::setDefaultFormat(format);
}

void QnClientModule::initSingletons()
{
#if defined(Q_OS_WIN)
    // Enable full crash dumps if needed. Do not disable here as it can be enabled elsewhere.
    if (d->clientSettings->createFullCrashDump())
        win32_exception::setCreateFullCrashDump(true);
#endif

    initializeServerCompatibilityValidator();

    // Runtime settings are standalone, but some actual values are taken from persistent settings.
    auto clientRuntimeSettings = std::make_unique<QnClientRuntimeSettings>(d->startupParameters);
    clientRuntimeSettings->setGLDoubleBuffer(d->clientSettings->isGlDoubleBuffer());
    clientRuntimeSettings->setMaximumLiveBufferMs(d->clientSettings->maximumLiveBufferMs());
    clientRuntimeSettings->setLocale(d->clientSettings->locale());

    // Initializing SessionManager.

    m_processInterface.reset(new session::DefaultProcessInterface());
    using SessionManager = session::SessionManager;
    session::Config sessionConfig;
    //sessionConfig.rootGuid = pcUuid;
    sessionConfig.sharedPrefix = nx::branding::customization() + "/SessionManager";
    const QString appDataLocation = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    sessionConfig.storagePath = QDir(appDataLocation).absoluteFilePath("sessions");
    m_sessionManager.reset(new SessionManager(sessionConfig, m_processInterface.data()));
    //m_sessionManager->setRestoreUserSessionData(clientSettings->restoreUserSessionData());

    const auto clientPeerType = d->startupParameters.videoWallGuid.isNull()
        ? nx::vms::api::PeerType::desktopClient
        : nx::vms::api::PeerType::videowallClient;

    // This must be done before QnCommonModule instantiation.
    nx::utils::OsInfo::override(
        ini().currentOsVariantOverride, ini().currentOsVariantVersionOverride);

    if (d->startupParameters.vmsProtocolVersion > 0)
    {
        vms::api::protocolVersionOverride = d->startupParameters.vmsProtocolVersion;
        NX_WARNING(this, "Starting with overridden protocol version: %1",
            d->startupParameters.vmsProtocolVersion);
    }

    // Do not override cloudhost if automatic switching is enabled.
    m_staticCommon.reset(new QnStaticCommonModule(
        clientPeerType,
        isAutoCloudHostDeductionMode() ? QString() : ini().cloudHost));

    QnUuid peerId = d->runningInstancesManager
        ? d->runningInstancesManager->currentInstanceGuid()
        : QnUuid();

    d->systemContext = std::make_unique<SystemContext>(peerId);
    ApplicationContext::instance()->addSystemContext(d->systemContext.get());

    m_clientCoreModule.reset(new QnClientCoreModule(
        QnClientCoreModule::Mode::desktopClient,
        d->systemContext.get()));

    // In self-update mode we do not have runningInstancesManager.
    if (!d->startupParameters.selfUpdateMode)
    {
        // Message processor should exist before networking is initialized.
        d->systemContext->createMessageProcessor<QnDesktopClientMessageProcessor>(this);

        // Must be called when peerId is set.
        nx::network::SocketGlobals::cloud().outgoingTunnelPool().assignOwnPeerId(
            "dc", d->systemContext->peerId());

        m_clientCoreModule->initializeNetworking(
            clientPeerType,
            serializationFormat(),
            nx::vms::client::core::settings()->certificateValidationLevel());
    }

    const auto qmlEngine = m_clientCoreModule->mainQmlEngine();
    qmlEngine->addImageProvider("resource", new ResourceIconProvider());
    qmlEngine->addImageProvider("right_panel", new RightPanelImageProvider());

    // Settings migration depends on client core settings.
    d->clientSettings->migrate();

    // We should load translations before major client's services are started to prevent races.
    d->initializeTranslations();

    // Pass ownership to the common module.
    auto commonModule = m_clientCoreModule->commonModule();
    commonModule->store(clientRuntimeSettings.release());

    initRuntimeParams(d->startupParameters);

    // Used in self-update to modify desktop shortcuts.
    commonModule->store(new QnPlatformAbstraction());

    // Shortened initialization if run in self-update mode.
    if (d->startupParameters.selfUpdateMode)
        return;

    commonModule->store(new ApplauncherGuard(commonModule));

    d->clientShowOnceSettings = std::make_unique<QnClientShowOnceSettings>();
    IpcSettingsSynchronizer::setup(
        d->clientSettings.get(),
        d->clientShowOnceSettings.get(),
        d->sharedMemoryManager.get());

    // Depends on QnClientSettings, is never used directly.
    commonModule->store(new QnClientAutoRunWatcher());

    m_radassController = commonModule->store(new RadassController());


    commonModule->store(new QnIncompatibleServerWatcher());
    commonModule->store(new QnClientResourceFactory());

    commonModule->store(new ServerRuntimeEventConnector(d->systemContext->messageProcessor()));

    commonModule->store(new QnCameraBookmarksManager());

    // Depends on ServerRuntimeEventConnector.
    commonModule->store(new QnServerStorageManager());

    commonModule->instance<QnLayoutTourManager>();

    commonModule->store(new QnVoiceSpectrumAnalyzer());

    commonModule->store(new ResourceWidgetPixmapCache());


    commonModule->store(new QnResourceRuntimeDataManager(commonModule));
    initLocalInfo(clientPeerType);

    initializeStatisticsManager(commonModule);
    m_uploadManager = new UploadManager(commonModule);
    m_virtualCameraManager = new VirtualCameraManager(commonModule);

    m_videoCache = new VideoCache(commonModule);

    commonModule->store(m_uploadManager);
    commonModule->store(m_virtualCameraManager);

#ifdef Q_OS_WIN
    commonModule->store(new QnIexploreUrlHandler());
#endif

    commonModule->store(new QnQtbugWorkaround());

    commonModule->store(new nx::cloud::gateway::VmsGatewayEmbeddable(true));
    commonModule->store(new LocalProxyServer());

    m_cameraDataManager = commonModule->store(new QnCameraDataManager(commonModule));

    commonModule->store(new SystemInternetAccessWatcher(commonModule));
    commonModule->findInstance<nx::vms::client::core::watchers::KnownServerConnections>()->start();

    d->analyticsSettingsManager = AnalyticsSettingsManagerFactory::createAnalyticsSettingsManager(
        d->systemContext->resourcePool(),
        d->systemContext->messageProcessor());

    d->analyticsAttributeHelper = std::make_unique<
        nx::vms::client::desktop::analytics::AttributeHelper>(
            d->systemContext->analyticsTaxonomyStateWatcher());

    m_analyticsMetadataProviderFactory.reset(new AnalyticsMetadataProviderFactory());
    m_analyticsMetadataProviderFactory->registerMetadataProviders();

    registerResourceDataProviders();
    integrations::initialize(this);

    rest::ServerConnection::setDebugFlag(rest::ServerConnection::DebugFlag::disableThumbnailRequests,
        ini().debugDisableCameraThumbnails);

    m_performanceMonitor.reset(new PerformanceMonitor());
    m_licenseHealthWatcher.reset(new LicenseHealthWatcher(d->systemContext->licensePool()));

    d->debugInfoStorage = std::make_unique<DebugInfoStorage>();
}

void QnClientModule::initRuntimeParams([[maybe_unused]] const QnStartupParameters& startupParams)
{
    if (!d->startupParameters.engineVersion.isEmpty())
    {
        nx::utils::SoftwareVersion version(d->startupParameters.engineVersion);
        if (!version.isNull())
        {
            qWarning() << "Starting with overridden version: " << version.toString();
            m_clientCoreModule->commonModule()->setEngineVersion(version);
        }
    }

    if (qnRuntime->lightModeOverride() != -1)
        PerformanceTest::detectLightMode();

#ifdef Q_OS_MACX
    if (mac_isSandboxed())
        qnRuntime->setLightMode(qnRuntime->lightMode() | Qn::LightModeNoNewWindow);
#endif

    auto qmlRoot = d->startupParameters.qmlRoot.isEmpty() ? kQmlRoot : d->startupParameters.qmlRoot;
    if (!qmlRoot.endsWith('/'))
        qmlRoot.append('/');
    NX_INFO(this, nx::format("Setting QML root to %1").arg(qmlRoot));

    m_clientCoreModule->mainQmlEngine()->setBaseUrl(
        qmlRoot.startsWith("qrc:")
            ? QUrl(qmlRoot)
            : QUrl::fromLocalFile(qmlRoot));
    m_clientCoreModule->mainQmlEngine()->addImportPath(qmlRoot);

    for (const QString& path: d->startupParameters.qmlImportPaths)
        m_clientCoreModule->mainQmlEngine()->addImportPath(path);
}

void QnClientModule::initLog()
{
    nx::utils::enableQtMessageAsserts();

    using namespace nx::utils::log;

    const auto logFileNameSuffix = calculateLogNameSuffix(
        d->sharedMemoryManager.get(),
        d->startupParameters);

    static const QString kLogConfig("desktop_client_log");

    if (initLogFromFile(kLogConfig + logFileNameSuffix))
        return;

    if (initLogFromFile(kLogConfig, logFileNameSuffix))
        return;

    NX_INFO(this, "Log is initialized from the settings");
    QSettings rawSettings;
    const auto maxVolumeSize = rawSettings.value(kMaxLogVolumeSizeSymbolicName, 100 * 1024 * 1024).toUInt();
    const auto maxFileSize = rawSettings.value(kMaxLogFileSizeSymbolicName, 10 * 1024 * 1024).toUInt();

    auto logLevel = d->startupParameters.logLevel;
    auto logFile = d->startupParameters.logFile;

    if (logLevel.isEmpty())
    {
        logLevel = qnSettings->logLevel();
        NX_INFO(this, "Log level is initialized from the settings");
    }
    else
    {
        NX_INFO(this, "Log level is initialized from the command line");
    }

    Settings logSettings;
    logSettings.loggers.resize(1);
    auto& logger = logSettings.loggers.front();
    logger.maxVolumeSizeB = maxVolumeSize;
    logger.maxFileSizeB = maxFileSize;
    logger.level.parse(logLevel);
    logger.logBaseName = logFile.isEmpty()
        ? ("client_log" + logFileNameSuffix)
        : logFile;
    logSettings.updateDirectoryIfEmpty(
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/log");

    setMainLogger(
        buildLogger(
            logSettings,
            nx::branding::desktopClientInternalName(),
            qApp->applicationFilePath()));
}

bool QnClientModule::initLogFromFile(const QString& filename, const QString& suffix)
{
    const QDir iniFilesDir(nx::kit::IniConfig::iniFilesDir());
    const QString logConfigFile(iniFilesDir.absoluteFilePath(filename + ".ini"));

    if (!QFileInfo(logConfigFile).exists())
        return false;

    NX_INFO(this, "Log is initialized from the %1", logConfigFile);
    NX_INFO(this, "Log options from settings are ignored!");

    return nx::utils::log::initializeFromConfigFile(
        logConfigFile,
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/log",
        nx::branding::desktopClientInternalName(),
        qApp->applicationFilePath(),
        suffix);
}

void QnClientModule::initNetwork()
{
    auto commonModule = m_clientCoreModule->commonModule();

    if (!d->startupParameters.enforceSocketType.isEmpty())
    {
        nx::network::SocketFactory::enforceStreamSocketType(
            d->startupParameters.enforceSocketType.toStdString());
    }

    if (!d->startupParameters.enforceMediatorEndpoint.isEmpty())
    {
        nx::network::SocketGlobals::cloud().mediatorConnector().mockupMediatorAddress({
            d->startupParameters.enforceMediatorEndpoint.toStdString(),
            d->startupParameters.enforceMediatorEndpoint.toStdString()});
    }

    if (!d->startupParameters.videoWallGuid.isNull())
        commonModule->setVideowallGuid(d->startupParameters.videoWallGuid);

    commonModule->moduleDiscoveryManager()->start();

    commonModule->instance<QnSystemsFinder>();
    commonModule->store(new QnForgottenSystemsManager());
    commonModule->store(new nx::vms::client::core::SystemsVisibilityManager());

    // Depends on qnSystemsFinder
    commonModule->instance<QnServerInterfaceWatcher>();
}

void QnClientModule::initSkin()
{
    QStringList paths;
    paths << ":/skin";

    QScopedPointer<Skin> skin(new Skin(paths));

    QApplication::setWindowIcon(qnSkin->icon(":/logo.png"));
    QApplication::setStyle(skin->newStyle());

    auto commonModule = m_clientCoreModule->commonModule();
    commonModule->store(skin.take());
    commonModule->store(new ColorTheme());
    commonModule->store(new CustomCursors(Skin::instance()));

    QApplication::setPalette(makeApplicationPalette());
    QToolTip::setPalette(QApplication::palette());

    m_clientCoreModule->mainQmlEngine()->addImageProvider("svg", new SvgIconProvider());
}

void QnClientModule::initWebEngine()
{
    // QtWebEngine uses a dedicated process to handle web pages. That process needs to know from
    // where to load libraries. It's not a problem for release packages since everything is
    // gathered in one place, but it is a problem for development builds. The simplest solution for
    // this is to set library search path variable. In Linux this variable is needed only for
    // QtWebEngine::defaultSettings() call which seems to create a WebEngine process. After the
    // variable could be restored to the original value. In macOS it's needed for every web page
    // constructor, so we just set it for the whole lifetime of Client application.

    const QByteArray libraryPathVariable =
        nx::build_info::isLinux()
            ? "LD_LIBRARY_PATH"
            : nx::build_info::isMacOsX()
                ? "DYLD_LIBRARY_PATH"
                : "";

    const QByteArray originalLibraryPath =
        libraryPathVariable.isEmpty() ? QByteArray() : qgetenv(libraryPathVariable);

    if (!libraryPathVariable.isEmpty())
    {
        QString libPath = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../lib");
        if (!originalLibraryPath.isEmpty())
        {
            libPath += ':';
            libPath += originalLibraryPath;
        }

        qputenv(libraryPathVariable, libPath.toLocal8Bit());
    }

    qputenv("QTWEBENGINE_DIALOG_SET", "QtQuickControls2");

    const auto settings = QWebEngineSettings::defaultSettings();
    // We must support Flash for some camera admin pages to work.
    settings->setAttribute(QWebEngineSettings::PluginsEnabled, true);

    // TODO: Add ini parameters for WebEngine attributes
    //settings->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, true);
    //settings->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);

    if (!nx::build_info::isMacOsX())
    {
        if (!originalLibraryPath.isEmpty())
            qputenv(libraryPathVariable, originalLibraryPath);
        else
            qunsetenv(libraryPathVariable);
    }
}

void QnClientModule::initLocalResources()
{
    auto commonModule = m_clientCoreModule->commonModule();
    // client uses ordinary QT file to access file system

    commonModule->storagePluginFactory()->registerStoragePlugin(QLatin1String("file"), QnQtFileStorageResource::instance, true);
    commonModule->storagePluginFactory()->registerStoragePlugin(QLatin1String("qtfile"), QnQtFileStorageResource::instance);
    commonModule->storagePluginFactory()->registerStoragePlugin(QLatin1String("layout"), QnLayoutFileStorageResource::instance);

    auto resourceProcessor = commonModule->store(new QnClientResourceProcessor());

    auto resourceDiscoveryManager = new QnResourceDiscoveryManager(commonModule);
    commonModule->setResourceDiscoveryManager(resourceDiscoveryManager);
    resourceProcessor->moveToThread(resourceDiscoveryManager);
    resourceDiscoveryManager->setResourceProcessor(resourceProcessor);

    resourceDiscoveryManager->setReady(true);
    commonModule->store(new QnSystemsWeightsManager());
    commonModule->store(new QnLocalResourceStatusWatcher());
    if (!d->startupParameters.skipMediaFolderScan && !d->startupParameters.acsMode)
    {
        m_resourceDirectoryBrowser.reset(new ResourceDirectoryBrowser());
        m_resourceDirectoryBrowser->setLocalResourcesDirectories(qnSettings->mediaFolders());
    }
}

QnCameraDataManager* QnClientModule::cameraDataManager() const
{
    return m_cameraDataManager;
}

QnClientCoreModule* QnClientModule::clientCoreModule() const
{
    return m_clientCoreModule.data();
}

SystemContext* QnClientModule::systemContext() const
{
    return d->systemContext.get();
}

QnClientModule::SessionManager* QnClientModule::sessionManager() const
{
    return m_sessionManager.get();
}

ClientStateHandler* QnClientModule::clientStateHandler() const
{
    return d->clientStateHandler.get();
}

SharedMemoryManager* QnClientModule::sharedMemoryManager() const
{
    return d->sharedMemoryManager.get();
}

RunningInstancesManager* QnClientModule::runningInstancesManager() const
{
    return d->runningInstancesManager.get();
}

RadassController* QnClientModule::radassController() const
{
    return m_radassController;
}

QnStartupParameters QnClientModule::startupParameters() const
{
    return d->startupParameters;
}

UploadManager* QnClientModule::uploadManager() const
{
    return m_uploadManager;
}

VirtualCameraManager* QnClientModule::virtualCameraManager() const
{
    return m_virtualCameraManager;
}

VideoCache* QnClientModule::videoCache() const
{
    return m_videoCache;
}

PerformanceMonitor* QnClientModule::performanceMonitor() const
{
    return m_performanceMonitor.data();
}

nx::vms::license::VideoWallLicenseUsageHelper* QnClientModule::videoWallLicenseUsageHelper() const
{
    return d->videoWallLicenseUsageHelper.get();
}

nx::vms::client::desktop::analytics::TaxonomyManager* QnClientModule::taxonomyManager() const
{
    using TaxonomyManager = nx::vms::client::desktop::analytics::TaxonomyManager;

    return m_clientCoreModule->mainQmlEngine()->singletonInstance<TaxonomyManager*>(
        qmlTypeId("nx.vms.client.desktop.analytics", 1, 0, "TaxonomyManager"));
}

nx::vms::client::desktop::analytics::AttributeHelper*
    QnClientModule::analyticsAttributeHelper() const
{
    return d->analyticsAttributeHelper.get();
}

ResourceDirectoryBrowser* QnClientModule::resourceDirectoryBrowser() const
{
    return m_resourceDirectoryBrowser.data();
}

AnalyticsSettingsManager* QnClientModule::analyticsSettingsManager() const
{
    return d->analyticsSettingsManager.get();
}

ServerRuntimeEventConnector* QnClientModule::serverRuntimeEventConnector() const
{
    return m_clientCoreModule->commonModule()->findInstance<ServerRuntimeEventConnector>();
}

nx::vms::utils::TranslationManager* QnClientModule::translationManager() const
{
    return d->translationManager.get();
}

void QnClientModule::initLocalInfo(nx::vms::api::PeerType peerType)
{
    nx::vms::api::RuntimeData runtimeData;
    runtimeData.peer.id = d->systemContext->peerId();
    runtimeData.peer.instanceId = d->systemContext->sessionId();
    runtimeData.peer.peerType = peerType;
    runtimeData.peer.dataFormat = serializationFormat();
    runtimeData.brand = ini().developerMode ? QString() : nx::branding::brand();
    runtimeData.customization = ini().developerMode ? QString() : nx::branding::customization();
    runtimeData.videoWallInstanceGuid = d->startupParameters.videoWallItemGuid;
    d->systemContext->runtimeInfoManager()->updateLocalItem(runtimeData); // initializing localInfo
}

void QnClientModule::registerResourceDataProviders()
{
    auto factory = qnClientCoreModule->dataProviderFactory();
    factory->registerResourceType<QnAviResource>();
    factory->registerResourceType<QnClientCameraResource>();
    #if defined(Q_OS_WIN)
        factory->registerResourceType<QnWinDesktopResource>();
    #endif
}
