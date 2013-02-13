//#define QN_USE_VLD

#ifdef QN_USE_VLD
#   include <vld.h>
#endif

#ifdef Q_OS_LINUX
#   include <unistd.h>
#endif

#include "version.h"
#include "ui/widgets/main_window.h"
#include "utils/settings.h"

#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <QtCore/QTranslator>
#include <QtGui/QApplication>
#include <QtGui/QDesktopWidget>

#include <QtSingleApplication>

#include "decoders/video/ipp_h264_decoder.h"

#include <utils/common/command_line_parser.h>
#include "ui/workbench/workbench_context.h"
#include "ui/actions/action_manager.h"
#include "ui/style/skin.h"
#include "decoders/video/abstractdecoder.h"
#ifdef Q_OS_WIN
    #include "device_plugins/desktop_win_only/device/desktop_resource_searcher.h"
#endif
#include "libavformat/avio.h"
#include "utils/common/util.h"
#include "plugins/resources/archive/avi_files/avi_resource.h"
#include "core/resource_managment/resource_discovery_manager.h"
#include "core/resource_managment/resource_pool.h"
#include "plugins/resources/arecontvision/resource/av_resource_searcher.h"
#include "api/app_server_connection.h"
#include "device_plugins/server_camera/server_camera.h"
#include "device_plugins/server_camera/appserver.h"
#include "utils/util.h"

#define TEST_RTSP_SERVER
//#define STANDALONE_MODE

#include "core/resource/media_server_resource.h"
#include "core/resource/storage_resource.h"

#include "plugins/resources/axis/axis_resource_searcher.h"
#include "plugins/pluginmanager.h"
#include "core/resource/resource_directory_browser.h"

#include "tests/auto_tester.h"
#include "plugins/resources/d-link/dlink_resource_searcher.h"
#include "api/session_manager.h"
#include "plugins/resources/droid/droid_resource_searcher.h"
#include "ui/actions/action_manager.h"
#include "plugins/resources/iqinvision/iqinvision_resource_searcher.h"
#include "plugins/resources/droid_ipwebcam/ipwebcam_droid_resource_searcher.h"
#include "plugins/resources/isd/isd_resource_searcher.h"
//#include "plugins/resources/onvif/onvif_ws_searcher.h"
#include "utils/network/socket.h"
#include "utils/common/module_resources.h"


#include "plugins/storage/file_storage/qtfile_storage_resource.h"
#include "plugins/storage/file_storage/layout_storage_resource.h"
#include "core/resource/camera_history.h"
#include "client_message_processor.h"
#include "ui/workbench/workbench_translation_manager.h"

#ifdef Q_OS_LINUX
    #include "ui/workaround/x11_launcher_workaround.h"
#endif
#include "utils/common/cryptographic_hash.h"
#include "ui/style/globals.h"
#include "openal/qtvaudiodevice.h"
#include "ui/workaround/fglrx_full_screen.h"

#ifdef Q_OS_WIN
    #include "ui/workaround/iexplore_url_handler.h"
#endif

#include "ui/help/help_handler.h"
#include "client/client_module.h"
#include <client/client_connection_data.h>
#include "platform/platform_abstraction.h"


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
    resource->removeFlag(QnResource::local); // to initialize access to resource throught RTSP server
    resource->addFlag(QnResource::remote); // to initialize access to resource throught RTSP server
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
    resource2->removeFlag(QnResource::local); // to initialize access to resource throught RTSP server
    resource2->addFlag(QnResource::remote); // to initialize access to resource throught RTSP server
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

void initAppServerConnection()
{
    QUrl appServerUrl = qnSettings->lastUsedConnection().url;

    if(!appServerUrl.isValid())
        appServerUrl = qnSettings->defaultConnection().url;

    // TODO: #Ivan. Enable it when removing all places on receiving messages.
    // QnAppServerConnectionFactory::setClientGuid(QUuid::createUuid().toString());
    QnAppServerConnectionFactory::setDefaultUrl(appServerUrl);
    QnAppServerConnectionFactory::setDefaultFactory(&QnServerCameraFactory::instance());
}

static QtMsgHandler defaultMsgHandler = 0;

