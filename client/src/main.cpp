//#define QN_USE_VLD

#ifdef QN_USE_VLD
#   include <vld.h>
#endif

#include <qglobal.h>

#ifdef Q_OS_LINUX
#   include <unistd.h>
#endif

#include <utils/common/app_info.h>
#include "ui/widgets/main_window.h"
#include "client/client_settings.h"
#include <api/global_settings.h>

#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <QtCore/QTranslator>
#include <QtCore/QStandardPaths>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>
#include <QtGui/QDesktopServices>

#include <QtSingleApplication>

extern "C"
{
    #include <libavformat/avformat.h>
    #include <libavformat/avio.h>
}

#include "decoders/video/ipp_h264_decoder.h"

#include <utils/common/log.h>
#include <utils/common/command_line_parser.h>
#include "ui/workbench/workbench_context.h"
#include "ui/actions/action_manager.h"
#include "ui/style/skin.h"
#include "decoders/video/abstractdecoder.h"
#ifdef Q_OS_WIN
    #include <plugins/resource/desktop_win/desktop_resource_searcher.h>
#endif
#include "utils/common/util.h"
#include "plugins/resource/avi/avi_resource.h"
#include "core/resource_management/resource_discovery_manager.h"
#include "core/resource_management/resource_pool.h"
#include "plugins/resource/arecontvision/resource/av_resource_searcher.h"
#include "api/app_server_connection.h"
#include <plugins/resource/server_camera/server_camera.h>
#include <plugins/resource/server_camera/server_camera_factory.h>


#define TEST_RTSP_SERVER

#include <core/resource/camera_user_attribute_pool.h>
#include "core/resource/media_server_resource.h"
#include <core/resource/media_server_user_attributes.h>
#include "core/resource/storage_resource.h"

#include "plugins/resource/axis/axis_resource_searcher.h"
#include "plugins/plugin_manager.h"
#include "core/resource/resource_directory_browser.h"

#include "tests/auto_tester.h"
#include "plugins/resource/d-link/dlink_resource_searcher.h"
#include "api/session_manager.h"
#include "plugins/resource/droid/droid_resource_searcher.h"
#include "ui/actions/action_manager.h"
#include "plugins/resource/iqinvision/iqinvision_resource_searcher.h"
#include "plugins/resource/droid_ipwebcam/ipwebcam_droid_resource_searcher.h"
#include "plugins/resource/isd/isd_resource_searcher.h"
//#include "plugins/resource/onvif/onvif_ws_searcher.h"
#include "utils/network/socket.h"


#include "plugins/storage/file_storage/qtfile_storage_resource.h"
#include "plugins/storage/file_storage/layout_storage_resource.h"
#include "core/resource/camera_history.h"
#include "client/client_message_processor.h"
#include "client/client_translation_manager.h"

#ifdef Q_OS_LINUX
    #include "ui/workaround/x11_launcher_workaround.h"
#endif
#include "utils/common/cryptographic_hash.h"
#include "utils/performance_test.h"
#include "ui/style/globals.h"
#include "openal/qtvaudiodevice.h"
#include "ui/workaround/fglrx_full_screen.h"
#include "ui/workaround/qtbug_workaround.h"

#ifdef Q_OS_WIN
    #include "ui/workaround/iexplore_url_handler.h"
    #include "common/systemexcept_win32.h"
#endif

#include "ui/help/help_handler.h"
#include <client/client_module.h>
#include <client/client_connection_data.h>
#include <client/client_resource_processor.h>
#include "platform/platform_abstraction.h"
#include "utils/common/long_runnable.h"
#include <utils/common/synctime.h>

#include "text_to_wav.h"
#include "common/common_module.h"
#include "ui/style/noptix_style.h"
#include "ui/customization/customizer.h"
#include "core/ptz/client_ptz_controller_pool.h"
#include "ui/dialogs/message_box.h"
#include <nx_ec/ec2_lib.h>
#include <nx_ec/dummy_handler.h>
#include <utils/network/module_finder.h>
#include <utils/network/global_module_finder.h>
#include <utils/network/router.h>
#include <api/network_proxy_factory.h>
#include <utils/server_interface_watcher.h>

