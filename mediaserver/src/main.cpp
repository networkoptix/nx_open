#include <qtsinglecoreapplication.h>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <QtCore/QUrl>
#include <QtCore/QUuid>

#include <QtNetwork/QUdpSocket>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QHostInfo>
#include <QtNetwork/QNetworkInterface>

#include "version.h"
#include "utils/common/util.h"
#include "plugins/resources/archive/avi_files/avi_resource.h"
#include "core/resourcemanagment/resource_discovery_manager.h"
#include "core/resourcemanagment/resource_pool.h"
#include "utils/common/sleep.h"
#include "rtsp/rtsp_listener.h"
#include "plugins/resources/arecontvision/resource/av_resource_searcher.h"
#include "recorder/recording_manager.h"
#include "recorder/storage_manager.h"
#include "api/AppServerConnection.h"
#include "appserver/processor.h"
#include "recording/file_deletor.h"
#include "rest/server/rest_server.h"
#include "rest/handlers/recorded_chunks.h"
#include "core/resource/video_server.h"
#include "api/SessionManager.h"
#include <signal.h>
#include "core/misc/scheduleTask.h"
#include "qtservice.h"
#include "eventmanager.h"
#include "settings.h"

#include <fstream>
#include "plugins/resources/axis/axis_resource_searcher.h"
#include "plugins/resources/d-link/dlink_resource_searcher.h"
#include "utils/common/log.h"
#include "camera/camera_pool.h"
#include "plugins/resources/iqinvision/iqinvision_resource_searcher.h"
#include "serverutil.h"
#include "plugins/resources/droid_ipwebcam/ipwebcam_droid_resource_searcher.h"
#include "plugins/resources/droid/droid_resource_searcher.h"
#include "plugins/resources/isd/isd_resource_searcher.h"
#include "plugins/resources/test_camera/testcamera_resource_searcher.h"
#include "plugins/resources/onvif/onvif_ws_searcher.h"
#include "utils/common/command_line_parser.h"
#include "plugins/resources/pulse/pulse_resource_searcher.h"
//#include "plugins/storage/file_storage/file_storage_protocol.h"
#include "plugins/storage/file_storage/file_storage_resource.h"
#include "plugins/storage/coldstore/coldstore_storage.h"
#include "main.h"
#include "rest/handlers/fs_checker.h"


static const char SERVICE_NAME[] = "Network Optix VMS Media Server";

class QnMain;
static QnMain* serviceMainInstance = 0;
void stopServer(int signal);

//#include "device_plugins/arecontvision/devices/av_device_server.h"

//#define TEST_RTSP_SERVER

static const int DEFAUT_RTSP_PORT = 50000;

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

QHostAddress resolveHost(const QString& hostString)
{
    QHostAddress host(hostString);
    if (host.toIPv4Address() != 0)
        return host;

    QHostInfo info = QHostInfo::fromName(hostString);

    // Can't resolve
    if (info.error() != QHostInfo::NoError)
    {
        cl_log.log("Couldn't resolve host ", hostString, cl_logERROR);
        return QHostAddress();
    }

    // Initialize to zero
    host = QHostAddress();
    foreach (const QHostAddress &address, info.addresses())
    {
        if (address.toIPv4Address() != 0)
        {
            host = address;
            break;
        }
    }

    if (host.toIPv4Address() == 0)
        cl_log.log("No ipv4 address associated with host ", hostString, cl_logERROR);

    return host;
}


QList<QHostAddress> allLocalAddresses()
{
    QList<QHostAddress> rez;

    // if nothing else works use first enabled hostaddr
    QList<QHostAddress> ipaddrs = getAllIPv4Addresses();

    for (int i = 0; i < ipaddrs.size();++i)
    {
        QString addr = ipaddrs.at(i).toString();
        bool isLocalAddress = addr == "localhost" || addr == "127.0.0.1";
        if (isLocalAddress || !QUdpSocket().bind(ipaddrs.at(i), 0))
            continue;
        rez << ipaddrs.at(i);
    }
    if (rez.isEmpty())
        rez << QHostAddress("127.0.0.1");

    return rez;
}

