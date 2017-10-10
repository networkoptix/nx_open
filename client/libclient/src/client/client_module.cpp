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
#include <nx/utils/crash_dump/systemexcept.h>

#include <camera/camera_bookmarks_manager.h>

#include <client_core/client_core_settings.h>

#include <client/client_app_info.h>
#include <client/client_settings.h>
#include <client/client_runtime_settings.h>
#include <client/client_meta_types.h>
#include <client/client_translation_manager.h>
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

#include <core/ptz/client_ptz_controller_pool.h>
#include <core/resource/client_camera_factory.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/resource/resource_directory_browser.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/resource_runtime_data.h>

#include <decoders/video/abstract_video_decoder.h>

#include <finders/systems_finder.h>
#include <network/module_finder.h>
#include <network/router.h>

#include <nx/network/socket_global.h>
#include <nx/network/http/http_mod_manager.h>
#include <vms_gateway_embeddable.h>
#include <nx/utils/log/log.h>
#include <nx_ec/dummy_handler.h>
#include <nx_ec/ec2_lib.h>

#include <platform/platform_abstraction.h>

#include <plugins/plugin_manager.h>
#include <plugins/resource/desktop_camera/desktop_resource_searcher.h>
#include <plugins/storage/file_storage/qtfile_storage_resource.h>
#include <plugins/storage/file_storage/layout_storage_resource.h>

#include <redass/redass_controller.h>

#include <server/server_storage_manager.h>

#include <utils/common/app_info.h>
#include <utils/common/cryptographic_hash.h>
#include <utils/common/command_line_parser.h>
#include <utils/common/synctime.h>
#include <utils/media/ffmpeg_initializer.h>
#include <utils/media/voice_spectrum_analyzer.h>
#include <utils/performance_test.h>
#include <utils/server_interface_watcher.h>

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

    qnLogMsgHandler(type, ctx, msg);
}


namespace
{
    typedef std::unique_ptr<QnClientTranslationManager> QnClientTranslationManagerPtr;

    QnClientTranslationManagerPtr initializeTranslations(QnClientSettings *settings
                                                         , const QString &dynamicTranslationPath)
    {
        QnClientTranslationManagerPtr translationManager(new QnClientTranslationManager());

        QnTranslation translation;
        if (!dynamicTranslationPath.isEmpty()) /* From command line. */
            translation = translationManager->loadTranslation(dynamicTranslationPath);

        if (translation.isEmpty()) /* By path. */
            translation = translationManager->loadTranslation(settings->translationPath());

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
        statManager->setSettings(QnStatisticsSettingsPtr(new QnStatisticsSettingsWatcher()));
    }
}

QnClientModule::QnClientModule(const QnStartupParameters &startupParams
                               , QObject *parent)
    : QObject(parent)
{
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

    QNetworkProxyFactory::setApplicationProxyFactory(nullptr);

    QApplication::setOrganizationName(QString());
    QApplication::setApplicationName(QString());
    QApplication::setApplicationDisplayName(QString());
    QApplication::setApplicationVersion(QString());

    //restoring default message handler
    qInstallMessageHandler(defaultMsgHandler);
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
    auto desktopSearcher = qnCommon->store(new QnDesktopResourceSearcher(window));
    desktopSearcher->setLocal(true);
    QnResourceDiscoveryManager::instance()->addDeviceServer(desktopSearcher);
}

void QnClientModule::startLocalSearchers()
{
    QnResourceDiscoveryManager::instance()->start();
}

void QnClientModule::initMetaInfo()
{
    Q_INIT_RESOURCE(appserver2);
    Q_INIT_RESOURCE(libclient);
    QnClientMetaTypes::initialize();
}

