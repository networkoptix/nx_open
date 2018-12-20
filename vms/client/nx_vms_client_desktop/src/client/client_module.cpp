#include "client_module.h"

#include <memory>

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
#include <nx/vms/client/desktop/layout_templates/layout_template_manager.h>
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

    if (startupParams.selfUpdateMode)
    {
        // we hope self-updater will run only once per time and will not overflow log-file
        // qnClientInstanceManager is not initialized in self-update mode
        return "self_update";
    }

    if (qnRuntime->isAcsMode())
    {
        return "ax";
    }

    if (qnClientInstanceManager && qnClientInstanceManager->isValid())
    {
        int idx = qnClientInstanceManager->instanceIndex();
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
    initSingletons(startupParams);
    initLog(startupParams);

    /* Do not initialize anything else because we must exit immediately if run in self-update mode. */
    if (startupParams.selfUpdateMode)
        return;

    initNetwork(startupParams);
    initSkin(startupParams);
    initLocalResources(startupParams);
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
    m_clientCoreModule->commonModule()->resourceDiscoveryManager()->stop();

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

#ifdef Q_OS_MACX
    QApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
#endif
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

    if (qnSettings->lightMode().testFlag(Qn::LightModeNoMultisampling))
        format.setSamples(2);
    format.setSwapBehavior(qnSettings->isGlDoubleBuffer()
        ? QSurfaceFormat::DoubleBuffer
        : QSurfaceFormat::SingleBuffer);
    format.setSwapInterval(qnSettings->isVSyncEnabled() ? 1 : 0);

    QSurfaceFormat::setDefaultFormat(format);
    QGLFormat::setDefaultFormat(QGLFormat::fromSurfaceFormat(format));
}

void QnClientModule::initSingletons(const QnStartupParameters& startupParams)
{
    vms::api::PeerType clientPeerType = startupParams.videoWallGuid.isNull()
        ? vms::api::PeerType::desktopClient
        : vms::api::PeerType::videowallClient;
    const auto brand = startupParams.isDevMode() ? QString() : QnAppInfo::productNameShort();
    const auto customization = startupParams.isDevMode() ? QString() : QnAppInfo::customizationName();

    m_staticCommon.reset(new QnStaticCommonModule(
        clientPeerType,
        brand,
        customization,
        QLatin1String(ini().cloudHost)));

    m_clientCoreModule.reset(new QnClientCoreModule());

    /* Just to feel safe */
    QScopedPointer<QnClientSettings> clientSettingsPtr(new QnClientSettings(startupParams.forceLocalSettings));
    QnClientSettings* clientSettings = clientSettingsPtr.data();
    nx::vms::client::desktop::settings::migrate();

    /* Init crash dumps as early as possible. */
#ifdef Q_OS_WIN
    win32_exception::setCreateFullCrashDump(clientSettings->createFullCrashDump());
#endif

    /// We should load translations before major client's services are started to prevent races
    QnTranslationManagerPtr translationManager(initializeTranslations(clientSettings));

    /* Init singletons. */

    auto commonModule = m_clientCoreModule->commonModule();

    commonModule->store(new QnResourceRuntimeDataManager(commonModule));
    commonModule->store(translationManager.release());
    commonModule->store(new QnClientRuntimeSettings());
    commonModule->store(clientSettingsPtr.take()); /* Now common owns the link. */

    initRuntimeParams(startupParams);

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
    nx::network::SocketGlobals::cloud().outgoingTunnelPool()
        .assignOwnPeerId("dc", commonModule->moduleGUID());

    commonModule->store(new QnGlobals());

    m_radassController = commonModule->store(new RadassController());
    commonModule->store(new nx::vms::client::desktop::MetadataAnalyticsController());

    commonModule->store(new QnPlatformAbstraction());

    commonModule->createMessageProcessor<QnDesktopClientMessageProcessor>();
    commonModule->store(new QnClientResourceFactory());

    commonModule->store(new QnCameraBookmarksManager());
    commonModule->store(new QnServerStorageManager());
    commonModule->instance<QnLayoutTourManager>();

    commonModule->store(new QnVoiceSpectrumAnalyzer());

    commonModule->store(new ResourceWidgetPixmapCache());

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

    m_uploadManager = new nx::vms::client::desktop::UploadManager(commonModule);
    m_wearableManager = new nx::vms::client::desktop::WearableManager(commonModule);

    commonModule->store(m_uploadManager);
    commonModule->store(m_wearableManager);

#ifdef Q_OS_WIN
    commonModule->store(new QnIexploreUrlHandler());
#endif

    commonModule->store(new QnQtbugWorkaround());

    commonModule->store(new nx::cloud::gateway::VmsGatewayEmbeddable(true));

    m_cameraDataManager = commonModule->store(new QnCameraDataManager(commonModule));

    commonModule->store(new LayoutTemplateManager());
    commonModule->store(new ObjectDisplaySettings());

    auto internetAccessWatcher = new nx::vms::client::desktop::SystemInternetAccessWatcher(commonModule);
    commonModule->store(internetAccessWatcher);

    commonModule->findInstance<nx::vms::client::core::watchers::KnownServerConnections>()->start();

    m_analyticsMetadataProviderFactory.reset(new AnalyticsMetadataProviderFactory());
    m_analyticsMetadataProviderFactory->registerMetadataProviders();

    registerResourceDataProviders();
}