static void myMsgHandler(QtMsgType type, const char *msg)
{
    if (defaultMsgHandler) {
        defaultMsgHandler(type, msg);
    } else { /* Default message handler. */
#ifndef QN_NO_STDERR_MESSAGE_OUTPUT
        QTextStream err(stderr);
        err << msg << endl << flush;
#endif
    }

    qnLogMsgHandler( type, msg );
}

#ifdef Q_WS_X11
#include <X11/Xlib.h>
#endif

#ifndef API_TEST_MAIN

int qnMain(int argc, char *argv[])
{
    QnClientModule client(argc, argv);
    
#ifdef Q_WS_X11
	XInitThreads();
#endif

    QTextStream out(stdout);
    QThread::currentThread()->setPriority(QThread::HighestPriority);

#ifdef Q_OS_WIN
    AllowSetForegroundWindow(ASFW_ANY);
#endif

    /* Set up application parameters so that QSettings know where to look for settings. */
    QApplication::setOrganizationName(QLatin1String(QN_ORGANIZATION_NAME));
    QApplication::setApplicationName(QLatin1String(QN_APPLICATION_NAME));
    QApplication::setApplicationVersion(QLatin1String(QN_APPLICATION_VERSION));

    /* We don't want changes in desktop color settings to mess up our custom style. */
    QApplication::setDesktopSettingsAware(false);

    /* Parse command line. */
    QnAutoTester autoTester(argc, argv);

    qnSettings->updateFromCommandLine(argc, argv, stderr);

    QString devModeKey;
    bool noSingleApplication = false;
    int screen = -1;
    QString authenticationString, delayedDrop, instantDrop, logLevel;
    QString translationPath = qnSettings->translationPath();
    bool devBackgroundEditable = false;
    bool skipMediaFolderScan = false;
    
    QnCommandLineParser commandLineParser;
    commandLineParser.addParameter(&noSingleApplication,    "--no-single-application",      NULL,   QString());
    commandLineParser.addParameter(&authenticationString,   "--auth",                       NULL,   QString());
    commandLineParser.addParameter(&screen,                 "--screen",                     NULL,   QString());
    commandLineParser.addParameter(&delayedDrop,            "--delayed-drop",               NULL,   QString());
    commandLineParser.addParameter(&instantDrop,            "--instant-drop",               NULL,   QString());
    commandLineParser.addParameter(&logLevel,               "--log-level",                  NULL,   QString());
    commandLineParser.addParameter(&translationPath,        "--translation",                NULL,   QString());
    commandLineParser.addParameter(&devModeKey,             "--dev-mode-key",               NULL,   QString());
    commandLineParser.addParameter(&devBackgroundEditable,  "--dev-background-editable",    NULL,   QString());
    commandLineParser.addParameter(&skipMediaFolderScan,    "--skip-media-folder-scan",     NULL,   QString());
    commandLineParser.parse(argc, argv, stderr);

    /* Dev mode. */
    if(QnCryptographicHash::hash(devModeKey.toLatin1(), QnCryptographicHash::Md5) == QByteArray("\x4f\xce\xdd\x9b\x93\x71\x56\x06\x75\x4b\x08\xac\xca\x2d\xbc\x7f")) { /* MD5("razrazraz") */
        qnSettings->setDevMode(true);
        qnSettings->setBackgroundEditable(devBackgroundEditable);
    } else {
        qnSettings->setBackgroundAnimated(true);
        qnSettings->setBackgroundColor(qnGlobals->backgroundGradientColor());
    }

    /* Set authentication parameters from command line. */
    QUrl authentication = QUrl::fromUserInput(authenticationString);
    if(authentication.isValid()) {
        out << QObject::tr("Using authentication parameters from command line: %1.").arg(authentication.toString()) << endl;
        qnSettings->setLastUsedConnection(QnConnectionData(QString(), authentication));
    }

    /* Create application instance. */
    QtSingleApplication *singleApplication = NULL;
    QScopedPointer<QApplication> application;
    if(noSingleApplication) {
        application.reset(new QApplication(argc, argv));
    } else {
        singleApplication = new QtSingleApplication(argc, argv);
        application.reset(singleApplication);
    }
    application->setQuitOnLastWindowClosed(true);
    application->setWindowIcon(qnSkin->icon("window_icon.png"));

    QScopedPointer<QnPlatformAbstraction> platform(new QnPlatformAbstraction());
    platform->monitor()->totalPartitionSpaceInfo();

#ifdef Q_WS_X11
 //   QnX11LauncherWorkaround x11LauncherWorkaround;
 //   application->installEventFilter(&x11LauncherWorkaround);
#endif

#ifdef Q_OS_WIN
    QnIexploreUrlHandler iexploreUrlHanderWorkaround;
    // all effects are placed in the constructor
    Q_UNUSED(iexploreUrlHanderWorkaround)
#endif

    if(singleApplication) {
        QString argsMessage;
        for (int i = 1; i < argc; ++i)
            argsMessage += fromNativePath(QFile::decodeName(argv[i])) + QLatin1Char('\n');

        while (singleApplication->isRunning()) {
            if (singleApplication->sendMessage(argsMessage))
                return 0;
        }
    }

    /* Initialize connections. */
    initAppServerConnection();
    qnSettings->save();
    cl_log.log(QLatin1String("Using ") + qnSettings->mediaFolder() + QLatin1String(" as media root directory"), cl_logALWAYS);


    /* Initialize application instance. */
    application->setStartDragDistance(20);
    QnWorkbenchTranslationManager::installTranslation(translationPath);
    QDir::setCurrent(QFileInfo(QFile::decodeName(argv[0])).absolutePath());

    
    /* Initialize sound. */
    QtvAudioDevice::instance()->setVolume(qnSettings->audioVolume());


    /* Initialize log. */
    const QString dataLocation = getDataDirectory();
    if (!QDir().mkpath(dataLocation + QLatin1String("/log")))
        return 0;
    if (!cl_log.create(dataLocation + QLatin1String("/log/log_file"), 1024*1024*10, 5, cl_logDEBUG1))
        return 0;


    QnHelpHandler helpHandler;
    qApp->installEventFilter(&helpHandler);


    QnLog::initLog(logLevel);
    cl_log.log(QN_APPLICATION_NAME, " started", cl_logALWAYS);
    cl_log.log("Software version: ", QN_APPLICATION_VERSION, cl_logALWAYS);
    cl_log.log("binary path: ", QFile::decodeName(argv[0]), cl_logALWAYS);

    defaultMsgHandler = qInstallMsgHandler(myMsgHandler);


    // Create and start SessionManager
    QnSessionManager* sm = QnSessionManager::instance();
    QThread *thread = new QThread(); // TODO: leaking thread.
    sm->moveToThread(thread);
    QObject::connect(sm, SIGNAL(destroyed()), thread, SLOT(quit()));
    QObject::connect(thread , SIGNAL(finished()), thread, SLOT(deleteLater()));
    thread->start();
    sm->start();

    QnResourcePool::instance(); // to initialize net state;
    ffmpegInit();

    //===========================================================================

    CLVideoDecoderFactory::setCodecManufacture( CLVideoDecoderFactory::AUTO );

    QnServerCameraProcessor serverCameraProcessor;
    QnResourceDiscoveryManager::init(new QnResourceDiscoveryManager());
    QnResourceDiscoveryManager::instance()->setResourceProcessor(&serverCameraProcessor);

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

#ifdef STANDALONE_MODE
    QnPlArecontResourceSearcher::instance().setLocal(true);
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnPlArecontResourceSearcher::instance());

    QnPlAxisResourceSearcher::instance().setLocal(true);
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnPlAxisResourceSearcher::instance());

    QnPlDlinkResourceSearcher::instance().setLocal(true);
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnPlDlinkResourceSearcher::instance());

    QnPlDroidResourceSearcher::instance().setLocal(true);
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnPlDroidResourceSearcher::instance());

    QnPlIqResourceSearcher::instance().setLocal(true);
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnPlIqResourceSearcher::instance());

    //QnPlIpWebCamResourceSearcher::instance().setLocal(true);
    //QnResourceDiscoveryManager::instance()->addDeviceServer(&QnPlIpWebCamResourceSearcher::instance());

    QnPlISDResourceSearcher::instance().setLocal(true);
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnPlISDResourceSearcher::instance());

    QnPlOnvifWsSearcher::instance().setLocal(true);
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnPlOnvifWsSearcher::instance());

    QnPlPulseSearcher::instance().setLocal(true);
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnPlPulseSearcher::instance());
    