#ifdef Q_OS_MAC
#include "ui/workaround/mac_utils.h"
#endif
#include "api/runtime_info_manager.h"
#include "core/resource_management/resource_properties.h"
#include "core/resource_management/status_dictionary.h"

void decoderLogCallback(void* /*pParam*/, int i, const char* szFmt, va_list args)
{
    //USES_CONVERSION;

    //Ignore debug and info (i == 2 || i == 1) messages
    if(AV_LOG_ERROR != i)
    {
        //return;
    }

    // AVCodecContext* pCtxt = (AVCodecContext*)pParam;

    char szMsg[1024];
    vsprintf(szMsg, szFmt, args);
    //if(szMsg[strlen(szMsg)] == '\n')
    {
        szMsg[strlen(szMsg)-1] = 0;
    }

    cl_log.log(QLatin1String("FFMPEG "), QString::fromLocal8Bit(szMsg), cl_logERROR);
}

#ifndef UNICLIENT_TESTS

static int lockmgr(void **mtx, enum AVLockOp op)
{
    QMutex** qMutex = (QMutex**) mtx;
    switch(op) {
        case AV_LOCK_CREATE:
            *qMutex = new QMutex();
            return 0;
        case AV_LOCK_OBTAIN:
            (*qMutex)->lock();
            return 0;
        case AV_LOCK_RELEASE:
            (*qMutex)->unlock();
            return 0;
        case AV_LOCK_DESTROY:
            delete *qMutex;
            return 0;
    }
    return 1;
}

void ffmpegInit()
{
    //avcodec_init();
    av_register_all();

    if(av_lockmgr_register(lockmgr) != 0)
    {
        qCritical() << "Failed to register ffmpeg lock manager";
    }

    // client uses ordinary QT file to access file system, server uses buffering access implemented inside QnFileStorageResource
    QnStoragePluginFactory::instance()->registerStoragePlugin(QLatin1String("file"), QnQtFileStorageResource::instance, true);
    QnStoragePluginFactory::instance()->registerStoragePlugin(QLatin1String("qtfile"), QnQtFileStorageResource::instance);
    QnStoragePluginFactory::instance()->registerStoragePlugin(QLatin1String("layout"), QnLayoutFileStorageResource::instance);
    //QnStoragePluginFactory::instance()->registerStoragePlugin(QLatin1String("memory"), QnLayoutFileStorageResource::instance);

    /*
    extern URLProtocol ufile_protocol;
    av_register_protocol2(&ufile_protocol, sizeof(ufile_protocol));

    extern URLProtocol qtufile_protocol;
    av_register_protocol2(&qtufile_protocol, sizeof(qtufile_protocol));
    */
}

#ifdef TEST_RTSP_SERVER

void addTestFile(const QString& fileName, const QString& resId)
{
    Q_UNUSED(resId)
    QnAviResourcePtr resource(new QnAviResource(fileName));
    qnResPool->addResource(QnResourcePtr(resource));
}

void addTestData()
{
    /*
    QnAviResourcePtr resource(new QnAviResource("E:/Users/roman76r/video/ROCKNROLLA/BDMV/STREAM/00000.m2ts"));
    resource->removeFlag(Qn::local); // to initialize access to resource throught RTSP server
    resource->addFlag(Qn::remote); // to initialize access to resource throught RTSP server
    resource->setParentId(server->getId());
    qnResPool->addResource(QnResourcePtr(resource));
    */

    /*
    QnFakeCameraPtr testCamera(new QnFakeCamera());
    testCamera->setParentId(server->getId());
    testCamera->setMAC(QnMacAddress("00000"));
    testCamera->setUrl("00000");
    testCamera->setName("testCamera");
    qnResPool->addResource(QnResourcePtr(testCamera));
    */

    /*
    addTestFile("e:/Users/roman76r/blake/3PM PRIVATE SESSION, HOLLYWOOD Jayme.flv", "q1");
    addTestFile("e:/Users/roman76r/blake/8 FEATURE PREMIERE_Paid Companions_Bottled-Up_h.wmv", "q2");
    addTestFile("e:/Users/roman76r/blake/9  FEATURE PREMIERE_Paid Compan_Afternoon Whores.wmv", "q3");
    addTestFile("e:/Users/roman76r/blake/A CUT ABOVE Aria & Justine.flv", "q4");
    addTestFile("e:/Users/roman76r/blake/A DOLL'S LIFE Jacqueline, Nika & Jade.flv", "q5");
    */

    /*
    QnAviResourcePtr resource2(new QnAviResource("C:/Users/physic/Videos/HighDef_-_Audio_-_Japan.avi"));
    resource2->removeFlag(Qn::local); // to initialize access to resource throught RTSP server
    resource2->addFlag(Qn::remote); // to initialize access to resource throught RTSP server
    resource2->setParentId(server->getId());
    qnResPool->addResource(QnResourcePtr(resource2));
    */

    /*
    QnNetworkResourceList testList;
    testList << testCamera;
    QnTimePeriodList periods = server->apiConnection()->recordedTimePeriods(testList);
    for (int i = 0; i < periods.size(); ++i)
    {
        qDebug() << periods[i].startTime << ' ' << periods[i].duration;
    }
    */

}
#endif

