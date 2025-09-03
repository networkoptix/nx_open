// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "application_context.h"

#include <chrono>

#include <QtCore/QStandardPaths>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtGui/QPalette>
#include <QtQml/QQmlEngine>
#include <QtWidgets/QApplication>
#include <QtWidgets/QToolTip>

#include <api/server_rest_connection.h>
#include <client/client_autorun_watcher.h>
#include <client/client_meta_types.h>
#include <client/client_runtime_settings.h>
#include <client/client_startup_parameters.h>
#include <client/desktop_client_message_processor.h>
#include <client/forgotten_systems_manager.h>
#include <core/resource/local_resource_status_watcher.h>
#include <core/resource/resource.h>
#include <core/resource/resource_directory_browser.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/storage/file_storage/layout_storage_resource.h>
#include <core/storage/file_storage/qtfile_storage_resource.h>
#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/cloud/vms_gateway/vms_gateway_embeddable.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/cloud/tunnel/outgoing_tunnel_pool.h>
#include <nx/network/http/log/har.h>
#include <nx/p2p/p2p_ini.h>
#include <nx/speech_synthesizer/text_to_wave_server.h>
#include <nx/utils/crash_dump/systemexcept.h>
#include <nx/utils/debug.h>
#include <nx/utils/external_resources.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/utils/trace/session.h>
#include <nx/vms/api/protocol_version.h>
#include <nx/vms/client/core/analytics/analytics_icon_manager.h>
#include <nx/vms/client/core/analytics/object_display_settings.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/core/cross_system/cloud_layouts_manager.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection_factory.h>
#include <nx/vms/client/core/resource/resource_processor.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_resource_searcher.h>
#include <nx/vms/client/core/resource/unified_resource_pool.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/settings/systems_visibility_manager.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/font_config.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/utils/font_loader.h>
#include <nx/vms/client/desktop/common/widgets/loading_indicator.h>
#include <nx/vms/client/desktop/debug_utils/components/debug_info_storage.h>
#include <nx/vms/client/desktop/debug_utils/utils/debug_custom_actions.h>
#include <nx/vms/client/desktop/debug_utils/utils/performance_monitor.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/integrations/integrations.h>
#include <nx/vms/client/desktop/joystick/settings/manager.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/radass/radass_controller.h>
#include <nx/vms/client/desktop/resource/resource_factory.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/resource/screen_recording/audio_video_win/audio_video_input.h>
#include <nx/vms/client/desktop/settings/ipc_settings_synchronizer.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/settings/message_bar_settings.h>
#include <nx/vms/client/desktop/settings/screen_recording_settings.h>
#include <nx/vms/client/desktop/settings/show_once_settings.h>
#include <nx/vms/client/desktop/state/client_process_runner.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/state/fallback_shared_memory.h>
#include <nx/vms/client/desktop/state/file_based_shared_memory.h>
#include <nx/vms/client/desktop/state/qt_based_shared_memory.h>
#include <nx/vms/client/desktop/state/running_instances_manager.h>
#include <nx/vms/client/desktop/state/shared_memory_manager.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/client/desktop/style/old_style.h>
#include <nx/vms/client/desktop/style/style.h>
#include <nx/vms/client/desktop/system_administration/watchers/logs_management_watcher.h>
#include <nx/vms/client/desktop/system_logon/logic/connection_delegate_helper.h>
#include <nx/vms/client/desktop/ui/common/custom_cursors.h>
#include <nx/vms/client/desktop/ui/image_providers/resource_icon_provider.h>
#include <nx/vms/client/desktop/utils/applauncher_guard.h>
#include <nx/vms/client/desktop/utils/local_proxy_server.h>
#include <nx/vms/client/desktop/utils/upload_manager.h>
#include <nx/vms/client/desktop/webpage/web_page_data_cache.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/common/network/server_compatibility_validator.h>
#include <platform/platform_abstraction.h>
#include <ui/workaround/qtbug_workaround.h>
#include <utils/math/color_transformations.h>

#if defined(Q_OS_WIN)
    #include <nx/vms/client/desktop/resource/screen_recording/audio_video_win/windows_desktop_resource_searcher_impl.h>
#endif

#if defined(Q_OS_MACOS)
    #include <ui/workaround/mac_utils.h>
#endif

#include "system_context.h"
#include "window_context.h"

// Resources initialization must be located outside of the namespace.
static void initializeResources()
{
    Q_INIT_RESOURCE(nx_vms_client_desktop);
}

using namespace std::chrono;