QString defaultLocalAddress(const QHostAddress& target)
{
    {
        QUdpSocket socket;
        socket.connectToHost(target, 53);

        if (socket.localAddress() != QHostAddress::LocalHost)
            return socket.localAddress().toString(); // if app server is on other computer we use same address as used to connect to app server
    }

    {
        // try select default interface
        QUdpSocket socket;
        socket.connectToHost("8.8.8.8", 53);
        QString result = socket.localAddress().toString();

        if (result.length()>0)
            return result;
    }


    {
        // if nothing else works use first enabled hostaddr
        QList<QHostAddress> ipaddrs = getAllIPv4Addresses();

        for (int i = 0; i < ipaddrs.size();++i)
        {
            QUdpSocket socket;
            if (!socket.bind(ipaddrs.at(i), 0))
                continue;

            QString result = socket.localAddress().toString();

            Q_ASSERT(result.length() > 0 );

            return result;
        }
    }

    return "127.0.0.1";

}

void ffmpegInit()
{
    //avcodec_init();
    av_register_all();

    QnStoragePluginFactory::instance()->registerStoragePlugin("file", QnFileStorageResource::instance, true); // true means use it plugin if no <protocol>:// prefix
    QnStoragePluginFactory::instance()->registerStoragePlugin("coldstore", QnPlColdStoreStorage::instance, false); // true means use it plugin if no <protocol>:// prefix
}

QnAbstractStorageResourcePtr createDefaultStorage()
{
    //QnStorageResourcePtr storage(new QnStorageResource());
    QnAbstractStorageResourcePtr storage(QnStoragePluginFactory::instance()->createStorage("ufile"));
    storage->setName("Initial");
#ifdef Q_OS_WIN
    storage->setUrl(QDir::fromNativeSeparators(qSettings.value("mediaDir", "c:/records").toString()));
#else
    storage->setUrl(QDir::fromNativeSeparators(qSettings.value("mediaDir", "/tmp/vmsrecords").toString()));
#endif
    storage->setSpaceLimit(5ll * 1000000000);

    cl_log.log("Creating new storage", cl_logINFO);

    return storage;
}

void setServerNameAndUrls(QnVideoServerResourcePtr server, const QString& myAddress)
{
    if (server->getName().isEmpty())
        server->setName(QString("Server ") + myAddress);

#ifdef _TEST_TWO_SERVERS
    server->setUrl(QString("rtsp://") + myAddress + QString(':') + QString::number(55001));
    server->setApiUrl(QString("http://") + myAddress + QString(':') + QString::number(55002));
#else
    server->setUrl(QString("rtsp://") + myAddress + QString(':') + qSettings.value("rtspPort", DEFAUT_RTSP_PORT).toString());
    server->setApiUrl(QString("http://") + myAddress + QString(':') + qSettings.value("apiPort", DEFAULT_REST_PORT).toString());
#endif
}

QnVideoServerResourcePtr createServer()
{
    QnVideoServerResourcePtr serverPtr(new QnVideoServerResource());
    serverPtr->setGuid(serverGuid());

    serverPtr->setStorages(QnAbstractStorageResourceList() << createDefaultStorage());

    return serverPtr;
}

QnVideoServerResourcePtr findServer(QnAppServerConnectionPtr appServerConnection)
{
    QnVideoServerResourceList servers;

    while (servers.isEmpty())
    {
        QByteArray errorString;
        if (appServerConnection->getServers(servers, errorString) == 0)
            break;

        qDebug() << "findServer(): Call to registerServer failed. Reason: " << errorString;
        QnSleep::msleep(1000);
    }

    foreach(QnVideoServerResourcePtr server, servers)
    {
        if (server->getGuid() == serverGuid())
            return server;
    }

    return QnVideoServerResourcePtr();
}

QnVideoServerResourcePtr registerServer(QnAppServerConnectionPtr appServerConnection, QnVideoServerResourcePtr serverPtr)
{
    QnVideoServerResourceList servers;
    QByteArray errorString;
    if (appServerConnection->registerServer(serverPtr, servers, errorString) != 0)
    {
        qDebug() << "registerServer(): Call to registerServer failed. Reason: " << errorString;

        return QnVideoServerResourcePtr();
    }

    return servers.at(0);
}

#ifdef Q_OS_WIN
#include <windows.h>
#include <stdio.h>
BOOL WINAPI stopServer_WIN(DWORD dwCtrlType)
{
    stopServer(dwCtrlType);
    return true;
}
#endif

static QtMsgHandler defaultMsgHandler = 0;

