// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "application_context.h"

#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>
#include <QtCore/QThread>
#include <QtQml/QQmlEngine>
#include <QtWidgets/QApplication>

#include <api/server_rest_connection.h>
#include <client/client_autorun_watcher.h>
#include <client/client_meta_types.h>
#include <client/client_runtime_settings.h>
#include <client/client_settings.h>
#include <client/client_show_once_settings.h>
#include <client/client_startup_parameters.h>
#include <client/desktop_client_message_processor.h>
#include <client_core/client_core_module.h>
#include <core/resource/resource.h>
#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/cloud/tunnel/outgoing_tunnel_pool.h>
#include <nx/utils/crash_dump/systemexcept.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/vms/api/protocol_version.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/utils/font_loader.h>
#include <nx/vms/client/desktop/analytics/analytics_icon_manager.h>
#include <nx/vms/client/desktop/analytics/object_display_settings.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/desktop/cross_system/cloud_layouts_manager.h>
#include <nx/vms/client/desktop/cross_system/cross_system_layouts_watcher.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/radass/radass_controller.h>
#include <nx/vms/client/desktop/resource/resource_factory.h>
#include <nx/vms/client/desktop/resource/unified_resource_pool.h>
#include <nx/vms/client/desktop/session_manager/default_process_interface.h>
#include <nx/vms/client/desktop/session_manager/session_manager.h>
#include <nx/vms/client/desktop/settings/ipc_settings_synchronizer.h>
#include <nx/vms/client/desktop/state/client_process_runner.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/state/fallback_shared_memory.h>
#include <nx/vms/client/desktop/state/qt_based_shared_memory.h>
#include <nx/vms/client/desktop/state/running_instances_manager.h>
#include <nx/vms/client/desktop/state/shared_memory_manager.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/client/desktop/system_administration/watchers/logs_management_watcher.h>
#include <nx/vms/client/desktop/ui/image_providers/resource_icon_provider.h>
#include <nx/vms/client/desktop/ui/right_panel/models/right_panel_models_adapter.h>
#include <nx/vms/client/desktop/utils/applauncher_guard.h>
#include <nx/vms/client/desktop/utils/upload_manager.h>
#include <nx/vms/common/network/server_compatibility_validator.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/utils/external_resources.h>
#include <nx/vms/utils/translation/translation_manager.h>
#include <platform/platform_abstraction.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/storage/file_storage/layout_storage_resource.h>
#include <core/storage/file_storage/qtfile_storage_resource.h>
#include <core/resource/storage_plugin_factory.h>
#include <client/client_resource_processor.h>
#include <client/system_weights_manager.h>
#include <core/resource/local_resource_status_watcher.h>
#include <core/resource/resource_directory_browser.h>

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

