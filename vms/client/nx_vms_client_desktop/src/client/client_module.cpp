#include "client_module.h"

#include <memory>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include <QtWidgets/QApplication>
#include <QtWebKit/QWebSettings>
#include <QtQml/QQmlEngine>
#include <QtOpenGL/QtOpenGL>
#include <QtGui/QSurfaceFormat>

#include <api/app_server_connection.h>
#include <api/global_settings.h>
#include <api/network_proxy_factory.h>
#include <api/runtime_info_manager.h>
#include <api/session_manager.h>

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <camera/camera_data_manager.h>

#include <nx/utils/crash_dump/systemexcept.h>

#include <camera/camera_bookmarks_manager.h>

#include <client_core/client_core_settings.h>
#include <client_core/client_core_module.h>

#include <nx/vms/client/desktop/settings/migration.h>
#include <client/client_app_info.h>
#include <client/client_settings.h>
#include <client/client_runtime_settings.h>
#include <client/client_meta_types.h>
#include <client/client_instance_manager.h>
#include <client/client_resource_processor.h>
#include <client/desktop_client_message_processor.h>
#include <client/system_weights_manager.h>
#include <client/forgotten_systems_manager.h>
#include <client/startup_tile_manager.h>
#include <client/client_settings_watcher.h>
#include <client/client_show_once_settings.h>
#include <client/client_autorun_watcher.h>

#include <cloud/cloud_connection.h>

#include <core/resource/client_camera.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/client_camera_factory.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/resource/resource_directory_browser.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_runtime_data.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/dataprovider/data_provider_factory.h>

#include <decoders/video/abstract_video_decoder.h>

#include <finders/systems_finder.h>
#include <nx/vms/discovery/manager.h>

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/cloud/tunnel/outgoing_tunnel_pool.h>
#include <nx/network/socket_global.h>
#include <nx/network/http/http_mod_manager.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/utils/app_info.h>
#include <nx_ec/dummy_handler.h>
#include <nx/cloud/vms_gateway/vms_gateway_embeddable.h>

#include <platform/platform_abstraction.h>

#include <plugins/resource/desktop_camera/desktop_resource_searcher.h>
#include <plugins/resource/desktop_audio_only/desktop_audio_only_resource.h>
#if defined(Q_OS_WIN)
    #include <plugins/resource/desktop_win/desktop_resource.h>
#endif

#include <core/storage/file_storage/qtfile_storage_resource.h>
#include <core/storage/file_storage/layout_storage_resource.h>

#include <nx/vms/client/desktop/analytics/camera_metadata_analytics_controller.h>
#include <nx/vms/client/desktop/radass/radass_controller.h>

#include <server/server_storage_manager.h>

#include <translation/translation_manager.h>
#include <translation/datetime_formatter.h>

#include <utils/common/app_info.h>
#include <utils/common/command_line_parser.h>

#include <utils/media/voice_spectrum_analyzer.h>
#include <nx/vms/client/desktop/utils/performance_test.h>
#include <watchers/server_interface_watcher.h>
#include <nx/client/core/watchers/known_server_connections.h>
#include <nx/client/core/utils/font_loader.h>
#include <nx/vms/client/desktop/utils/applauncher_guard.h>
#include <nx/vms/client/desktop/utils/resource_widget_pixmap_cache.h>
#include <nx/vms/client/desktop/analytics/analytics_metadata_provider_factory.h>
#include <nx/vms/client/desktop/utils/upload_manager.h>
#include <nx/vms/client/desktop/utils/wearable_manager.h>
#include <nx/vms/client/desktop/analytics/object_display_settings.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/system_health/system_internet_access_watcher.h>

#include <statistics/statistics_manager.h>
#include <statistics/storage/statistics_file_storage.h>
#include <statistics/settings/statistics_settings_watcher.h>

#include <ui/customization/customization.h>
#include <ui/customization/customizer.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>

#ifdef Q_OS_WIN
#include <ui/workaround/iexplore_url_handler.h>
#endif
#include <ui/workaround/qtbug_workaround.h>

#ifdef Q_OS_MAC
#include <ui/workaround/mac_utils.h>
#endif

