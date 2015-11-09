//#define QN_USE_VLD

#ifdef QN_USE_VLD
#   include <vld.h>
#endif

#include <qglobal.h>

#ifdef Q_OS_LINUX
#   include <unistd.h>
#endif

#ifdef Q_WS_X11
#include <X11/Xlib.h>
#endif

#include <iostream>

#include <utils/common/app_info.h>
#include "ui/widgets/main_window.h"
#include <api/global_settings.h>


#include <QtCore/QStandardPaths>
#include <QtCore/QString>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <QtCore/QTranslator>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>
#include <QtGui/QDesktopServices>
#include <QScopedPointer>
#include <QtSingleApplication>

extern "C"
{
    #include <libavformat/avformat.h>
    #include <libavformat/avio.h>
}

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>
#include <client/client_module.h>
#include <client/client_connection_data.h>
#include <client/client_resource_processor.h>
#include <client/client_startup_parameters.h>

#include "core/resource/media_server_resource.h"
#include "core/resource/storage_resource.h"
#include "core/resource/resource_directory_browser.h"
#include <core/resource_management/resource_pool.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/resource/client_camera_factory.h>

#include "decoders/video/ipp_h264_decoder.h"

#include <utils/common/log.h>
#include <utils/common/command_line_parser.h>
#include <utils/network/http/http_mod_manager.h>
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

#include "api/app_server_connection.h"

#include "plugins/plugin_manager.h"

#ifdef _DEBUG
#include "tests/auto_tester.h"
#endif

#include "api/session_manager.h"
#include "ui/actions/action_manager.h"
#include "utils/network/socket.h"


#include "plugins/storage/file_storage/qtfile_storage_resource.h"
#include "plugins/storage/file_storage/layout_storage_resource.h"
#include "core/resource/camera_history.h"

#ifdef Q_OS_LINUX
    #include "ui/workaround/x11_launcher_workaround.h"
    #include "common/systemexcept_linux.h"
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

#include "utils/common/long_runnable.h"

#include "text_to_wav.h"
#include "common/common_module.h"
#include "ui/style/noptix_style.h"
#include "ui/customization/customizer.h"
#include "ui/dialogs/message_box.h"
#include <nx_ec/ec2_lib.h>
#include <nx_ec/dummy_handler.h>
#include <utils/network/module_finder.h>
#include <utils/network/router.h>
#include <api/network_proxy_factory.h>
#include <utils/server_interface_watcher.h>
#include <utils/network/socket_global.h>

#ifdef Q_OS_MAC
#include "ui/workaround/mac_utils.h"
#endif
#include "api/runtime_info_manager.h"
#include <utils/common/timermanager.h>

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

static int lockmgr(void **mtx, enum AVLockOp op)
{
    QnMutex** qMutex = (QnMutex**) mtx;
    switch(op) {
        case AV_LOCK_CREATE:
            *qMutex = new QnMutex();
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

/** Initialize log. */
void initLog(
    QString logLevel,
    const QString fileNameSuffix,
    QString ec2TranLogLevel)
{
    static const int DEFAULT_MAX_LOG_FILE_SIZE = 10*1024*1024;
    static const int DEFAULT_MSG_LOG_ARCHIVE_SIZE = 5;

    const QString dataLocation = QStandardPaths::writableLocation(QStandardPaths::DataLocation);

    if( logLevel.isEmpty() )
        logLevel = qnSettings->logLevel();

    QnLog::initLog(logLevel);
    QString logFileLocation = dataLocation + QLatin1String("/log");
    QString logFileName = logFileLocation + QLatin1String("/log_file") + fileNameSuffix;
    if (!QDir().mkpath(logFileLocation))
        cl_log.log(lit("Could not create log folder: ") + logFileLocation, cl_logALWAYS);
    if (!cl_log.create(logFileName, DEFAULT_MAX_LOG_FILE_SIZE, DEFAULT_MSG_LOG_ARCHIVE_SIZE, QnLog::instance()->logLevel()))
        cl_log.log(lit("Could not create log file") + logFileName, cl_logALWAYS);
    cl_log.log(QLatin1String("================================================================================="), cl_logALWAYS);

    if( ec2TranLogLevel.isEmpty() )
        ec2TranLogLevel = qnSettings->ec2TranLogLevel();

    //preparing transaction log
    if( ec2TranLogLevel != lit("none") )
    {
        QnLog::instance(QnLog::EC2_TRAN_LOG)->create(
            dataLocation + QLatin1String("/log/ec2_tran"),
            DEFAULT_MAX_LOG_FILE_SIZE,
            DEFAULT_MSG_LOG_ARCHIVE_SIZE,
            QnLog::logLevelFromString(ec2TranLogLevel) );
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("================================================================================="), cl_logALWAYS);
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("================================================================================="), cl_logALWAYS);
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("================================================================================="), cl_logALWAYS);
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("%1 started").arg(qApp->applicationName()), cl_logALWAYS );
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("Software version: %1").arg(QCoreApplication::applicationVersion()), cl_logALWAYS);
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("Software revision: %1").arg(QnAppInfo::applicationRevision()), cl_logALWAYS);
    }
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