static void myMsgHandler(QtMsgType type, const char *msg)
{
    if (defaultMsgHandler)
        defaultMsgHandler(type, msg);

    qnLogMsgHandler(type, msg);
}

int serverMain(int argc, char *argv[])
{
    Q_UNUSED(argc)
    Q_UNUSED(argv)

#ifdef Q_OS_WIN
    SetConsoleCtrlHandler(stopServer_WIN, true);
#endif
    signal(SIGINT, stopServer);
    signal(SIGABRT, stopServer);
    signal(SIGTERM, stopServer);

//    av_log_set_callback(decoderLogCallback);

    QCoreApplication::setOrganizationName(QLatin1String(ORGANIZATION_NAME));
    QCoreApplication::setApplicationName(QLatin1String(APPLICATION_NAME));
    QCoreApplication::setApplicationVersion(QLatin1String(APPLICATION_VERSION));

    QString dataLocation = getDataDirectory();
    QDir::setCurrent(QFileInfo(QFile::decodeName(qApp->argv()[0])).absolutePath());

    QDir dataDirectory;
    dataDirectory.mkpath(dataLocation + QLatin1String("/log"));

    QString logFileName = dataLocation + QLatin1String("/log/log_file");
    qSettings.setValue("logFile", logFileName);

    if (!cl_log.create(logFileName, 1024*1024*10, 5, cl_logDEBUG1))
    {
        qApp->quit();

        return 0;
    }

    QnCommandLineParser commandLinePreParser;
    commandLinePreParser.addParameter(QnCommandLineParameter(QnCommandLineParameter::String, "--log-level", NULL, NULL));
    commandLinePreParser.parse(argc, argv);

    QnLog::initLog(commandLinePreParser.value("--log-level").toString());
    cl_log.log(APPLICATION_NAME, " started", cl_logALWAYS);
    cl_log.log("Software version: ", APPLICATION_VERSION, cl_logALWAYS);
	cl_log.log("Software revision: ", APPLICATION_REVISION, cl_logALWAYS);
    cl_log.log("binary path: ", QFile::decodeName(argv[0]), cl_logALWAYS);

    defaultMsgHandler = qInstallMsgHandler(myMsgHandler);

    ffmpegInit();

    // ------------------------------------------
#ifdef TEST_RTSP_SERVER
    addTestData();
#endif

    QDir stateDirectory;
    stateDirectory.mkpath(dataLocation + QLatin1String("/state"));
    qnFileDeletor->init(dataLocation + QLatin1String("/state")); // constructor got root folder for temp files

    return 0;
}

void initAppServerConnection(const QSettings &settings)
{
    QUrl appServerUrl;

    // ### remove
    appServerUrl.setScheme(QLatin1String("http"));
    appServerUrl.setHost(settings.value("appserverHost", QLatin1String(DEFAULT_APPSERVER_HOST)).toString());
    appServerUrl.setPort(settings.value("appserverPort", DEFAULT_APPSERVER_PORT).toInt());
    appServerUrl.setUserName(settings.value("appserverLogin", QLatin1String("admin")).toString());
    appServerUrl.setPassword(settings.value("appserverPassword", QLatin1String("123")).toString());

    cl_log.log("Connect to enterprise controller server ", appServerUrl.toString(), cl_logINFO);
    QnAppServerConnectionFactory::setDefaultUrl(appServerUrl);
    QnAppServerConnectionFactory::setDefaultFactory(&QnResourceDiscoveryManager::instance());
}

void initAppServerEventConnection(const QSettings &settings, const QnVideoServerResourcePtr& mediaServer)
{
    QUrl appServerEventsUrl;

    // ### remove
    appServerEventsUrl.setScheme(QLatin1String("http"));
    appServerEventsUrl.setHost(settings.value("appserverHost", QLatin1String(DEFAULT_APPSERVER_HOST)).toString());
    appServerEventsUrl.setPort(settings.value("appserverPort", DEFAULT_APPSERVER_PORT).toInt());
    appServerEventsUrl.setUserName(settings.value("appserverLogin", QLatin1String("admin")).toString());
    appServerEventsUrl.setPassword(settings.value("appserverPassword", QLatin1String("123")).toString());
    appServerEventsUrl.setPath("/events");
    appServerEventsUrl.addQueryItem("id", mediaServer->getId().toString());

    static const int EVENT_RECONNECT_TIMEOUT = 3000;

    QnEventManager* eventManager = QnEventManager::instance();
    eventManager->init(appServerEventsUrl, EVENT_RECONNECT_TIMEOUT);
}