#include <watchers/cloud_status_watcher.h>

#if defined(Q_OS_WIN)
#include <plugins/resource/desktop_win/desktop_resource_searcher_impl.h>
#else
#include <plugins/resource/desktop_audio_only/desktop_audio_only_resource_searcher_impl.h>
#endif

#include <nx/vms/client/desktop/ini.h>

using namespace nx;
using namespace nx::vms::client::desktop;

static const QString kQmlRoot = QStringLiteral("qrc:///qml");

namespace {

typedef std::unique_ptr<QnTranslationManager> QnTranslationManagerPtr;

QnTranslationManagerPtr initializeTranslations(QnClientSettings* settings)
{
    QnTranslationManagerPtr translationManager(new QnTranslationManager());
    translationManager->addPrefix("client_base");
    translationManager->addPrefix("client_ui");
    translationManager->addPrefix("client_core");
    translationManager->addPrefix("client_qml");

    QnTranslation translation;
    if (translation.isEmpty()) /* By path. */
        translation = translationManager->loadTranslation(settings->locale());

    /* Check if qnSettings value is invalid. */
    if (translation.isEmpty())
        translation = translationManager->defaultTranslation();

    translationManager->installTranslation(translation);

    // It is now safe to localize time and date formats. Mind the dot.
    datetime::initLocale();

    return translationManager;
}

void initializeStatisticsManager(QnCommonModule* commonModule)
{
    const auto statManager = commonModule->instance<QnStatisticsManager>();

    statManager->setClientId(qnSettings->pcUuid());
    statManager->setStorage(QnStatisticsStoragePtr(new QnStatisticsFileStorage()));
    statManager->setSettings(QnStatisticsLoaderPtr(new QnStatisticsSettingsWatcher()));
}

QString calculateLogNameSuffix(const QnStartupParameters& startupParams)
{
    if (!startupParams.videoWallGuid.isNull())
    {
        QString result = startupParams.videoWallItemGuid.isNull()
            ? startupParams.videoWallGuid.toString()
            : startupParams.videoWallItemGuid.toString();
        result.replace(QRegExp(QLatin1String("[{}]")), QLatin1String("_"));
        return result;
    }

    // We hope self-updater will run only once per time and will not overflow log-file because
    // qnClientInstanceManager is not initialized in self-update mode.
    if (startupParams.selfUpdateMode)
        return "self_update";

    if (startupParams.acsMode)
        return "ax";

    if (qnClientInstanceManager && qnClientInstanceManager->isValid())
    {
        const int idx = qnClientInstanceManager->instanceIndex();
        if (idx > 0)
            return L'_' + QString::number(idx);
    }

    return QString();
}

} // namespace

QnClientModule::QnClientModule(const QnStartupParameters& startupParams, QObject* parent):
    QObject(parent),
    m_startupParameters(startupParams)
{
    ini().reload();

    initThread();
    initMetaInfo();
    initApplication();
    initSingletons();

    /* Do not initialize anything else because we must exit immediately if run in self-update mode. */
    if (startupParams.selfUpdateMode)
        return;

    initNetwork();
    initSkin();
    initLocalResources();
    initSurfaceFormat();

    // WebKit initialization must occur only once per application run. Actual for ActiveX module.
    static bool isWebKitInitialized = false;
    if (!isWebKitInitialized)
    {
        const auto settings = QWebSettings::globalSettings();
        settings->setAttribute(QWebSettings::PluginsEnabled, ini().enableWebKitPlugins);
        settings->enablePersistentStorage();

        if (ini().enableWebKitDeveloperExtras)
            settings->setAttribute(QWebSettings::DeveloperExtrasEnabled, true);

        isWebKitInitialized = true;
    }
}

