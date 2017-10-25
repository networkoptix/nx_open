#include "client_module.h"

#include <memory>

#include <QtWidgets/QApplication>
#include <QtWebKit/QWebSettings>

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

#include <camera/video_decoder_factory.h>

#include <cloud/cloud_connection.h>

#include <core/resource/client_camera_factory.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/resource/resource_directory_browser.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_runtime_data.h>
#include <core/resource_management/layout_tour_manager.h>

#include <decoders/video/abstract_video_decoder.h>

#include <finders/systems_finder.h>
#include <nx/vms/discovery/manager.h>

#include <nx/network/socket_global.h>
#include <nx/network/http/http_mod_manager.h>
#include <vms_gateway_embeddable.h>
#include <nx/utils/log/log_initializer.h>
#include <nx_ec/dummy_handler.h>

#include <platform/platform_abstraction.h>

#include <plugins/plugin_manager.h>
#include <plugins/resource/desktop_camera/desktop_resource_searcher.h>
#include <plugins/storage/file_storage/qtfile_storage_resource.h>
#include <plugins/storage/file_storage/layout_storage_resource.h>

#include <nx/client/desktop/analytics/camera_metadata_analytics_controller.h>
#include <nx/client/desktop/radass/radass_controller.h>

#include <server/server_storage_manager.h>

#include <translation/translation_manager.h>

#include <utils/common/app_info.h>
#include <utils/common/command_line_parser.h>

#include <utils/media/voice_spectrum_analyzer.h>
#include <utils/performance_test.h>
#include <utils/server_interface_watcher.h>
#include <nx/client/core/watchers/known_server_connections.h>
#include <nx/client/desktop/utils/applauncher_guard.h>
#include <nx/client/desktop/layout_templates/layout_template_manager.h>

#include <statistics/statistics_manager.h>
#include <statistics/storage/statistics_file_storage.h>
#include <statistics/settings/statistics_settings_watcher.h>

#include <ui/helpers/font_loader.h>
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

#include <ini.h>

using namespace nx::client::desktop;

static QtMessageHandler defaultMsgHandler = 0;

static void myMsgHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    if (defaultMsgHandler)
    {
        defaultMsgHandler(type, ctx, msg);
    }
    else
    { /* Default message handler. */
#ifndef QN_NO_STDERR_MESSAGE_OUTPUT
        QTextStream err(stderr);
        err << msg << endl << flush;
#endif
    }

    NX_EXPECT(!msg.contains(lit("QString:")), msg);
    NX_EXPECT(!msg.contains(lit("QObject:")), msg);
    qnLogMsgHandler(type, ctx, msg);
}


namespace
{
    typedef std::unique_ptr<QnTranslationManager> QnTranslationManagerPtr;

    QnTranslationManagerPtr initializeTranslations(QnClientSettings* settings)
    {
        QnTranslationManagerPtr translationManager(new QnTranslationManager());
        translationManager->addPrefix(lit("client_base"));
        translationManager->addPrefix(lit("client_ui"));
        translationManager->addPrefix(lit("client_core"));
        translationManager->addPrefix(lit("client_qml"));

        QnTranslation translation;
        if (translation.isEmpty()) /* By path. */
            translation = translationManager->loadTranslation(settings->locale());

        /* Check if qnSettings value is invalid. */
        if (translation.isEmpty())
            translation = translationManager->defaultTranslation();

        translationManager->installTranslation(translation);
        return translationManager;
    }

    void initializeStatisticsManager(QnCommonModule *commonModule)
    {
        const auto statManager = commonModule->instance<QnStatisticsManager>();

        statManager->setClientId(qnSettings->pcUuid());
        statManager->setStorage(QnStatisticsStoragePtr(new QnStatisticsFileStorage()));
        statManager->setSettings(QnStatisticsLoaderPtr(new QnStatisticsSettingsWatcher()));
    }
}