void QnClientModule::initRuntimeParams(const QnStartupParameters& startupParams)
{
    qnRuntime->setDevMode(startupParams.isDevMode());
    qnRuntime->setGLDoubleBuffer(qnSettings->isGlDoubleBuffer());
    qnRuntime->setLocale(qnSettings->locale());
    qnRuntime->setSoftwareYuv(startupParams.softwareYuv);
    qnRuntime->setShowFullInfo(startupParams.showFullInfo);
    qnRuntime->setProfilerMode(startupParams.profilerMode);

    if (!startupParams.engineVersion.isEmpty())
    {
        nx::utils::SoftwareVersion version(startupParams.engineVersion);
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
        qnRuntime->setLightModeOverride(Qn::LightModeVideoWall);
    }

    if (startupParams.acsMode)
    {
        qnRuntime->setAcsMode(true);
        qnRuntime->setLightModeOverride(Qn::LightModeACS);
    }

    /* Here the value from LightModeOverride will be copied to LightMode */
    PerformanceTest::detectLightMode();

#ifdef Q_OS_MACX
    if (mac_isSandboxed())
        qnSettings->setLightMode(qnSettings->lightMode() | Qn::LightModeNoNewWindow);
#endif

    auto qmlRoot = startupParams.qmlRoot.isEmpty() ? kQmlRoot : startupParams.qmlRoot;
    if (!qmlRoot.endsWith(L'/'))
        qmlRoot.append(L'/');
    NX_INFO(this, lm("Setting QML root to %1").arg(qmlRoot));

    m_clientCoreModule->mainQmlEngine()->setBaseUrl(
        qmlRoot.startsWith("qrc:")
            ? QUrl(qmlRoot)
            : QUrl::fromLocalFile(qmlRoot));
    m_clientCoreModule->mainQmlEngine()->addImportPath(qmlRoot);
}

void QnClientModule::initLog(const QnStartupParameters& startupParams)
{
    auto logLevel = startupParams.logLevel;
    auto logFile = startupParams.logFile;
    auto ec2TranLogLevel = startupParams.ec2TranLogLevel;

    const QString logFileNameSuffix = calculateLogNameSuffix(startupParams);

    if (logLevel.isEmpty())
        logLevel = qnSettings->logLevel();

    if (ec2TranLogLevel.isEmpty())
        ec2TranLogLevel = qnSettings->ec2TranLogLevel();

    nx::utils::log::Settings logSettings;
    logSettings.loggers.resize(1);
    logSettings.loggers.front().maxBackupCount =
        qnSettings->rawSettings()->value("logArchiveSize", 10).toUInt();
    logSettings.loggers.front().maxFileSize =
        qnSettings->rawSettings()->value("maxLogFileSize", 10 * 1024 * 1024).toUInt();
    logSettings.updateDirectoryIfEmpty(
        QStandardPaths::writableLocation(QStandardPaths::DataLocation));

    logSettings.loggers.front().level.parse(logLevel);
    logSettings.loggers.front().logBaseName = logFile.isEmpty()
        ? ("client_log" + logFileNameSuffix)
        : logFile;

    nx::utils::log::setMainLogger(
        nx::utils::log::buildLogger(
            logSettings,
            qApp->applicationName(),
            qApp->applicationFilePath()));

    if (ec2TranLogLevel != "none")
    {
        logSettings.loggers.front().level.parse(ec2TranLogLevel);
        logSettings.loggers.front().logBaseName = "ec2_tran" + logFileNameSuffix;
        nx::utils::log::addLogger(
            nx::utils::log::buildLogger(
                logSettings,
                qApp->applicationName(),
                qApp->applicationFilePath(),
                {QnLog::EC2_TRAN_LOG}));
    }

    {
        // TODO: #dklychkov #3.1 or #3.2 Remove this block when log filters are implemented.
        nx::utils::log::addLogger(
            std::make_unique<nx::utils::log::Logger>(
                std::set<nx::utils::log::Tag>{
                    nx::utils::log::Tag(QStringLiteral("DecodedPictureToOpenGLUploader"))},
                nx::utils::log::Level::info));
    }

    nx::utils::enableQtMessageAsserts();
}