QnClientModule::~QnClientModule()
{
    // Discovery manager may not exist in self-update mode.
    if (auto discoveryManager = m_clientCoreModule->commonModule()->resourceDiscoveryManager())
        discoveryManager->stop();

    // Stop all long runnables before deinitializing singletons. Pool may not exist in update mode.
    if (auto longRunnablePool = QnLongRunnablePool::instance())
        longRunnablePool->stopAll();

    m_clientCoreModule->commonModule()->resourcePool()->threadPool()->waitForDone();

    m_networkProxyFactory = nullptr; // Object will be deleted by QNetworkProxyFactory
    QNetworkProxyFactory::setApplicationProxyFactory(nullptr);

    QApplication::setOrganizationName(QString());
    QApplication::setApplicationName(QString());
    QApplication::setApplicationDisplayName(QString());
    QApplication::setApplicationVersion(QString());

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
    QApplication::setOrganizationName(QnAppInfo::organizationName());
    QApplication::setApplicationName(QnClientAppInfo::applicationName());
    QApplication::setApplicationDisplayName(QnClientAppInfo::applicationDisplayName());

    const QString applicationVersion = QnClientAppInfo::metaVersion().isEmpty()
        ? QnAppInfo::applicationVersion()
        : QString("%1 %2").arg(QnAppInfo::applicationVersion(), QnClientAppInfo::metaVersion());

    QApplication::setApplicationVersion(applicationVersion);
    QApplication::setStartDragDistance(20);

    /* We don't want changes in desktop color settings to mess up our custom style. */
    QApplication::setDesktopSettingsAware(false);
    QApplication::setQuitOnLastWindowClosed(true);

    if (nx::utils::AppInfo::isMacOsX())
        QApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
}

void QnClientModule::initDesktopCamera(QGLWidget* window)
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
    commonModule->resourceDiscoveryManager()->addDeviceServer(desktopSearcher);
}

void QnClientModule::startLocalSearchers()
{
    auto commonModule = m_clientCoreModule->commonModule();
    commonModule->resourceDiscoveryManager()->start();
}

QnNetworkProxyFactory* QnClientModule::networkProxyFactory() const
{
    return m_networkProxyFactory;
}

void QnClientModule::initMetaInfo()
{
    Q_INIT_RESOURCE(nx_vms_client_desktop);
    QnClientMetaTypes::initialize();
}

void QnClientModule::initSurfaceFormat()
{
    QSurfaceFormat format;

    if (qnRuntime->lightMode().testFlag(Qn::LightModeNoMultisampling))
        format.setSamples(2);
    format.setSwapBehavior(qnSettings->isGlDoubleBuffer()
        ? QSurfaceFormat::DoubleBuffer
        : QSurfaceFormat::SingleBuffer);
    format.setSwapInterval(qnRuntime->isVSyncEnabled() ? 1 : 0);

    QSurfaceFormat::setDefaultFormat(format);
    QGLFormat::setDefaultFormat(QGLFormat::fromSurfaceFormat(format));
}