QnMain::QnMain(int argc, char* argv[])
    : m_argc(argc),
    m_argv(argv),
    m_processor(0),
    m_rtspListener(0),
    m_restServer(0)
{
    serviceMainInstance = this;
}

QnMain::~QnMain()
{
    quit();
    stop();
    stopObjects();
}

void QnMain::stopObjects()
{
    if (m_restServer)
    {
        delete m_restServer;
        m_restServer = 0;
    }

    if (m_rtspListener)
    {
        delete m_rtspListener;
        m_rtspListener = 0;
    }

    if (m_processor)
    {
        delete m_processor;
        m_processor = 0;
    }
}

void QnMain::loadResourcesFromECS()
{
    QnAppServerConnectionPtr appServerConnection = QnAppServerConnectionFactory::createConnection();

    QByteArray errorString;

    QnVirtualCameraResourceList cameras;
    while (appServerConnection->getCameras(cameras, m_videoServer->getId(), errorString) != 0)
    {
        qDebug() << "QnMain::run(): Can't get cameras. Reason: " << errorString;
        QnSleep::msleep(1000);
    }
    foreach(const QnSecurityCamResourcePtr &camera, cameras)
    {
        QnResourcePtr ownResource = qnResPool->getResourceById(camera->getId());
        if (ownResource)
            ownResource->update(camera);
        else
            qnResPool->addResource(camera);
    }

    QnCameraHistoryList cameraHistoryList;
    while (appServerConnection->getCameraHistoryList(cameraHistoryList, errorString) != 0)
    {
        qDebug() << "QnMain::run(): Can't get cameras history. Reason: " << errorString;
        QnSleep::msleep(1000);
    }

    foreach(QnCameraHistoryPtr history, cameraHistoryList)
    {
        QnCameraHistoryPool::instance()->addCameraHistory(history);
    }
}