void initAppServerConnection(const QnUuid &videowallGuid, const QnUuid &videowallInstanceGuid) {
    QnAppServerConnectionFactory::setClientGuid(QnUuid::createUuid().toString());
    QnAppServerConnectionFactory::setDefaultFactory(QnServerCameraFactory::instance());
    if (!videowallGuid.isNull()) {
        QnAppServerConnectionFactory::setVideowallGuid(videowallGuid);
        QnAppServerConnectionFactory::setInstanceGuid(videowallInstanceGuid);
    }
}

/** Initialize log. */
void initLog(const QString &logLevel, const QString fileNameSuffix) {
    QnLog::initLog(logLevel);
    const QString dataLocation = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    QString logFileLocation = dataLocation + QLatin1String("/log");
    QString logFileName = logFileLocation + QLatin1String("/log_file") + fileNameSuffix;
    if (!QDir().mkpath(logFileLocation))
        cl_log.log(lit("Could not create log folder: ") + logFileLocation, cl_logALWAYS);
    if (!cl_log.create(logFileName, 1024*1024*10, 5, cl_logDEBUG1))
        cl_log.log(lit("Could not create log file") + logFileName, cl_logALWAYS);
    cl_log.log(QLatin1String("================================================================================="), cl_logALWAYS);
}

static QtMessageHandler defaultMsgHandler = 0;

static void myMsgHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    if (defaultMsgHandler) {
        defaultMsgHandler(type, ctx, msg);
    } else { /* Default message handler. */
#ifndef QN_NO_STDERR_MESSAGE_OUTPUT
        QTextStream err(stderr);
        err << msg << endl << flush;
#endif
    }

    qnLogMsgHandler(type, ctx, msg);
}

#ifdef Q_WS_X11
#include <X11/Xlib.h>
#endif

#include <iostream>

#ifndef API_TEST_MAIN