void QnClientModule::initSingletons()
{
    // Persistent settings are standalone.
    auto clientSettings = std::make_unique<QnClientSettings>(m_startupParameters);

#ifdef Q_OS_WIN
    // As soon as settings are ready, setup full crash dumps if needed.
    win32_exception::setCreateFullCrashDump(clientSettings->createFullCrashDump());
#endif

    // Runtime settings are standalone, but some actual values are taken from persistent settings.
    auto clientRuntimeSettings = std::make_unique<QnClientRuntimeSettings>(m_startupParameters);
    clientRuntimeSettings->setGLDoubleBuffer(clientSettings->isGlDoubleBuffer());
    clientRuntimeSettings->setLocale(clientSettings->locale());

    // Depends on QnClientSettings.
    auto clientInstanceManager = std::make_unique<QnClientInstanceManager>();

    // Log initialization depends on QnClientInstanceManager.
    initLog();

    const auto clientPeerType = m_startupParameters.videoWallGuid.isNull()
        ? nx::vms::api::PeerType::desktopClient
        : nx::vms::api::PeerType::videowallClient;
    const auto brand = m_startupParameters.isDevMode() ? QString() : QnAppInfo::productNameShort();
    const auto customization = m_startupParameters.isDevMode()
        ? QString()
        : QnAppInfo::customizationName();

    m_staticCommon.reset(new QnStaticCommonModule(
        clientPeerType,
        brand,
        customization,
        ini().cloudHost));

    m_clientCoreModule.reset(new QnClientCoreModule());

    // Settings migration depends on client core settings.
    nx::vms::client::desktop::settings::migrate();

    // We should load translations before major client's services are started to prevent races
    QnTranslationManagerPtr translationManager(initializeTranslations(clientSettings.get()));

    auto commonModule = m_clientCoreModule->commonModule();
    commonModule->setModuleGUID(clientInstanceManager->instanceGuid());

    commonModule->store(new QnResourceRuntimeDataManager(commonModule));

    // Pass ownership to the common module.
    commonModule->store(translationManager.release());
    commonModule->store(clientRuntimeSettings.release());
    commonModule->store(clientSettings.release());
    commonModule->store(clientInstanceManager.release());

    initRuntimeParams(commonModule, m_startupParameters);

    // Shortened initialization if run in self-update mode.
    if (m_startupParameters.selfUpdateMode)
        return;

    commonModule->store(new ApplauncherGuard(commonModule));

    // Depends on nothing.
    commonModule->store(new QnClientShowOnceSettings());

    // Depends on QnClientSettings, QnClientInstanceManager and QnClientShowOnceSettings, is never
    // used directly.
    commonModule->store(new QnClientSettingsWatcher());

    // Depends on QnClientSettings, is never used directly.
    commonModule->store(new QnClientAutoRunWatcher());

    nx::network::SocketGlobals::cloud().outgoingTunnelPool()
        .assignOwnPeerId("dc", commonModule->moduleGUID());

    commonModule->store(new QnGlobals());

    m_radassController = commonModule->store(new RadassController());
    commonModule->store(new MetadataAnalyticsController());

    commonModule->store(new QnPlatformAbstraction());

    commonModule->createMessageProcessor<QnDesktopClientMessageProcessor>();
    commonModule->store(new QnClientResourceFactory());

    commonModule->store(new QnCameraBookmarksManager());
    commonModule->store(new QnServerStorageManager());
    commonModule->instance<QnLayoutTourManager>();

    commonModule->store(new QnVoiceSpectrumAnalyzer());

    commonModule->store(new ResourceWidgetPixmapCache());

    // Must be called before QnCloudStatusWatcher but after setModuleGUID() call.
    initLocalInfo();

    initializeStatisticsManager(commonModule);

    /* Long runnables depend on QnCameraHistoryPool and other singletons. */
    commonModule->store(new QnLongRunnablePool());

    commonModule->store(new QnCloudConnectionProvider());
    m_cloudStatusWatcher = commonModule->store(
        new QnCloudStatusWatcher(commonModule, /*isMobile*/ false));

    //NOTE:: QNetworkProxyFactory::setApplicationProxyFactory takes ownership of object
    m_networkProxyFactory = new QnNetworkProxyFactory(commonModule);
    QNetworkProxyFactory::setApplicationProxyFactory(m_networkProxyFactory);

    m_uploadManager = new UploadManager(commonModule);
    m_wearableManager = new WearableManager(commonModule);

    commonModule->store(m_uploadManager);
    commonModule->store(m_wearableManager);

#ifdef Q_OS_WIN
    commonModule->store(new QnIexploreUrlHandler());
#endif

    commonModule->store(new QnQtbugWorkaround());

    commonModule->store(new nx::cloud::gateway::VmsGatewayEmbeddable(true));

    m_cameraDataManager = commonModule->store(new QnCameraDataManager(commonModule));

    commonModule->store(new ObjectDisplaySettings());
    commonModule->store(new SystemInternetAccessWatcher(commonModule));
    commonModule->findInstance<nx::vms::client::core::watchers::KnownServerConnections>()->start();

    m_analyticsMetadataProviderFactory.reset(new AnalyticsMetadataProviderFactory());
    m_analyticsMetadataProviderFactory->registerMetadataProviders();

    registerResourceDataProviders();
}

