//#define QN_USE_VLD

#ifdef QN_USE_VLD
#   include <vld.h>
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
#include "device_plugins/desktop/device/desktop_device_server.h"
#include "libavformat/avio.h"
#include "utils/common/util.h"
#include "plugins/resources/archive/avi_files/avi_resource.h"
#include "core/resourcemanagment/resource_discovery_manager.h"
#include "core/resourcemanagment/resource_pool.h"
#include "utils/client_util.h"
#include "plugins/resources/arecontvision/resource/av_resource_searcher.h"
#include "api/app_server_connection.h"
#include "device_plugins/server_camera/server_camera.h"
#include "device_plugins/server_camera/appserver.h"
#include "utils/util.h"

#define TEST_RTSP_SERVER
//#define STANDALONE_MODE

#include "core/resource/video_server_resource.h"
#include "core/resource/storage_resource.h"

#include "plugins/resources/axis/axis_resource_searcher.h"
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
#include "ui/workbench/workbench_tranlation_manager.h"

#ifdef Q_WS_X11
    #include "utils/app_focus_listener.h"
    #include "utils/wmctrl.h"
#endif

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

void ffmpegInit()
{
    //avcodec_init();
    av_register_all();

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
    QnVideoServerPtr server(new QnVideoServer());
    server->setUrl("rtsp://localhost:50000");
    server->setApiUrl("rtsp://localhost:8080");
    //server->startRTSPListener();
    qnResPool->addResource(QnResourcePtr(server));

    QnServerCameraPtr testCamera(new QnServerCamera());
    testCamera->setParentId(server->getId());
    testCamera->setMAC(QnMacAddress("00-1A-07-00-A5-76"));
    testCamera->setName("testCamera");
    qnResPool->addResource(QnResourcePtr(testCamera));
    */

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

    // TODO: Ivan. Enable it when removing all places on receiving messages.
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

#ifndef API_TEST_MAIN

int main(int argc, char *argv[])
{
    QN_INIT_MODULE_RESOURCES(common);

    QTextStream out(stdout);
    QThread::currentThread()->setPriority(QThread::HighestPriority);

#ifdef Q_OS_WIN
    AllowSetForegroundWindow(ASFW_ANY);
#endif

    /* Set up application parameters so that QSettings know where to look for settings. */
    QApplication::setOrganizationName(QLatin1String(ORGANIZATION_NAME));
    QApplication::setApplicationName(QLatin1String(APPLICATION_NAME));
    QApplication::setApplicationVersion(QLatin1String(APPLICATION_VERSION));

    /* Parse command line. */
    QnAutoTester autoTester(argc, argv);

    qnSettings->updateFromCommandLine(argc, argv, stderr);

    bool noSingleApplication = false;
    int screen = -1;
    QString authenticationString, delayedDrop, logLevel;
    QString translationPath = qnSettings->translationPath();
    
    QnCommandLineParser commandLineParser;
    commandLineParser.addParameter(&noSingleApplication,    "--no-single-application",  NULL,   QString(),    true);
    commandLineParser.addParameter(&authenticationString,   "--auth",                   NULL,   QString());
    commandLineParser.addParameter(&screen,                 "--screen",                 NULL,   QString());
    commandLineParser.addParameter(&delayedDrop,            "--delayed-drop",           NULL,   QString());
    commandLineParser.addParameter(&logLevel,               "--log-level",              NULL,   QString());
    commandLineParser.addParameter(&translationPath,        "--translation",            NULL,   QString());
    commandLineParser.parse(argc, argv, stderr);

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

#ifdef Q_WS_X11
    QnAppFocusListener *appFocusListener = new QnAppFocusListener();
    application->installEventFilter(appFocusListener);
    appFocusListener->hideLauncher();
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

    const QString dataLocation = getDataDirectory();
    if (!QDir().mkpath(dataLocation + QLatin1String("/log")))
        return 0;
    if (!cl_log.create(dataLocation + QLatin1String("/log/log_file"), 1024*1024*10, 5, cl_logDEBUG1))
        return 0;


    QnLog::initLog(logLevel);
    cl_log.log(APPLICATION_NAME, " started", cl_logALWAYS);
    cl_log.log("Software version: ", APPLICATION_VERSION, cl_logALWAYS);
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

    CLVideoDecoderFactory::setCodecManufacture( CLVideoDecoderFactory::FFMPEG );

    QnServerCameraProcessor serverCameraProcessor;
    QnResourceDiscoveryManager::instance().setResourceProcessor(&serverCameraProcessor);

    //============================
    //QnResourceDirectoryBrowser
    QnResourceDirectoryBrowser::instance().setLocal(true);
    QStringList dirs;
    dirs << qnSettings->mediaFolder();
    dirs << qnSettings->extraMediaFolders();
    QnResourceDirectoryBrowser::instance().setPathCheckList(dirs);
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnResourceDirectoryBrowser::instance());

#ifdef STANDALONE_MODE
    QnPlArecontResourceSearcher::instance().setLocal(true);
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlArecontResourceSearcher::instance());

    QnPlAxisResourceSearcher::instance().setLocal(true);
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlAxisResourceSearcher::instance());

    QnPlDlinkResourceSearcher::instance().setLocal(true);
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlDlinkResourceSearcher::instance());

    QnPlDroidResourceSearcher::instance().setLocal(true);
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlDroidResourceSearcher::instance());

    QnPlIqResourceSearcher::instance().setLocal(true);
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlIqResourceSearcher::instance());

    //QnPlIpWebCamResourceSearcher::instance().setLocal(true);
    //QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlIpWebCamResourceSearcher::instance());

    QnPlISDResourceSearcher::instance().setLocal(true);
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlISDResourceSearcher::instance());

    QnPlOnvifWsSearcher::instance().setLocal(true);
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlOnvifWsSearcher::instance());

    QnPlPulseSearcher::instance().setLocal(true);
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlPulseSearcher::instance());
    
