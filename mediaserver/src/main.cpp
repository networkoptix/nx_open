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
#include "utils/common/module_resources.h"

#include "plugins/resources/archive/avi_files/avi_resource.h"
#include "core/resource_managment/resource_discovery_manager.h"
#include "core/resource_managment/resource_pool.h"
#include "utils/common/sleep.h"
#include "rtsp/rtsp_listener.h"
#include "plugins/resources/arecontvision/resource/av_resource_searcher.h"
#include "recorder/recording_manager.h"
#include "recorder/storage_manager.h"
#include "api/app_server_connection.h"
#include "appserver/processor.h"
#include "rest/server/rest_server.h"
#include "rest/handlers/recorded_chunks_handler.h"
#include "core/resource/media_server_resource.h"
#include "api/session_manager.h"
#include <signal.h>
#include "core/misc/schedule_task.h"
#include "qtservice.h"
#include "server_message_processor.h"
#include "settings.h"

#include <fstream>
#include "soap/soapserver.h"
#include "plugins/resources/onvif/onvif_resource_searcher.h"
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
#include "utils/common/command_line_parser.h"
#include "plugins/resources/pulse/pulse_resource_searcher.h"
//#include "plugins/storage/file_storage/file_storage_protocol.h"
#include "plugins/storage/file_storage/file_storage_resource.h"
#include "plugins/storage/coldstore/coldstore_storage.h"
#include "main.h"
#include "rest/handlers/file_system_handler.h"
#include "rest/handlers/statistics_handler.h"
#include "rest/handlers/camera_settings_handler.h"
#include "rest/handlers/manual_camera_addition_handler.h"
#include "rest/server/rest_connection_processor.h"
#include "rtsp/rtsp_connection.h"
#include "network/default_tcp_connection_processor.h"
#include "rest/handlers/ptz_handler.h"
#include "plugins/storage/dts/coldstore/coldstore_dts_resource_searcher.h"
#include "rest/handlers/image_handler.h"
#include "events/mserver_business_rule_processor.h"
#include "events/business_event_rule.h"
#include "events/business_rule_processor.h"
#include "rest/handlers/exec_action_handler.h"
#include "rest/handlers/time_handler.h"
#include "platform/platform_abstraction.h"
#include "recorder/file_deletor.h"
#include "rest/handlers/version_handler.h"

#define USE_SINGLE_STREAMING_PORT

//#include "plugins/resources/digitalwatchdog/dvr/dw_dvr_resource_searcher.h"

// This constant is used while checking for compatibility.
// Do not change it until you know what you're doing.
static const char COMPONENT_NAME[] = "MediaServer";

static QString SERVICE_NAME = QString(QLatin1String(VER_COMPANYNAME_STR)) + QString(QLatin1String(" Media Server"));

class QnMain;
static QnMain* serviceMainInstance = 0;
void stopServer(int signal);

//#include "device_plugins/arecontvision/devices/av_device_server.h"

//#define TEST_RTSP_SERVER

static const int DEFAUT_RTSP_PORT = 50000;
static const int DEFAULT_STREAMING_PORT = 50000;

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
        QList<QnInterfaceAndAddr> interfaces = getAllIPv4Interfaces();

        for (int i = 0; i < interfaces.size();++i)
        {
            QUdpSocket socket;
            if (!socket.bind(interfaces.at(i).address, 0))
                continue;

            QString result = socket.localAddress().toString();

            Q_ASSERT(result.length() > 0 );

            return result;
        }
    }

    return "127.0.0.1";

}


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

    QnStoragePluginFactory::instance()->registerStoragePlugin("file", QnFileStorageResource::instance, true); // true means use it plugin if no <protocol>:// prefix
    QnStoragePluginFactory::instance()->registerStoragePlugin("coldstore", QnPlColdStoreStorage::instance, false); // true means use it plugin if no <protocol>:// prefix
}

QnAbstractStorageResourcePtr createDefaultStorage()
{
    //QnStorageResourcePtr storage(new QnStorageResource());
    QnAbstractStorageResourcePtr storage(QnStoragePluginFactory::instance()->createStorage("ufile"));
    storage->setName("Initial");
    storage->setUrl(defaultStoragePath());
    storage->setSpaceLimit(5ll * 1000000000);

    cl_log.log("Creating new storage", cl_logINFO);

    return storage;
}

