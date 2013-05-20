#include <qtsinglecoreapplication.h>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <QtCore/QUrl>
#include <QtCore/QUuid>
#include <QThreadPool>

#include <QtNetwork/QUdpSocket>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QHostInfo>
#include <QtNetwork/QNetworkInterface>

#include "version.h"
#include "utils/common/util.h"
#include "media_server/media_server_module.h"

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
#include "motion/motion_helper.h"

#include <fstream>
#include "soap/soapserver.h"
#include "plugins/resources/onvif/onvif_resource_searcher.h"
#include "plugins/resources/axis/axis_resource_searcher.h"
#include "plugins/resources/acti/acti_resource_searcher.h"
#include "plugins/resources/d-link/dlink_resource_searcher.h"
#include "plugins/resources/third_party/third_party_resource_searcher.h"
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
#include <business/business_event_rule.h>
#include <business/business_rule_processor.h>
#include "rest/handlers/exec_action_handler.h"
#include "rest/handlers/time_handler.h"
#include "rest/handlers/ping_handler.h"
#include "rest/handlers/events_handler.h"
#include "platform/platform_abstraction.h"
#include "recorder/file_deletor.h"
#include "rest/handlers/ext_bevent_handler.h"
#include <business/business_event_connector.h>
#include "utils/common/synctime.h"
#include "plugins/resources/flex_watch/flexwatch_resource_searcher.h"
#include "core/resource_managment/mserver_resource_discovery_manager.h"
#include "plugins/resources/mserver_resource_searcher.h"
#include "rest/handlers/log_handler.h"
#include "plugins/storage/dts/vmax480/vmax480_resource_searcher.h"
#include "business/events/reasoned_business_event.h"
#include "rest/handlers/favico_handler.h"
#include "rest/handlers/storage_space_handler.h"
#include "common/customization.h"
#include "plugins/resources/stardot/stardot_resource_searcher.h"
#include "plugins/plugin_manager.h"
#include "core/resource_managment/camera_driver_restriction_list.h"
#include <utils/network/multicodec_rtp_reader.h>


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

QnStorageResourcePtr createStorage(const QString& path)
{
    QnStorageResourcePtr storage(QnStoragePluginFactory::instance()->createStorage("ufile"));
    storage->setName("Initial");
    storage->setUrl(path);
    storage->setSpaceLimit(5ll * 1000000000);
    storage->setUsedForWriting(storage->isStorageAvailableForWriting());

    return storage;
}

#ifdef Q_OS_WIN
static int freeGB(QString drive)
{
    ULARGE_INTEGER freeBytes;

    GetDiskFreeSpaceEx(drive.toStdWString().c_str(), &freeBytes, 0, 0);

    return freeBytes.HighPart * 4 + (freeBytes.LowPart>> 30);
}
#endif

static QStringList listRecordFolders()
{
    QStringList folderPaths;

#ifdef Q_OS_WIN
    //QString maxFreeSpaceDrive;
    //int maxFreeSpace = 0;

    foreach (QFileInfo drive, QDir::drives()) {
        if (!drive.isWritable())
            continue;

        
        QString path = drive.absolutePath();
        
        if (GetDriveType(path.toStdWString().c_str()) != DRIVE_FIXED)
            continue;

        folderPaths.append(path + QN_MEDIA_FOLDER_NAME);
        /*
        int freeSpace = freeGB(path);

        if (maxFreeSpaceDrive.isEmpty() || freeSpace > maxFreeSpace) {
            maxFreeSpaceDrive = path;
            maxFreeSpace = freeSpace;
        }

        if (freeSpace >= 100) {
            cl_log.log(QString("Drive %1 has more than 100GB free space. Using it for storage.").arg(path), cl_logINFO);
            folderPaths.append(path + QN_MEDIA_FOLDER_NAME);
        }
        */
    }
    /*
    if (folderPaths.isEmpty()) {
        cl_log.log(QString("There are no drives with more than 100GB free space. Using drive %1 as it has the most free space: %2 GB").arg(maxFreeSpaceDrive).arg(maxFreeSpace), cl_logINFO);
        folderPaths.append(maxFreeSpaceDrive + QN_MEDIA_FOLDER_NAME);
    }
    */
#endif

#ifdef Q_OS_LINUX
    folderPaths.append(getDataDirectory() + "/data");
#endif

    return folderPaths;
}

