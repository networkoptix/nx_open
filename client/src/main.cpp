//#include <vld.h>
#include "eve_app.h"

#include "version.h"
#include "mainwnd.h"
#include "serial.h"
#include "settings.h"

#include "decoders/video/ipp_h264_decoder.h"

#include "ui/device_settings/dlg_factory.h"
#include "ui/device_settings/plugins/arecontvision/arecont_dlg.h"
#include "ui/video_cam_layout/layout_manager.h"
#include "ui/context_menu_helper.h"
#include "decoders/video/abstractdecoder.h"
#include "device_plugins/desktop/device/desktop_device_server.h"
#include "libavformat/avio.h"
#include "utils/common/util.h"
#include "plugins/resources/archive/avi_files/avi_device.h"
#include "core/resourcemanagment/asynch_seacher.h"
#include "core/resourcemanagment/resource_pool.h"
#include "recording/storage_manager.h"
#include "client_util.h"
#include "plugins/resources/arecontvision/resource/av_resource_searcher.h"
#include "api/AppServerConnection.h"
#include "device_plugins/server_camera/server_camera.h"
#include "device_plugins/server_camera/appserver.h"
#include "device/video_server.h"

#include "core/resource/file_resource.h"

#define TEST_RTSP_SERVER

#include "ui/scene/scenecontroller.h"



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

#include "ui/videoitem/timeslider.h"

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
    
    QnVideoServerPtr server(new QnVideoServer());
    server->setUrl("rtsp://localhost:50000");
    //server->startRTSPListener();
    qnResPool->addResource(QnResourcePtr(server));

    /*
    QnAviResourcePtr resource(new QnAviResource("E:/Users/roman76r/video/ROCKNROLLA/BDMV/STREAM/00000.m2ts"));
    resource->removeFlag(QnResource::local); // to initialize access to resource throught RTSP server
    resource->addFlag(QnResource::remove); // to initialize access to resource throught RTSP server
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

    QnServerCameraPtr testCamera(new QnServerCamera());
    testCamera->setParentId(server->getId());
    testCamera->setMAC(QnMacAddress("00-1A-07-00-9A-2F"));
    testCamera->setName("testCamera");
    //qnResPool->addResource(QnResourcePtr(testCamera));

    
    addTestFile("e:/Users/roman76r/blake/3PM PRIVATE SESSION, HOLLYWOOD Jayme.flv", "q1");
    addTestFile("e:/Users/roman76r/blake/8 FEATURE PREMIERE_Paid Companions_Bottled-Up_h.wmv", "q2");
    addTestFile("e:/Users/roman76r/blake/9  FEATURE PREMIERE_Paid Compan_Afternoon Whores.wmv", "q3");
    addTestFile("e:/Users/roman76r/blake/A CUT ABOVE Aria & Justine.flv", "q4");
    addTestFile("e:/Users/roman76r/blake/A DOLL'S LIFE Jacqueline, Nika & Jade.flv", "q5");
    

    /*
    QnAviResourcePtr resource2(new QnAviResource("C:/Users/physic/Videos/HighDef_-_Audio_-_Japan.avi"));
    resource2->removeFlag(QnResource::local); // to initialize access to resource throught RTSP server
    resource2->addFlag(QnResource::remove); // to initialize access to resource throught RTSP server
    resource2->setParentId(server->getId());
    qnResPool->addResource(QnResourcePtr(resource2));
    */
}
#endif

#ifndef API_TEST_MAIN
int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    AllowSetForegroundWindow(ASFW_ANY);
#endif

//    av_log_set_callback(decoderLogCallback);

    QApplication::setOrganizationName(QLatin1String(ORGANIZATION_NAME));
    QApplication::setApplicationName(QLatin1String(APPLICATION_NAME));
    QApplication::setApplicationVersion(QLatin1String(APPLICATION_VERSION));

    EveApplication application(argc, argv);

    QString argsMessage;
    for (int i = 1; i < argc; ++i)
    {
        argsMessage += fromNativePath(QString::fromLocal8Bit(argv[i]));
        if (i < argc-1)
            argsMessage += QLatin1Char('\0'); // ### QString doesn't support \0 in the string
    }

    while (application.isRunning())
    {
        if (application.sendMessage(argsMessage))
            return 0;
    }

    QString dataLocation = getDataDirectory();
    QDir::setCurrent(QFileInfo(QFile::decodeName(argv[0])).absolutePath());

    QDir dataDirectory;
    dataDirectory.mkpath(dataLocation + QLatin1String("/log"));

    if (!cl_log.create(dataLocation + QLatin1String("/log/log_file"), 1024*1024*10, 5, cl_logDEBUG1))
    {
        application.quit();

        return 0;
    }

#ifdef _DEBUG
     //cl_log.setLogLevel(cl_logDEBUG1);
    cl_log.setLogLevel(cl_logWARNING);
