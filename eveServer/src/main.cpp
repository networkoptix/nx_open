#include <QApplication>
#include <QDir>

#include "version.h"
#include "utils/common/util.h"
#include "plugins/resources/archive/avi_files/avi_device.h"
#include "core/resourcemanagment/asynch_seacher.h"
#include "core/resourcemanagment/resource_pool.h"
#include "rtsp/rtsp_listener.h"
#include "plugins/resources/arecontvision/resource/av_resource_searcher.h"
#include "recorder/recording_manager.h"
#include "recording/storage_manager.h"
#include "api/AppServerConnection.h"
#include <QAuthenticator>
#include "recording/file_deletor.h"
#include "rest/server/rest_server.h"

//#include "device_plugins/arecontvision/devices/av_device_server.h"

//#define TEST_RTSP_SERVER


QMutex global_ffmpeg_mutex;

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
    server->startRTSPListener();
    qnResPool->addResource(QnResourcePtr(server));

    QnAviResourcePtr resource(new QnAviResource("E:/Users/roman76r/video/ROCKNROLLA/BDMV/STREAM/00000.m2ts"));
    resource->removeFlag(QnResource::local); // to initialize access to resource throught RTSP server
    resource->addFlag(QnResource::remove); // to initialize access to resource throught RTSP server
    resource->setParentId(server->getId());
    qnResPool->addResource(QnResourcePtr(resource));

    QnFakeCameraPtr testCamera(new QnFakeCamera());
    testCamera->setParentId(server->getId());
    testCamera->setMAC(QnMacAddress("00000"));
    testCamera->setUrl("00000");
    testCamera->setName("testCamera");
    qnResPool->addResource(QnResourcePtr(testCamera));
    */

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
//    av_log_set_callback(decoderLogCallback);

    QApplication::setOrganizationName(QLatin1String(ORGANIZATION_NAME));
    QApplication::setApplicationName(QLatin1String(APPLICATION_NAME));
    QApplication::setApplicationVersion(QLatin1String(APPLICATION_VERSION));

    QApplication application(argc, argv);

    QString argsMessage;
    for (int i = 1; i < argc; ++i)
    {
        argsMessage += fromNativePath(QString::fromLocal8Bit(argv[i]));
        if (i < argc-1)
            argsMessage += QLatin1Char('\0'); // ### QString doesn't support \0 in the string
    }

    /*
    while (application.isRunning())
    {
        if (application.sendMessage(argsMessage))
            return 0;
    }
    */

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

    /*
    Settings& settings = Settings::instance();
    settings.load(getDataDirectory() + QLatin1String("/settings.xml"));

    if (!settings.isAfterFirstRun() && !getMoviesDirectory().isEmpty())
        settings.addAuxMediaRoot(getMoviesDirectory());

    settings.save();

    cl_log.log(QLatin1String("Using ") + settings.mediaRoot() + QLatin1String(" as media root directory"), cl_logALWAYS);
    */

    QnResource::startCommandProc();

    QnResourcePool::instance(); // to initialize net state;
    ffmpegInit();


    // ------------------------------------------
#ifdef TEST_RTSP_SERVER
    addTestData();
#endif

    QHostAddress host("10.0.2.3");
    QAuthenticator auth;
    auth.setUser("appserver");
    auth.setPassword("123");
    QnAppServerConnection appServerConnection(host, auth, QnResourceDiscoveryManager::instance());


    QList<QnResourceTypePtr> resourceTypeList;
    appServerConnection.getResourceTypes(resourceTypeList);
    qnResTypePool->addResourceTypeList(resourceTypeList);


    QnRtspListener rtspListener(QHostAddress::Any, 50000);
    rtspListener.start();

    QnRestServer restServer(QHostAddress::Any, DEFAULT_REST_PORT);


    QnStoragePtr storage0(new QnStorage());
    storage0->setUrl("g:/records");
    storage0->setIndex(0);
    storage0->setSpaceLimit(238500ll * 1000 * 1024);
    qnResPool->addResource(storage0);
    qnStorageMan->addStorage(storage0);

    QnFileDeletor fileDeletor(storage0->getUrl()); // constructor got root folder for temp files

    QnRecordingManager::instance()->start();
    // ------------------------------------------


    //===========================================================================
    //IPPH264Decoder::dll.init();

    //============================
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlArecontResourceSearcher::instance());
    QnResourceDiscoveryManager::instance().start();
    //CLDeviceManager::instance().getDeviceSearcher().addDeviceServer(&FakeDeviceServer::instance());
    //CLDeviceSearcher::instance()->addDeviceServer(&IQEyeDeviceServer::instance());
    
    int result = application.exec();

    QnResource::stopCommandProc();
    QnResourceDiscoveryManager::instance().stop();

    return result;
}
#endif // API_TEST_MAIN
#endif