QnAbstractStorageResourceList createStorages()
{
    static const qint64 BIG_STORAGE_THRESHOLD = 1000000000ll * 100; // 100Gb

    QnAbstractStorageResourceList storages;
    bool isBigStorageExist = false;
    foreach(QString folderPath, listRecordFolders()) {
        QnStorageResourcePtr storage = createStorage(folderPath);
        isBigStorageExist |= storage->isUsedForWriting() && storage->getTotalSpace() > BIG_STORAGE_THRESHOLD;
        storages.append(storage);
        cl_log.log(QString("Creating new storage: %1").arg(folderPath), cl_logINFO);
    }
    if (isBigStorageExist) {
        for (int i = 0; i < storages.size(); ++i) {
            QnStorageResourcePtr storage = storages[i].dynamicCast<QnStorageResource>();
            if (storage->getTotalSpace() <= BIG_STORAGE_THRESHOLD)
                storage->setUsedForWriting(false);
        }
    }

    return storages;
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

QnMediaServerResourcePtr findServer(QnAppServerConnectionPtr appServerConnection, QnMediaServerResource::PanicMode* pm)
{
    QnMediaServerResourceList servers;
    *pm = QnMediaServerResource::PM_None;

    while (servers.isEmpty())
    {
        if (appServerConnection->getServers(servers) == 0)
            break;

        qDebug() << "findServer(): Call to getServers failed. Reason: " << appServerConnection->getLastError();
        QnSleep::msleep(1000);
    }

    foreach(QnMediaServerResourcePtr server, servers)
    {
        *pm = server->getPanicMode();
        if (server->getGuid() == serverGuid())
            return server;
    }

    return QnMediaServerResourcePtr();
}

QnMediaServerResourcePtr registerServer(QnAppServerConnectionPtr appServerConnection, QnMediaServerResourcePtr serverPtr)
{
    QnMediaServerResourceList servers;
    serverPtr->setStatus(QnResource::Online);

    QByteArray authKey;
    if (appServerConnection->saveServer(serverPtr, servers, authKey) != 0)
    {
        qDebug() << "registerServer(): Call to registerServer failed. Reason: " << appServerConnection->getLastError();

        return QnMediaServerResourcePtr();
    }

    if (!authKey.isEmpty()) {
        qSettings.setValue("authKey", authKey);
        QnAppServerConnectionFactory::setAuthKey(authKey);
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
#ifdef Q_OS_WIN
    SetConsoleCtrlHandler(stopServer_WIN, true);
#endif
    signal(SIGINT, stopServer);
	//signal(SIGABRT, stopServer);
    signal(SIGTERM, stopServer);

//    av_log_set_callback(decoderLogCallback);

    QCoreApplication::setOrganizationName(QLatin1String(QN_ORGANIZATION_NAME));
    QCoreApplication::setApplicationName(QLatin1String(QN_APPLICATION_NAME));
    QCoreApplication::setApplicationVersion(QLatin1String(QN_APPLICATION_VERSION));

    QString dataLocation = getDataDirectory();
    QDir::setCurrent(qApp->applicationDirPath());

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
    QString rebuildArchive;

    QnCommandLineParser commandLineParser;
    commandLineParser.addParameter(&logLevel, "--log-level", NULL, QString());
    commandLineParser.addParameter(&rebuildArchive, "--rebuild", NULL, QString(), "all");
    commandLineParser.parse(argc, argv, stderr);

    QnLog::initLog(logLevel);
    if (rebuildArchive == "all")
        DeviceFileCatalog::setRebuildArchive(DeviceFileCatalog::Rebuild_All);
    else if (rebuildArchive == "hq")
        DeviceFileCatalog::setRebuildArchive(DeviceFileCatalog::Rebuild_HQ);
    else if (rebuildArchive == "lq")
        DeviceFileCatalog::setRebuildArchive(DeviceFileCatalog::Rebuild_LQ);
    
    cl_log.log(QN_APPLICATION_NAME, " started", cl_logALWAYS);
    cl_log.log("Software version: ", QN_APPLICATION_VERSION, cl_logALWAYS);
    cl_log.log("Software revision: ", QN_APPLICATION_REVISION, cl_logALWAYS);
    cl_log.log("binary path: ", QFile::decodeName(argv[0]), cl_logALWAYS);

    defaultMsgHandler = qInstallMsgHandler(myMsgHandler);

    // TODO: #Elric use QnPlatformProcess here.
#ifdef Q_OS_WIN
    int priority = REALTIME_PRIORITY_CLASS;
    int hrez = SetPriorityClass(GetCurrentProcess(), priority);
    if (hrez == 0)
        qWarning() << "Error increasing process priority. " << strerror(errno);
    else
        qDebug() << "Successfully increasing process priority to" << priority;
#endif
#ifdef Q_OS_LINUX
    errno = 0;
    int newNiceVal = nice( -20 );
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
    QnAppServerConnectionFactory::setAuthKey(authKey());
    QnAppServerConnectionFactory::setClientGuid(serverGuid());
    QnAppServerConnectionFactory::setDefaultUrl(appServerUrl);
    QnAppServerConnectionFactory::setDefaultFactory(QnResourceDiscoveryManager::instance());
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
    eventManager->init(appServerEventsUrl, settings.value("authKey").toString().toAscii(), EVENT_RECONNECT_TIMEOUT);
}

QnMain::QnMain(int argc, char* argv[])
    : m_argc(argc),
    m_argv(argv),
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

void QnMain::stopSync()
{
    if (serviceMainInstance) {
        serviceMainInstance->pleaseStop();
        serviceMainInstance->exit();
        serviceMainInstance->wait();
        serviceMainInstance = 0;
    }
    qApp->quit();
}

void QnMain::stopAsync()
{
    QTimer::singleShot(0, this, SLOT(stopSync()));
}


void QnMain::stopObjects()
{
    qWarning() << "QnMain::stopObjects() called";

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
}

static const unsigned int APP_SERVER_REQUEST_ERROR_TIMEOUT_MS = 5500;

void QnMain::loadResourcesFromECS()
{
    QnAppServerConnectionPtr appServerConnection = QnAppServerConnectionFactory::createConnection();

    QnVirtualCameraResourceList cameras;
    while (appServerConnection->getCameras(cameras, m_mediaServer->getId()) != 0)
    {
        qDebug() << "QnMain::run(): Can't get cameras. Reason: " << appServerConnection->getLastError();
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
    QnResourceDiscoveryManager::instance()->registerManualCameras(manualCameras);

    //reading media servers list
    QnMediaServerResourceList mediaServerList;
    while( appServerConnection->getServers( mediaServerList) != 0 )
    {
        NX_LOG( QString::fromLatin1("QnMain::run(). Can't get media servers. Reason %1").arg(QLatin1String(appServerConnection->getLastError())), cl_logERROR );
        QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
    }

    //reading other servers' cameras
    foreach( const QnMediaServerResourcePtr& mediaServer, mediaServerList )
    {
        if( mediaServer->getGuid() == serverGuid() )
            continue;

        qnResPool->addResource( mediaServer );
        //requesting remote server cameras
        QnVirtualCameraResourceList cameras;
        while( appServerConnection->getCameras(cameras, mediaServer->getId()) != 0 )
        {
            NX_LOG( QString::fromLatin1("QnMain::run(). Error retreiving server %1(%2) cameras from enterprise controller. %3").
                arg(mediaServer->getId()).arg(mediaServer->getGuid()).arg(QLatin1String(appServerConnection->getLastError())), cl_logERROR );
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
        }
        foreach( const QnVirtualCameraResourcePtr &camera, cameras )
        {
            camera->addFlags( QnResource::foreigner );  //marking resource as not belonging to us
            qnResPool->addResource( camera );
        }
    }

    QnCameraHistoryList cameraHistoryList;
    while (appServerConnection->getCameraHistoryList(cameraHistoryList) != 0)
    {
        qDebug() << "QnMain::run(): Can't get cameras history. Reason: " << appServerConnection->getLastError();
        QnSleep::msleep(1000);
    }

    foreach(QnCameraHistoryPtr history, cameraHistoryList)
    {
        QnCameraHistoryPool::instance()->addCameraHistory(history);
    }

    //loading business rules
    QnUserResourceList users;
    while( appServerConnection->getUsers(users) != 0 )
    {
        qDebug() << "QnMain::run(): Can't get users. Reason: " << appServerConnection->getLastError();
        QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
    }

    foreach(const QnUserResourcePtr &user, users)
        qnResPool->addResource(user);

    //loading business rules
    QnBusinessEventRuleList rules;
    while( appServerConnection->getBusinessRules(rules) != 0 )
    {
        qDebug() << "QnMain::run(): Can't get business rules. Reason: " << appServerConnection->getLastError();
        QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
    }

    foreach(const QnBusinessEventRulePtr &rule, rules)
        qnBusinessRuleProcessor->addBusinessRule( rule );
}

void QnMain::at_localInterfacesChanged()
{
    QnAppServerConnectionPtr appServerConnection = QnAppServerConnectionFactory::createConnection();

    m_mediaServer->setNetAddrList(allLocalAddresses());

    appServerConnection->saveAsync(m_mediaServer, this, SLOT(at_serverSaved(int, const QnResourceList&, int)));
}

void QnMain::at_serverSaved(int status, const QnResourceList &, int)
{
    if (status != 0)
        qWarning() << "Error saving server.";
}

void QnMain::at_timer()
{
    qSettings.setValue("lastRunningTime", qnSyncTime->currentMSecsSinceEpoch());
}

void QnMain::at_noStorages()
{
    qnBusinessRuleConnector->at_NoStorages(m_mediaServer);
}

void QnMain::at_storageFailure(QnResourcePtr storage)
{
    qnBusinessRuleConnector->at_storageFailure(m_mediaServer, qnSyncTime->currentUSecsSinceEpoch(), QnBusiness::StorageIssueIoError, storage);
}

void QnMain::at_cameraIPConflict(QHostAddress host, QStringList macAddrList)
{
    qnBusinessRuleConnector->at_cameraIPConflict(
        m_mediaServer,
        host,
        macAddrList,
        qnSyncTime->currentUSecsSinceEpoch());
}


void QnMain::initTcpListener()
{
    int rtspPort = qSettings.value("rtspPort", DEFAUT_RTSP_PORT).toInt();
#ifdef USE_SINGLE_STREAMING_PORT
    QnRestConnectionProcessor::registerHandler("api/RecordedTimePeriods", new QnRecordedChunksHandler());
    QnRestConnectionProcessor::registerHandler("api/storageStatus", new QnFileSystemHandler());
    QnRestConnectionProcessor::registerHandler("api/storageSpace", new QnStorageSpaceHandler());
    QnRestConnectionProcessor::registerHandler("api/statistics", new QnStatisticsHandler());
    QnRestConnectionProcessor::registerHandler("api/getCameraParam", new QnGetCameraParamHandler());
    QnRestConnectionProcessor::registerHandler("api/setCameraParam", new QnSetCameraParamHandler());
    QnRestConnectionProcessor::registerHandler("api/manualCamera", new QnManualCameraAdditionHandler());
    QnRestConnectionProcessor::registerHandler("api/ptz", new QnPtzHandler());
    QnRestConnectionProcessor::registerHandler("api/image", new QnImageHandler());
    QnRestConnectionProcessor::registerHandler("api/execAction", new QnExecActionHandler());
    QnRestConnectionProcessor::registerHandler("api/onEvent", new QnExternalBusinessEventHandler());
    QnRestConnectionProcessor::registerHandler("api/gettime", new QnTimeHandler());
    QnRestConnectionProcessor::registerHandler("api/ping", new QnRestPingHandler());
    QnRestConnectionProcessor::registerHandler("api/events", new QnRestEventsHandler());
    QnRestConnectionProcessor::registerHandler("api/showLog", new QnRestLogHandler());
    QnRestConnectionProcessor::registerHandler("favicon.ico", new QnRestFavicoHandler());

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
    QnBusinessRuleProcessor::init(new QnMServerBusinessRuleProcessor());
    QnEventsDB::init();

    // Create SessionManager
    QnSessionManager::instance()->start();
    
    //starting soap server to accept event notifications from onvif servers
    QnSoapServer::initStaticInstance( new QnSoapServer(8083) ); //TODO/IMPL get port from settings or use any unused port?
    QnSoapServer::instance()->start();

    QnResourcePool::initStaticInstance( new QnResourcePool() );

    QnVideoCameraPool::initStaticInstance( new QnVideoCameraPool() );

    QnMotionHelper::initStaticInstance( new QnMotionHelper() );

    QnBusinessEventConnector::initStaticInstance( new QnBusinessEventConnector() );
    std::auto_ptr<QThread> connectorThread( new QThread() );
    connectorThread->start();
    qnBusinessRuleConnector->moveToThread(connectorThread.get());

    CameraDriverRestrictionList cameraDriverRestrictionList;

    QnResourceDiscoveryManager::init(new QnMServerResourceDiscoveryManager(cameraDriverRestrictionList));
    initAppServerConnection(qSettings);

    QnMulticodecRtpReader::setDefaultTransport( qSettings.value(QLatin1String("rtspTransport"), RtpTransport::_auto).toString().toUpper() );

    QnAppServerConnectionPtr appServerConnection = QnAppServerConnectionFactory::createConnection();
    connect(QnResourceDiscoveryManager::instance(), SIGNAL(CameraIPConflict(QHostAddress, QStringList)), this, SLOT(at_cameraIPConflict(QHostAddress, QStringList)));
    connect(QnStorageManager::instance(), SIGNAL(noStoragesAvailable()), this, SLOT(at_noStorages()));
    connect(QnStorageManager::instance(), SIGNAL(storageFailure(QnResourcePtr)), this, SLOT(at_storageFailure(QnResourcePtr)));

    QnConnectInfoPtr connectInfo(new QnConnectInfo());
    while (!needToStop())
    {
        if (appServerConnection->connect(connectInfo) == 0)
            break;

        cl_log.log("Can't connect to Enterprise Controller: ", appServerConnection->getLastError(), cl_logWARNING);
        if (!needToStop())
            QnSleep::msleep(1000);
    }
    QnAppServerConnectionFactory::setDefaultMediaProxyPort(connectInfo->proxyPort);


    QnMServerResourceSearcher::initStaticInstance( new QnMServerResourceSearcher() );
    QnMServerResourceSearcher::instance()->setAppPServerGuid(connectInfo->ecsGuid.toUtf8());
    QnMServerResourceSearcher::instance()->start();

    //Initializing plugin manager
    PluginManager::instance()->loadPlugins();

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

    QnMediaServerResource::PanicMode pm;
    while (m_mediaServer.isNull())
    {
        QnMediaServerResourcePtr server = findServer(appServerConnection, &pm);

        if (!server) {
            server = QnMediaServerResourcePtr(new QnMediaServerResource());
            server->setGuid(serverGuid());
            server->setPanicMode(pm);
        }

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
        if (storages.isEmpty())
            server->setStorages(createStorages());

        m_mediaServer = registerServer(appServerConnection, server);
        if (m_mediaServer.isNull())
            QnSleep::msleep(1000);
    }

    syncStoragesToSettings(m_mediaServer);

    int status;
    do
    {
        status = appServerConnection->setResourceStatus(m_mediaServer->getId(), QnResource::Online);
    } while (status != 0);


    foreach (QnAbstractStorageResourcePtr storage, m_mediaServer->getStorages())
    {
        qnResPool->addResource(storage);
        QnStorageResourcePtr physicalStorage = qSharedPointerDynamicCast<QnStorageResource>(storage);
        if (physicalStorage)
            qnStorageMan->addStorage(physicalStorage);
    }
    qnStorageMan->loadFullFileCatalog();

    initAppServerEventConnection(qSettings, m_mediaServer);
    QnServerMessageProcessor* eventManager = QnServerMessageProcessor::instance();
    eventManager->run();

    std::auto_ptr<QnAppserverResourceProcessor> m_processor( new QnAppserverResourceProcessor(m_mediaServer->getId()) );

    QnRecordingManager::initStaticInstance( new QnRecordingManager() );
    QnRecordingManager::instance()->start();
    qnResPool->addResource(m_mediaServer);

    // ------------------------------------------


    //===========================================================================
    //IPPH264Decoder::dll.init();

    //============================
    UPNPDeviceSearcher::initGlobalInstance( new UPNPDeviceSearcher() );

    QnResourceDiscoveryManager::instance()->setResourceProcessor(m_processor.get());

    QnResourceDiscoveryManager::instance()->setDisabledVendors(qSettings.value("disabledVendors").toString().split(";"));

    //NOTE plugins have higher priority than built-in drivers
    ThirdPartyResourceSearcher::initStaticInstance( new ThirdPartyResourceSearcher( &cameraDriverRestrictionList ) );
    QnResourceDiscoveryManager::instance()->addDeviceServer(ThirdPartyResourceSearcher::instance());

    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnPlArecontResourceSearcher::instance());
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnPlDlinkResourceSearcher::instance());
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnPlIpWebCamResourceSearcher::instance());
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnPlDroidResourceSearcher::instance());
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnTestCameraResourceSearcher::instance());
    //QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlPulseSearcher::instance()); native driver does not support dual streaming! new pulse cameras works via onvif
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnPlAxisResourceSearcher::instance());
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnActiResourceSearcher::instance());
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnStardotResourceSearcher::instance());
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnPlIqResourceSearcher::instance());
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnPlISDResourceSearcher::instance());
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnPlISDResourceSearcher::instance());