void QnClientModule::initSingletons(const QnStartupParameters& startupParams)
{
    /* Just to feel safe */
    QScopedPointer<QnClientSettings> clientSettingsPtr(new QnClientSettings(startupParams.forceLocalSettings));
    QnClientSettings* clientSettings = clientSettingsPtr.data();

    /* Init crash dumps as early as possible. */
#ifdef Q_OS_WIN
    win32_exception::setCreateFullCrashDump(clientSettings->createFullCrashDump());
#endif

    /// We should load translations before major client's services are started to prevent races
    QnClientTranslationManagerPtr translationManager(initializeTranslations(
        clientSettings, startupParams.dynamicTranslationPath));

    /* Init singletons. */

    QnCommonModule *common = new QnCommonModule(this);

    common->store(new QnResourceRuntimeDataManager());
    common->store(translationManager.release());
    common->store(new QnClientCoreSettings());
    common->store(new QnClientRuntimeSettings());
    common->store(clientSettingsPtr.take()); /* Now common owns the link. */

    /* Shorted initialization if run in self-update mode. */
    if (startupParams.selfUpdateMode)
        return;

    // TODO: #dklychkov Move to client core module
    common->store(new QnFfmpegInitializer());

    /* Depends on QnClientSettings. */
    auto clientInstanceManager = common->store(new QnClientInstanceManager());

    /* Depends on nothing. */
    common->store(new QnClientShowOnceSettings());

    /* Depends on QnClientSettings, QnClientInstanceManager and QnClientShowOnceSettings, never used directly. */
    common->store(new QnClientSettingsWatcher());

    /* Depends on QnClientSettings, never used directly. */
    common->store(new QnClientAutoRunWatcher());

    common->setModuleGUID(clientInstanceManager->instanceGuid());
    nx::network::SocketGlobals::outgoingTunnelPool()
        .assignOwnPeerId("dc", common->moduleGUID());

    common->store(new QnGlobals());
    common->store(new QnSessionManager());

    common->store(new QnRedAssController());

    common->store(new QnPlatformAbstraction());

    common->store(new QnClientPtzControllerPool());
    common->store(new QnDesktopClientMessageProcessor());
    common->store(new QnCameraHistoryPool());
    common->store(new QnRuntimeInfoManager());
    common->store(new QnClientResourceFactory());

    common->store(new QnResourcesChangesManager());
    common->store(new QnCameraBookmarksManager());
    common->store(new QnServerStorageManager());

    common->store(new QnVoiceSpectrumAnalyzer());

    // Must be called before QnCloudStatusWatcher but after setModuleGUID() call.
    initLocalInfo(startupParams);

    initializeStatisticsManager(common);

    /* Long runnables depend on QnCameraHistoryPool and other singletons. */
    common->store(new QnLongRunnablePool());

    /* Just to feel safe */
    common->store(new QnCloudConnectionProvider());
    common->store(new QnCloudStatusWatcher());

    //NOTE:: QNetworkProxyFactory::setApplicationProxyFactory takes ownership of object
    QNetworkProxyFactory::setApplicationProxyFactory(new QnNetworkProxyFactory());

    QnAppServerConnectionFactory::setDefaultFactory(QnClientResourceFactory::instance());

#ifdef Q_OS_WIN
    common->store(new QnIexploreUrlHandler());
#endif

    common->store(new QnQtbugWorkaround());
    common->store(new nx::cloud::gateway::VmsGatewayEmbeddable(true));
}

void QnClientModule::initRuntimeParams(const QnStartupParameters& startupParams)
{
    /* Dev mode. */
    if (QnCryptographicHash::hash(startupParams.devModeKey.toLatin1(), QnCryptographicHash::Md5)
        == QByteArray("\x4f\xce\xdd\x9b\x93\x71\x56\x06\x75\x4b\x08\xac\xca\x2d\xbc\x7f"))
    { /* MD5("razrazraz") */
        qnRuntime->setDevMode(true);
    }

    qnRuntime->setGLDoubleBuffer(qnSettings->isGlDoubleBuffer());
    qnRuntime->setTranslationPath(qnSettings->translationPath());
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
            qnCommon->setEngineVersion(version);
        }
    }

    //TODO: #GDM Should we always overwrite persistent setting with command-line values? o_O
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

    //TODO: #GDM fix it
    /* Here the value from LightModeOverride will be copied to LightMode */
