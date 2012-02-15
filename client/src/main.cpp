#include "version.h"
#include "ui/widgets/main_window.h"
#include "settings.h"

#include <QtSingleApplication>

#include "decoders/video/ipp_h264_decoder.h"

#include <utils/common/command_line_parser.h>
#include "ui/device_settings/dlg_factory.h"
#include "ui/style/skin.h"
#include "decoders/video/abstractdecoder.h"
#include "device_plugins/desktop/device/desktop_device_server.h"
#include "libavformat/avio.h"
#include "utils/common/util.h"
#include "plugins/resources/archive/avi_files/avi_resource.h"
#include "core/resourcemanagment/resource_discovery_manager.h"
#include "core/resourcemanagment/resource_pool.h"
#include "client_util.h"
#include "plugins/resources/arecontvision/resource/av_resource_searcher.h"
#include "api/AppServerConnection.h"
#include "device_plugins/server_camera/server_camera.h"
#include "device_plugins/server_camera/appserver.h"

#define TEST_RTSP_SERVER
//#define STANDALONE_MODE

#include "core/resource/video_server.h"
#include "core/resource/storage_resource.h"

#include <xercesc/util/PlatformUtils.hpp>
#include "plugins/resources/axis/axis_resource_searcher.h"
#include "eventmanager.h"
#include "core/resource/resource_directory_browser.h"

#include "tests/auto_tester.h"
#include "plugins/resources/d-link/dlink_resource_searcher.h"
#include "api/SessionManager.h"
#include "plugins/resources/droid/droid_resource_searcher.h"
#include "ui/menu/context_menu.h"

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
    avcodec_init();
    av_register_all();

    extern URLProtocol ufile_protocol;
    av_register_protocol2(&ufile_protocol, sizeof(ufile_protocol));
}

#ifdef TEST_RTSP_SERVER

void addTestFile(const QString& fileName, const QString& resId)
{
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
    static const char* DEFAULT_CONNECTION_NAME = "default";

    QUrl appServerUrl;

    QnSettings::ConnectionData lastUsedConnection = qnSettings->lastUsedConnection();

    bool hasDefaultConnection = false;
    QList<QnSettings::ConnectionData> connections = qnSettings->connections();
    int defaultConnectionIndex = -1;
    for (int i = 0; i < connections.size(); i++)
    {
        const QnSettings::ConnectionData& connection = connections[i];
        if (connection.name == DEFAULT_CONNECTION_NAME)
        {
            defaultConnectionIndex = i;
            break;
        }
    }

    // If there is default connection, use lastUsedConnection even it's invalid
    if (defaultConnectionIndex != -1 && qnSettings->isAfterFirstRun())
    {
        appServerUrl = lastUsedConnection.url;
    }
    // otherwise load default connection, add it as default and store it as last used
    // In case we run first time after installation, replace default connection with registry settings
    else
    {
        QSettings settings;
        appServerUrl.setScheme(QLatin1String("http"));
        appServerUrl.setHost(settings.value("appserverHost", QLatin1String(DEFAULT_APPSERVER_HOST)).toString());
        appServerUrl.setPort(settings.value("appserverPort", DEFAULT_APPSERVER_PORT).toInt());
        appServerUrl.setUserName(settings.value("appserverLogin", QLatin1String("appserver")).toString());
        appServerUrl.setPassword(settings.value("appserverPassword", QLatin1String("123")).toString());

        if (defaultConnectionIndex == -1)
        {
            QnSettings::ConnectionData connection;
            connection.name = DEFAULT_CONNECTION_NAME;
            connection.url = appServerUrl;
            connection.readOnly = true;

            connections.append(connection);
        } else
        {
            connections[defaultConnectionIndex].url = appServerUrl;
        }

        qnSettings->setConnections(connections);

        lastUsedConnection.url = appServerUrl;

        qnSettings->setLastUsedConnection(lastUsedConnection);
    }

    QnAppServerConnectionFactory::setDefaultUrl(appServerUrl);
    QnAppServerConnectionFactory::setDefaultFactory(&QnServerCameraFactory::instance());
}