#ifdef Q_OS_WIN
    if (qnCustomization() == Qn::DwSpectrumCustomization)
        QnResourceDiscoveryManager::instance()->addDeviceServer(&QnPlVmax480ResourceSearcher::instance());
#endif

    //Onvif searcher should be the last:
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnFlexWatchResourceSearcher::instance());
    QnResourceDiscoveryManager::instance()->addDeviceServer(&OnvifResourceSearcher::instance());

    

    QnResourceDiscoveryManager::instance()->addDTSServer(&QnColdStoreDTSSearcher::instance());

    //QnResourceDiscoveryManager::instance().addDeviceServer(&DwDvrResourceSearcher::instance());

    //

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

    QnResourceDiscoveryManager::instance()->setReady(true);
    QnResourceDiscoveryManager::instance()->start();


    connect(QnResourceDiscoveryManager::instance(), SIGNAL(localInterfacesChanged()), this, SLOT(at_localInterfacesChanged()));

    qint64 lastRunningTime = qSettings.value("lastRunningTime").toLongLong();
    if (lastRunningTime)
        qnBusinessRuleConnector->at_mserverFailure(m_mediaServer,
                                                   lastRunningTime*1000,
                                                   QnBusiness::MServerIssueStarted);

    at_timer();
    QTimer timer;
    connect(&timer, SIGNAL(timeout()), this, SLOT(at_timer()), Qt::DirectConnection);
    timer.start(60 * 1000);


    exec();

    stopObjects();

    QnResource::stopCommandProc();

    delete QnRecordingManager::instance();
    QnRecordingManager::initStaticInstance( NULL );

    delete QnMServerResourceSearcher::instance();
    QnMServerResourceSearcher::initStaticInstance( NULL );

    QnResourceDiscoveryManager::instance()->stop();
    QnResource::stopAsyncTasks();

    delete QnResourceDiscoveryManager::instance();
    QnResourceDiscoveryManager::init( NULL );

    m_processor.reset();

    delete ThirdPartyResourceSearcher::instance();
    ThirdPartyResourceSearcher::initStaticInstance( NULL );

    delete UPNPDeviceSearcher::instance();
    UPNPDeviceSearcher::initGlobalInstance( NULL );

    connectorThread->quit();
    connectorThread->wait();

    //deleting object from wrong thread, but its no problem, since object's thread has been stopped and no event can be delivered to the object
    delete QnBusinessEventConnector::instance();
    QnBusinessEventConnector::initStaticInstance( NULL );

    QnBusinessRuleProcessor::fini();
    QnEventsDB::fini();

    delete QnMotionHelper::instance();
    QnMotionHelper::initStaticInstance( NULL );

    delete QnVideoCameraPool::instance();
    QnVideoCameraPool::initStaticInstance( NULL );

    delete QnResourcePool::instance();
    QnResourcePool::initStaticInstance( NULL );

    delete QnSoapServer::instance();
    QnSoapServer::initStaticInstance( NULL );

    av_lockmgr_register(NULL);

    qSettings.setValue("lastRunningTime", 0);
}