#endif

#ifdef Q_OS_WIN
//    QnResourceDiscoveryManager::instance()->addDeviceServer(&DesktopDeviceServer::instance());
#endif // Q_OS_WIN
    QnResourceDiscoveryManager::instance()->start();

    qApp->setStyle(qnSkin->style());

    /* Create workbench context. */
    QScopedPointer<QnWorkbenchContext> context(new QnWorkbenchContext(qnResPool));
    context->instance<QnFglrxFullScreen>(); /* Init fglrx workaround. */

    /* Create main window. */
    QScopedPointer<QnMainWindow> mainWindow(new QnMainWindow(context.data()));
    mainWindow->setAttribute(Qt::WA_QuitOnClose);

    if(screen != -1) {
        QDesktopWidget *desktop = qApp->desktop();
        if(screen >= 0 && screen < desktop->screenCount()) {
            QPoint screenDelta = mainWindow->pos() - desktop->screenGeometry(mainWindow.data()).topLeft();

            mainWindow->move(desktop->screenGeometry(screen).topLeft() + screenDelta);
        }
    }

    mainWindow->show();
    context->action(Qn::EffectiveMaximizeAction)->trigger();

    //initializing plugin manager. TODO supply plugin dir (from settings)
    PluginManager::instance()->loadPlugins();

    /* Process input files. */
    for (int i = 1; i < argc; ++i)
        mainWindow->handleMessage(QFile::decodeName(argv[i]));
    if(singleApplication)
        QObject::connect(singleApplication, SIGNAL(messageReceived(const QString &)), mainWindow.data(), SLOT(handleMessage(const QString &)));