#ifndef API_TEST_MAIN
//#define ENABLE_DYNAMIC_CUSTOMIZATION

int runApplication(QtSingleApplication* application, int argc, char **argv) {
    // these functions should be called in every thread that wants to use rand() and qrand()
    srand(::time(NULL));
    qsrand(::time(NULL));

    int result = 0;

    QThread::currentThread()->setPriority(QThread::HighestPriority);

    QnStartupParameters startupParams = QnStartupParameters::fromCommandLineArg(argc, argv);

    QnClientModule client(startupParams);

    /// TODO: #ynikitenkov move other initialization to QnClientModule constructor

    /* Parse command line. */
#ifdef _DEBUG
    QnAutoTester autoTester(argc, argv);
#endif

    ec2::DummyHandler dummyEc2RequestHandler;

    nx_http::HttpModManager httpModManager;

    /* Dev mode. */
    if(QnCryptographicHash::hash(startupParams.devModeKey.toLatin1(), QnCryptographicHash::Md5) 
        == QByteArray("\x4f\xce\xdd\x9b\x93\x71\x56\x06\x75\x4b\x08\xac\xca\x2d\xbc\x7f")) { /* MD5("razrazraz") */
        qnRuntime->setDevMode(true);
    }

    qnRuntime->setSoftwareYuv(startupParams.softwareYuv);
    qnRuntime->setShowFullInfo(startupParams.showFullInfo);

    if (!startupParams.engineVersion.isEmpty()) {
        QnSoftwareVersion version(startupParams.engineVersion);
        if (!version.isNull()) {
            qWarning() << "Starting with overridden version: " << version.toString();
            qnCommon->setEngineVersion(version);
        }
    }

    QString logFileNameSuffix;
    if (!startupParams.customUri.isNull()) {
        startupParams.allowMultipleClientInstances = true;


    } else if (!startupParams.videoWallGuid.isNull()) {
        qnRuntime->setVideoWallMode(true);
        startupParams.allowMultipleClientInstances= true;
        startupParams.fullScreenDisabled = true;
        startupParams.versionMismatchCheckDisabled = true;
        qnRuntime->setLightModeOverride(Qn::LightModeVideoWall);

        logFileNameSuffix = startupParams.videoWallItemGuid.isNull() 
            ? startupParams.videoWallGuid.toString() 
            : startupParams.videoWallItemGuid.toString();
        logFileNameSuffix.replace(QRegExp(lit("[{}]")), lit("_"));
    }

    initLog(startupParams.logLevel, logFileNameSuffix, startupParams.ec2TranLogLevel);

    // TODO: #mu ON/OFF switch in settings?
    nx::SocketGlobals::mediatorConnector().enable(true);

	// TODO: #Elric why QString???
    if (!startupParams.lightMode.isEmpty() && startupParams.videoWallGuid.isNull()) {
        bool ok;
        Qn::LightModeFlags lightModeOverride(startupParams.lightMode.toInt(&ok));
        if (ok)
            qnRuntime->setLightModeOverride(lightModeOverride);
        else
            qnRuntime->setLightModeOverride(Qn::LightModeFull);
    }

    //TODO: #GDM fix it
    /* Here the value from LightModeOverride will be copied to LightMode */
    QnPerformanceTest::detectLightMode();

#ifdef Q_OS_MACX
    if (mac_isSandboxed())
        qnSettings->setLightMode(qnSettings->lightMode() | Qn::LightModeNoNewWindow);
#endif

    qnSettings->setVSyncEnabled(!startupParams.vsyncDisabled);

    qnSettings->setClientUpdateDisabled(startupParams.clientUpdateDisabled);

#ifdef ENABLE_DYNAMIC_CUSTOMIZATION
    QString skinRoot = dynamicCustomizationPath.isEmpty() 
        ? lit(":") 
        : dynamicCustomizationPath;

    QString customizationPath = qnSettings->clientSkin() == Qn::LightSkin 
        ? skinRoot + lit("/skin_light") 
        : skinRoot + lit("/skin_dark");
    QScopedPointer<QnSkin> skin(new QnSkin(QStringList() << skinRoot + lit("/skin") << customizationPath));
#else
    QString customizationPath = qnSettings->clientSkin() == Qn::LightSkin ? lit(":/skin_light") : lit(":/skin_dark");
    QScopedPointer<QnSkin> skin(new QnSkin(QStringList() << lit(":/skin") << customizationPath));
#endif // ENABLE_DYNAMIC_CUSTOMIZATION


    

    QnCustomization customization;
    customization.add(QnCustomization(skin->path("customization_common.json")));
    customization.add(QnCustomization(skin->path("customization_base.json")));
    customization.add(QnCustomization(skin->path("customization_child.json")));

    QScopedPointer<QnCustomizer> customizer(new QnCustomizer(customization));
    customizer->customize(qnGlobals);

    /* Initialize application instance. */
    QApplication::setQuitOnLastWindowClosed(true);
    QApplication::setWindowIcon(qnSkin->icon("window_icon.png"));
    
    QApplication::setStyle(skin->newStyle()); // TODO: #Elric here three qWarning's are issued (bespin bug), qnDeleteLater with null receiver
#ifdef Q_OS_MACX
    application->setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
#endif

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

    if(!startupParams.allowMultipleClientInstances) {
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



    /* Initialize connections. */
    if (!startupParams.videoWallGuid.isNull()) {
        QnAppServerConnectionFactory::setVideowallGuid(startupParams.videoWallGuid);
        QnAppServerConnectionFactory::setInstanceGuid(startupParams.videoWallItemGuid);
    }

    std::unique_ptr<ec2::AbstractECConnectionFactory> ec2ConnectionFactory(
        getConnectionFactory( startupParams.videoWallGuid.isNull() ? Qn::PT_DesktopClient : Qn::PT_VideowallClient ) );
    ec2::ResourceContext resCtx(
        QnClientCameraFactory::instance(),
        qnResPool,
        qnResTypePool );
	ec2ConnectionFactory->setContext( resCtx );
    QnAppServerConnectionFactory::setEC2ConnectionFactory( ec2ConnectionFactory.get() );

    ec2::ApiRuntimeData runtimeData;
    runtimeData.peer.id = qnCommon->moduleGUID();
    runtimeData.peer.instanceId = qnCommon->runningInstanceGUID();
    runtimeData.peer.peerType = startupParams.videoWallItemGuid.isNull()
        ? Qn::PT_DesktopClient
        : Qn::PT_VideowallClient;
    runtimeData.brand = QnAppInfo::productNameShort();
    runtimeData.videoWallInstanceGuid = startupParams.videoWallItemGuid;
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

    cl_log.log(qApp->applicationDisplayName(), " started", cl_logALWAYS);
    cl_log.log("Software version: ", QApplication::applicationVersion(), cl_logALWAYS);
    cl_log.log("binary path: ", QFile::decodeName(argv[0]), cl_logALWAYS);

    defaultMsgHandler = qInstallMessageHandler(myMsgHandler);

    ffmpegInit();

    QScopedPointer<QnModuleFinder> moduleFinder(new QnModuleFinder(true, qnRuntime->isDevMode()));
    moduleFinder->start();

    QScopedPointer<QnRouter> router(new QnRouter(moduleFinder.data()));

    QScopedPointer<QnServerInterfaceWatcher> serverInterfaceWatcher(new QnServerInterfaceWatcher(router.data()));

    // ===========================================================================

    CLVideoDecoderFactory::setCodecManufacture( CLVideoDecoderFactory::AUTO );

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
    Qt::WindowFlags flags = qnRuntime->isVideoWallMode()
            ? Qt::FramelessWindowHint | Qt::X11BypassWindowManagerHint
            : static_cast<Qt::WindowFlags>(0);
    QScopedPointer<QnMainWindow> mainWindow(new QnMainWindow(context.data(), NULL, flags));
    context->setMainWindow(mainWindow.data());
    mainWindow->setAttribute(Qt::WA_QuitOnClose);
    application->setActivationWindow(mainWindow.data());

    if (startupParams.screen != QnStartupParameters::kInvalidScreen) {
        QDesktopWidget *desktop = qApp->desktop();
        if (startupParams.screen >= 0 && startupParams.screen < desktop->screenCount()) {
            QPoint screenDelta = mainWindow->pos() - desktop->screenGeometry(mainWindow.data()).topLeft();
            mainWindow->move(desktop->screenGeometry(startupParams.screen).topLeft() + screenDelta);
        }
    }

    mainWindow->show();
    if (!startupParams.fullScreenDisabled)
        context->action(Qn::EffectiveMaximizeAction)->trigger();
    else
        mainWindow->updateDecorationsState();

    if(startupParams.versionMismatchCheckDisabled)
        context->action(Qn::VersionMismatchMessageAction)->setVisible(false); // TODO: #Elric need a better mechanism for this

    /* Process input files. */
    bool haveInputFiles = false;
    {
        bool skipArg = true;
        for (const auto& arg: qApp->arguments())
        {
            if (!skipArg)
                haveInputFiles |= mainWindow->handleMessage(arg);
            skipArg = false;
        }
    }

    if(!startupParams.allowMultipleClientInstances)
        QObject::connect(application, SIGNAL(messageReceived(const QString &)), mainWindow.data(), SLOT(handleMessage(const QString &)));

#ifdef _DEBUG
    if(autoTester.tests() != 0 && autoTester.state() == QnAutoTester::Initial) {
        QObject::connect(&autoTester, SIGNAL(finished()), application, SLOT(quit()));
        autoTester.start();
    }
#endif

    /* Process pending events before executing actions. */
    qApp->processEvents();

    // show beta version warning message for the main instance only
    if (!startupParams.allowMultipleClientInstances &&
        !qnRuntime->isDevMode() &&
        QnAppInfo::beta())
        context->action(Qn::BetaVersionMessageAction)->trigger();

#ifdef _DEBUG
    /* Show FPS in debug. */
    context->menu()->trigger(Qn::ShowFpsAction);
#endif

    /************************************************************************/
    /* Initializing resource searchers                                      */
    /************************************************************************/
    QnClientResourceProcessor resourceProcessor;
    std::unique_ptr<QnResourceDiscoveryManager> resourceDiscoveryManager( new QnResourceDiscoveryManager() );
    resourceProcessor.moveToThread( QnResourceDiscoveryManager::instance() );
    QnResourceDiscoveryManager::instance()->setResourceProcessor(&resourceProcessor);

    // ============================
    //QnResourceDirectoryBrowser
    if(!startupParams.skipMediaFolderScan) {
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


    if (!startupParams.customUri.isEmpty()) {
        /* Set authentication parameters from uri. */
        QUrl appServerUrl = QUrl::fromUserInput(startupParams.customUri);
        context->menu()->trigger(Qn::ConnectAction, QnActionParameters().withArgument(Qn::UrlRole, appServerUrl));
    } 
    /* If no input files were supplied --- open connection settings dialog.   
     * Do not try to connect in the following cases:
     * * we were not connected and clicked "Open in new window"
     * * we have opened exported exe-file
     * Otherwise we should try to connect or show Login Dialog.
     */
    else if (startupParams.instantDrop.isEmpty() && !haveInputFiles) {
        /* Set authentication parameters from command line. */
        QUrl appServerUrl = QUrl::fromUserInput(startupParams.authenticationString);
        if (!startupParams.videoWallGuid.isNull()) {
            Q_ASSERT(appServerUrl.isValid());
            if (!appServerUrl.isValid()) {
                return -1;
            }
            appServerUrl.setUserName(startupParams.videoWallGuid.toString());
        }
        context->menu()->trigger(Qn::ConnectAction, QnActionParameters().withArgument(Qn::UrlRole, appServerUrl));
    }

    if (!startupParams.videoWallGuid.isNull()) {
        context->menu()->trigger(Qn::DelayedOpenVideoWallItemAction, QnActionParameters()
                             .withArgument(Qn::VideoWallGuidRole, startupParams.videoWallGuid)
                             .withArgument(Qn::VideoWallItemGuidRole, startupParams.videoWallItemGuid));
    } else if(!startupParams.delayedDrop.isEmpty()) { /* Drop resources if needed. */
        Q_ASSERT(startupParams.instantDrop.isEmpty());

        QByteArray data = QByteArray::fromBase64(startupParams.delayedDrop.toLatin1());
        context->menu()->trigger(Qn::DelayedDropResourcesAction, QnActionParameters().withArgument(Qn::SerializedDataRole, data));
    } else if (!startupParams.instantDrop.isEmpty()){
        QByteArray data = QByteArray::fromBase64(startupParams.instantDrop.toLatin1());
        context->menu()->trigger(Qn::InstantDropResourcesAction, QnActionParameters().withArgument(Qn::SerializedDataRole, data));
    }

    result = application->exec();

#ifdef _DEBUG
    if(autoTester.state() == QnAutoTester::Finished) {
        if(!autoTester.succeeded())
            result = 1;

        cl_log.log(autoTester.message(), cl_logALWAYS);
    }
#endif

    QnResourceDiscoveryManager::instance()->stop();

    QnAppServerConnectionFactory::setEc2Connection(NULL);

    ec2ConnectionFactory.reset();

    QnAppServerConnectionFactory::setUrl(QUrl());

    /* Write out settings. */
    qnSettings->setAudioVolume(QtvAudioDevice::instance()->volume());
    av_lockmgr_register(NULL);

    //restoring default message handler
    qInstallMessageHandler( defaultMsgHandler );

#ifdef Q_OS_WIN
    QnDesktopResourceSearcher::initStaticInstance( NULL );
#endif

    return result;
}

int main(int argc, char **argv)
{
#ifdef Q_WS_X11
    XInitThreads();
#endif

#ifdef Q_OS_WIN
    AllowSetForegroundWindow(ASFW_ANY);
    win32_exception::installGlobalUnhandledExceptionHandler();
#endif

#ifdef Q_OS_LINUX
    linux_exception::installCrashSignalHandler();
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
     

#ifdef Q_OS_MAC
    mac_restoreFileAccess();
#endif

    int result = runApplication(application.data(), argc, argv);


#ifdef Q_OS_MAC
    mac_stopFileAccess();
#endif

    return result;
}

#endif // API_TEST_MAIN