class QnVideoService : public QtService<QtSingleCoreApplication>
{
public:
    QnVideoService(int argc, char **argv): 
        QtService<QtSingleCoreApplication>(argc, argv, SERVICE_NAME),
        m_main(argc, argv),
        m_argc(argc),
        m_argv(argv)
    {
        setServiceDescription(SERVICE_NAME);
    }

protected:
    virtual int executeApplication() override { 

        QScopedPointer<QnPlatformAbstraction> platform(new QnPlatformAbstraction());
        QScopedPointer<QnMediaServerModule> module(new QnMediaServerModule(m_argc, m_argv));

        const int result = application()->exec();
        //QnBusinessRuleProcessor::fini();

        return result;
    }

    virtual void start() override
    {
        QtSingleCoreApplication *application = this->application();

        QString guid = serverGuid();
        if (guid.isEmpty())
        {
            cl_log.log("Can't save guid. Run once as administrator.", cl_logERROR);
            qApp->quit();
            return;
        }

        if (application->isRunning())
        {
            cl_log.log("Server already started", cl_logERROR);
            qApp->quit();
            return;
        }

        serverMain(m_argc, m_argv);
        m_main.start();
    }

    virtual void stop() override
    {
        if (serviceMainInstance)
            serviceMainInstance->stopSync();
    }

private:
    QnMain m_main;
    int m_argc;
    char **m_argv;

};

void stopServer(int signal)
{
    if (serviceMainInstance) {
        qWarning() << "got signal" << signal << "stop server!";
        serviceMainInstance->stopAsync();
    }
}

int main(int argc, char* argv[])
{
    QnVideoService service(argc, argv);

    int result = service.exec();

    return result;
}
