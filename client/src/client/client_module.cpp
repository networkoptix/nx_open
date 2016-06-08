#include "client_module.h"

#include <memory>

#include <QtWidgets/QApplication>

#include <api/app_server_connection.h>
#include <api/global_settings.h>
#include <api/network_proxy_factory.h>
#include <api/runtime_info_manager.h>
#include <api/session_manager.h>

#include <common/common_module.h>

#ifdef Q_OS_WIN
#include <common/systemexcept_win32.h>
#endif

#include <camera/camera_bookmarks_manager.h>

#include <client/client_app_info.h>
#include <client/client_startup_parameters.h>
#include <client/client_settings.h>
#include <client/client_runtime_settings.h>
#include <client/client_meta_types.h>
#include <client/client_translation_manager.h>
#include <client/client_instance_manager.h>
#include <client/desktop_client_message_processor.h>
#include <client/client_recent_connections_manager.h>

#include <core/core_settings.h>
#include <core/ptz/client_ptz_controller_pool.h>
#include <core/resource/client_camera_factory.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>

#include <nx/network/socket_global.h>
#include <nx/network/http/http_mod_manager.h>
#include <nx/utils/log/log.h>
#include <nx_ec/dummy_handler.h>

#include <platform/platform_abstraction.h>

#include <plugins/plugin_manager.h>

#include <redass/redass_controller.h>

#include <server/server_storage_manager.h>

#include <utils/common/app_info.h>
#include <utils/common/cryptographic_hash.h>
#include <utils/common/command_line_parser.h>
#include <utils/common/synctime.h>
#include <utils/media/ffmpeg_initializer.h>
#include <utils/media/voice_spectrum_analyzer.h>
#include <utils/performance_test.h>

#include <statistics/statistics_manager.h>
#include <statistics/storage/statistics_file_storage.h>
#include <statistics/settings/statistics_settings_watcher.h>

#include <ui/helpers/font_loader.h>
#include <ui/customization/customization.h>
#include <ui/customization/customizer.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/statistics/modules/controls_statistics_module.h>

#ifdef Q_OS_MAC
#include <ui/workaround/mac_utils.h>
#endif

#include <watchers/cloud_status_watcher.h>

#include "text_to_wav.h" //TODO: #GDM move to subdir

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
        return std::move(translationManager);
    }

    void initializeStatisticsManager(QnCommonModule *commonModule)
    {
        const auto statManager = commonModule->instance<QnStatisticsManager>();

        statManager->setClientId(qnSettings->pcUuid());
        statManager->setStorage(QnStatisticsStoragePtr(new QnStatisticsFileStorage()));
        statManager->setSettings(QnStatisticsSettingsPtr(new QnStatisticsSettingsWatcher()));

        static const QScopedPointer<QnControlsStatisticsModule> controlsStatisticsModule(
            new QnControlsStatisticsModule());

        statManager->registerStatisticsModule(lit("controls"), controlsStatisticsModule.data());

        QObject::connect(QnClientMessageProcessor::instance(), &QnClientMessageProcessor::connectionClosed
                         , statManager, &QnStatisticsManager::saveCurrentStatistics);
        QObject::connect(QnClientMessageProcessor::instance(), &QnClientMessageProcessor::connectionOpened
                         , statManager, &QnStatisticsManager::resetStatistics);
        QObject::connect(QnClientMessageProcessor::instance(), &QnClientMessageProcessor::initialResourcesReceived
                         , statManager, &QnStatisticsManager::sendStatistics);

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
    initNetworkSockets(startupParams);
    initSkin(startupParams);
}

QnClientModule::~QnClientModule()
{
    QNetworkProxyFactory::setApplicationProxyFactory(nullptr);

    QApplication::setOrganizationName(QString());
    QApplication::setApplicationName(QString());
    QApplication::setApplicationDisplayName(QString());
    QApplication::setApplicationVersion(QString());
}