void QnClientModule::initNetwork(const QnStartupParameters& startupParams)
{
    auto commonModule = m_clientCoreModule->commonModule();

    //TODO #ak get rid of this class!
    commonModule->store(new ec2::DummyHandler());
    if (!startupParams.enforceSocketType.isEmpty())
        nx::network::SocketFactory::enforceStreamSocketType(startupParams.enforceSocketType);

    if (!startupParams.enforceMediatorEndpoint.isEmpty())
    {
        nx::network::SocketGlobals::cloud().mediatorConnector().mockupMediatorAddress({
            startupParams.enforceMediatorEndpoint,
            startupParams.enforceMediatorEndpoint});
    }

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
    paths << ":/skin";
    paths << ":/skin_dark";

#ifdef ENABLE_DYNAMIC_CUSTOMIZATION
    if (!startupParams.dynamicCustomizationPath.isEmpty())
    {
        QDir base(startupParams.dynamicCustomizationPath);
        paths << base.absoluteFilePath("skin");
        paths << base.absoluteFilePath("skin_dark");
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

void QnClientModule::initLocalResources(const QnStartupParameters& startupParams)
{
    auto commonModule = m_clientCoreModule->commonModule();
    // client uses ordinary QT file to access file system
    QnStoragePluginFactory::instance()->registerStoragePlugin(QLatin1String("file"), QnQtFileStorageResource::instance, true);
    QnStoragePluginFactory::instance()->registerStoragePlugin(QLatin1String("qtfile"), QnQtFileStorageResource::instance);
    QnStoragePluginFactory::instance()->registerStoragePlugin(QLatin1String("layout"), QnLayoutFileStorageResource::instance);

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

QnClientCoreModule* QnClientModule::clientCoreModule() const
{
    return m_clientCoreModule.data();
}

nx::vms::client::desktop::RadassController* QnClientModule::radassController() const
{
    return m_radassController;
}

QnStartupParameters QnClientModule::startupParameters() const
{
    return m_startupParameters;
}

nx::vms::client::desktop::UploadManager* QnClientModule::uploadManager() const
{
    return m_uploadManager;
}

nx::vms::client::desktop::WearableManager* QnClientModule::wearableManager() const
{
    return m_wearableManager;
}

void QnClientModule::initLocalInfo(const QnStartupParameters& startupParams)
{
    auto commonModule = m_clientCoreModule->commonModule();

    vms::api::PeerType clientPeerType = startupParams.videoWallGuid.isNull()
        ? vms::api::PeerType::desktopClient
        : vms::api::PeerType::videowallClient;

    nx::vms::api::RuntimeData runtimeData;
    runtimeData.peer.id = commonModule->moduleGUID();
    runtimeData.peer.instanceId = commonModule->runningInstanceGUID();
    runtimeData.peer.peerType = clientPeerType;
    runtimeData.brand = qnRuntime->isDevMode() ? QString() : QnAppInfo::productNameShort();
    runtimeData.customization = qnRuntime->isDevMode() ? QString() : QnAppInfo::customizationName();
    runtimeData.videoWallInstanceGuid = startupParams.videoWallItemGuid;
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