#else
    cl_log.setLogLevel(cl_logWARNING);
#endif

    CL_LOG(cl_logALWAYS)
    {
        cl_log.log(QLatin1String("\n\n========================================"), cl_logALWAYS);
        cl_log.log(cl_logALWAYS, "Software version %s", APPLICATION_VERSION);
        cl_log.log(QFile::decodeName(argv[0]), cl_logALWAYS);
    }

    Settings& settings = Settings::instance();
    settings.load(getDataDirectory() + QLatin1String("/settings.xml"));

    if (!settings.isAfterFirstRun() && !getMoviesDirectory().isEmpty())
        settings.addAuxMediaRoot(getMoviesDirectory());

    settings.save();

    cl_log.log(QLatin1String("Using ") + settings.mediaRoot() + QLatin1String(" as media root directory"), cl_logALWAYS);

    QHostAddress host(QSettings().value("appserverAddress", "10.0.2.3").toString());
    int port = 8000;
    QAuthenticator auth;
    auth.setUser("appserver");
    auth.setPassword("123");
    QnAppServerConnection appServerConnection(host, port, auth, QnServerCameraFactory::instance());


    QList<QnResourceTypePtr> resourceTypeList;
    if (appServerConnection.getResourceTypes(resourceTypeList) == 0)
    {
        qnResTypePool->addResourceTypeList(resourceTypeList);
        qDebug() << "Got " << resourceTypeList.size() << " resource types";

        // The following code works. Commented just to not create new entities on each run.
#if 0
        QnLocalFileResource server("server name");
        QList<QnResourcePtr> servers;

        if (appServerConnection.addServer(server, servers) == 0)
        {
            qDebug() << "Added server";
        } else
        {
            qDebug() << "Can't add server";
        }

        QnLocalFileResource camera("server name");
        camera.setTypeId(QnId("17"));
        QList<QnResourcePtr> cameras;

        if (appServerConnection.addCamera(camera, servers[0]->getId(), cameras) == 0)
        {
            qDebug() << "Added camera";
        } else
        {
            qDebug() << "Can't add camera";
        }
#endif
    } else
    {
        qDebug() << "Can't get resource types";
    }

    qApp->exit();

    QnResource::startCommandProc();

    QnResourcePool::instance(); // to initialize net state;
    ffmpegInit();

    //===========================================================================
    //IPPH264Decoder::dll.init();

    CLVideoDecoderFactory::setCodecManufacture(CLVideoDecoderFactory::FFMPEG);

    QnServerCameraProcessor serverCameraProcessor;

    QnResourceDiscoveryManager::instance().addResourceProcessor(&serverCameraProcessor);

    //============================
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlArecontResourceSearcher::instance());
    QnResourceDiscoveryManager::instance().start();
    //CLDeviceManager::instance().getDeviceSearcher().addDeviceServer(&FakeDeviceServer::instance());
    //CLDeviceSearcher::instance()->addDeviceServer(&IQEyeDeviceServer::instance());
    
#ifdef Q_OS_WIN
    QnResourceDiscoveryManager::instance().addDeviceServer(&DesktopDeviceServer::instance());
#endif // Q_OS_WIN

    QnResourceDiscoveryManager::instance().addDeviceServer(&QnAppServerResourceSearcher::instance());

    CLDeviceSettingsDlgFactory::registerDlgManufacture(&AreconVisionDlgManufacture::instance());
    //============================

    qApp->setQuitOnLastWindowClosed(true);

    QnStoragePtr storage0(new QnStorage());
    storage0->setUrl(getRecordingDir());
    storage0->setIndex(0);
    qnResPool->addResource(storage0);
    qnStorageMan->addStorage(storage0);

    //=========================================================

#ifndef Q_OS_DARWIN
    qApp->setStyle(QLatin1String("Bespin"));
#endif

    qApp->setStyleSheet(QLatin1String(
            "QMenu {\n"
            "font-family: Bodoni MT;\n"
            "font-size: 18px;\n"
            "}"));

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

    initContextMenu();
    
#if 0
    QWidget *widget = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    QGraphicsScene *scene = new QGraphicsScene(widget);
    QGraphicsView *view = new QGraphicsView(scene);
    SceneController *controller = new SceneController(view, scene, widget);
    layout->addWidget(view);
    widget->resize(800, 600);
    widget->show();

#else
    MainWnd mainWindow(argc, argv);
    mainWindow.show();

#ifdef TEST_RTSP_SERVER
    addTestData();
#endif

    QObject::connect(&application, SIGNAL(messageReceived(const QString&)),
        &mainWindow, SLOT(handleMessage(const QString&)));
#endif

    int result = application.exec();

    QnResource::stopCommandProc();
    QnResourceDiscoveryManager::instance().stop();

    return result;
}
#endif // API_TEST_MAIN
#endif