void initAppServerEventConnection()
{
    QUrl appServerEventsUrl = QnAppServerConnectionFactory::defaultUrl();

    appServerEventsUrl.setPath("/events");

    static const int EVENT_RECONNECT_TIMEOUT = 3000;

    QnEventManager* eventManager = QnEventManager::instance();
    eventManager->init(appServerEventsUrl, EVENT_RECONNECT_TIMEOUT);
}

static QtMsgHandler defaultMsgHandler = 0;

static void myMsgHandler(QtMsgType type, const char *msg)
{
    if (defaultMsgHandler) {
        defaultMsgHandler(type, msg);
    } else { /* Default message handler. */
#ifdef _DEBUG
        QTextStream err(stderr);
        err << msg << endl << flush;
#endif
    }

    clLogMsgHandler(type, msg);
}

#ifndef API_TEST_MAIN
int main(int argc, char *argv[])
{
    /* Initialize everything. */
#ifdef Q_OS_WIN
    AllowSetForegroundWindow(ASFW_ANY);
#endif

    xercesc::XMLPlatformUtils::Initialize ();

//    av_log_set_callback(decoderLogCallback);

    QApplication::setOrganizationName(QLatin1String(ORGANIZATION_NAME));
    QApplication::setApplicationName(QLatin1String(APPLICATION_NAME));
    QApplication::setApplicationVersion(QLatin1String(APPLICATION_VERSION));


    /* Pre-parse command line. */
    QnAutoTester autoTester(argc, argv);

    QnCommandLineParser commandLinePreParser;
    commandLinePreParser.addParameter(QnCommandLineParameter(QnCommandLineParameter::FLAG, "--no-single-application", NULL, NULL));
    commandLinePreParser.parse(argc, argv);
    

    /* Create application instance. */
    QtSingleApplication *singleApplication = NULL;
    QScopedPointer<QApplication> application;
    if(commandLinePreParser.value("--no-single-application").toBool()) {
        application.reset(new QApplication(argc, argv));
    } else {
        singleApplication = new QtSingleApplication(argc, argv);
        application.reset(singleApplication);
    }
    application->setQuitOnLastWindowClosed(true);
    application->setWindowIcon(Skin::icon(QLatin1String("appicon.png")));

    if(singleApplication) {
        QString argsMessage;
        for (int i = 1; i < argc; ++i)
            argsMessage += fromNativePath(QFile::decodeName(argv[i])) + QLatin1Char('\n');

        while (singleApplication->isRunning()) {
            if (singleApplication->sendMessage(argsMessage))
                return 0;
        }
    }


    QDir::setCurrent(QFileInfo(QFile::decodeName(argv[0])).absolutePath());

    const QString dataLocation = getDataDirectory();
    if (!QDir().mkpath(dataLocation + QLatin1String("/log")))
        return 0;
    if (!cl_log.create(dataLocation + QLatin1String("/log/log_file"), 1024*1024*10, 5, cl_logDEBUG1))
        return 0;

#ifdef _DEBUG
    cl_log.setLogLevel(cl_logDEBUG1);
    //cl_log.setLogLevel(cl_logWARNING);
#else
    cl_log.setLogLevel(cl_logWARNING);
#endif

    CL_LOG(cl_logALWAYS)
    {
        cl_log.log(QLatin1String("\n\n========================================"), cl_logALWAYS);
        cl_log.log(cl_logALWAYS, "Software version %s", APPLICATION_VERSION);
        cl_log.log(QFile::decodeName(argv[0]), cl_logALWAYS);
    }

    defaultMsgHandler = qInstallMsgHandler(myMsgHandler);

    QnSettings *settings = qnSettings;
    settings->load();

	initAppServerConnection();
    initAppServerEventConnection();

	if (!settings->isAfterFirstRun() && !getMoviesDirectory().isEmpty())
        settings->addAuxMediaRoot(getMoviesDirectory());

    settings->save();

    cl_log.log(QLatin1String("Using ") + settings->mediaRoot() + QLatin1String(" as media root directory"), cl_logALWAYS);

    //qApp->exit();

    // Create and start SessionManager
    SessionManager* sm = SessionManager::instance();

    QThread *thread = new QThread();
    sm->moveToThread(thread);

    QObject::connect(sm, SIGNAL(destroyed()), thread, SLOT(quit()));
    QObject::connect(thread , SIGNAL(finished()), thread, SLOT(deleteLater()));

    thread->start();
    sm->start();
    //

    QnResource::startCommandProc();

    QnResourcePool::instance(); // to initialize net state;
    ffmpegInit();

    //===========================================================================

    CLVideoDecoderFactory::setCodecManufacture(CLVideoDecoderFactory::FFMPEG);

    QnServerCameraProcessor serverCameraProcessor;
    QnResourceDiscoveryManager::instance().setResourceProcessor(&serverCameraProcessor);

    //============================
    //QnResourceDirectoryBrowser
    QnResourceDirectoryBrowser::instance().setLocal(true);
    QStringList dirs;
    dirs << qnSettings->mediaRoot();
    dirs << qnSettings->auxMediaRoots();
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


#endif

    //CLDeviceManager::instance().getDeviceSearcher().addDeviceServer(&FakeDeviceServer::instance());
    //CLDeviceSearcher::instance()->addDeviceServer(&IQEyeDeviceServer::instance());

#ifdef Q_OS_WIN
//    QnResourceDiscoveryManager::instance().addDeviceServer(&DesktopDeviceServer::instance());
#endif // Q_OS_WIN

#ifndef STANDALONE_MODE
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnAppServerResourceSearcher::instance());
#endif

    QnResourceDiscoveryManager::instance().start();

    CLDeviceSettingsDlgFactory::initialize();

    //============================
    /*
    QnStorageResourcePtr storage0(new QnStorageResource());
    storage0->setUrl(getRecordingDir());
    storage0->setIndex(0);
    qnResPool->addResource(storage0);
    qnStorageMan->addStorage(storage0);
    */
    //=========================================================

    qApp->setStyle(Skin::style());

    /*qApp->setStyleSheet(QLatin1String(
            "QMenu {\n"
            "font-family: Bodoni MT;\n"
            "font-size: 18px;\n"
            "}"));*/

    /*qApp->setStyleSheet(QLatin1String(
        "QMenu {\n"
        "background-color: black;\n"
        "font-family: Bodoni MT;\n"
        "font-size: 18px;\n"
        "border: 1px solid rgb(110, 110, 110);\n"
        "}\n"
        "QMenu::item{\n"
        "color: white;\n"
        "padding-top: 4px;\n"
        "padding-left: 5px;\n"
        "padding-right: 15px;\n"
        "padding-bottom: 4px;\n"
        "}\n"
        "QMenu::item:disabled{\n"
        "color: rgb(110, 110, 110);\n"
        "}\n"
        "QMenu::item:selected {\n"
        "background: rgb(40, 40, 40);\n"
        "}\n"
        "QMenu::separator {\n"
        "height: 3px;\n"
        "background: rgb(20, 20, 20);\n"
        "}"));

    */
    //=========================================================

    /*
    QList<QSharedPointer<QnRecorder> > recorders =  qnResPool->getResourcesByType<QnRecorder>();
    foreach(QnResourcePtr dev, recorders)
    {
        QString id = dev->getUniqueId();
        CLSceneLayoutManager::instance().addRecorderLayoutContent(id);
    }
    */

    //=========================================================

    QnMainWindow mainWindow(argc, argv);
    mainWindow.setAttribute(Qt::WA_QuitOnClose);
    mainWindow.showFullScreen();
    mainWindow.show();

#ifdef TEST_RTSP_SERVER
    addTestData();
#endif

    if(singleApplication)
        QObject::connect(singleApplication, SIGNAL(messageReceived(const QString&)), &mainWindow, SLOT(handleMessage(const QString&)));

    QnEventManager::instance()->run();

    if(autoTester.tests() != 0 && autoTester.state() == QnAutoTester::INITIAL) {
        QObject::connect(&autoTester, SIGNAL(finished()), application.data(), SLOT(quit()));
        autoTester.start();
    }

    int result = application->exec();

    if(autoTester.state() == QnAutoTester::FINISHED) {
        if(!autoTester.succeeded())
            result = 1;

        QTextStream out(stdout);
        out << autoTester.message();
    }

    QnResource::stopCommandProc();
    QnResourceDiscoveryManager::instance().stop();

    xercesc::XMLPlatformUtils::Terminate();

    return result;
}
#endif // API_TEST_MAIN
#endif