int runApplication(QtSingleApplication* application, int argc, char **argv) {
    // these functions should be called in every thread that wants to use rand() and qrand()
    srand(::time(NULL));
    qsrand(::time(NULL));

    int result = 0;

    QThread::currentThread()->setPriority(QThread::HighestPriority);

    /* Parse command line. */
    QnAutoTester autoTester(argc, argv);

    QnSyncTime syncTime;

    qnSettings->updateFromCommandLine(argc, argv, stderr);

    QString devModeKey;
    bool noSingleApplication = false;
    int screen = -1;
    QString authenticationString, delayedDrop, instantDrop, logLevel;
    QString translationPath;
    QString customizationPath = qnSettings->clientSkin() == Qn::LightSkin ? lit(":/skin_light") : lit(":/skin_dark");
    bool skipMediaFolderScan = false;
#ifdef Q_OS_MAC
    bool noFullScreen = true;
#else
    bool noFullScreen = false;
#endif
    bool noVersionMismatchCheck = false;
    QString lightMode;
    bool noVSync = false;
    QString sVideoWallGuid;
    QString sVideoWallItemGuid;
    QString engineVersion;
    bool noClientUpdate = false;

    QnCommandLineParser commandLineParser;
    commandLineParser.addParameter(&noSingleApplication,    "--no-single-application",      NULL,   QString());
    commandLineParser.addParameter(&authenticationString,   "--auth",                       NULL,   QString());
    commandLineParser.addParameter(&screen,                 "--screen",                     NULL,   QString());
    commandLineParser.addParameter(&delayedDrop,            "--delayed-drop",               NULL,   QString());
    commandLineParser.addParameter(&instantDrop,            "--instant-drop",               NULL,   QString());
    commandLineParser.addParameter(&logLevel,               "--log-level",                  NULL,   QString());
#ifdef ENABLE_DYNAMIC_TRANSLATION
    commandLineParser.addParameter(&translationPath,        "--translation",                NULL,   QString());
#endif
    commandLineParser.addParameter(&devModeKey,             "--dev-mode-key",               NULL,   QString());
    commandLineParser.addParameter(&skipMediaFolderScan,    "--skip-media-folder-scan",     NULL,   QString());
    commandLineParser.addParameter(&noFullScreen,           "--no-fullscreen",              NULL,   QString());
    commandLineParser.addParameter(&noVersionMismatchCheck, "--no-version-mismatch-check",  NULL,   QString());
#ifdef ENABLE_DYNAMIC_CUSTOMIZATION
    commandLineParser.addParameter(&customizationPath,      "--customization",              NULL,   QString());
#endif
    commandLineParser.addParameter(&lightMode,              "--light-mode",                 NULL,   QString(), lit("full"));
    commandLineParser.addParameter(&noVSync,                "--no-vsync",                   NULL,   QString());
    commandLineParser.addParameter(&sVideoWallGuid,         "--videowall",                  NULL,   QString());
    commandLineParser.addParameter(&sVideoWallItemGuid,     "--videowall-instance",         NULL,   QString());
    commandLineParser.addParameter(&engineVersion,          "--override-version",           NULL,   QString());
    commandLineParser.addParameter(&noClientUpdate,         "--no-client-update",           NULL,   QString());

    commandLineParser.parse(argc, argv, stderr, QnCommandLineParser::RemoveParsedParameters);

    ec2::DummyHandler dummyEc2RequestHandler;

    /* Dev mode. */
    if(QnCryptographicHash::hash(devModeKey.toLatin1(), QnCryptographicHash::Md5) == QByteArray("\x4f\xce\xdd\x9b\x93\x71\x56\x06\x75\x4b\x08\xac\xca\x2d\xbc\x7f")) { /* MD5("razrazraz") */
        qnSettings->setDevMode(true);
    }

    if (!engineVersion.isEmpty()) {
        QnSoftwareVersion version(engineVersion);
        if (!version.isNull()) {
            qWarning() << "Starting with overriden version: " << version.toString();
            qnCommon->setEngineVersion(version);
        }
    }

    QnUuid videowallGuid(sVideoWallGuid);
    QnUuid videowallInstanceGuid(sVideoWallItemGuid);

    QString logFileNameSuffix;
    if (!videowallGuid.isNull()) {
        qnSettings->setVideoWallMode(true);
        noSingleApplication = true;
        noFullScreen = true;
        noVersionMismatchCheck = true;
        qnSettings->setLightModeOverride(Qn::LightModeVideoWall);

        logFileNameSuffix = videowallInstanceGuid.isNull() 
            ? videowallGuid.toString() 
            : videowallInstanceGuid.toString();
        logFileNameSuffix.replace(QRegExp(lit("[{}]")), lit("_"));
    }

    initLog(logLevel, logFileNameSuffix);

	// TODO: #Elric why QString???
    if (!lightMode.isEmpty()) {
        bool ok;
        int lightModeOverride = lightMode.toInt(&ok);
        if (ok)
            qnSettings->setLightModeOverride(lightModeOverride);
        else
            qnSettings->setLightModeOverride(Qn::LightModeFull);
    }

    QnPerformanceTest::detectLightMode();

#ifdef Q_OS_MACX
    if (mac_isSandboxed())
        qnSettings->setLightMode(qnSettings->lightMode() | Qn::LightModeNoNewWindow);
#endif

    qnSettings->setVSyncEnabled(!noVSync);

    qnSettings->setClientUpdateDisabled(noClientUpdate);

    QScopedPointer<QnSkin> skin(new QnSkin(QStringList() << lit(":/skin") << customizationPath));

    QnCustomization customization;
    customization.add(QnCustomization(skin->path("customization_common.json")));
    customization.add(QnCustomization(skin->path("customization_base.json")));
    customization.add(QnCustomization(skin->path("customization_child.json")));

    QScopedPointer<QnCustomizer> customizer(new QnCustomizer(customization));
    customizer->customize(qnGlobals);

    /* Initialize application instance. */
    application->setQuitOnLastWindowClosed(true);
    application->setWindowIcon(qnSkin->icon("window_icon.png"));
    application->setStartDragDistance(20);
    application->setStyle(skin->newStyle()); // TODO: #Elric here three qWarning's are issued (bespin bug), qnDeleteLater with null receiver
#ifdef Q_OS_MACX
    application->setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
#endif

    QnResourcePropertyDictionary dictionary;
    QnResourceStatusDiscionary statusDictionary;
    QScopedPointer<QnPlatformAbstraction> platform(new QnPlatformAbstraction());
    QScopedPointer<QnLongRunnablePool> runnablePool(new QnLongRunnablePool());
    QScopedPointer<QnClientPtzControllerPool> clientPtzPool(new QnClientPtzControllerPool());
    QScopedPointer<QnGlobalSettings> globalSettings(new QnGlobalSettings());
    QScopedPointer<QnClientMessageProcessor> clientMessageProcessor(new QnClientMessageProcessor());
    QScopedPointer<QnRuntimeInfoManager> runtimeInfoManager(new QnRuntimeInfoManager());

    QScopedPointer<TextToWaveServer> textToWaveServer(new TextToWaveServer());
    textToWaveServer->start();

#ifdef Q_WS_X11
    //   QnX11LauncherWorkaround x11LauncherWorkaround;
    //   application->installEventFilter(&x11LauncherWorkaround);
#endif

#ifdef Q_OS_WIN
    new QnIexploreUrlHandler(application); /* All effects are placed in the constructor. */
    new QnQtbugWorkaround(application);
#endif

    if(!noSingleApplication) {
        QString argsMessage;
        for (int i = 1; i < argc; ++i)
            argsMessage += fromNativePath(QFile::decodeName(argv[i])) + QLatin1Char('\n');

        while (application->isRunning()) {
            if (application->sendMessage(argsMessage)) {
                cl_log.log(lit("Another instance is already running"), cl_logALWAYS);
                return 0;
            }
        }
    }

    QScopedPointer<QnServerCameraFactory> serverCameraFactory(new QnServerCameraFactory());

    //NOTE QNetworkProxyFactory::setApplicationProxyFactory takes ownership of object
    QNetworkProxyFactory::setApplicationProxyFactory( new QnNetworkProxyFactory() );

    /* Initialize connections. */
    initAppServerConnection(videowallGuid, videowallInstanceGuid);

    std::unique_ptr<ec2::AbstractECConnectionFactory> ec2ConnectionFactory(
        getConnectionFactory( videowallGuid.isNull() ? Qn::PT_DesktopClient : Qn::PT_VideowallClient ) );
    ec2::ResourceContext resCtx(
        QnServerCameraFactory::instance(),
        qnResPool,
        qnResTypePool );
	ec2ConnectionFactory->setContext( resCtx );
    QnAppServerConnectionFactory::setEC2ConnectionFactory( ec2ConnectionFactory.get() );

    QObject::connect( qnResPool, &QnResourcePool::resourceAdded, application, []( const QnResourcePtr& resource ){
        if( resource->hasFlags(Qn::foreigner) )
            return;
        QnMediaServerResource* mediaServerRes = dynamic_cast<QnMediaServerResource*>(resource.data());
        ec2::AbstractECConnectionPtr ecConnection = QnAppServerConnectionFactory::getConnection2();
        if( mediaServerRes && ecConnection && (mediaServerRes->getSystemName() == ecConnection->connectionInfo().systemName) )
            mediaServerRes->determineOptimalNetIF();
    } );

    ec2::ApiRuntimeData runtimeData;
    runtimeData.peer.id = qnCommon->moduleGUID();
    runtimeData.peer.instanceId = qnCommon->runningInstanceGUID();
    runtimeData.peer.peerType = videowallInstanceGuid.isNull()
        ? Qn::PT_DesktopClient
        : Qn::PT_VideowallClient;
    runtimeData.brand = QnAppInfo::productNameShort();
    runtimeData.videoWallInstanceGuid = videowallInstanceGuid;
    QnRuntimeInfoManager::instance()->updateLocalItem(runtimeData);    // initializing localInfo

    qnSettings->save();
    if (!QDir(qnSettings->mediaFolder()).exists())
        QDir().mkpath(qnSettings->mediaFolder());

    cl_log.log(QLatin1String("Using ") + qnSettings->mediaFolder() + QLatin1String(" as media root directory"), cl_logALWAYS);

    QDir::setCurrent(QFileInfo(QFile::decodeName(argv[0])).absolutePath());


    /* Initialize sound. */
    QtvAudioDevice::instance()->setVolume(qnSettings->audioVolume());

    QnHelpHandler helpHandler;
    qApp->installEventFilter(&helpHandler);

    cl_log.log(qApp->applicationName(), " started", cl_logALWAYS);
    cl_log.log("Software version: ", QApplication::applicationVersion(), cl_logALWAYS);
    cl_log.log("binary path: ", QFile::decodeName(argv[0]), cl_logALWAYS);

    defaultMsgHandler = qInstallMessageHandler(myMsgHandler);

    // Create and start SessionManager
    QnSessionManager::instance()->start();

    ffmpegInit();

    QScopedPointer<QnModuleFinder> moduleFinder(new QnModuleFinder(true));
    moduleFinder->setCompatibilityMode(qnSettings->isDevMode());
    moduleFinder->start();

    QScopedPointer<QnGlobalModuleFinder> globalModuleFinder(new QnGlobalModuleFinder());

    QScopedPointer<QnRouter> router(new QnRouter(moduleFinder.data(), true));

    QScopedPointer<QnServerInterfaceWatcher> serverInterfaceWatcher(new QnServerInterfaceWatcher(router.data()));

    //===========================================================================

    CLVideoDecoderFactory::setCodecManufacture( CLVideoDecoderFactory::AUTO );

    /* Load translation. */
    QnClientTranslationManager *translationManager = qnCommon->instance<QnClientTranslationManager>();
    QnTranslation translation;
    if(!translationPath.isEmpty()) /* From command line. */
        translation = translationManager->loadTranslation(translationPath);

    if(translation.isEmpty()) /* By path. */
        translation = translationManager->loadTranslation(qnSettings->translationPath());

    translationManager->installTranslation(translation);

    /* Create workbench context. */
    QScopedPointer<QnWorkbenchContext> context(new QnWorkbenchContext(qnResPool));
    context->instance<QnFglrxFullScreen>(); /* Init fglrx workaround. */

    Qn::ActionId effectiveMaximizeActionId = Qn::FullscreenAction;
#ifdef Q_OS_LINUX
    /* In Ubuntu its launcher is configured to be shown when a non-fullscreen window has appeared.
     * In our case it means that launcher overlaps our fullscreen window when the user opens any dialogs.
     * To prevent such overlapping there was an attempt to hide unity launcher when the main window
     * has been activated. But now we can't hide launcher window because there is no any visible window for it.
     * Unity-3D launcher is like a 3D-effect activated by compiz window manager.
     * We can investigate possibilities of changing the behavior of unity compiz plugin but now
     * we just disable fullscreen for unity-3d desktop session.
     */
    if (QnX11LauncherWorkaround::isUnity3DSession())
        effectiveMaximizeActionId = Qn::MaximizeAction;
#endif
    context->menu()->registerAlias(Qn::EffectiveMaximizeAction, effectiveMaximizeActionId);

    /* Create main window. */
    Qt::WindowFlags flags = qnSettings->isVideoWallMode()
            ? Qt::FramelessWindowHint | Qt::X11BypassWindowManagerHint
            : static_cast<Qt::WindowFlags>(0);
    QScopedPointer<QnMainWindow> mainWindow(new QnMainWindow(context.data(), NULL, flags));
    context->setMainWindow(mainWindow.data());
    mainWindow->setAttribute(Qt::WA_QuitOnClose);
    application->setActivationWindow(mainWindow.data());

    if(screen != -1) {
        QDesktopWidget *desktop = qApp->desktop();
        if(screen >= 0 && screen < desktop->screenCount()) {
            QPoint screenDelta = mainWindow->pos() - desktop->screenGeometry(mainWindow.data()).topLeft();

            mainWindow->move(desktop->screenGeometry(screen).topLeft() + screenDelta);
        }
    }

    mainWindow->show();
    if (!noFullScreen)
        context->action(Qn::EffectiveMaximizeAction)->trigger();
    else
        mainWindow->updateDecorationsState();

    if(noVersionMismatchCheck)
        context->action(Qn::VersionMismatchMessageAction)->setVisible(false); // TODO: #Elric need a better mechanism for this

    //initializing plugin manager. TODO supply plugin dir (from settings)
    //PluginManager::instance()->loadPlugins( PluginManager::QtPlugin );

    /* Process input files. */
    bool haveInputFiles = false;
    for (int i = 1; i < argc; ++i)
        haveInputFiles |= mainWindow->handleMessage(QFile::decodeName(argv[i]));
    if(!noSingleApplication)
        QObject::connect(application, SIGNAL(messageReceived(const QString &)), mainWindow.data(), SLOT(handleMessage(const QString &)));

#ifdef TEST_RTSP_SERVER
    addTestData();
#endif

    if(autoTester.tests() != 0 && autoTester.state() == QnAutoTester::Initial) {
        QObject::connect(&autoTester, SIGNAL(finished()), application, SLOT(quit()));
        autoTester.start();
    }

    /* Process pending events before executing actions. */
    qApp->processEvents();

    // show beta version warning message for the main instance only
    if (!noSingleApplication &&
        !qnSettings->isDevMode() &&
        QnAppInfo::beta())
        context->action(Qn::BetaVersionMessageAction)->trigger();

    /* If no input files were supplied --- open connection settings dialog. */
    
    /* 
     * Do not try to connect in the following cases:
     * * we were not connected and clicked "Open in new window"
     * * we have opened exported exe-file 
     * Otherwise we should try to connect or show Login Dialog.
     */    
    if (instantDrop.isEmpty() && !haveInputFiles) {
        /* Set authentication parameters from command line. */
        QUrl appServerUrl = QUrl::fromUserInput(authenticationString);
        if (!videowallGuid.isNull()) {
            Q_ASSERT(appServerUrl.isValid());
            if (!appServerUrl.isValid()) {
                return -1;
            }
            appServerUrl.setUserName(videowallGuid.toString());
        }
        context->menu()->trigger(Qn::ConnectAction, QnActionParameters().withArgument(Qn::UrlRole, appServerUrl));
    }

    if (!videowallGuid.isNull()) {
        context->menu()->trigger(Qn::DelayedOpenVideoWallItemAction, QnActionParameters()
                             .withArgument(Qn::VideoWallGuidRole, videowallGuid)
                             .withArgument(Qn::VideoWallItemGuidRole, videowallInstanceGuid));
    } else if(!delayedDrop.isEmpty()) { /* Drop resources if needed. */
        Q_ASSERT(instantDrop.isEmpty());

        QByteArray data = QByteArray::fromBase64(delayedDrop.toLatin1());
        context->menu()->trigger(Qn::DelayedDropResourcesAction, QnActionParameters().withArgument(Qn::SerializedDataRole, data));
    } else if (!instantDrop.isEmpty()){
        QByteArray data = QByteArray::fromBase64(instantDrop.toLatin1());
        context->menu()->trigger(Qn::InstantDropResourcesAction, QnActionParameters().withArgument(Qn::SerializedDataRole, data));
    }

#ifdef _DEBUG
    /* Show FPS in debug. */
    context->menu()->trigger(Qn::ShowFpsAction);
#endif

    /************************************************************************/
    /* Initializing resource searchers                                      */
    /************************************************************************/
    QnClientResourceProcessor resourceProcessor;
    QnResourceDiscoveryManager::init(new QnResourceDiscoveryManager());
    resourceProcessor.moveToThread( QnResourceDiscoveryManager::instance() );
    QnResourceDiscoveryManager::instance()->setResourceProcessor(&resourceProcessor);

    //============================
    //QnResourceDirectoryBrowser
    if(!skipMediaFolderScan) {
        QnResourceDirectoryBrowser::instance().setLocal(true);
        QStringList dirs;
        dirs << qnSettings->mediaFolder();
        dirs << qnSettings->extraMediaFolders();
        QnResourceDirectoryBrowser::instance().setPathCheckList(dirs);
        QnResourceDiscoveryManager::instance()->addDeviceServer(&QnResourceDirectoryBrowser::instance());
    }

    /* Initialize desktop camera searcher. */
#ifdef Q_OS_WIN
    QnDesktopResourceSearcher desktopSearcher(dynamic_cast<QGLWidget *>(mainWindow->viewport()));
    QnDesktopResourceSearcher::initStaticInstance(&desktopSearcher);
    QnDesktopResourceSearcher::instance().setLocal(true);
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnDesktopResourceSearcher::instance());
#endif

    QnResourceDiscoveryManager::instance()->setReady(true);
    QnResourceDiscoveryManager::instance()->start();

    result = application->exec();

    if(autoTester.state() == QnAutoTester::Finished) {
        if(!autoTester.succeeded())
            result = 1;

        cl_log.log(autoTester.message(), cl_logALWAYS);
    }

    QnSessionManager::instance()->stop();

    QnResource::stopCommandProc();
    QnResourceDiscoveryManager::instance()->stop();

    QnAppServerConnectionFactory::setEc2Connection( ec2::AbstractECConnectionPtr() );

    /* Write out settings. */
    qnSettings->setAudioVolume(QtvAudioDevice::instance()->volume());
    av_lockmgr_register(NULL);

    //restoring default message handler
    qInstallMessageHandler( defaultMsgHandler );

    return result;
}