void QnMain::run()
{
    // Create SessionManager
    SessionManager* sm = SessionManager::instance();

    QThread *thread = new QThread();
    sm->moveToThread(thread);

    QObject::connect(sm, SIGNAL(destroyed()), thread, SLOT(quit()));
    QObject::connect(thread , SIGNAL(finished()), thread, SLOT(deleteLater()));

    thread->start();
    sm->start();

    initAppServerConnection(qSettings);

    QnAppServerConnectionPtr appServerConnection = QnAppServerConnectionFactory::createConnection();

    while (!needToStop() && !initResourceTypes(appServerConnection))
    {
        QnSleep::msleep(1000);
    }


    while (!needToStop() && !initLicenses(appServerConnection))
    {
        QnSleep::msleep(1000);
    }

    if (needToStop())
        return;

    QnResource::startCommandProc();

    QnResourcePool::instance(); // to initialize net state;

    QString appserverHostString = qSettings.value("appserverHost", QLatin1String(DEFAULT_APPSERVER_HOST)).toString();

    QHostAddress appserverHost;
    do
    {
        appserverHost = resolveHost(appserverHostString);
    } while (appserverHost.toIPv4Address() == 0);

    while (m_videoServer.isNull())
    {
        QnVideoServerResourcePtr server = findServer(appServerConnection);

        if (!server)
            server = createServer();

        setServerNameAndUrls(server, defaultLocalAddress(appserverHost));
        server->setNetAddrList(allLocalAddresses());

        if (server->getStorages().isEmpty())
            server->setStorages(QnAbstractStorageResourceList() << createDefaultStorage());

        m_videoServer = registerServer(appServerConnection, server);
        if (m_videoServer.isNull())
            QnSleep::msleep(1000);
    }

    QByteArray errorString;

    int status;
    do
    {
        status = appServerConnection->setResourceStatus(m_videoServer->getId(), QnResource::Online, errorString);
    } while (status != 0);

    initAppServerEventConnection(qSettings, m_videoServer);
    QnEventManager* eventManager = QnEventManager::instance();
    eventManager->run();

    m_processor = new QnAppserverResourceProcessor(m_videoServer->getId());

    QUrl rtspUrl(m_videoServer->getUrl());
    QUrl apiUrl(m_videoServer->getApiUrl());

    m_restServer = new QnRestServer(QHostAddress::Any, apiUrl.port());
    m_restServer->registerHandler("api/RecordedTimePeriods", new QnRecordedChunkListHandler());
    m_restServer->registerHandler("api/CheckPath", new QnFsHelperHandler());

    foreach (QnAbstractStorageResourcePtr storage, m_videoServer->getStorages())
    {
        qnResPool->addResource(storage);
        QnStorageResourcePtr physicalStorage = qSharedPointerDynamicCast<QnStorageResource>(storage);
        if (physicalStorage)
            qnStorageMan->addStorage(physicalStorage);
    }
    qnStorageMan->loadFullFileCatalog();

    QnRecordingManager::instance()->start();
    qnResPool->addResource(m_videoServer);

    // ------------------------------------------


    //===========================================================================
    //IPPH264Decoder::dll.init();

    //============================
    QnResourceDiscoveryManager::instance().setServer(true);
    QnResourceDiscoveryManager::instance().setResourceProcessor(m_processor);
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlArecontResourceSearcher::instance());
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlAxisResourceSearcher::instance());
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlDlinkResourceSearcher::instance());
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlIqResourceSearcher::instance());

    QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlIpWebCamResourceSearcher::instance());
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlDroidResourceSearcher::instance());
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlISDResourceSearcher::instance());

    QnResourceDiscoveryManager::instance().addDeviceServer(&QnTestCameraResourceSearcher::instance());
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlOnvifWsSearcher::instance());
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlPulseSearcher::instance());

    //

    connect(qnResPool, SIGNAL(statusChanged(QnResourcePtr)), m_processor, SLOT(onResourceStatusChanged(QnResourcePtr)));

    //CLDeviceManager::instance().getDeviceSearcher().addDeviceServer(&FakeDeviceServer::instance());
    //CLDeviceSearcher::instance()->addDeviceServer(&IQEyeDeviceServer::instance());

    loadResourcesFromECS();

    connect(eventManager, SIGNAL(connectionReset()), this, SLOT(loadResourcesFromECS()));

    /*
    QnScheduleTaskList scheduleTasks;
    foreach (const QnScheduleTask &scheduleTask, scheduleTasks)
    {
        QString str;
        QTextStream stream(&str);

        stream << "ScheduleTask "
               << scheduleTask.getId().toString()
               << scheduleTask.getAfterThreshold()
               << scheduleTask.getBeforeThreshold()
               << scheduleTask.getDayOfWeek()
               << scheduleTask.getDoRecordAudio()
               << scheduleTask.getStartTime()
               << scheduleTask.getEndTime()
               << scheduleTask.getRecordingType()
               << scheduleTask.getResourceId().toString();
        cl_log.log(str, cl_logALWAYS);
    }
    */

    QnResourceDiscoveryManager::instance().setReady(true);
    QnResourceDiscoveryManager::instance().start();

    m_rtspListener = new QnRtspListener(QHostAddress::Any, rtspUrl.port());
    m_rtspListener->start();

    exec();
}

class QnVideoService : public QtService<QtSingleCoreApplication>
{
public:
    QnVideoService(int argc, char **argv)
        : QtService<QtSingleCoreApplication>(argc, argv, SERVICE_NAME),
        m_main(argc, argv)
    {
        setServiceDescription(SERVICE_NAME);
    }

protected:
    void start()
    {
        QtSingleCoreApplication *app = application();
        QString guid = serverGuid();

        if (guid.isEmpty())
        {
            cl_log.log("Can't save guid. Run once as administrator.", cl_logERROR);
            qApp->quit();
            return;
        }

        if (app->isRunning())
        {
            cl_log.log("Server already started", cl_logERROR);
            qApp->quit();
            return;
        }

        serverMain(app->argc(), app->argv());
        m_main.start();
    }

    void stop()
    {
        stopServer(0);
    }

private:
    QnMain m_main;
};

void stopServer(int signal)
{
    qApp->quit();
}

#include "api/SessionManager.h"

int main(int argc, char* argv[])
{
    QnVideoService service(argc, argv);

    int result = service.exec();

    QnResource::stopCommandProc();
    QnResourceDiscoveryManager::instance().stop();
    QnRecordingManager::instance()->stop();

    QnVideoCameraPool::instance()->stop();

    if (serviceMainInstance)
    {
        serviceMainInstance->stopObjects();
        serviceMainInstance = 0;
    }

    return result;
}