#ifndef __arm__
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


    static const int DEFAULT_MAX_LOG_FILE_SIZE = 10 * 1024 * 1024;
    static const int DEFAULT_MSG_LOG_ARCHIVE_SIZE = 5;

    const QString dataLocation = QStandardPaths::writableLocation(QStandardPaths::DataLocation);

    if (logLevel.isEmpty())
        logLevel = qnSettings->logLevel();

    QnLog::initLog(logLevel);
    QString logFileLocation = dataLocation + QLatin1String("/log");
    QString logFileName = logFileLocation + QLatin1String("/log_file") + logFileNameSuffix;
    if (!QDir().mkpath(logFileLocation))
        cl_log.log(lit("Could not create log folder: ") + logFileLocation, cl_logALWAYS);
    if (!cl_log.create(logFileName, DEFAULT_MAX_LOG_FILE_SIZE, DEFAULT_MSG_LOG_ARCHIVE_SIZE, QnLog::instance()->logLevel()))
        cl_log.log(lit("Could not create log file") + logFileName, cl_logALWAYS);
    cl_log.log(QLatin1String("================================================================================="), cl_logALWAYS);

    if (ec2TranLogLevel.isEmpty())
        ec2TranLogLevel = qnSettings->ec2TranLogLevel();

    //preparing transaction log
    if (ec2TranLogLevel != lit("none"))
    {
        QnLog::instance(QnLog::EC2_TRAN_LOG)->create(
            dataLocation + QLatin1String("/log/ec2_tran"),
            DEFAULT_MAX_LOG_FILE_SIZE,
            DEFAULT_MSG_LOG_ARCHIVE_SIZE,
            QnLog::logLevelFromString(ec2TranLogLevel));
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("================================================================================="), cl_logALWAYS);
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("================================================================================="), cl_logALWAYS);
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("================================================================================="), cl_logALWAYS);
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("%1 started").arg(qApp->applicationName()), cl_logALWAYS);
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("Software version: %1").arg(QCoreApplication::applicationVersion()), cl_logALWAYS);
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("Software revision: %1").arg(QnAppInfo::applicationRevision()), cl_logALWAYS);
    }

    defaultMsgHandler = qInstallMessageHandler(myMsgHandler);

    //TODO: #GDM Standartize log header and calling conventions
    cl_log.log(qApp->applicationDisplayName(), " started", cl_logALWAYS);
    cl_log.log("Software version: ", QApplication::applicationVersion(), cl_logALWAYS);
    cl_log.log("binary path: ", qApp->applicationFilePath(), cl_logALWAYS);
}

void QnClientModule::initNetwork(const QnStartupParameters& startupParams)
{
    //TODO #ak get rid of this class!
    qnCommon->store(new ec2::DummyHandler());
    qnCommon->store(new nx_http::HttpModManager());
    if (!startupParams.enforceSocketType.isEmpty())
        SocketFactory::enforceStreamSocketType(startupParams.enforceSocketType);

    if (!startupParams.enforceMediatorEndpoint.isEmpty())
        nx::network::SocketGlobals::mediatorConnector().mockupAddress(
            startupParams.enforceMediatorEndpoint);

    // TODO: #mu ON/OFF switch in settings?
    nx::network::SocketGlobals::mediatorConnector().enable(true);

    /* Initialize connections. */

    Qn::PeerType clientPeerType = startupParams.videoWallGuid.isNull()
        ? Qn::PT_DesktopClient
        : Qn::PT_VideowallClient;

    NX_ASSERT(nx::utils::TimerManager::instance());
    QScopedPointer<ec2::AbstractECConnectionFactory> ec2ConnectionFactory(
        getConnectionFactory(clientPeerType, nx::utils::TimerManager::instance()));
    QnAppServerConnectionFactory::setEC2ConnectionFactory(ec2ConnectionFactory.data());
    qnCommon->store(ec2ConnectionFactory.take());

    if (!startupParams.videoWallGuid.isNull())
    {
        QnAppServerConnectionFactory::setVideowallGuid(startupParams.videoWallGuid);
        QnAppServerConnectionFactory::setInstanceGuid(startupParams.videoWallItemGuid);
    }

    auto moduleFinder = qnCommon->store(new QnModuleFinder(true)); //TODO: #GDM make it common way via scoped pointer somehow
    moduleFinder->start();

    qnCommon->store(new QnSystemsFinder());
    qnCommon->store(new QnForgottenSystemsManager());

    // Depends on qnSystemsFinder
    qnCommon->store(new QnStartupTileManager());

    auto router = qnCommon->store(new QnRouter(moduleFinder));
    qnCommon->store(new QnServerInterfaceWatcher(router));
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
    customization.add(QnCustomization(skin->path("customization_base.json")));

    QScopedPointer<QnCustomizer> customizer(new QnCustomizer(customization));
    customizer->customize(qnGlobals);

    /* Initialize application UI. Skip if run in console (e.g. unit-tests). */
    QGuiApplication* ui = qobject_cast<QGuiApplication*>(qApp);
    if (ui)
    {
        QnFontLoader::loadFonts(QDir(QApplication::applicationDirPath()).absoluteFilePath(lit("fonts")));

        // Window icon is taken from 'icons' customization project. Suppress check.
        QApplication::setWindowIcon(qnSkin->icon(":/logo.png")); // _IGNORE_VALIDATION_
        QApplication::setStyle(skin->newStyle(customizer->genericPalette()));
    }

    qnCommon->store(skin.take());
    qnCommon->store(customizer.take());
}