#endif

#ifdef Q_OS_WIN
//    QnResourceDiscoveryManager::instance().addDeviceServer(&DesktopDeviceServer::instance());
#endif // Q_OS_WIN
    QnResourceDiscoveryManager::instance().start();

    qApp->setStyle(qnSkin->style());

#if 0
    // todo: remove me! debug only
    QnCameraHistoryPtr history1(new QnCameraHistory());
    qint64 dt = QDateTime::fromString("2012-04-12T19:19:00", Qt::ISODate).toMSecsSinceEpoch();
    QnCameraTimePeriod period1(dt-1000*60*30, 1000*60*30, 2);
    QnCameraTimePeriod period2(dt, 3600*24*1000ll, 43);
    history1->setMacAddress("00-40-8C-BF-92-CE");
    history1->addTimePeriod(period1);
    history1->addTimePeriod(period2);
    QnCameraHistoryPool::instance()->addCameraHistory(history1);
#endif

    QScopedPointer<QnWorkbenchContext> context(new QnWorkbenchContext(qnResPool));
    QScopedPointer<QnMainWindow> mainWindow(new QnMainWindow(context.data()));
    mainWindow->setAttribute(Qt::WA_QuitOnClose);

    if(screen != -1) {
        QDesktopWidget *desktop = qApp->desktop();
        if(screen >= 0 && screen < desktop->screenCount()) {
            QPoint screenDelta = mainWindow->pos() - desktop->screenGeometry(mainWindow.data()).topLeft();

            mainWindow->move(desktop->screenGeometry(screen).topLeft() + screenDelta);
        }
    }

    mainWindow->showFullScreen();
    mainWindow->show();
        
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
    QnResourceDiscoveryManager::instance().stop();

#ifdef Q_WS_X11
    delete appFocusListener;
#endif

    return result;
}
#endif // API_TEST_MAIN
#endif