void setServerNameAndUrls(QnMediaServerResourcePtr server, const QString& myAddress)
{
    if (server->getName().isEmpty())
        server->setName(QString("Server ") + myAddress);

#ifdef _TEST_TWO_SERVERS
    server->setUrl(QString("rtsp://") + myAddress + QString(':') + QString::number(55001));
    server->setApiUrl(QString("http://") + myAddress + QString(':') + QString::number(55002));
#else
    server->setUrl(QString("rtsp://") + myAddress + QString(':') + qSettings.value("rtspPort", DEFAUT_RTSP_PORT).toString());
#ifdef USE_SINGLE_STREAMING_PORT
    server->setApiUrl(QString("http://") + myAddress + QString(':') + qSettings.value("rtspPort", DEFAUT_RTSP_PORT).toString());
    server->setStreamingUrl(QString("http://") + myAddress + QString(':') + qSettings.value("rtspPort", DEFAUT_RTSP_PORT).toString());
#else
    server->setApiUrl(QString("http://") + myAddress + QString(':') + qSettings.value("apiPort", DEFAULT_REST_PORT).toString());
    server->setStreamingUrl(QString("https://") + myAddress + QString(':') + qSettings.value("streamingPort", DEFAULT_STREAMING_PORT).toString());
#endif
#endif
}

QnMediaServerResourcePtr createServer()
{
    QnMediaServerResourcePtr serverPtr(new QnMediaServerResource());
    serverPtr->setGuid(serverGuid());

    serverPtr->setStorages(QnAbstractStorageResourceList() << createDefaultStorage());

    return serverPtr;
}

QnMediaServerResourcePtr findServer(QnAppServerConnectionPtr appServerConnection)
{
    QnMediaServerResourceList servers;

    while (servers.isEmpty())
    {
        QByteArray errorString;
        if (appServerConnection->getServers(servers, errorString) == 0)
            break;

        qDebug() << "findServer(): Call to registerServer failed. Reason: " << errorString;
        QnSleep::msleep(1000);
    }

    foreach(QnMediaServerResourcePtr server, servers)
    {
        if (server->getGuid() == serverGuid())
            return server;
    }

    return QnMediaServerResourcePtr();
}