void QnClientModule::initThread()
{
    // these functions should be called in every thread that wants to use rand() and qrand()
    srand(::time(NULL));
    qsrand(::time(NULL));
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

void QnClientModule::initMetaInfo()
{
    Q_INIT_RESOURCE(client);
    Q_INIT_RESOURCE(appserver2);
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

    common->store<QnFfmpegInitializer>(new QnFfmpegInitializer());
    common->store<QnTranslationManager>(translationManager.release());
    common->store<QnCoreSettings>(new QnCoreSettings());
    common->store<QnClientRuntimeSettings>(new QnClientRuntimeSettings());
    common->store<QnClientSettings>(clientSettingsPtr.take()); /* Now common owns the link. */

    auto clientInstanceManager = new QnClientInstanceManager(); /* Depends on QnClientSettings */
    common->store<QnClientInstanceManager>(clientInstanceManager);
    common->setModuleGUID(clientInstanceManager->instanceGuid());

    common->store<QnGlobals>(new QnGlobals());
    common->store<QnSessionManager>(new QnSessionManager());

    common->store<QnRedAssController>(new QnRedAssController());

    common->store<QnPlatformAbstraction>(new QnPlatformAbstraction());

    common->store<QnClientPtzControllerPool>(new QnClientPtzControllerPool());
    common->store<QnDesktopClientMessageProcessor>(new QnDesktopClientMessageProcessor());
    common->store<QnCameraHistoryPool>(new QnCameraHistoryPool());
    common->store<QnRuntimeInfoManager>(new QnRuntimeInfoManager());
    common->store<QnClientResourceFactory>(new QnClientResourceFactory());

    common->store<QnResourcesChangesManager>(new QnResourcesChangesManager());
    common->store<QnCameraBookmarksManager>(new QnCameraBookmarksManager());
    common->store<QnServerStorageManager>(new QnServerStorageManager());
    common->store<QnClientRecentConnectionsManager>(new QnClientRecentConnectionsManager());

    common->store<QnVoiceSpectrumAnalyzer>(new QnVoiceSpectrumAnalyzer());

    initializeStatisticsManager(common);

    /* Long runnables depend on QnCameraHistoryPool and other singletons. */
    common->store<QnLongRunnablePool>(new QnLongRunnablePool());

    /* Just to feel safe */
    QScopedPointer<QnCloudStatusWatcher> cloudStatusWatcher(new QnCloudStatusWatcher());
    cloudStatusWatcher->setCloudEndpoint(clientSettings->cdbEndpoint());
    cloudStatusWatcher->setCloudCredentials(clientSettings->cloudLogin(), clientSettings->cloudPassword(), true);
    common->store<QnCloudStatusWatcher>(cloudStatusWatcher.take());

    common->store<PluginManager>(new PluginManager());

    QScopedPointer<TextToWaveServer> textToWaveServer(new TextToWaveServer());
    textToWaveServer->start();
    common->store<TextToWaveServer>(textToWaveServer.take());

    //NOTE:: QNetworkProxyFactory::setApplicationProxyFactory takes ownership of object
    QNetworkProxyFactory::setApplicationProxyFactory(new QnNetworkProxyFactory());

    QnAppServerConnectionFactory::setDefaultFactory(QnClientResourceFactory::instance());
}

void QnClientModule::initRuntimeParams(const QnStartupParameters& startupParams)
{
    /* Dev mode. */
    if (QnCryptographicHash::hash(startupParams.devModeKey.toLatin1(), QnCryptographicHash::Md5)
        == QByteArray("\x4f\xce\xdd\x9b\x93\x71\x56\x06\x75\x4b\x08\xac\xca\x2d\xbc\x7f"))
    { /* MD5("razrazraz") */
        qnRuntime->setDevMode(true);
    }

    qnRuntime->setSoftwareYuv(startupParams.softwareYuv);
    qnRuntime->setShowFullInfo(startupParams.showFullInfo);
    qnRuntime->setIgnoreVersionMismatch(startupParams.ignoreVersionMismatch);

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
        bool ok;
        Qn::LightModeFlags lightModeOverride(startupParams.lightMode.toInt(&ok));
        if (ok)
            qnRuntime->setLightModeOverride(lightModeOverride);
        else
            qnRuntime->setLightModeOverride(Qn::LightModeFull);
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
        logFileNameSuffix.replace(QRegExp(lit("[{}]")), lit("_"));
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
}

void QnClientModule::initNetworkSockets(const QnStartupParameters& startupParams)
{
    //TODO #ak get rid of this class!
    ec2::DummyHandler dummyEc2RequestHandler;

    nx_http::HttpModManager httpModManager;
    if (!startupParams.enforceSocketType.isEmpty())
        SocketFactory::enforceStreamSocketType(startupParams.enforceSocketType);

    if (!startupParams.enforceMediatorEndpoint.isEmpty())
        nx::network::SocketGlobals::mediatorConnector().mockupAddress(
            startupParams.enforceMediatorEndpoint);


    // TODO: #mu ON/OFF switch in settings?
    nx::network::SocketGlobals::mediatorConnector().enable(true);
}

//#define ENABLE_DYNAMIC_CUSTOMIZATION
void QnClientModule::initSkin(const QnStartupParameters& startupParams)
{
#ifdef ENABLE_DYNAMIC_CUSTOMIZATION
    QString skinRoot = startupParams.dynamicCustomizationPath.isEmpty()
        ? lit(":")
        : startupParams.dynamicCustomizationPath;

    QString customizationPath = skinRoot + lit("/skin_dark");
    QScopedPointer<QnSkin> skin(new QnSkin(QStringList() << skinRoot + lit("/skin") << customizationPath));
#else
    Q_UNUSED(startupParams);
    QString customizationPath = lit(":/skin_dark");
    QScopedPointer<QnSkin> skin(new QnSkin(QStringList() << lit(":/skin") << customizationPath));
#endif // ENABLE_DYNAMIC_CUSTOMIZATION

    QnFontLoader::loadFonts(QDir(qApp->applicationDirPath()).absoluteFilePath(lit("fonts")));

    QnCustomization customization;
    customization.add(QnCustomization(skin->path("customization_common.json")));
    customization.add(QnCustomization(skin->path("customization_base.json")));
    customization.add(QnCustomization(skin->path("customization_child.json")));

    QScopedPointer<QnCustomizer> customizer(new QnCustomizer(customization));
    customizer->customize(qnGlobals);

    /* Initialize application instance. */
    QApplication::setWindowIcon(qnSkin->icon("window_icon.png"));
    QApplication::setStyle(skin->newStyle(customizer->genericPalette()));

    qnCommon->store<QnSkin>(skin.take());
    qnCommon->store<QnCustomizer>(customizer.take());
}