#include <QtCore/QStandardPaths>
#include <QtCore/QString>

#include <QtCore/QStandardPaths>
#include <QtCore/QString>

int main(int argc, char **argv)
{
#ifdef Q_WS_X11
    XInitThreads();
#endif

#ifdef Q_OS_WIN
    AllowSetForegroundWindow(ASFW_ANY);
    win32_exception::installGlobalUnhandledExceptionHandler();
#endif

#ifdef Q_OS_MAC
    mac_setLimits();
#endif

    QScopedPointer<QtSingleApplication> application(new QtSingleApplication(argc, argv));

    // this is neccessary to prevent crashes when we want use QDesktopWidget from the non-main thread before any window has been created
    qApp->desktop();

    //adding exe dir to plugin search path
    QStringList pluginDirs = QCoreApplication::libraryPaths();
    pluginDirs << QCoreApplication::applicationDirPath();
    QCoreApplication::setLibraryPaths( pluginDirs );
#ifdef Q_OS_LINUX
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, lit("/etc/xdg"));
    QSettings::setPath(QSettings::NativeFormat, QSettings::SystemScope, lit("/etc/xdg"));
#endif

    QnClientModule client(argc, argv);

    QnSessionManager::instance();
    std::unique_ptr<QnCameraUserAttributePool> cameraUserAttributePool( new QnCameraUserAttributePool() );
    std::unique_ptr<QnMediaServerUserAttributesPool> mediaServerUserAttributesPool( new QnMediaServerUserAttributesPool() );
    QnResourcePool::initStaticInstance( new QnResourcePool() );

#ifdef Q_OS_MAC
    mac_restoreFileAccess();
#endif

    int result = runApplication(application.data(), argc, argv);

    delete QnResourcePool::instance();
    QnResourcePool::initStaticInstance( NULL );

#ifdef Q_OS_WIN
    QnDesktopResourceSearcher::initStaticInstance( NULL );
#endif

#ifdef Q_OS_MAC
    mac_stopFileAccess();
#endif

//    qApp->processEvents(); //TODO: #Elric crashes
    return result;
}

#endif // API_TEST_MAIN
#endif