void QnClientModule::initLocalResources(const QnStartupParameters& startupParams)
{
    qnCommon->store(new PluginManager());
    // client uses ordinary QT file to access file system
    QnStoragePluginFactory::instance()->registerStoragePlugin(QLatin1String("file"), QnQtFileStorageResource::instance, true);
    QnStoragePluginFactory::instance()->registerStoragePlugin(QLatin1String("qtfile"), QnQtFileStorageResource::instance);
    QnStoragePluginFactory::instance()->registerStoragePlugin(QLatin1String("layout"), QnLayoutFileStorageResource::instance);

    QnVideoDecoderFactory::setCodecManufacture(QnVideoDecoderFactory::AUTO);

    auto resourceProcessor = qnCommon->store(new QnClientResourceProcessor());
    auto resourceDiscoveryManager = qnCommon->store(new QnResourceDiscoveryManager());
    resourceProcessor->moveToThread(QnResourceDiscoveryManager::instance());
    resourceDiscoveryManager->setResourceProcessor(resourceProcessor);

    if (!startupParams.skipMediaFolderScan)
    {
        auto localFilesSearcher = qnCommon->store(new QnResourceDirectoryBrowser());

        localFilesSearcher->setLocal(true);
        QStringList dirs;
        dirs << qnSettings->mediaFolder();
        dirs << qnSettings->extraMediaFolders();
        localFilesSearcher->setPathCheckList(dirs);
        resourceDiscoveryManager->addDeviceServer(localFilesSearcher);
    }


    QnResourceDiscoveryManager::instance()->setReady(true);

    qnCommon->store(new QnSystemsWeightsManager());
}

void QnClientModule::initLocalInfo(const QnStartupParameters& startupParams)
{
    Qn::PeerType clientPeerType = startupParams.videoWallGuid.isNull()
        ? Qn::PT_DesktopClient
        : Qn::PT_VideowallClient;

    ec2::ApiRuntimeData runtimeData;
    runtimeData.peer.id = qnCommon->moduleGUID();
    runtimeData.peer.instanceId = qnCommon->runningInstanceGUID();
    runtimeData.peer.peerType = clientPeerType;
    runtimeData.brand = qnRuntime->isDevMode() ? QString() : QnAppInfo::productNameShort();
    runtimeData.customization = qnRuntime->isDevMode() ? QString() : QnAppInfo::customizationName();
    runtimeData.videoWallInstanceGuid = startupParams.videoWallItemGuid;
    QnRuntimeInfoManager::instance()->updateLocalItem(runtimeData);    // initializing localInfo
}