QnClientModule::QnClientModule(const QnStartupParameters& startupParams, QObject* parent):
    QObject(parent)
{
    ini().reload();

    initThread();
    initMetaInfo();
    initApplication();
    initSingletons(startupParams);
    initRuntimeParams(startupParams);
    initLog(startupParams);

    /* Do not initialize anything else because we must exit immediately if run in self-update mode. */
    if (startupParams.selfUpdateMode)
        return;

    initNetwork(startupParams);
    initSkin(startupParams);
    initLocalResources(startupParams);

    // WebKit initialization must occur only once per application run. Actual for ActiveX module.
    static bool isWebKitInitialized = false;
    if (!isWebKitInitialized)
    {
        QWebSettings::globalSettings()->setAttribute(QWebSettings::PluginsEnabled, true);
        QWebSettings::globalSettings()->enablePersistentStorage();
        isWebKitInitialized = true;
    }
}

QnClientModule::~QnClientModule()
{
    // Stop all long runnables before deinitializing singletons. Pool may not exist in update mode.
    if (auto longRunnablePool = QnLongRunnablePool::instance())
        longRunnablePool->stopAll();

    QnResource::stopAsyncTasks();

    m_networkProxyFactory = nullptr; // Object will be deleted by QNetworkProxyFactory
    QNetworkProxyFactory::setApplicationProxyFactory(nullptr);

    QApplication::setOrganizationName(QString());
    QApplication::setApplicationName(QString());
    QApplication::setApplicationDisplayName(QString());
    QApplication::setApplicationVersion(QString());

    //restoring default message handler
    qInstallMessageHandler(defaultMsgHandler);

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
    if (QApplication::applicationVersion().isEmpty())
        QApplication::setApplicationVersion(QnAppInfo::applicationVersion());
    QApplication::setStartDragDistance(20);

    /* We don't want changes in desktop color settings to mess up our custom style. */
    QApplication::setDesktopSettingsAware(false);
    QApplication::setQuitOnLastWindowClosed(true);

#ifdef Q_OS_MACX
    QApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
#endif
}

void QnClientModule::initDesktopCamera(QGLWidget* window)
{
    /* Initialize desktop camera searcher. */
    auto commonModule = m_clientCoreModule->commonModule();
    auto desktopSearcher = commonModule->store(new QnDesktopResourceSearcher(window));
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
    Q_INIT_RESOURCE(nx_client_desktop);
    QnClientMetaTypes::initialize();
}