#ifdef TEST_RTSP_SERVER
    addTestData();
#endif

    if(autoTester.tests() != 0 && autoTester.state() == QnAutoTester::INITIAL) {
        QObject::connect(&autoTester, SIGNAL(finished()), application.data(), SLOT(quit()));
        autoTester.start();
    }

    /* Process pending events before executing actions. */
    qApp->processEvents();

    if (argc <= 1) {
        /* If no input files were supplied --- open connection settings dialog. */
        if(!authentication.isValid()) {
            context->menu()->trigger(Qn::ConnectToServerAction);
        } else {
            context->menu()->trigger(Qn::ReconnectAction);
        }
    }

    /* Drop resources if needed. */
    if(!delayedDrop.isEmpty()) {
        qnSettings->setLayoutsOpenedOnLogin(false);

        QByteArray data = QByteArray::fromBase64(delayedDrop.toLatin1());
        context->menu()->trigger(Qn::DelayedDropResourcesAction, QnActionParameters().withArgument(Qn::SerializedResourcesParameter, data));
    }

    if (!instantDrop.isEmpty()){
        qnSettings->setLayoutsOpenedOnLogin(false);

        QByteArray data = QByteArray::fromBase64(instantDrop.toLatin1());
        context->menu()->trigger(Qn::InstantDropResourcesAction, QnActionParameters().withArgument(Qn::SerializedResourcesParameter, data));
    }

#ifdef _DEBUG
    /* Show FPS in debug. */
    context->menu()->trigger(Qn::ShowFpsAction);
#endif

    int result = application->exec();

    if(autoTester.state() == QnAutoTester::FINISHED) {
        if(!autoTester.succeeded())
            result = 1;

        out << autoTester.message();
    }

    QnClientMessageProcessor::instance()->stop();
    QnSessionManager::instance()->stop();

    QnResource::stopCommandProc();
    QnResourceDiscoveryManager::instance()->stop();

    /* Write out settings. */
    qnSettings->setAudioVolume(QtvAudioDevice::instance()->volume());
    av_lockmgr_register(NULL);
    return result;
}

int main(int argc, char *argv[]) {
    // TODO: this is an ugly hack for a problem with threads not being stopped before globals are destroyed.

    int result = qnMain(argc, argv);
#if defined(Q_OS_WIN)
    Sleep(3000);
#elif defined(Q_OS_LINUX)
    sleep(3);
#endif
    return result;
}

#endif // API_TEST_MAIN
#endif