namespace nx::vms::client::desktop {

namespace {

/**
 * Temporary implementation of the local resources search and maintain structure. Later will be
 * replaced by a separate SystemContext descendant. As of current version, works over provided
 * system context and must be destroyed before.
 */
struct LocalResourcesContext
{
    LocalResourcesContext(
        SystemContext* systemContext,
        const QnStartupParameters& startupParameters)
    {
        resourceDiscoveryManager = std::make_unique<QnResourceDiscoveryManager>(systemContext);
        resourceProcessor = std::make_unique<core::ResourceProcessor>(systemContext);
        resourceProcessor->moveToThread(resourceDiscoveryManager.get());
        resourceDiscoveryManager->setResourceProcessor(resourceProcessor.get());
        resourceDiscoveryManager->setReady(true);

        localResourceStatusWatcher =
            std::make_unique<QnLocalResourceStatusWatcher>(systemContext);

        if (!startupParameters.skipMediaFolderScan && !startupParameters.acsMode)
            resourceDirectoryBrowser = std::make_unique<ResourceDirectoryBrowser>(systemContext);
    }

    std::unique_ptr<QnResourceDiscoveryManager> resourceDiscoveryManager;
    std::unique_ptr<core::ResourceProcessor> resourceProcessor;
    std::unique_ptr<QnLocalResourceStatusWatcher> localResourceStatusWatcher;
    std::unique_ptr<ResourceDirectoryBrowser> resourceDirectoryBrowser;
};

core::ApplicationContext::Mode toCoreMode(ApplicationContext::Mode value)
{
    if (value == ApplicationContext::Mode::unitTests)
        return core::ApplicationContext::Mode::unitTests;

    return core::ApplicationContext::Mode::desktopClient;
}

nx::vms::api::PeerType peerType(const QnStartupParameters& startupParams)
{
    return startupParams.videoWallGuid.isNull()
        ? nx::vms::api::PeerType::desktopClient
        : nx::vms::api::PeerType::videowallClient;
}

Qn::SerializationFormat appSerializationFormat(const QnStartupParameters& startupParameters)
{
    return api::canPeerUseUbjson(peerType(startupParameters)) && nx::p2p::ini().forceUbjson
        ? Qn::SerializationFormat::ubjson
        : Qn::SerializationFormat::json;
}

void initDeveloperOptions(const QnStartupParameters& startupParameters)
{
    // Enable full crash dumps if needed. Do not disable here as it can be enabled elsewhere.
    #if defined(Q_OS_WIN)
        if (ini().profilerMode || ini().developerMode)
            win32_exception::setCreateFullCrashDump(true);
    #endif

    nx::utils::OsInfo::overrideVariant(
        ini().currentOsVariantOverride,
        ini().currentOsVariantVersionOverride);

    if (startupParameters.vmsProtocolVersion > 0)
        nx::vms::api::protocolVersionOverride = startupParameters.vmsProtocolVersion;

    rest::ServerConnection::setDebugFlag(
        rest::ServerConnection::DebugFlag::disableThumbnailRequests,
        ini().debugDisableCameraThumbnails);

    if (!startupParameters.enforceSocketType.isEmpty())
    {
        nx::network::SocketFactory::enforceStreamSocketType(
            startupParameters.enforceSocketType.toStdString());
    }

    if (!startupParameters.enforceMediatorEndpoint.isEmpty())
    {
        nx::network::SocketGlobals::cloud().mediatorConnector().mockupMediatorAddress({
            startupParameters.enforceMediatorEndpoint.toStdString(),
            startupParameters.enforceMediatorEndpoint.toStdString()});
    }
}

void initializeExternalResources()
{
    using namespace nx::utils;

    nx::vms::client::core::FontLoader::loadFonts(
        externalResourcesDirectory().absoluteFilePath("fonts"));
}

QFont initializeBaseFont()
{
    QFont font("Roboto", /*pointSize*/ -1, QFont::Normal);
    font.setPixelSize(13);
    return font;
}

void initializeDefaultApplicationFont()
{
    qApp->setFont(fontConfig()->normal());
}

QString calculateLogNameSuffix(
    SharedMemoryManager* sharedMemoryManager,
    const QnStartupParameters& startupParams)
{
    if (!startupParams.videoWallGuid.isNull())
    {
        QString result = startupParams.videoWallItemGuid.isNull()
            ? startupParams.videoWallGuid.toString(QUuid::WithBraces)
            : startupParams.videoWallItemGuid.toString(QUuid::WithBraces);
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

bool initializeLogFromFile(const QString& filename, const QString& suffix)
{
    const QDir iniFilesDir(nx::kit::IniConfig::iniFilesDir());
    const QString logConfigFile(iniFilesDir.absoluteFilePath(filename + ".ini"));

    if (!QFileInfo(logConfigFile).exists())
        return false;

    NX_INFO(NX_SCOPE_TAG, "Log is initialized from the %1", logConfigFile);
    NX_INFO(NX_SCOPE_TAG, "Log options from settings are ignored!");

    return nx::log::initializeFromConfigFile(
        logConfigFile,
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/log",
        nx::branding::desktopClientInternalName(),
        qApp->applicationFilePath(),
        suffix);
}

QPalette makeApplicationPalette()
{
    auto colorTheme = core::colorTheme();

    QPalette result(QApplication::palette());
    result.setColor(QPalette::WindowText, colorTheme->color("light16"));
    result.setColor(QPalette::Button, colorTheme->color("dark11"));
    result.setColor(QPalette::Light, colorTheme->color("light10"));
    result.setColor(QPalette::Midlight, colorTheme->color("dark13"));
    result.setColor(QPalette::Dark, colorTheme->color("dark9"));
    result.setColor(QPalette::Mid, colorTheme->color("dark10"));
    result.setColor(QPalette::Text, colorTheme->color("light4"));
    result.setColor(QPalette::BrightText, colorTheme->color("light1"));
    result.setColor(QPalette::ButtonText, colorTheme->color("light4"));
    result.setColor(QPalette::Base, colorTheme->color("dark7"));
    result.setColor(QPalette::Window, colorTheme->color("dark7"));
    result.setColor(QPalette::Shadow, colorTheme->color("dark5"));
    result.setColor(QPalette::Highlight, colorTheme->color("brand"));
    result.setColor(QPalette::HighlightedText, colorTheme->color("brand_contrast"));
    result.setColor(QPalette::Link, colorTheme->color("brand"));
    result.setColor(QPalette::LinkVisited, colorTheme->color("brand_l"));
    result.setColor(QPalette::AlternateBase, colorTheme->color("dark7"));
    result.setColor(QPalette::ToolTipBase, colorTheme->color("light4"));
    result.setColor(QPalette::ToolTipText, colorTheme->color("dark4"));
    result.setColor(QPalette::PlaceholderText, colorTheme->color("light16"));

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

struct ApplicationContext::Private
{
    ApplicationContext* const q;
    const Mode mode;
    const QnStartupParameters startupParameters;
    std::optional<nx::utils::SoftwareVersion> overriddenVersion;
    std::vector<QPointer<WindowContext>> windowContexts;
    std::unique_ptr<SystemContext> mainSystemContext; //< Main System Context;
    std::unique_ptr<core::UnifiedResourcePool> unifiedResourcePool;

    // Performance tracing.
    std::unique_ptr<nx::utils::trace::Session> tracingSession;

    // Settings modules.
    std::unique_ptr<LocalSettings> localSettings;
    std::unique_ptr<QnClientRuntimeSettings> runtimeSettings;
    std::unique_ptr<core::ObjectDisplaySettings> objectDisplaySettings;
    std::unique_ptr<ScreenRecordingSettings> screenRecordingSettings;
    std::unique_ptr<ShowOnceSettings> showOnceSettings;
    std::unique_ptr<MessageBarSettings> messageBarSettings;

    // State modules.
    std::unique_ptr<ClientStateHandler> clientStateHandler;
    std::unique_ptr<SharedMemoryManager> sharedMemoryManager;
    std::unique_ptr<RunningInstancesManager> runningInstancesManager;

    // Local resources search modules.
    std::unique_ptr<LocalResourcesContext> localResourcesContext;

    // Specialized context parts.
    std::unique_ptr<ContextStatisticsModule> statisticsModule;

    // UI Skin parts.
    std::unique_ptr<CustomCursors> customCursors;

    // Miscellaneous modules.
    std::unique_ptr<DebugInfoStorage> debugInfoStorage;
    std::unique_ptr<QnPlatformAbstraction> platformAbstraction;
    std::unique_ptr<PerformanceMonitor> performanceMonitor;

    std::unique_ptr<ApplauncherGuard> applauncherGuard;
    std::unique_ptr<QnClientAutoRunWatcher> autoRunWatcher;
    std::unique_ptr<integrations::Storage> integrationStorage;
    std::unique_ptr<RadassController> radassController;
    std::unique_ptr<ResourceFactory> resourceFactory;
    std::unique_ptr<UploadManager> uploadManager;
    std::unique_ptr<QnForgottenSystemsManager> forgottenSystemsManager;
    std::unique_ptr<ResourcesChangesManager> resourcesChangesManager;
    std::unique_ptr<nx::speech_synthesizer::TextToWaveServer> textToWaveServer;
    std::unique_ptr<WebPageDataCache> webPageDataCache;
    std::unique_ptr<QnQtbugWorkaround> qtBugWorkarounds;
    std::unique_ptr<core::DesktopResourceSearcher> desktopResourceSearcher;
    std::unique_ptr<nx::vms::client::desktop::joystick::Manager> joystickManager;
    std::unique_ptr<nx::vms::client::desktop::AudioVideoInput> audioVideoInput;

    // Network modules
    std::unique_ptr<nx::cloud::gateway::VmsGatewayEmbeddable> cloudGateway;
    std::unique_ptr<LocalProxyServer> localProxyServer;

    void initializeSettings()
    {
        // Make sure application is initialized correctly for settings to be loaded.
        NX_ASSERT(!QCoreApplication::applicationName().isEmpty());
        NX_ASSERT(!QCoreApplication::organizationName().isEmpty());

        localSettings = std::make_unique<LocalSettings>();

        // Enable full crash dumps if needed. Do not disable here as it can be enabled elsewhere.
        #if defined(Q_OS_WIN)
            if (localSettings->createFullCrashDump())
                win32_exception::setCreateFullCrashDump(true);
        #endif

        // Runtime settings are standalone, but some values are initialized from the persistent
        // settings.
        runtimeSettings = std::make_unique<QnClientRuntimeSettings>(startupParameters);
        runtimeSettings->setGLDoubleBuffer(localSettings->glDoubleBuffer());
        runtimeSettings->setMaximumLiveBufferMs(localSettings->maximumLiveBufferMs());

        GraphicsApi graphicsApi = GraphicsApi::opengl;
        if (NX_ASSERT(nx::reflect::enumeration::fromString(ini().graphicsApi, &graphicsApi)))
            runtimeSettings->setGraphicsApi(graphicsApi);

        common::appContext()->setLocale(appContext()->coreSettings()->locale());

        #ifdef Q_OS_MACOS
            if (mac_isSandboxed())
            {
                runtimeSettings->setLightMode(
                    runtimeSettings->lightMode() | Qn::LightModeNoNewWindow);
            }
        #endif

        objectDisplaySettings = std::make_unique<core::ObjectDisplaySettings>();
        screenRecordingSettings = std::make_unique<ScreenRecordingSettings>();
        showOnceSettings = std::make_unique<ShowOnceSettings>();
        messageBarSettings = std::make_unique<MessageBarSettings>();
    }

    void initializeStateModules()
    {
        const QString dataLocation = QStandardPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation);
        const QString storageDirectory = QDir(dataLocation).absoluteFilePath("state");
        clientStateHandler = std::make_unique<ClientStateHandler>(storageDirectory);

        clientStateHandler->setClientProcessExecutionInterface(
            std::make_unique<ClientProcessRunner>());

        SharedMemoryInterfacePtr sharedMemoryInterface;
        if (ini().useFileBasedSharedMemory)
            sharedMemoryInterface = std::make_unique<FileBasedSharedMemory>();
        else
            sharedMemoryInterface = std::make_unique<QtBasedSharedMemory>();

        if (!sharedMemoryInterface->initialize())
            sharedMemoryInterface = std::make_unique<FallbackSharedMemory>();

        sharedMemoryManager = std::make_unique<SharedMemoryManager>(
            std::move(sharedMemoryInterface),
            std::make_unique<ClientProcessRunner>());

        clientStateHandler->setSharedMemoryManager(sharedMemoryManager.get());

        runningInstancesManager = std::make_unique<RunningInstancesManager>(
            localSettings.get(),
            sharedMemoryManager.get());

        IpcSettingsSynchronizer::setup(
            localSettings.get(),
            showOnceSettings.get(),
            messageBarSettings.get(),
            sharedMemoryManager.get());
    }

    void initializeLogging(const QnStartupParameters& startupParameters)
    {
        nx::enableQtMessageAsserts();

        using namespace nx::log;

        const auto logFileNameSuffix = calculateLogNameSuffix(
            sharedMemoryManager.get(),
            startupParameters);

        static const QString kLogConfig("desktop_client_log");

        if (initializeLogFromFile(kLogConfig + logFileNameSuffix, /*suffix*/ QString()))
            return;

        if (initializeLogFromFile(kLogConfig, logFileNameSuffix))
            return;

        NX_INFO(NX_SCOPE_TAG, "Log is initialized from the settings");
        auto logLevel = startupParameters.logLevel;
        auto logFile = startupParameters.logFile;

        if (logLevel.isEmpty())
        {
            logLevel = appContext()->localSettings()->logLevel();
            NX_INFO(NX_SCOPE_TAG, "Log level is initialized from the settings");
        }
        else
        {
            NX_INFO(NX_SCOPE_TAG, "Log level is initialized from the command line");
        }

        Settings logSettings;
        logSettings.loggers.resize(1);
        auto& logger = logSettings.loggers.front();
        logger.maxVolumeSizeB = appContext()->localSettings()->maxLogVolumeSizeB();
        logger.maxFileSizeB = appContext()->localSettings()->maxLogFileSizeB();
        logger.maxFileTimePeriodS = appContext()->localSettings()->maxLogFileTimePeriodS();
        logger.archivingEnabled = appContext()->localSettings()->logArchivingEnabled();
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

    void initializeNetworkLogging()
    {
        const auto logDir =
            QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/log";

        if (QString harFile = ini().harFile; !harFile.isEmpty())
        {
            const auto fileName = nx::utils::debug::formatFileName(harFile, logDir);
            nx::network::http::Har::instance()->setFileName(fileName);
            NX_INFO(NX_SCOPE_TAG, "HAR logging is enabled: %1", fileName);
        }
    }

    void initializePlatformAbstraction()
    {
        platformAbstraction = std::make_unique<QnPlatformAbstraction>();
    }

    void initializeServerCompatibilityValidator()
    {
        using namespace nx::vms::common;
        using DeveloperFlag = ServerCompatibilityValidator::DeveloperFlag;

        ServerCompatibilityValidator::DeveloperFlags developerFlags;
        if (ini().developerMode || ini().demoMode)
            developerFlags.setFlag(DeveloperFlag::ignoreCustomization);

        if (core::ini().isAutoCloudHostDeductionMode() || ini().demoMode)
            developerFlags.setFlag(DeveloperFlag::ignoreCloudHost);

        if (ini().developerMode || startupParameters.isVideoWallLauncherMode())
            developerFlags.setFlag(DeveloperFlag::ignoreProtocolVersion);

        ServerCompatibilityValidator::initialize(
            q->localPeerType(), q->serializationFormat(), developerFlags);
    }

    void initializeNetworkModules()
    {
        nx::network::SocketGlobals::cloud().outgoingTunnelPool().assignOwnPeerId(
            "dc",
            q->peerId());
        initializeServerCompatibilityValidator();
        q->core::ApplicationContext::initializeNetworkModules(ini().enableHolePunching);
    }

    void initializeCrossSystemModules()
    {
        q->core::ApplicationContext::initializeCrossSystemModules();

        registerDebugAction("Cross-site contexts reset",
            [this](auto)
            {
                q->cloudCrossSystemManager()->resetCloudSystems(/*enableCloudSystems*/ false);
            });

        registerDebugAction("Cross-site contexts restore",
            [this](auto)
            {
                q->cloudCrossSystemManager()->resetCloudSystems(/*enableCloudSystems*/ true);
            });

        connect(q->cloudCrossSystemManager(),
            &core::CloudCrossSystemManager::cloudAuthorizationRequested,
            q,
            [this]()
            {
                q->mainWindowContext()->menu()->trigger(menu::LoginToCloud, {Qn::ForceRole, true});
            });
    }

    void initializeSystemContext()
    {
        const nx::Uuid peerId = mode == Mode::unitTests
            ? nx::Uuid::createUuid()
            : q->peerId();
        NX_ASSERT(!peerId.isNull());

        mainSystemContext.reset(q->createSystemContext(
            mode == Mode::unitTests
                ? SystemContext::Mode::unitTests
                : SystemContext::Mode::client)->as<SystemContext>());

        q->addMainContext(mainSystemContext.get());

        if (mode == Mode::desktopClient)
        {
            mainSystemContext->createMessageProcessor<QnDesktopClientMessageProcessor>(q);
        }

        if (auto networkModule = q->networkModule())
        {
            networkModule->connectionFactory()->setUserInteractionDelegate(
                createConnectionUserInteractionDelegate(mainSystemContext.get(),
                    []() { return appContext()->mainWindowContext()->mainWindowWidget(); }));
        }
    }

    void initializeTracing(const QnStartupParameters& startupParameters)
    {
        if (startupParameters.traceFile.isEmpty())
            return;

        tracingSession = nx::utils::trace::Session::start(startupParameters.traceFile);
    }

    // This is a temporary step until QnClientCoreModule contents is moved to the corresponding
    // contexts.
    void updateVersionIfNeeded()
    {
        if (!startupParameters.engineVersion.isEmpty())
        {
            nx::utils::SoftwareVersion version(startupParameters.engineVersion);
            if (!version.isNull())
            {
                NX_INFO(this, "Override engine version: %1", version);
                overriddenVersion = version;
            }
        }
    }

    void initSkin()
    {
        // On MacOS the icon from the package is used by default, and it has the better size.
        if (!nx::build_info::isMacOsX())
            QApplication::setWindowIcon(q->skin()->icon(":/logo.png"));

        QApplication::setStyle([]() { return new OldStyle(new Style()); }());

        customCursors = std::make_unique<nx::vms::client::desktop::CustomCursors>(q->skin());

        QApplication::setPalette(makeApplicationPalette());
        QToolTip::setPalette(QApplication::palette());

        // Add a generated pixmap that matches one frame of the Loading Indicator animation using
        // the naming of the corresponding icon in our resource tree (with a .gen suffix). It is
        // used instead of the original SVG because the gradient coloring there is unsupported.
        q->skin()->addGeneratedPixmap(
            "20x20/Outline/loaders.svg.gen", LoadingIndicator::createPixmap());
    }

    void initializeQml()
    {
        static const QString kQmlRoot = QStringLiteral("qrc:///qml");

        const auto qmlEngine = q->qmlEngine();
        qmlEngine->addImageProvider("resource", new ResourceIconProvider());

        auto qmlRoot = startupParameters.qmlRoot.isEmpty() ? kQmlRoot : startupParameters.qmlRoot;
        if (!qmlRoot.endsWith('/'))
            qmlRoot.append('/');
        NX_INFO(this, "Setting QML root to %1", qmlRoot);

        qmlEngine->setBaseUrl(
            qmlRoot.startsWith("qrc:")
                ? QUrl(qmlRoot)
                : QUrl::fromLocalFile(qmlRoot));
        qmlEngine->addImportPath(qmlRoot);

        for (const QString& path: startupParameters.qmlImportPaths)
            qmlEngine->addImportPath(path);
    }

    void initializeLocalResourcesSearch()
    {
        // client uses ordinary QT file to access file system
        q->storagePluginFactory()->registerStoragePlugin(
            "file",
            QnQtFileStorageResource::instance,
            /*isDefaultProtocol*/ true);
        q->storagePluginFactory()->registerStoragePlugin(
            "qtfile",
            QnQtFileStorageResource::instance);
        q->storagePluginFactory()->registerStoragePlugin(
            "layout",
            QnLayoutFileStorageResource::instance);

        // In the future there will be a separate system context for local resources.
        localResourcesContext = std::make_unique<LocalResourcesContext>(
            mainSystemContext.get(),
            startupParameters);
    }

    void initializeExceptionHandlerGuard()
    {
#if defined(Q_OS_WIN)
        // Intel graphics drivers call `SetUnhandledExceptionFilter` and therefore overwrite our
        // crash dumps collecting implementation. Thus we repeat installation when client window is
        // drawn already, and all driver libraries are loaded.
        // https://community.intel.com/t5/Graphics/igdumdim64-dll-shouldn-t-call-SetUnhandledExceptionFilter/m-p/1494853
        static constexpr auto kReinstallExceptionHandlerPeriod = 30s;
        auto timer = new QTimer(q);
        timer->callOnTimeout([] { win32_exception::installGlobalUnhandledExceptionHandler(); });
        timer->start(kReinstallExceptionHandlerPeriod);
#endif
    }
};

using CoreFeatureFlag = core::ApplicationContext::FeatureFlag;

ApplicationContext::ApplicationContext(
    Mode mode,
    Features features,
    const QnStartupParameters& startupParameters,
    QObject* parent)
    :
    core::ApplicationContext(
        toCoreMode(mode),
        appSerializationFormat(startupParameters),
        peerType(startupParameters),
        features.core,
        /*customCloudHost*/ QString(),
        /*customExternalResourceFile*/ "client_external.dat",
        parent),
    d(new Private{
        .q = this,
        .mode = mode,
        .startupParameters = startupParameters
    })
{
    d->initializeTracing(startupParameters);
    initDeveloperOptions(startupParameters);
    initializeResources();
    initializeExternalResources();
    initializeMetaTypes();
    if (features.core.flags.testFlag(CoreFeatureFlag::qml))
        registerQmlTypes();

    d->initSkin();
    storeFontConfig(
        new FontConfig(initializeBaseFont(), ":/skin/basic_fonts.json", ini().fontConfigPath));
    initializeDefaultApplicationFont();
    d->initializeSettings();
    d->initializeExceptionHandlerGuard();

    switch (mode)
    {
        // Initialize only minimal set of modules for unit tests.
        case Mode::unitTests:
        {
            d->initializeSystemContext();
            if (features.core.flags.testFlag(CoreFeatureFlag::cross_site))
                d->initializeCrossSystemModules();
            d->updateVersionIfNeeded();
            if (features.core.flags.testFlag(CoreFeatureFlag::qml))
                d->initializeQml();
            d->resourcesChangesManager = std::make_unique<ResourcesChangesManager>();
            break;
        }

        // Initialize only minimal set of modules when running client in self-update mode.
        case Mode::selfUpdate:
        {
            d->initializeLogging(startupParameters);
            d->initializePlatformAbstraction();
            break;
        }

        // Full initialization.
        case Mode::desktopClient:
        {
            d->initializeStateModules();
            d->initializeLogging(startupParameters); //< Depends on state modules.
            d->initializeNetworkLogging();
            initializeTranslations(coreSettings()->locale());
            d->initializePlatformAbstraction();
            d->debugInfoStorage = std::make_unique<DebugInfoStorage>();
            d->textToWaveServer = nx::speech_synthesizer::TextToWaveServer::create(
                QCoreApplication::applicationDirPath());
            d->performanceMonitor = std::make_unique<PerformanceMonitor>();
            d->statisticsModule = std::make_unique<ContextStatisticsModule>();
            d->initializeNetworkModules();
            d->sharedMemoryManager->connectToCloudStatusWatcher(); //< Depends on CloudStatusWatcher.
            d->initializeSystemContext();
            if (features.core.flags.testFlag(CoreFeatureFlag::cross_site))
                d->initializeCrossSystemModules();
            d->mainSystemContext->startModuleDiscovery(moduleDiscoveryManager());
            d->updateVersionIfNeeded();
            d->initializeQml();
            d->initializeLocalResourcesSearch();
            d->applauncherGuard = std::make_unique<ApplauncherGuard>();
            d->autoRunWatcher = std::make_unique<QnClientAutoRunWatcher>();
            d->integrationStorage = std::make_unique<integrations::Storage>(
                runtimeSettings()->isDesktopMode());
            d->radassController = std::make_unique<RadassController>();
            d->resourceFactory = std::make_unique<ResourceFactory>();
            d->uploadManager = std::make_unique<UploadManager>();
            d->forgottenSystemsManager = std::make_unique<QnForgottenSystemsManager>();
            d->resourcesChangesManager = std::make_unique<ResourcesChangesManager>();
            d->webPageDataCache = std::make_unique<WebPageDataCache>();
            d->qtBugWorkarounds = std::make_unique<QnQtbugWorkaround>();
            d->cloudGateway = std::make_unique<nx::cloud::gateway::VmsGatewayEmbeddable>();
            d->localProxyServer = std::make_unique<LocalProxyServer>();
            d->joystickManager.reset(joystick::Manager::create());

            QObject::connect(d->sharedMemoryManager.get(),
                &SharedMemoryManager::clientCommandRequested,
                cloudLayoutsManager(),
                [this](SharedMemoryData::Command command, const QByteArray& /*data*/)
                {
                    if (command == SharedMemoryData::Command::updateCloudLayouts)
                        cloudLayoutsManager()->updateLayouts();
                });

            break;
        }
    }

    // PerformanceMonitor provides additional trace data.
    if (d->tracingSession)
        d->performanceMonitor->setEnabled(true);
}

ApplicationContext::~ApplicationContext()
{
    // For now it depends on a System Context, later it will be moved to the Local System Context.
    d->desktopResourceSearcher.reset();

    // Local resources context temporary implementation depends on main system context.
    d->localResourcesContext.reset();

    // Web Page icon cache uses application context.
    d->webPageDataCache.reset();

    // Main system context does not exist in self-update mode.
    if (d->mainSystemContext)
    {
        // Terminate running session if it sill exists. Session depends on common module, so we
        // must clean all shared pointers before destroying ClientCoreModule.
        d->mainSystemContext->setSession({});
        if (d->mainSystemContext->messageProcessor())
            d->mainSystemContext->deleteMessageProcessor();

        // Remote session must be fully destroyed while application context still exists.
        removeSystemContext(d->mainSystemContext.release());
    }

    // Currently, throughout the derived client code (desktop, mobile) it's assumed everywhere
    // that all system contexts are of that most derived level. I.e. desktop client code expects
    // desktop::SystemContext, mobile client code expects mobile::SystemContext everywhere.
    // As soon as some system context creation code appeared at the client::core level,
    // we had to introduce in ApplicationContext a virtual function to create system contexts
    // of the most derived type. However, system contexts often rely on the application context
    // - apparently of the same most derived level. For that reason we must destroy all
    // system contexts in the most derived ApplicationContext destructor.
    destroyCrossSystemModules();

    // System contexts destruction is delayed and delegated to QnLongRunableCleanup.
    // We must enforce it to finish here synchronously.
    common::ApplicationContext::stopAll();
}

core::SystemContext* ApplicationContext::createSystemContext(
    SystemContext::Mode mode, QObject* parent)
{
    return new SystemContext(mode, peerId(), parent);
}

nx::utils::SoftwareVersion ApplicationContext::version() const
{
    return d->overriddenVersion
        ? *d->overriddenVersion
        : nx::utils::SoftwareVersion(nx::build_info::vmsVersion());
}

WindowContext* ApplicationContext::mainWindowContext() const
{
    if (NX_ASSERT(!d->windowContexts.empty()))
        return d->windowContexts.front();

    return nullptr;
}

void ApplicationContext::addWindowContext(WindowContext* windowContext)
{
    d->windowContexts.push_back(windowContext);
}

void ApplicationContext::removeWindowContext(WindowContext* windowContext)
{
    auto iter = std::find(d->windowContexts.begin(), d->windowContexts.end(), windowContext);
    if (NX_ASSERT(iter != d->windowContexts.end()))
        d->windowContexts.erase(iter);
}

void ApplicationContext::initializeDesktopCamera([[maybe_unused]] QOpenGLWidget* window)
{
#if defined(Q_OS_WIN)
    d->audioVideoInput = std::make_unique<AudioVideoInput>(window);
    auto impl = new WindowsDesktopResourceSearcherImpl();
    d->desktopResourceSearcher = std::make_unique<core::DesktopResourceSearcher>(
        impl,
        currentSystemContext()); //< TODO: #sivanov Use the same system context as for local files.
    d->desktopResourceSearcher->setLocal(true);
    resourceDiscoveryManager()->addDeviceSearcher(d->desktopResourceSearcher.get());
#endif
}

nx::Uuid ApplicationContext::peerId() const
{
    if (d->runningInstancesManager)
        return d->runningInstancesManager->currentInstanceGuid();

    return base_type::peerId();
}

nx::Uuid ApplicationContext::videoWallInstanceId() const
{
    return d->startupParameters.videoWallItemGuid;
}

SystemContext* ApplicationContext::currentSystemContext() const
{
    return base_type::currentSystemContext()
        ? base_type::currentSystemContext()->as<SystemContext>()
        : nullptr;
}

ContextStatisticsModule* ApplicationContext::statisticsModule() const
{
    return d->statisticsModule.get();
}

LocalSettings* ApplicationContext::localSettings() const
{
    return d->localSettings.get();
}

QnClientRuntimeSettings* ApplicationContext::runtimeSettings() const
{
    return d->runtimeSettings.get();
}

ScreenRecordingSettings* ApplicationContext::screenRecordingSettings() const
{
    return d->screenRecordingSettings.get();
}

ShowOnceSettings* ApplicationContext::showOnceSettings() const
{
    return d->showOnceSettings.get();
}

MessageBarSettings* ApplicationContext::messageBarSettings() const
{
    return d->messageBarSettings.get();
}

core::ObjectDisplaySettings* ApplicationContext::objectDisplaySettings() const
{
    return d->objectDisplaySettings.get();
}

ClientStateHandler* ApplicationContext::clientStateHandler() const
{
    return d->clientStateHandler.get();
}

SharedMemoryManager* ApplicationContext::sharedMemoryManager() const
{
    return d->sharedMemoryManager.get();
}

void ApplicationContext::setSharedMemoryManager(std::unique_ptr<SharedMemoryManager> value)
{
    d->sharedMemoryManager = std::move(value);
}

RunningInstancesManager* ApplicationContext::runningInstancesManager() const
{
    return d->runningInstancesManager.get();
}

DebugInfoStorage* ApplicationContext::debugInfoStorage() const
{
    return d->debugInfoStorage.get();
}

integrations::Storage* ApplicationContext::integrationStorage() const
{
    return d->integrationStorage.get();
}

PerformanceMonitor* ApplicationContext::performanceMonitor() const
{
    return d->performanceMonitor.get();
}

QnPlatformAbstraction* ApplicationContext::platform() const
{
    return d->platformAbstraction.get();
}

RadassController* ApplicationContext::radassController() const
{
    return d->radassController.get();
}

ResourceFactory* ApplicationContext::resourceFactory() const
{
    return d->resourceFactory.get();
}

UploadManager* ApplicationContext::uploadManager() const
{
    return d->uploadManager.get();
}

QnResourceDiscoveryManager* ApplicationContext::resourceDiscoveryManager() const
{
    return d->localResourcesContext->resourceDiscoveryManager.get();
}

ResourceDirectoryBrowser* ApplicationContext::resourceDirectoryBrowser() const
{
    return d->localResourcesContext->resourceDirectoryBrowser.get();
}

ResourcesChangesManager* ApplicationContext::resourcesChangesManager() const
{
    return d->resourcesChangesManager.get();
}

WebPageDataCache* ApplicationContext::webPageDataCache() const
{
    return d->webPageDataCache.get();
}

QnForgottenSystemsManager* ApplicationContext::forgottenSystemsManager() const
{
    return d->forgottenSystemsManager.get();
}

nx::speech_synthesizer::TextToWaveServer* ApplicationContext::textToWaveServer() const
{
    return d->textToWaveServer.get();
}

nx::cloud::gateway::VmsGatewayEmbeddable* ApplicationContext::cloudGateway() const
{
    return d->cloudGateway.get();
}

LocalProxyServer* ApplicationContext::localProxyServer() const
{
    return d->localProxyServer.get();
}

CustomCursors* ApplicationContext::cursors() const
{
    return d->customCursors.get();
}

FontConfig* ApplicationContext::fontConfig() const
{
    return static_cast<FontConfig*>(core::ApplicationContext::fontConfig());
}

joystick::Manager* ApplicationContext::joystickManager() const
{
    return d->joystickManager.get();
}

const QnStartupParameters& ApplicationContext::startupParameters() const
{
    return d->startupParameters;
}

std::unique_ptr<QnAbstractStreamDataProvider> ApplicationContext::createAudioInputProvider() const
{
#if defined(Q_OS_WIN)
    return d->audioVideoInput->createAudioInputProvider();
#else
    return base_type::createAudioInputProvider();
#endif
}

} // namespace nx::vms::client::desktop