void QnClientModule::initSingletons(const QnStartupParameters& startupParams)
{
    Qn::PeerType clientPeerType = startupParams.videoWallGuid.isNull()
        ? Qn::PT_DesktopClient
        : Qn::PT_VideowallClient;
    const auto brand = startupParams.isDevMode() ? QString() : QnAppInfo::productNameShort();
    const auto customization = startupParams.isDevMode() ? QString() : QnAppInfo::customizationName();

    m_staticCommon.reset(new QnStaticCommonModule(clientPeerType, brand, customization));

    m_clientCoreModule.reset(new QnClientCoreModule());

    /* Just to feel safe */
    QScopedPointer<QnClientSettings> clientSettingsPtr(new QnClientSettings(startupParams.forceLocalSettings));
    QnClientSettings* clientSettings = clientSettingsPtr.data();

    /* Init crash dumps as early as possible. */
#ifdef Q_OS_WIN
    win32_exception::setCreateFullCrashDump(clientSettings->createFullCrashDump());
#endif

    /// We should load translations before major client's services are started to prevent races
    QnTranslationManagerPtr translationManager(initializeTranslations(clientSettings));

    /* Init singletons. */

    auto commonModule = m_clientCoreModule->commonModule();

    commonModule->store(new QnResourceRuntimeDataManager());
    commonModule->store(translationManager.release());
    commonModule->store(new QnClientRuntimeSettings());
    commonModule->store(clientSettingsPtr.take()); /* Now common owns the link. */

    /* Shorted initialization if run in self-update mode. */
    if (startupParams.selfUpdateMode)
        return;

    commonModule->store(new ApplauncherGuard());

    /* Depends on QnClientSettings. */
    auto clientInstanceManager = commonModule->store(new QnClientInstanceManager());

    /* Depends on nothing. */
    commonModule->store(new QnClientShowOnceSettings());

    /* Depends on QnClientSettings, QnClientInstanceManager and QnClientShowOnceSettings, never used directly. */
    commonModule->store(new QnClientSettingsWatcher());

    /* Depends on QnClientSettings, never used directly. */
    commonModule->store(new QnClientAutoRunWatcher());

    commonModule->setModuleGUID(clientInstanceManager->instanceGuid());
    nx::network::SocketGlobals::outgoingTunnelPool()
        .assignOwnPeerId("dc", commonModule->moduleGUID());

    commonModule->store(new QnGlobals());

    m_radassController = commonModule->store(new RadassController());
    commonModule->store(new nx::client::desktop::MetadataAnalyticsController());

    commonModule->store(new QnPlatformAbstraction());

    commonModule->createMessageProcessor<QnDesktopClientMessageProcessor>();
    commonModule->store(new QnClientResourceFactory());

    commonModule->store(new QnCameraBookmarksManager());
    commonModule->store(new QnServerStorageManager());
    commonModule->instance<QnLayoutTourManager>();

    commonModule->store(new QnVoiceSpectrumAnalyzer());

    // Must be called before QnCloudStatusWatcher but after setModuleGUID() call.
    initLocalInfo(startupParams);

    initializeStatisticsManager(commonModule);

    /* Long runnables depend on QnCameraHistoryPool and other singletons. */
    commonModule->store(new QnLongRunnablePool());

    /* Just to feel safe */
    commonModule->store(new QnCloudConnectionProvider());
    m_cloudStatusWatcher = commonModule->store(new QnCloudStatusWatcher(commonModule, /*isMobile*/ false));

    //NOTE:: QNetworkProxyFactory::setApplicationProxyFactory takes ownership of object
    m_networkProxyFactory = new QnNetworkProxyFactory(commonModule);
    QNetworkProxyFactory::setApplicationProxyFactory(m_networkProxyFactory);

#ifdef Q_OS_WIN
    commonModule->store(new QnIexploreUrlHandler());
#endif

    commonModule->store(new QnQtbugWorkaround());
    commonModule->store(new nx::cloud::gateway::VmsGatewayEmbeddable(true));

    m_cameraDataManager = commonModule->store(new QnCameraDataManager(commonModule));

    commonModule->store(new LayoutTemplateManager());

    commonModule->findInstance<nx::client::core::watchers::KnownServerConnections>()->start();
}

void QnClientModule::initRuntimeParams(const QnStartupParameters& startupParams)
{
    qnRuntime->setDevMode(startupParams.isDevMode());
    qnRuntime->setGLDoubleBuffer(qnSettings->isGlDoubleBuffer());
    qnRuntime->setLocale(qnSettings->locale());
    qnRuntime->setSoftwareYuv(startupParams.softwareYuv);
    qnRuntime->setShowFullInfo(startupParams.showFullInfo);
    qnRuntime->setIgnoreVersionMismatch(startupParams.ignoreVersionMismatch);
    qnRuntime->setProfilerMode(startupParams.profilerMode);

    if (!startupParams.engineVersion.isEmpty())
    {
        QnSoftwareVersion version(startupParams.engineVersion);
        if (!version.isNull())
        {
            qWarning() << "Starting with overridden version: " << version.toString();
            qnStaticCommon->setEngineVersion(version);
        }
    }

    // TODO: #GDM Should we always overwrite persistent setting with command-line values? o_O
    qnSettings->setVSyncEnabled(!startupParams.vsyncDisabled);
    qnSettings->setClientUpdateDisabled(startupParams.clientUpdateDisabled);

    // TODO: #Elric why QString???
    if (!startupParams.lightMode.isEmpty())
    {
        bool ok = false;
        Qn::LightModeFlags lightModeOverride(startupParams.lightMode.toInt(&ok));
        qnRuntime->setLightModeOverride(ok ? lightModeOverride : Qn::LightModeFull);
    }

    if (!startupParams.videoWallGuid.isNull())
    {
        qnRuntime->setVideoWallMode(true);
        qnRuntime->setIgnoreVersionMismatch(true);
        qnRuntime->setLightModeOverride(Qn::LightModeVideoWall);
    }

    // TODO: #GDM fix it
    /* Here the value from LightModeOverride will be copied to LightMode */
#if !defined(__arm__) && !defined(__aarch64__)
    QnPerformanceTest::detectLightMode();
#else
    // TODO: On NVidia TX1 this call leads to segfault in next QGLWidget
    //       constructor call. Need to find the way to work it around.
#endif


#ifdef Q_OS_MACX
    if (mac_isSandboxed())
        qnSettings->setLightMode(qnSettings->lightMode() | Qn::LightModeNoNewWindow);
#endif
}