void QnClientModule::initRuntimeParams(
    QnCommonModule* commonModule,
    const QnStartupParameters& startupParams)
{
    if (!m_startupParameters.engineVersion.isEmpty())
    {
        nx::utils::SoftwareVersion version(m_startupParameters.engineVersion);
        if (!version.isNull())
        {
            qWarning() << "Starting with overridden version: " << version.toString();
            commonModule->setEngineVersion(version);
        }
    }

    if (qnRuntime->lightModeOverride() != -1)
        PerformanceTest::detectLightMode();

#ifdef Q_OS_MACX
    if (mac_isSandboxed())
        qnRuntime->setLightMode(qnRuntime->lightMode() | Qn::LightModeNoNewWindow);
#endif

    auto qmlRoot = m_startupParameters.qmlRoot.isEmpty() ? kQmlRoot : m_startupParameters.qmlRoot;
    if (!qmlRoot.endsWith(L'/'))
        qmlRoot.append(L'/');
    NX_INFO(this, lm("Setting QML root to %1").arg(qmlRoot));

    m_clientCoreModule->mainQmlEngine()->setBaseUrl(
        qmlRoot.startsWith("qrc:")
            ? QUrl(qmlRoot)
            : QUrl::fromLocalFile(qmlRoot));
    m_clientCoreModule->mainQmlEngine()->addImportPath(qmlRoot);
}

void QnClientModule::initLog()
{
    using namespace nx::utils::log;

    const auto logFileNameSuffix = calculateLogNameSuffix(m_startupParameters);

    static const QString kLogConfig("desktop_client_log.ini");
    const QDir iniFilesDir(nx::kit::IniConfig::iniFilesDir());
    const QString logConfigFile(iniFilesDir.absoluteFilePath(kLogConfig));
    if (QFileInfo(logConfigFile).exists())
    {
        NX_ALWAYS(this, "Log is initialized from the %1", logConfigFile);
        NX_ALWAYS(this, "Log options from settings are ignored!");
        QSettings logConfig(logConfigFile, QSettings::IniFormat);
        Settings logSettings(&logConfig);
        logSettings.updateDirectoryIfEmpty(
            QStandardPaths::writableLocation(QStandardPaths::DataLocation));
        for (auto& logger: logSettings.loggers)
        {
            if (const auto target = logger.logBaseName; target != '-')
                logger.logBaseName = target + logFileNameSuffix;
        }

        setMainLogger(
            buildLogger(logSettings, qApp->applicationName(),qApp->applicationFilePath()));
    }
    else
    {
        QSettings rawSettings;
        const auto maxBackupCount = rawSettings.value("logArchiveSize", 10).toUInt();
        const auto maxFileSize = rawSettings.value("maxLogFileSize", 10 * 1024 * 1024).toUInt();

        auto logLevel = m_startupParameters.logLevel;
        auto logFile = m_startupParameters.logFile;

        if (logLevel.isEmpty())
            logLevel = qnSettings->logLevel();

        Settings logSettings;
        logSettings.loggers.resize(1);
        auto& logger = logSettings.loggers.front();
        logger.maxBackupCount = maxBackupCount;
        logger.maxFileSize = maxFileSize;
        logger.level.parse(logLevel);
        logger.logBaseName = logFile.isEmpty()
            ? ("client_log" + logFileNameSuffix)
            : logFile;

        setMainLogger(
            buildLogger(logSettings, qApp->applicationName(), qApp->applicationFilePath()));
    }

    nx::utils::enableQtMessageAsserts();
}

void QnClientModule::initNetwork()
{
    auto commonModule = m_clientCoreModule->commonModule();

    //TODO #ak get rid of this class!
    commonModule->store(new ec2::DummyHandler());
    if (!m_startupParameters.enforceSocketType.isEmpty())
        nx::network::SocketFactory::enforceStreamSocketType(m_startupParameters.enforceSocketType);

    if (!m_startupParameters.enforceMediatorEndpoint.isEmpty())
    {
        nx::network::SocketGlobals::cloud().mediatorConnector().mockupMediatorAddress({
            m_startupParameters.enforceMediatorEndpoint,
            m_startupParameters.enforceMediatorEndpoint});
    }

    if (!m_startupParameters.videoWallGuid.isNull())
        commonModule->setVideowallGuid(m_startupParameters.videoWallGuid);

    commonModule->moduleDiscoveryManager()->start();

    commonModule->instance<QnSystemsFinder>();
    commonModule->store(new QnForgottenSystemsManager());

    // Depends on qnSystemsFinder
    commonModule->store(new QnStartupTileManager());
    commonModule->instance<QnServerInterfaceWatcher>();
}