QnMediaServerResourcePtr registerServer(QnAppServerConnectionPtr appServerConnection, QnMediaServerResourcePtr serverPtr)
{
    QnMediaServerResourceList servers;
    QByteArray errorString;
    serverPtr->setStatus(QnResource::Online);
    if (appServerConnection->registerServer(serverPtr, servers, errorString) != 0)
    {
        qDebug() << "registerServer(): Call to registerServer failed. Reason: " << errorString;

        return QnMediaServerResourcePtr();
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

    QCoreApplication::setOrganizationName(QLatin1String(QN_ORGANIZATION_NAME));
    QCoreApplication::setApplicationName(QLatin1String(QN_APPLICATION_NAME));
    QCoreApplication::setApplicationVersion(QLatin1String(QN_APPLICATION_VERSION));

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

    QString logLevel;

    QnCommandLineParser commandLineParser;
    commandLineParser.addParameter(&logLevel, "--log-level", NULL, QString());
    commandLineParser.parse(argc, argv, stderr);

    QnLog::initLog(logLevel);
    cl_log.log(QN_APPLICATION_NAME, " started", cl_logALWAYS);
    cl_log.log("Software version: ", QN_APPLICATION_VERSION, cl_logALWAYS);
    cl_log.log("Software revision: ", QN_APPLICATION_REVISION, cl_logALWAYS);
    cl_log.log("binary path: ", QFile::decodeName(argv[0]), cl_logALWAYS);

    defaultMsgHandler = qInstallMsgHandler(myMsgHandler);

#ifdef Q_OS_WIN
    int priority = ABOVE_NORMAL_PRIORITY_CLASS;
    int hrez = SetPriorityClass(GetCurrentProcess(), priority);
    if (hrez == 0)
        qWarning() << "Error increasing process priority. " << strerror(errno);
    else
        qDebug() << "Successfully increasing process priority to" << priority;
#endif
#ifdef Q_OS_LINUX
    errno = 0;
    int newNiceVal = nice( -10 );
    if( newNiceVal == -1 && errno != 0 )
        qWarning() << "Error increasing process priority. " << strerror(errno);
    else
        qDebug() << "Successfully increasing process priority to" << newNiceVal;
#endif


    ffmpegInit();

    // ------------------------------------------
#ifdef TEST_RTSP_SERVER
    addTestData();
#endif

    QDir stateDirectory;
    stateDirectory.mkpath(dataLocation + QLatin1String("/state"));
    qnFileDeletor->init(dataLocation + QLatin1String("/state")); // constructor got root folder for temp files

    QnBusinessRuleProcessor::init(new QnMServerBusinessRuleProcessor());

    return 0;
}

void initAppServerConnection(const QSettings &settings)
{
    QUrl appServerUrl;

    // ### remove
    appServerUrl.setScheme(QLatin1String("https"));
    appServerUrl.setHost(settings.value("appserverHost", QLatin1String(DEFAULT_APPSERVER_HOST)).toString());
    appServerUrl.setPort(settings.value("appserverPort", DEFAULT_APPSERVER_PORT).toInt());
    appServerUrl.setUserName(settings.value("appserverLogin", QLatin1String("admin")).toString());
    appServerUrl.setPassword(settings.value("appserverPassword", QLatin1String("123")).toString());

    QUrl urlNoPassword(appServerUrl);
    urlNoPassword.setPassword("");
    cl_log.log("Connect to enterprise controller server ", urlNoPassword.toString(), cl_logINFO);
    QnAppServerConnectionFactory::setClientGuid(serverGuid());
    QnAppServerConnectionFactory::setDefaultUrl(appServerUrl);
    QnAppServerConnectionFactory::setDefaultFactory(&QnResourceDiscoveryManager::instance());
}

void initAppServerEventConnection(const QSettings &settings, const QnMediaServerResourcePtr& mediaServer)
{
    QUrl appServerEventsUrl;

    // ### remove
    appServerEventsUrl.setScheme(QLatin1String("https"));
    appServerEventsUrl.setHost(settings.value("appserverHost", QLatin1String(DEFAULT_APPSERVER_HOST)).toString());
    appServerEventsUrl.setPort(settings.value("appserverPort", DEFAULT_APPSERVER_PORT).toInt());
    appServerEventsUrl.setUserName(settings.value("appserverLogin", QLatin1String("admin")).toString());
    appServerEventsUrl.setPassword(settings.value("appserverPassword", QLatin1String("123")).toString());
    appServerEventsUrl.setPath("/events/");
    appServerEventsUrl.addQueryItem("xid", mediaServer->getId().toString());
    appServerEventsUrl.addQueryItem("guid", QnAppServerConnectionFactory::clientGuid());
    appServerEventsUrl.addQueryItem("version", QN_ENGINE_VERSION);
    appServerEventsUrl.addQueryItem("format", "pb");

    static const int EVENT_RECONNECT_TIMEOUT = 3000;

    QnServerMessageProcessor* eventManager = QnServerMessageProcessor::instance();
    eventManager->init(appServerEventsUrl, EVENT_RECONNECT_TIMEOUT);
}

bool checkIfAppServerIsOld()
{
    // Check if that was 1.0/1.1
    QUrl httpUrl;
    httpUrl.setHost(QnAppServerConnectionFactory::defaultUrl().host());
    httpUrl.setPort(QnAppServerConnectionFactory::defaultUrl().port());
    httpUrl.setScheme("https");
    httpUrl.setUserName("");
    httpUrl.setPassword("");

    QByteArray reply, errorString;
    if (QnSessionManager::instance()->sendGetRequest(httpUrl, "resourceEx", reply, errorString) == 204)
    {
        cl_log.log("Old Incomatible Enterprise Controller version detected. Please update yout Enterprise Controler", cl_logERROR);
        return true;
    }

    return false;
}

QnMain::QnMain(int argc, char* argv[])
    : m_argc(argc),
    m_argv(argv),
    m_processor(0),
    m_rtspListener(0),
    m_restServer(0),
    m_progressiveDownloadingServer(0),
    m_universalTcpListener(0)
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
        m_restServer->pleaseStop();
    if (m_progressiveDownloadingServer)
        m_progressiveDownloadingServer->pleaseStop();
    if (m_rtspListener)
        m_rtspListener->pleaseStop();
    if (m_universalTcpListener) {
        m_universalTcpListener->pleaseStop();
        delete m_universalTcpListener;
        m_universalTcpListener = 0;
    }

    if (m_restServer)
    {
        delete m_restServer;
        m_restServer = 0;
    }

    if (m_progressiveDownloadingServer) {
        delete m_progressiveDownloadingServer;
        m_progressiveDownloadingServer = 0;
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
    while (appServerConnection->getCameras(cameras, m_mediaServer->getId(), errorString) != 0)
    {
        qDebug() << "QnMain::run(): Can't get cameras. Reason: " << errorString;
        QnSleep::msleep(10000);
    }

    QnManualCamerasMap manualCameras;
    foreach(const QnSecurityCamResourcePtr &camera, cameras)
    {
        QnResourcePtr ownResource = qnResPool->getResourceById(camera->getId());
        if (ownResource) {
            ownResource->update(camera);
        }
        else {
            qnResPool->addResource(camera);
            QnVirtualCameraResourcePtr virtualCamera = qSharedPointerDynamicCast<QnVirtualCameraResource>(camera);
            if (virtualCamera->isManuallyAdded()) {
                QnResourceTypePtr resType = qnResTypePool->getResourceType(virtualCamera->getTypeId());
                manualCameras.insert(virtualCamera->getUrl(), QnManualCameraInfo(QUrl(virtualCamera->getUrl()), virtualCamera->getAuth(), resType->getName()));
            }
        }
    }
    QnResourceDiscoveryManager::instance().registerManualCameras(manualCameras);

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

    //loading business rules
    QnBusinessEventRules rules;
    while( appServerConnection->getBusinessRules( rules, errorString ) != 0 )
    {
        qDebug() << "QnMain::run(): Can't get business rules. Reason: " << errorString;
        QnSleep::msleep(1000);
    }

    foreach(QnBusinessEventRulePtr rule, rules)
        QnBusinessRuleProcessor::instance()->addBusinessRule( rule );
}

void QnMain::at_localInterfacesChanged()
{
    QnAppServerConnectionPtr appServerConnection = QnAppServerConnectionFactory::createConnection();

    m_mediaServer->setNetAddrList(allLocalAddresses());

    appServerConnection->saveAsync(m_mediaServer, this, SLOT(at_serverSaved(int, const QByteArray&, const QnResourceList&, int)));
}

void QnMain::at_serverSaved(int status, const QByteArray &errorString, const QnResourceList &, int)
{
    if (status != 0)
        qWarning() << "Error saving server: " << errorString;
}

void QnMain::initTcpListener()
{
    int rtspPort = qSettings.value("rtspPort", DEFAUT_RTSP_PORT).toInt();
#ifdef USE_SINGLE_STREAMING_PORT
    QnRestConnectionProcessor::registerHandler("api/RecordedTimePeriods", new QnRecordedChunksHandler());
    QnRestConnectionProcessor::registerHandler("api/CheckPath", new QnFileSystemHandler(true));
    QnRestConnectionProcessor::registerHandler("api/GetFreeSpace", new QnFileSystemHandler(false));
    QnRestConnectionProcessor::registerHandler("api/statistics", new QnStatisticsHandler());
    QnRestConnectionProcessor::registerHandler("api/getCameraParam", new QnGetCameraParamHandler());
    QnRestConnectionProcessor::registerHandler("api/setCameraParam", new QnSetCameraParamHandler());
    QnRestConnectionProcessor::registerHandler("api/manualCamera", new QnManualCameraAdditionHandler());
    QnRestConnectionProcessor::registerHandler("api/ptz", new QnPtzHandler());
    QnRestConnectionProcessor::registerHandler("api/image", new QnImageHandler());
    QnRestConnectionProcessor::registerHandler("api/execAction", new QnExecActionHandler());
    QnRestConnectionProcessor::registerHandler("api/gettime", new QnTimeHandler());
    QnRestConnectionProcessor::registerHandler("api/version", new QnVersionHandler());

    m_universalTcpListener = new QnUniversalTcpListener(QHostAddress::Any, rtspPort);
    m_universalTcpListener->addHandler<QnRtspConnectionProcessor>("RTSP", "*");
    m_universalTcpListener->addHandler<QnRestConnectionProcessor>("HTTP", "api");
    m_universalTcpListener->addHandler<QnProgressiveDownloadingConsumer>("HTTP", "media");
    m_universalTcpListener->addHandler<QnDefaultTcpConnectionProcessor>("HTTP", "*");
    m_universalTcpListener->start();
#else
    int apiPort = qSettings.value("apiPort", DEFAULT_REST_PORT).toInt();
    int streamingPort = qSettings.value("streamingPort", DEFAULT_STREAMING_PORT).toInt();

    m_restServer = new QnRestServer(QHostAddress::Any, apiPort);
    m_progressiveDownloadingServer = new QnProgressiveDownloadingServer(QHostAddress::Any, streamingPort);
    m_progressiveDownloadingServer->start();
    m_progressiveDownloadingServer->enableSSLMode();
    m_rtspListener = new QnRtspListener(QHostAddress::Any, rtspUrl.port());
    m_restServer->start();
    m_progressiveDownloadingServer->start();
    m_rtspListener->start();
#endif
}

QHostAddress QnMain::getPublicAddress()
{
    static const QString DEFAULT_URL_LIST("http://checkrealip.com; http://www.thisip.org/cgi-bin/thisip.cgi; http://checkip.eurodyndns.org");
    static const QRegExp iPRegExpr("[^a-zA-Z0-9\\.](([0-9]){1,3}\\.){3}([0-9]){1,3}[^a-zA-Z0-9\\.]");

    if (qSettings.value("publicIPEnabled").isNull())
        qSettings.setValue("publicIPEnabled", 1);

    int publicIPEnabled = qSettings.value("publicIPEnabled").toInt();

    if (publicIPEnabled == 0)
        return QHostAddress(); // disabled
    else if (publicIPEnabled > 1)
        return QHostAddress(qSettings.value("staticPublicIP").toString()); // manually added

    QStringList urls = qSettings.value("publicIPServers", DEFAULT_URL_LIST).toString().split(";");

    QNetworkAccessManager networkManager;
    QList<QNetworkReply*> replyList;
    for (int i = 0; i < urls.size(); ++i)
        replyList << networkManager.get(QNetworkRequest(urls[i].trimmed()));

    QString result;
    QTime t;
    t.start();
    while (t.elapsed() < 4000 && result.isEmpty()) 
    {
        msleep(1);
        qApp->processEvents();
        for (int i = 0; i < replyList.size(); ++i) 
        {
            if (replyList[i]->isFinished()) {
                QByteArray response = QByteArray(" ") + replyList[i]->readAll() + QByteArray(" ");
                int ipPos = iPRegExpr.indexIn(response);
                if (ipPos >= 0) {
                    result = response.mid(ipPos+1, iPRegExpr.matchedLength()-2);
                    break;
                }
            }
        }
    }
    for (int i = 0; i < replyList.size(); ++i)
        replyList[i]->deleteLater();

    return QHostAddress(result);
}

void QnMain::run()
{
    // Create SessionManager
    QnSessionManager* sm = QnSessionManager::instance();

    QThread *thread = new QThread();
    sm->moveToThread(thread);

    QObject::connect(sm, SIGNAL(destroyed()), thread, SLOT(quit()));
    QObject::connect(thread , SIGNAL(finished()), thread, SLOT(deleteLater()));

    thread->start();
    sm->start();

    initAppServerConnection(qSettings);

    QnAppServerConnectionPtr appServerConnection = QnAppServerConnectionFactory::createConnection();

    QByteArray errorString;
    QnConnectInfoPtr connectInfo(new QnConnectInfo());
    while (!needToStop())
    {
        if (checkIfAppServerIsOld())
            return;

        if (appServerConnection->connect(connectInfo, errorString) == 0)
            break;

        cl_log.log("Can't connect to Enterprise Controller: ", errorString.data(), cl_logWARNING);

        QnSleep::msleep(1000);
    }

    if (needToStop())
        return;

    QnCompatibilityChecker remoteChecker(connectInfo->compatibilityItems);
    QnCompatibilityChecker localChecker(localCompatibilityItems());

    QnCompatibilityChecker* compatibilityChecker;
    if (remoteChecker.size() > localChecker.size())
        compatibilityChecker = &remoteChecker;
    else
        compatibilityChecker = &localChecker;

    if (!compatibilityChecker->isCompatible(COMPONENT_NAME, QN_ENGINE_VERSION, "ECS", connectInfo->version))
    {
        cl_log.log(cl_logERROR, "Incompatible Enterprise Controller version detected! Giving up.");
        return;
    }

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

    initTcpListener();

    QHostAddress publicAddress = getPublicAddress();

    while (m_mediaServer.isNull())
    {
        QnMediaServerResourcePtr server = findServer(appServerConnection);

        if (!server)
            server = createServer();

        setServerNameAndUrls(server, defaultLocalAddress(appserverHost));

        QList<QHostAddress> serverIfaceList = allLocalAddresses();
        if (!publicAddress.isNull()) {
            bool isExists = false;
            for (int i = 0; i < serverIfaceList.size(); ++i)
            {
                if (serverIfaceList[i] == publicAddress)
                    isExists = true;
            }
            if (!isExists)
                serverIfaceList << publicAddress;
        }
        server->setNetAddrList(serverIfaceList);

        QnAbstractStorageResourceList storages = server->getStorages();
        if (storages.isEmpty() || (storages.size() == 1 && storages.at(0)->getUrl() != defaultStoragePath() ))
            server->setStorages(QnAbstractStorageResourceList() << createDefaultStorage());

        m_mediaServer = registerServer(appServerConnection, server);
        if (m_mediaServer.isNull())
            QnSleep::msleep(1000);
    }

    syncStoragesToSettings(m_mediaServer);

    int status;
    do
    {
        status = appServerConnection->setResourceStatus(m_mediaServer->getId(), QnResource::Online, errorString);
    } while (status != 0);

    initAppServerEventConnection(qSettings, m_mediaServer);
    QnServerMessageProcessor* eventManager = QnServerMessageProcessor::instance();
    eventManager->run();

    m_processor = new QnAppserverResourceProcessor(m_mediaServer->getId());

    foreach (QnAbstractStorageResourcePtr storage, m_mediaServer->getStorages())
    {
        qnResPool->addResource(storage);
        QnStorageResourcePtr physicalStorage = qSharedPointerDynamicCast<QnStorageResource>(storage);
        if (physicalStorage)
            qnStorageMan->addStorage(physicalStorage);
    }
    qnStorageMan->loadFullFileCatalog();

    QnRecordingManager::instance()->start();
    qnResPool->addResource(m_mediaServer);

    // ------------------------------------------


    //===========================================================================
    //IPPH264Decoder::dll.init();

    //============================
    QnResourceDiscoveryManager::instance().setServer(true);
    QnResourceDiscoveryManager::instance().setResourceProcessor(m_processor);

    QnResourceDiscoveryManager::instance().setDisabledVendors(qSettings.value("disabledVendors").toString().split(";"));
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlArecontResourceSearcher::instance());
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlDlinkResourceSearcher::instance());
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlIpWebCamResourceSearcher::instance());
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlDroidResourceSearcher::instance());
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnTestCameraResourceSearcher::instance());
    //QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlPulseSearcher::instance());
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlAxisResourceSearcher::instance());
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlIqResourceSearcher::instance());
    QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlISDResourceSearcher::instance());
    //Onvif searcher should be the last:
    QnResourceDiscoveryManager::instance().addDeviceServer(&OnvifResourceSearcher::instance());

    QnResourceDiscoveryManager::instance().addDTSServer(&QnColdStoreDTSSearcher::instance());

    //QnResourceDiscoveryManager::instance().addDeviceServer(&DwDvrResourceSearcher::instance());

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


    connect(&QnResourceDiscoveryManager::instance(), SIGNAL(localInterfacesChanged()), this, SLOT(at_localInterfacesChanged()));

    //starting soap server to accept event notifications from onvif servers
    QnSoapServer::instance()->initialize( 8083 );   //TODO/IMPL get port from settings or use any unused port?
    QnSoapServer::instance()->start();

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

        new QnPlatformAbstraction(app);

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
        m_main.exit();
        m_main.wait();
        stopServer(0);
    }

private:
    QnMain m_main;
};

void stopServer(int signal)
{
    Q_UNUSED(signal)

    QnResource::stopCommandProc();
    QnResourceDiscoveryManager::instance().stop();
    QnRecordingManager::instance()->stop();
    QnVideoCameraPool::instance()->stop();
    if (serviceMainInstance)
    {
        serviceMainInstance->stopObjects();
        serviceMainInstance = 0;
    }
    av_lockmgr_register(NULL);
    qApp->quit();
}

int main(int argc, char* argv[])
{
    QN_INIT_MODULE_RESOURCES(common);

    QnVideoService service(argc, argv);

    int result = service.exec();

    return result;
}