void QnClientModule::initLog(const QnStartupParameters& startupParams)
{
    auto logLevel = startupParams.logLevel;
    auto ec2TranLogLevel = startupParams.ec2TranLogLevel;

    QString logFileNameSuffix;
    if (!startupParams.videoWallGuid.isNull())
    {
        logFileNameSuffix = startupParams.videoWallItemGuid.isNull()
            ? startupParams.videoWallGuid.toString()
            : startupParams.videoWallItemGuid.toString();
        logFileNameSuffix.replace(QRegExp(QLatin1String("[{}]")), QLatin1String("_"));
    }
    else if (startupParams.selfUpdateMode)
    {
        // we hope self-updater will run only once per time and will not overflow log-file
        // qnClientInstanceManager is not initialized in self-update mode
        logFileNameSuffix = lit("self_update");
    }
    else if (qnRuntime->isActiveXMode())
    {
        logFileNameSuffix = lit("ax");
    }
    else if (qnClientInstanceManager && qnClientInstanceManager->isValid())
    {
        int idx = qnClientInstanceManager->instanceIndex();
        if (idx > 0)
            logFileNameSuffix = L'_' + QString::number(idx) + L'_';
    }

    if (logLevel.isEmpty())
        logLevel = qnSettings->logLevel();

    nx::utils::log::Settings logSettings;
    logSettings.level = nx::utils::log::levelFromString(logLevel);
    logSettings.maxFileSize = 10 * 1024 * 1024;
    logSettings.maxBackupCount = 5;
    logSettings.updateDirectoryIfEmpty(QStandardPaths::writableLocation(QStandardPaths::DataLocation));

    nx::utils::log::initialize(
        logSettings,
        qApp->applicationName(),
        qApp->applicationFilePath(),
        lit("log_file") + logFileNameSuffix);

    const auto ec2logger = nx::utils::log::addLogger({QnLog::EC2_TRAN_LOG});
    if (ec2TranLogLevel != lit("none"))
    {
        logSettings.level = nx::utils::log::levelFromString(ec2TranLogLevel);
        nx::utils::log::initialize(
            logSettings,
            qApp->applicationName(),
            qApp->applicationFilePath(),
            lit("ec2_tran") + logFileNameSuffix,
            ec2logger);
    }

    {
        // TODO: #dklychkov #3.1 or #3.2 Remove this block when log filters are implemented.
        const auto logger = nx::utils::log::addLogger({lit("DecodedPictureToOpenGLUploader")});
        logger->setDefaultLevel(nx::utils::log::Level::info);
    }

    defaultMsgHandler = qInstallMessageHandler(myMsgHandler);
}

void QnClientModule::initNetwork(const QnStartupParameters& startupParams)
{
    auto commonModule = m_clientCoreModule->commonModule();

    //TODO #ak get rid of this class!
    commonModule->store(new ec2::DummyHandler());
    commonModule->store(new nx_http::HttpModManager());
    if (!startupParams.enforceSocketType.isEmpty())
        SocketFactory::enforceStreamSocketType(startupParams.enforceSocketType);

    if (!startupParams.enforceMediatorEndpoint.isEmpty())
        nx::network::SocketGlobals::mediatorConnector().mockupMediatorUrl(
            startupParams.enforceMediatorEndpoint);

    // TODO: #mu ON/OFF switch in settings?
    nx::network::SocketGlobals::mediatorConnector().enable(true);

    if (!startupParams.videoWallGuid.isNull())
    {
        commonModule->setVideowallGuid(startupParams.videoWallGuid);
        //commonModule->setInstanceGuid(startupParams.videoWallItemGuid);
    }

    commonModule->moduleDiscoveryManager()->start();

    commonModule->instance<QnSystemsFinder>();
    commonModule->store(new QnForgottenSystemsManager());

    // Depends on qnSystemsFinder
    commonModule->store(new QnStartupTileManager());
    commonModule->instance<QnServerInterfaceWatcher>();
}