namespace nx::vms::client::desktop {

namespace {

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

QString actualCloudHost()
{
    return ini().isAutoCloudHostDeductionMode()
        ? QString()
        : ini().cloudHost;
}

Qn::SerializationFormat serializationFormat()
{
    return ini().forceJsonConnection ? Qn::JsonFormat : Qn::UbjsonFormat;
}

void initDeveloperOptions(const QnStartupParameters& startupParameters)
{
    // Enable full crash dumps if needed. Do not disable here as it can be enabled elsewhere.
    #if defined(Q_OS_WIN)
        if (ini().profilerMode)
            win32_exception::setCreateFullCrashDump(true);
    #endif

    nx::utils::OsInfo::override(
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
    using namespace nx::vms::utils;

    nx::vms::client::core::FontLoader::loadFonts(
        externalResourcesDirectory().absoluteFilePath("fonts"));

    registerExternalResource("client_external.dat");
    registerExternalResource("bytedance_iconpark.dat",
        analytics::IconManager::librariesRoot() + "bytedance.iconpark/");
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

bool initializeLogFromFile(const QString& filename, const QString& suffix)
{
    const QDir iniFilesDir(nx::kit::IniConfig::iniFilesDir());
    const QString logConfigFile(iniFilesDir.absoluteFilePath(filename + ".ini"));

    if (!QFileInfo(logConfigFile).exists())
        return false;

    NX_INFO(NX_SCOPE_TAG, "Log is initialized from the %1", logConfigFile);
    NX_INFO(NX_SCOPE_TAG, "Log options from settings are ignored!");

    return nx::utils::log::initializeFromConfigFile(
        logConfigFile,
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/log",
        nx::branding::desktopClientInternalName(),
        qApp->applicationFilePath(),
        suffix);
}

} // namespace

static ApplicationContext* s_instance = nullptr;

struct ApplicationContext::Private
{
    void initializeSettings()
    {
        // Make sure application is initialized correctly for settings to be loaded.
        NX_ASSERT(!QCoreApplication::applicationName().isEmpty());
        NX_ASSERT(!QCoreApplication::organizationName().isEmpty());

        settings = std::make_unique<QnClientSettings>(startupParameters);
        settings->migrate();

        // Enable full crash dumps if needed. Do not disable here as it can be enabled elsewhere.
        #if defined(Q_OS_WIN)
            if (settings->createFullCrashDump())
                win32_exception::setCreateFullCrashDump(true);
        #endif

        // Runtime settings are standalone, but some values are initialized from the persistent
        // settings.
        runtimeSettings = std::make_unique<QnClientRuntimeSettings>(startupParameters);
        runtimeSettings->setGLDoubleBuffer(settings->isGlDoubleBuffer());
        runtimeSettings->setMaximumLiveBufferMs(settings->maximumLiveBufferMs());
        runtimeSettings->setLocale(settings->locale());

        #ifdef Q_OS_MACX
            if (mac_isSandboxed())
            {
                runtimeSettings->setLightMode(
                    runtimeSettings->lightMode() | Qn::LightModeNoNewWindow);
            }
        #endif

        objectDisplaySettings = std::make_unique<ObjectDisplaySettings>();
        showOnceSettings = std::make_unique<QnClientShowOnceSettings>();
    }

    void initializeStateModules()
    {
        const QString dataLocation = QStandardPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation);
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
            settings.get(),
            sharedMemoryManager.get());

        processInterface = std::make_unique<session::DefaultProcessInterface>();
        using SessionManager = session::SessionManager;
        session::Config sessionConfig;
        sessionConfig.sharedPrefix = nx::branding::customization() + "/SessionManager";
        const QString appDataLocation = QStandardPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation);
        sessionConfig.storagePath = QDir(appDataLocation).absoluteFilePath("sessions");
        sessionManager = std::make_unique<SessionManager>(sessionConfig, processInterface.get());

        IpcSettingsSynchronizer::setup(
            settings.get(),
            showOnceSettings.get(),
            sharedMemoryManager.get());
    }