//#define ENABLE_DYNAMIC_CUSTOMIZATION
void QnClientModule::initSkin()
{
    QStringList paths;
    paths << ":/skin";
    paths << ":/skin_dark";

#ifdef ENABLE_DYNAMIC_CUSTOMIZATION
    if (!m_startupParameters.dynamicCustomizationPath.isEmpty())
    {
        QDir base(startupParams.dynamicCustomizationPath);
        paths << base.absoluteFilePath("skin");
        paths << base.absoluteFilePath("skin_dark");
    }
#endif // ENABLE_DYNAMIC_CUSTOMIZATION

    QScopedPointer<QnSkin> skin(new QnSkin(paths));

    QnCustomization customization;
    customization.add(QnCustomization(skin->path("customization_common.json")));
    customization.add(QnCustomization(skin->path("skin.json")));

    QScopedPointer<QnCustomizer> customizer(new QnCustomizer(customization));
    customizer->customize(qnGlobals);

    /* Initialize application UI. Skip if run in console (e.g. unit-tests). */
    if (qApp)
    {
        nx::vms::client::core::FontLoader::loadFonts(
            QDir(QApplication::applicationDirPath()).absoluteFilePath(
                nx::utils::AppInfo::isMacOsX() ? "../Resources/fonts" : "fonts"));

        QApplication::setWindowIcon(qnSkin->icon(":/logo.png"));
        QApplication::setStyle(skin->newStyle(customizer->genericPalette()));
    }

    auto commonModule = m_clientCoreModule->commonModule();
    commonModule->store(skin.take());
    commonModule->store(customizer.take());
    commonModule->store(new ColorTheme());
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

    if (!m_startupParameters.skipMediaFolderScan)
    {
        auto localFilesSearcher = commonModule->store(new QnResourceDirectoryBrowser());

        localFilesSearcher->setLocal(true);
        QStringList dirs;
        dirs << qnSettings->mediaFolder();
        dirs << qnSettings->extraMediaFolders();
        localFilesSearcher->setPathCheckList(dirs);
        resourceDiscoveryManager->addDeviceServer(localFilesSearcher);
    }
    resourceDiscoveryManager->setReady(true);
    commonModule->store(new QnSystemsWeightsManager());
}

QnCloudStatusWatcher* QnClientModule::cloudStatusWatcher() const
{
    return m_cloudStatusWatcher;
}

QnCameraDataManager* QnClientModule::cameraDataManager() const
{
    return m_cameraDataManager;
}

QnClientCoreModule* QnClientModule::clientCoreModule() const
{
    return m_clientCoreModule.data();
}

RadassController* QnClientModule::radassController() const
{
    return m_radassController;
}

QnStartupParameters QnClientModule::startupParameters() const
{
    return m_startupParameters;
}

UploadManager* QnClientModule::uploadManager() const
{
    return m_uploadManager;
}

WearableManager* QnClientModule::wearableManager() const
{
    return m_wearableManager;
}

void QnClientModule::initLocalInfo()
{
    auto commonModule = m_clientCoreModule->commonModule();

    vms::api::PeerType clientPeerType = m_startupParameters.videoWallGuid.isNull()
        ? vms::api::PeerType::desktopClient
        : vms::api::PeerType::videowallClient;

    nx::vms::api::RuntimeData runtimeData;
    runtimeData.peer.id = commonModule->moduleGUID();
    runtimeData.peer.instanceId = commonModule->runningInstanceGUID();
    runtimeData.peer.peerType = clientPeerType;
    runtimeData.brand = qnRuntime->isDevMode() ? QString() : QnAppInfo::productNameShort();
    runtimeData.customization = qnRuntime->isDevMode() ? QString() : QnAppInfo::customizationName();
    runtimeData.videoWallInstanceGuid = m_startupParameters.videoWallItemGuid;
    commonModule->runtimeInfoManager()->updateLocalItem(runtimeData); // initializing localInfo
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