//#define ENABLE_DYNAMIC_CUSTOMIZATION
void QnClientModule::initSkin(const QnStartupParameters& startupParams)
{
    QStringList paths;
    paths << lit(":/skin");
    paths << lit(":/skin_dark");

#ifdef ENABLE_DYNAMIC_CUSTOMIZATION
    if (!startupParams.dynamicCustomizationPath.isEmpty())
    {
        QDir base(startupParams.dynamicCustomizationPath);
        paths << base.absoluteFilePath(lit("skin"));
        paths << base.absoluteFilePath(lit("skin_dark"));
    }
#else
    Q_UNUSED(startupParams);
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
        QnFontLoader::loadFonts(QDir(QApplication::applicationDirPath()).absoluteFilePath(lit("fonts")));

        // Window icon is taken from 'icons' customization project. Suppress check.
        QApplication::setWindowIcon(qnSkin->icon(":/logo.png")); // _IGNORE_VALIDATION_
        QApplication::setStyle(skin->newStyle(customizer->genericPalette()));
    }

    auto commonModule = m_clientCoreModule->commonModule();
    commonModule->store(skin.take());
    commonModule->store(customizer.take());
}

void QnClientModule::initLocalResources(const QnStartupParameters& startupParams)
{
    auto commonModule = m_clientCoreModule->commonModule();
    // client uses ordinary QT file to access file system
    QnStoragePluginFactory::instance()->registerStoragePlugin(QLatin1String("file"), QnQtFileStorageResource::instance, true);
    QnStoragePluginFactory::instance()->registerStoragePlugin(QLatin1String("qtfile"), QnQtFileStorageResource::instance);
    QnStoragePluginFactory::instance()->registerStoragePlugin(QLatin1String("layout"), QnLayoutFileStorageResource::instance);

    auto pluginManager = commonModule->store(new PluginManager(nullptr));

    QnVideoDecoderFactory::setCodecManufacture(QnVideoDecoderFactory::AUTO);
    QnVideoDecoderFactory::setPluginManager(pluginManager);

    auto resourceProcessor = commonModule->store(new QnClientResourceProcessor());

    auto resourceDiscoveryManager = new QnResourceDiscoveryManager(commonModule);
    commonModule->setResourceDiscoveryManager(resourceDiscoveryManager);
    resourceProcessor->moveToThread(resourceDiscoveryManager);
    resourceDiscoveryManager->setResourceProcessor(resourceProcessor);

    if (!startupParams.skipMediaFolderScan)
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

nx::client::desktop::RadassController* QnClientModule::radassController() const
{
    return m_radassController;
}

void QnClientModule::initLocalInfo(const QnStartupParameters& startupParams)
{
    auto commonModule = m_clientCoreModule->commonModule();

    Qn::PeerType clientPeerType = startupParams.videoWallGuid.isNull()
        ? Qn::PT_DesktopClient
        : Qn::PT_VideowallClient;

    ec2::ApiRuntimeData runtimeData;
    runtimeData.peer.id = commonModule->moduleGUID();
    runtimeData.peer.instanceId = commonModule->runningInstanceGUID();
    runtimeData.peer.peerType = clientPeerType;
    runtimeData.brand = qnRuntime->isDevMode() ? QString() : QnAppInfo::productNameShort();
    runtimeData.customization = qnRuntime->isDevMode() ? QString() : QnAppInfo::customizationName();
    runtimeData.videoWallInstanceGuid = startupParams.videoWallItemGuid;
    commonModule->runtimeInfoManager()->updateLocalItem(runtimeData); // initializing localInfo
}