    void initializeLogging(const QnStartupParameters& startupParameters)
    {
        nx::utils::enableQtMessageAsserts();

        using namespace nx::utils::log;

        const auto logFileNameSuffix = calculateLogNameSuffix(
            sharedMemoryManager.get(),
            startupParameters);

        static const QString kLogConfig("desktop_client_log");

        if (initializeLogFromFile(kLogConfig + logFileNameSuffix, /*suffix*/ QString()))
            return;

        if (initializeLogFromFile(kLogConfig, logFileNameSuffix))
            return;

        NX_INFO(NX_SCOPE_TAG, "Log is initialized from the settings");
        QSettings rawSettings;
        const auto maxVolumeSize = rawSettings.value(
            kMaxLogVolumeSizeSymbolicName,
            100 * 1024 * 1024).toUInt();
        const auto maxFileSize = rawSettings.value(
            kMaxLogFileSizeSymbolicName,
            10 * 1024 * 1024).toUInt();

        auto logLevel = startupParameters.logLevel;
        auto logFile = startupParameters.logFile;

        if (logLevel.isEmpty())
        {
            logLevel = qnSettings->logLevel();
            NX_INFO(NX_SCOPE_TAG, "Log level is initialized from the settings");
        }
        else
        {
            NX_INFO(NX_SCOPE_TAG, "Log level is initialized from the command line");
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

    void initializePlatformAbstraction()
    {
        platformAbstraction = std::make_unique<QnPlatformAbstraction>();
    }

    void initializeServerCompatibilityValidator()
    {
        using namespace nx::vms::common;
        using Protocol = ServerCompatibilityValidator::Protocol;

        ServerCompatibilityValidator::DeveloperFlags developerFlags;
        if (ini().developerMode)
            developerFlags.setFlag(ServerCompatibilityValidator::DeveloperFlag::ignoreCustomization);

        if (ini().isAutoCloudHostDeductionMode())
            developerFlags.setFlag(ServerCompatibilityValidator::DeveloperFlag::ignoreCloudHost);

        ServerCompatibilityValidator::initialize(
            q->localPeerType(),
            ini().forceJsonConnection ? Protocol::json : Protocol::ubjson,
            developerFlags);
    }

    void initializeNetworkModules()
    {
        nx::network::SocketGlobals::cloud().outgoingTunnelPool().assignOwnPeerId(
            "dc",
            q->peerId());
        initializeServerCompatibilityValidator();
        q->core::ApplicationContext::initializeNetworkModules();
    }

    void initializeCrossSystemModules()
    {
        cloudCrossSystemManager = std::make_unique<CloudCrossSystemManager>();
        if (ini().crossSystemLayouts)
        {
            cloudLayoutsManager = std::make_unique<CloudLayoutsManager>();
            crossSystemLayoutsWatcher = std::make_unique<CrossSystemLayoutsWatcher>();
        }
    }

    void initializeTranslations()
    {
        translationManager = std::make_unique<nx::vms::utils::TranslationManager>();
        bool loaded = translationManager->installTranslation(qnSettings->locale());
        if (!loaded)
            loaded = translationManager->installTranslation(nx::branding::defaultLocale());
        NX_ASSERT(loaded, "Translations could not be initialized");

        translationManager->setAssertOnOverlayInstallationFailure();
    }

    void initializeSystemContext()
    {
        const QnUuid peerId = mode == Mode::unitTests
            ? QnUuid::createUuid()
            : q->peerId();
        NX_ASSERT(!peerId.isNull());

        mainSystemContext = std::make_unique<SystemContext>(
            mode == Mode::unitTests
                ? SystemContext::Mode::unitTests
                : SystemContext::Mode::default_,
            peerId);
        systemContexts.push_back(mainSystemContext.get());

        if (mode == Mode::desktopClient)
        {
            mainSystemContext->createMessageProcessor<QnDesktopClientMessageProcessor>(q);
        }
    }

    // This is a temporary step until QnClientCoreModule contents is moved to the corresponding
    // contexts.
    void initializeClientCoreModule()
    {
        clientCoreModule = std::make_unique<QnClientCoreModule>(mainSystemContext.get());
        if (mode == Mode::desktopClient)
        {
            clientCoreModule->initializeNetworking(
                q->localPeerType(),
                serializationFormat(),
                core::settings()->certificateValidationLevel());
        }

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

    void initializeQml()
    {
        static const QString kQmlRoot = QStringLiteral("qrc:///qml");

        const auto qmlEngine = q->qmlEngine();
        qmlEngine->addImageProvider("resource", new ResourceIconProvider());
        qmlEngine->addImageProvider("right_panel", new RightPanelImageProvider());

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
        auto localContext = mainSystemContext.get();

        resourceDiscoveryManager = std::make_unique<QnResourceDiscoveryManager>(localContext);
        clientResourceProcessor = std::make_unique<QnClientResourceProcessor>(localContext);
        clientResourceProcessor->moveToThread(resourceDiscoveryManager.get());
        resourceDiscoveryManager->setResourceProcessor(clientResourceProcessor.get());
        resourceDiscoveryManager->setReady(true);

        localResourceStatusWatcher =
            std::make_unique<QnLocalResourceStatusWatcher>(localContext);

        if (!startupParameters.skipMediaFolderScan && !startupParameters.acsMode)
            resourceDirectoryBrowser = std::make_unique<ResourceDirectoryBrowser>(localContext);
    }

    ApplicationContext* const q;
    const Mode mode;
    const QnStartupParameters startupParameters;
    std::optional<nx::utils::SoftwareVersion> overriddenVersion;
    std::vector<QPointer<SystemContext>> systemContexts;
    std::vector<QPointer<WindowContext>> windowContexts;
    std::unique_ptr<SystemContext> mainSystemContext; //< Main System Context;
    std::unique_ptr<QnClientCoreModule> clientCoreModule;
    std::unique_ptr<UnifiedResourcePool> unifiedResourcePool;

    // Settings modules.
    std::unique_ptr<QnClientSettings> settings;
    std::unique_ptr<QnClientRuntimeSettings> runtimeSettings;
    std::unique_ptr<ObjectDisplaySettings> objectDisplaySettings;
    std::unique_ptr<QnClientShowOnceSettings> showOnceSettings;

    // State modules.
    std::unique_ptr<ClientStateHandler> clientStateHandler;
    std::unique_ptr<SharedMemoryManager> sharedMemoryManager;
    std::unique_ptr<RunningInstancesManager> runningInstancesManager;
    std::unique_ptr<session::DefaultProcessInterface> processInterface;
    std::unique_ptr<session::SessionManager> sessionManager;

    // Local resources search modules.
    std::unique_ptr<QnResourceDiscoveryManager> resourceDiscoveryManager;
    std::unique_ptr<QnClientResourceProcessor> clientResourceProcessor;
    std::unique_ptr<QnLocalResourceStatusWatcher> localResourceStatusWatcher;
    std::unique_ptr<ResourceDirectoryBrowser> resourceDirectoryBrowser;

    // Specialized context parts.
    std::unique_ptr<ContextStatisticsModule> statisticsModule;

    // Miscelaneous modules.
    std::unique_ptr<QnPlatformAbstraction> platformAbstraction;
    std::unique_ptr<nx::vms::utils::TranslationManager> translationManager;
    std::unique_ptr<ApplauncherGuard> applauncherGuard;
    std::unique_ptr<QnClientAutoRunWatcher> autoRunWatcher;
    std::unique_ptr<RadassController> radassController;
    std::unique_ptr<ResourceFactory> resourceFactory;
    std::unique_ptr<UploadManager> uploadManager;
    std::unique_ptr<QnSystemsWeightsManager> systemsWeightsManager;
    std::unique_ptr<LogsManagementWatcher> logaManagementWatcher;

    // Network modules
    std::unique_ptr<CloudCrossSystemManager> cloudCrossSystemManager;
    std::unique_ptr<CloudLayoutsManager> cloudLayoutsManager;
    std::unique_ptr<CrossSystemLayoutsWatcher> crossSystemLayoutsWatcher;
};

ApplicationContext::ApplicationContext(
    Mode mode,
    const QnStartupParameters& startupParameters,
    QObject* parent)
    :
    core::ApplicationContext(
        toCoreMode(mode),
        peerType(startupParameters),
        actualCloudHost(),
        parent),
    d(new Private{
        .q = this,
        .mode = mode,
        .startupParameters = startupParameters
    })
{
    if (NX_ASSERT(!s_instance))
        s_instance = this;

    initDeveloperOptions(startupParameters);
    initializeResources();
    initializeExternalResources();
    QnClientMetaTypes::initialize();
    d->initializeSettings();

    switch (mode)
    {
        // Initialize only minimal set of modules for unit tests.
        case Mode::unitTests:
        {
            d->initializeSystemContext();
            d->initializeCrossSystemModules(); //< For the resources tree tests.
            d->initializeClientCoreModule();
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
            d->initializePlatformAbstraction();
            d->initializeTranslations();
            d->statisticsModule = std::make_unique<ContextStatisticsModule>();
            d->initializeNetworkModules();
            d->initializeSystemContext();
            d->initializeCrossSystemModules();
            d->unifiedResourcePool = std::make_unique<UnifiedResourcePool>();
            d->mainSystemContext->enableRouting(moduleDiscoveryManager());
            d->initializeClientCoreModule();
            d->initializeQml();
            d->initializeLocalResourcesSearch();
            d->applauncherGuard = std::make_unique<ApplauncherGuard>();
            d->autoRunWatcher = std::make_unique<QnClientAutoRunWatcher>();
            d->radassController = std::make_unique<RadassController>();
            d->resourceFactory = std::make_unique<ResourceFactory>();
            d->uploadManager = std::make_unique<UploadManager>();
            d->systemsWeightsManager = std::make_unique<QnSystemsWeightsManager>();
            d->logaManagementWatcher = std::make_unique<LogsManagementWatcher>(currentSystemContext());
            break;
        }
    }
}

ApplicationContext::~ApplicationContext()
{
    if (d->mainSystemContext && d->mainSystemContext->messageProcessor())
        d->mainSystemContext->deleteMessageProcessor();

    if (NX_ASSERT(s_instance == this))
        s_instance = nullptr;
}

ApplicationContext* ApplicationContext::instance()
{
    NX_ASSERT(s_instance);
    return s_instance;
}

nx::utils::SoftwareVersion ApplicationContext::version() const
{
    return d->overriddenVersion
        ? *d->overriddenVersion
        : nx::utils::SoftwareVersion(nx::build_info::vmsVersion());
}

SystemContext* ApplicationContext::currentSystemContext() const
{
    if (NX_ASSERT(!d->systemContexts.empty()))
        return d->systemContexts.front();

    return nullptr;
}

std::vector<SystemContext*> ApplicationContext::systemContexts() const
{
    std::vector<SystemContext*> result;
    for (auto context: d->systemContexts)
    {
        if (NX_ASSERT(context))
            result.push_back(context.data());
    }
    return result;
}

void ApplicationContext::addSystemContext(SystemContext* systemContext)
{
    d->systemContexts.push_back(systemContext);
    emit systemContextAdded(systemContext);
}

void ApplicationContext::removeSystemContext(SystemContext* systemContext)
{
    auto iter = std::find(d->systemContexts.begin(), d->systemContexts.end(), systemContext);
    if (NX_ASSERT(iter != d->systemContexts.end()))
        d->systemContexts.erase(iter);
    emit systemContextRemoved(systemContext);
}

SystemContext* ApplicationContext::systemContextByCloudSystemId(const QString& cloudSystemId) const
{
    if (const auto cloudContext = d->cloudCrossSystemManager->systemContext(cloudSystemId))
        return cloudContext->systemContext();

    return nullptr;
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

QnUuid ApplicationContext::peerId() const
{
    return d->runningInstancesManager
        ? d->runningInstancesManager->currentInstanceGuid()
        : QnUuid();
}

QnUuid ApplicationContext::videoWallInstanceId() const
{
    return d->startupParameters.videoWallItemGuid;
}

QnClientCoreModule* ApplicationContext::clientCoreModule() const
{
    return d->clientCoreModule.get();
}

ContextStatisticsModule* ApplicationContext::statisticsModule() const
{
    return d->statisticsModule.get();
}

UnifiedResourcePool* ApplicationContext::unifiedResourcePool() const
{
    return d->unifiedResourcePool.get();
}

ObjectDisplaySettings* ApplicationContext::objectDisplaySettings() const
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

RunningInstancesManager* ApplicationContext::runningInstancesManager() const
{
    return d->runningInstancesManager.get();
}

session::SessionManager* ApplicationContext::sessionManager() const
{
    return d->sessionManager.get();
}

nx::vms::utils::TranslationManager* ApplicationContext::translationManager() const
{
    return d->translationManager.get();
}

CloudCrossSystemManager* ApplicationContext::cloudCrossSystemManager() const
{
    return d->cloudCrossSystemManager.get();
}

CloudLayoutsManager* ApplicationContext::cloudLayoutsManager() const
{
    return d->cloudLayoutsManager.get();
}

SystemContext* ApplicationContext::cloudLayoutsSystemContext() const
{
    if (NX_ASSERT(d->cloudLayoutsManager))
        return d->cloudLayoutsManager->systemContext();

    return {};
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
    return d->resourceDiscoveryManager.get();
}

LogsManagementWatcher* ApplicationContext::logsManagementWatcher() const
{
    return d->logaManagementWatcher.get();
}

} // namespace nx::vms::client::desktop
