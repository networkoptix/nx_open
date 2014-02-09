#include "main.h"

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <signal.h>

#include <qtsinglecoreapplication.h>
#include "qtservice.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <QtCore/QUrl>
#include <QtCore/QUuid>
#include <QtCore/QThreadPool>

#include <QtNetwork/QUdpSocket>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QHostInfo>
#include <QtNetwork/QNetworkInterface>

#include <api/app_server_connection.h>
#include <api/session_manager.h>
#include <api/global_settings.h>

#include <appserver/processor.h>

#include <business/business_event_connector.h>
#include <business/business_event_rule.h>
#include <business/business_rule_processor.h>
#include <business/events/reasoned_business_event.h>

#include <camera/camera_pool.h>

#include <core/misc/schedule_task.h>
#include <core/resource_management/camera_driver_restriction_list.h>
#include <core/resource_management/mserver_resource_discovery_manager.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>

#include <events/mserver_business_rule_processor.h>

#include <media_server/media_server_module.h>

#include <motion/motion_helper.h>

#include <network/authenticate_helper.h>
#include <network/default_tcp_connection_processor.h>
#include <nx_ec/dummy_handler.h>
#include <nx_ec/ec2_lib.h>
#include <nx_ec/ec_api.h>

#include <platform/core_platform_abstraction.h>

#include <plugins/plugin_manager.h>
#include <plugins/resources/acti/acti_resource_searcher.h>
#include <plugins/resources/archive/avi_files/avi_resource.h>
#include <plugins/resources/arecontvision/resource/av_resource_searcher.h>
#include <plugins/resources/axis/axis_resource_searcher.h>
#include <plugins/resources/desktop_camera/desktop_camera_registrator.h>
#include <plugins/resources/desktop_camera/desktop_camera_resource_searcher.h>
#include <plugins/resources/d-link/dlink_resource_searcher.h>
#include <plugins/resources/droid/droid_resource_searcher.h>
#include <plugins/resources/droid_ipwebcam/ipwebcam_droid_resource_searcher.h>
#include <plugins/resources/flex_watch/flexwatch_resource_searcher.h>
#include <plugins/resources/iqinvision/iqinvision_resource_searcher.h>
#include <plugins/resources/isd/isd_resource_searcher.h>
#include <plugins/resources/mserver_resource_searcher.h>
#include <plugins/resources/onvif/onvif_resource_searcher.h>
#include <plugins/resources/pulse/pulse_resource_searcher.h>
#include <plugins/resources/stardot/stardot_resource_searcher.h>
#include <plugins/resources/test_camera/testcamera_resource_searcher.h>
#include <plugins/resources/third_party/third_party_resource_searcher.h>
#include <plugins/storage/coldstore/coldstore_storage.h>
#include <plugins/storage/dts/coldstore/coldstore_dts_resource_searcher.h>
#include <plugins/storage/dts/vmax480/vmax480_resource_searcher.h>
#include <plugins/storage/file_storage/file_storage_resource.h>

#include <recorder/file_deletor.h>
#include <recorder/recording_manager.h>
#include <recorder/storage_manager.h>

#include <rest/handlers/camera_diagnostics_handler.h>
#include <rest/handlers/camera_event_handler.h>
#include <rest/handlers/camera_settings_handler.h>
#include <rest/handlers/events_handler.h>
#include <rest/handlers/exec_action_handler.h>
#include <rest/handlers/ext_bevent_handler.h>
#include <rest/handlers/favico_handler.h>
#include <rest/handlers/image_handler.h>
#include <rest/handlers/log_handler.h>
#include <rest/handlers/manual_camera_addition_handler.h>
#include <rest/handlers/ping_handler.h>
#include <rest/handlers/ptz_handler.h>
#include <rest/handlers/rebuild_archive_handler.h>
#include <rest/handlers/recorded_chunks_handler.h>
#include <rest/handlers/statistics_handler.h>
#include <rest/handlers/storage_space_handler.h>
#include <rest/handlers/storage_status_handler.h>
#include <rest/handlers/time_handler.h>
#include <rest/server/rest_connection_processor.h>
#include <rest/server/rest_server.h>

#include <rtsp/rtsp_connection.h>
#include <rtsp/rtsp_listener.h>

#include <soap/soapserver.h>

#include <utils/common/command_line_parser.h>
#include <utils/common/log.h>
#include <utils/common/sleep.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include <utils/network/multicodec_rtp_reader.h>
#include <utils/network/simple_http_client.h>
#include <utils/network/ssl_socket.h>


#include <media_server/server_message_processor.h>
#include <media_server/settings.h>
#include <media_server/serverutil.h>
#include "version.h"

#ifdef _WIN32
#include "common/systemexcept_win32.h"
#endif
#include "core/ptz/server_ptz_controller_pool.h"
#include "plugins/resources/acti/acti_resource.h"
#include "transaction/transaction_message_bus.h"

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

static const int PROXY_POOL_SIZE = 8;

//!TODO: #ak have to do something with settings
class CmdLineArguments
{
public:
    QString logLevel;
    QString rebuildArchive;
};

static CmdLineArguments cmdLineArguments;

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
    storage->setSpaceLimit( QnStorageManager::DEFAULT_SPACE_LIMIT );
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
    QnAbstractStorageResourceList storages;
    //bool isBigStorageExist = false;
    qint64 bigStorageThreshold = 0;
    foreach(QString folderPath, listRecordFolders()) {
        QnStorageResourcePtr storage = createStorage(folderPath);
        qint64 available = storage->getTotalSpace() - storage->getSpaceLimit();
        bigStorageThreshold = qMax(bigStorageThreshold, available);
        storages.append(storage);
        cl_log.log(QString("Creating new storage: %1").arg(folderPath), cl_logINFO);
    }
    bigStorageThreshold /= QnStorageManager::BIG_STORAGE_THRESHOLD_COEFF;

    for (int i = 0; i < storages.size(); ++i) {
        QnStorageResourcePtr storage = storages[i].dynamicCast<QnStorageResource>();
        qint64 available = storage->getTotalSpace() - storage->getSpaceLimit();
        if (available < bigStorageThreshold)
            storage->setUsedForWriting(false);
    }

    return storages;
}

void setServerNameAndUrls(QnMediaServerResourcePtr server, const QString& myAddress, int port)
{
    if (server->getName().isEmpty())
        server->setName(QString("Server ") + myAddress);

    server->setUrl(QString("rtsp://%1:%2").arg(myAddress).arg(port));
    server->setApiUrl(QString("http://%1:%2").arg(myAddress).arg(port));
}

QnMediaServerResourcePtr findServer(ec2::AbstractECConnectionPtr ec2Connection, Qn::PanicMode* pm)
{
    QnMediaServerResourceList servers;
    *pm = Qn::PM_None;

    while (servers.isEmpty())
    {
        ec2::ErrorCode rez = ec2Connection->getMediaServerManager()->getServersSync( &servers);
        if( rez == ec2::ErrorCode::ok )
            break;

        qDebug() << "findServer(): Call to getServers failed. Reason: " << ec2::toString(rez);
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

QnMediaServerResourcePtr registerServer(ec2::AbstractECConnectionPtr ec2Connection, QnMediaServerResourcePtr serverPtr)
{
    QnMediaServerResourceList servers;
    serverPtr->setStatus(QnResource::Online, true);

    ec2::ErrorCode rez = ec2Connection->getMediaServerManager()->saveServerSync(serverPtr, &servers);
    if (rez != ec2::ErrorCode::ok)
    {
        qDebug() << "registerServer(): Call to registerServer failed. Reason: " << ec2::toString(rez);
        return QnMediaServerResourcePtr();
    }
    
    /*
    if (!authKey.isEmpty()) {
        MSSettings::roSettings()->setValue("authKey", authKey);
        QnAppServerConnectionFactory::setAuthKey(authKey);
    }
    */

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

static QtMessageHandler defaultMsgHandler = 0;

static void myMsgHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    if (defaultMsgHandler)
        defaultMsgHandler(type, ctx, msg);

    qnLogMsgHandler(type, ctx, msg);
}

int serverMain(int argc, char *argv[])
{
    Q_UNUSED(argc)
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

    const QString& dataLocation = getDataDirectory();
    QDir::setCurrent(qApp->applicationDirPath());

    if (cmdLineArguments.rebuildArchive.isEmpty()) {
        cmdLineArguments.rebuildArchive = MSSettings::runTimeSettings()->value("rebuild").toString();
    }
    MSSettings::runTimeSettings()->remove("rebuild");

    if( cmdLineArguments.logLevel != QString::fromLatin1("none") )
    {
        const QString& logDir = MSSettings::roSettings()->value( "logDir", dataLocation + QLatin1String("/log/") ).toString();
        QDir().mkpath( logDir );
        const QString& logFileName = logDir + QLatin1String("/log_file");
        //MSSettings::roSettings()->setValue("logFile", logFileName);
        if (!cl_log.create(logFileName, 1024*1024*10, 25, cl_logDEBUG1))
        {
            qApp->quit();

            return 0;
        }

        QnLog::initLog(cmdLineArguments.logLevel);
    }

    if (cmdLineArguments.rebuildArchive == "all")
        DeviceFileCatalog::setRebuildArchive(DeviceFileCatalog::Rebuild_All);
    else if (cmdLineArguments.rebuildArchive == "hq")
        DeviceFileCatalog::setRebuildArchive(DeviceFileCatalog::Rebuild_HQ);
    else if (cmdLineArguments.rebuildArchive == "lq")
        DeviceFileCatalog::setRebuildArchive(DeviceFileCatalog::Rebuild_LQ);
    
    cl_log.log(QN_APPLICATION_NAME, " started", cl_logALWAYS);
    cl_log.log("Software version: ", QN_APPLICATION_VERSION, cl_logALWAYS);
    cl_log.log("Software revision: ", QN_APPLICATION_REVISION, cl_logALWAYS);
    cl_log.log("binary path: ", QFile::decodeName(argv[0]), cl_logALWAYS);

    if( cmdLineArguments.logLevel != QString::fromLatin1("none") )
        defaultMsgHandler = qInstallMessageHandler(myMsgHandler);

    qnPlatform->process(NULL)->setPriority(QnPlatformProcess::HighPriority);

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

void initAppServerConnection(const QSettings &settings, bool tryDirectConnect)
{
    QUrl appServerUrl;

    // ### remove
    appServerUrl.setScheme(settings.value("secureAppserverConnection", true).toBool() ? QLatin1String("https") : QLatin1String("http"));
    QString host = settings.value("appserverHost", QLatin1String(DEFAULT_APPSERVER_HOST)).toString();
    int port = settings.value("appserverPort", DEFAULT_APPSERVER_PORT).toInt();
    QString userName = settings.value("appserverLogin", QLatin1String("admin")).toString();
    QString password = settings.value("appserverPassword", QLatin1String("123")).toString();
    appServerUrl.setHost(host);
    appServerUrl.setPort(port);
    appServerUrl.setUserName(userName);
    appServerUrl.setPassword(password);

    // check if it proxy connection and direct EC access is available
    if (tryDirectConnect) {
        QAuthenticator auth;
        auth.setUser(userName);
        auth.setPassword(password);
        static const int TEST_DIRECT_CONNECT_TIMEOUT = 2000;
        CLSimpleHTTPClient testClient(host, port, TEST_DIRECT_CONNECT_TIMEOUT, auth);
        CLHttpStatus result = testClient.doGET(lit("proxy_api/ec_port"));
        if (result == CL_HTTP_SUCCESS)
        {
            QUrl directURL;
            QByteArray data;
            testClient.readAll(data);
            directURL = appServerUrl;
            directURL.setPort(data.toInt());
            appServerUrl = directURL;
        }
    }

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
    appServerEventsUrl.setScheme(settings.value("secureAppserverConnection", true).toBool() ? QLatin1String("https") : QLatin1String("http"));
    appServerEventsUrl.setHost(settings.value("appserverHost", QLatin1String(DEFAULT_APPSERVER_HOST)).toString());
    appServerEventsUrl.setPort(settings.value("appserverPort", DEFAULT_APPSERVER_PORT).toInt());
    appServerEventsUrl.setUserName(settings.value("appserverLogin", QLatin1String("admin")).toString());
    appServerEventsUrl.setPassword(settings.value("appserverPassword", QLatin1String("123")).toString());
    appServerEventsUrl.setPath("/events/");
    QUrlQuery appServerEventsUrlQuery;
    appServerEventsUrlQuery.addQueryItem("xid", mediaServer->getId().toString());
    appServerEventsUrlQuery.addQueryItem("guid", QnAppServerConnectionFactory::clientGuid());
    appServerEventsUrlQuery.addQueryItem("version", QN_ENGINE_VERSION);
    appServerEventsUrlQuery.addQueryItem("format", "pb");
    appServerEventsUrl.setQuery( appServerEventsUrlQuery );

    QnServerMessageProcessor::instance()->init(appServerEventsUrl, settings.value("authKey").toString());
}


QnMain::QnMain(int argc, char* argv[])
    : m_argc(argc),
    m_argv(argv),
    m_firstRunningTime(0),
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

    QnStorageManager::instance()->cancelRebuildCatalogAsync();
    qnFileDeletor->pleaseStop();


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
    ec2::AbstractECConnectionPtr ec2Connection;
    QnAppServerConnectionFactory::ec2ConnectionFactory()->connectSync( QUrl(), &ec2Connection );

    QnVirtualCameraResourceList cameras;
    ec2::ErrorCode rez;
    while ((rez = ec2Connection->getCameraManager()->getCamerasSync(m_mediaServer->getId(), &cameras)) != ec2::ErrorCode::ok)
    {
        qDebug() << "QnMain::run(): Can't get cameras. Reason: " << ec2::toString(rez);
        QnSleep::msleep(10000);
    }

    QnManualCameraInfoMap manualCameras;
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
    while( ec2Connection->getMediaServerManager()->getServersSync( &mediaServerList) != ec2::ErrorCode::ok )
    {
        qWarning() << "QnMain::run(). Can't get media servers."; //.arg(QLatin1String(appServerConnection->getLastError())), cl_logERROR );
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
        //while( appServerConnection->getCameras(cameras, mediaServer->getId()) != 0 )
        while (ec2Connection->getCameraManager()->getCamerasSync(mediaServer->getId(), &cameras) != ec2::ErrorCode::ok)
        {
            NX_LOG( QString::fromLatin1("QnMain::run(). Error retreiving server %1(%2) cameras from enterprise controller. %3").
                arg(mediaServer->getId()).arg(mediaServer->getGuid()).arg(QLatin1String("" /*appServerConnection->getLastError()*/)), cl_logERROR );
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
        }
        foreach( const QnVirtualCameraResourcePtr &camera, cameras )
        {
            camera->addFlags( QnResource::foreigner );  //marking resource as not belonging to us
            qnResPool->addResource( camera );
        }
    }

    QnCameraHistoryList cameraHistoryList;
    while (( rez = ec2Connection->getCameraManager()->getCameraHistoryListSync(&cameraHistoryList)) != ec2::ErrorCode::ok)
    {
        qDebug() << "QnMain::run(): Can't get cameras history. Reason: " << ec2::toString(rez);
        QnSleep::msleep(1000);
    }

    foreach(QnCameraHistoryPtr history, cameraHistoryList)
    {
        QnCameraHistoryPool::instance()->addCameraHistory(history);
    }

    //loading business rules
    QnUserResourceList users;
    while(( rez = ec2Connection->getUserManager()->getUsersSync(&users))  != ec2::ErrorCode::ok)
    {
        qDebug() << "QnMain::run(): Can't get users. Reason: " << ec2::toString(rez);
        QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
    }

    foreach(const QnUserResourcePtr &user, users)
        qnResPool->addResource(user);

    //loading business rules
    QnBusinessEventRuleList rules;
    while( (rez = ec2Connection->getBusinessEventManager()->getBusinessRulesSync(&rules)) != ec2::ErrorCode::ok )
    {
        qDebug() << "QnMain::run(): Can't get business rules. Reason: " << ec2::toString(rez);
        QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
    }

    foreach(const QnBusinessEventRulePtr &rule, rules)
        qnBusinessRuleProcessor->addBusinessRule( rule );
}

void QnMain::at_localInterfacesChanged()
{
    m_mediaServer->setNetAddrList(allLocalAddresses());
    ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::createConnection2Sync();
    ec2Connection->getMediaServerManager()->saveServer(m_mediaServer, this, &QnMain::at_serverSaved);
}

void QnMain::at_serverSaved(int, ec2::ErrorCode err, const QnResourceList &)
{
    if (err != ec2::ErrorCode::ok)
        qWarning() << "Error saving server.";
}

void QnMain::at_connectionOpened()
{
    if (m_firstRunningTime)
        qnBusinessRuleConnector->at_mserverFailure(qnResPool->getResourceByGuid(serverGuid()).dynamicCast<QnMediaServerResource>(), m_firstRunningTime*1000, QnBusiness::MServerIssueStarted);
    qnBusinessRuleConnector->at_mserverStarted(qnResPool->getResourceByGuid(serverGuid()).dynamicCast<QnMediaServerResource>(), qnSyncTime->currentUSecsSinceEpoch());
    m_firstRunningTime = 0;
}

void QnMain::at_timer()
{
    MSSettings::runTimeSettings()->setValue("lastRunningTime", qnSyncTime->currentMSecsSinceEpoch());
    foreach(QnResourcePtr res, qnResPool->getAllEnabledCameras()) 
    {
        QnVirtualCameraResourcePtr cam = res.dynamicCast<QnVirtualCameraResource>();
        if (cam)
            cam->noCameraIssues(); // decrease issue counter
    }
    
}

void QnMain::at_noStorages()
{
    qnBusinessRuleConnector->at_NoStorages(m_mediaServer);
}

void QnMain::at_storageFailure(QnResourcePtr storage, QnBusiness::EventReason reason)
{
    qnBusinessRuleConnector->at_storageFailure(m_mediaServer, qnSyncTime->currentUSecsSinceEpoch(), reason, storage);
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
    int rtspPort = MSSettings::roSettings()->value("rtspPort", DEFAUT_RTSP_PORT).toInt();
#ifdef USE_SINGLE_STREAMING_PORT
    QnRestProcessorPool::instance()->registerHandler("api/RecordedTimePeriods", new QnRecordedChunksHandler());
    QnRestProcessorPool::instance()->registerHandler("api/storageStatus", new QnStorageStatusHandler());
    QnRestProcessorPool::instance()->registerHandler("api/storageSpace", new QnStorageSpaceHandler());
    QnRestProcessorPool::instance()->registerHandler("api/statistics", new QnStatisticsHandler());
    QnRestProcessorPool::instance()->registerHandler("api/getCameraParam", new QnGetCameraParamHandler());
    QnRestProcessorPool::instance()->registerHandler("api/setCameraParam", new QnSetCameraParamHandler());
    QnRestProcessorPool::instance()->registerHandler("api/manualCamera", new QnManualCameraAdditionHandler());
    QnRestProcessorPool::instance()->registerHandler("api/ptz", new QnPtzHandler());
    QnRestProcessorPool::instance()->registerHandler("api/image", new QnImageHandler());
    QnRestProcessorPool::instance()->registerHandler("api/execAction", new QnExecActionHandler());
    QnRestProcessorPool::instance()->registerHandler("api/onEvent", new QnExternalBusinessEventHandler());
    QnRestProcessorPool::instance()->registerHandler("api/gettime", new QnTimeHandler());
    QnRestProcessorPool::instance()->registerHandler("api/ping", new QnRestPingHandler());
    QnRestProcessorPool::instance()->registerHandler("api/rebuildArchive", new QnRestRebuildArchiveHandler());
    QnRestProcessorPool::instance()->registerHandler("api/events", new QnRestEventsHandler());
    QnRestProcessorPool::instance()->registerHandler("api/showLog", new QnRestLogHandler());
    QnRestProcessorPool::instance()->registerHandler("api/doCameraDiagnosticsStep", new QnCameraDiagnosticsHandler());
#ifdef ENABLE_ACTI
    QnActiResource::setEventPort(rtspPort);
    QnRestProcessorPool::instance()->registerHandler("api/camera_event", new QnCameraEventHandler());  //used to receive event from acti camera. TODO: remove this from api
#endif
    QnRestProcessorPool::instance()->registerHandler("favicon.ico", new QnRestFavicoHandler());

    m_universalTcpListener = new QnUniversalTcpListener(QHostAddress::Any, rtspPort);
    m_universalTcpListener->enableSSLMode();
    m_universalTcpListener->addHandler<QnRtspConnectionProcessor>("RTSP", "*");
    m_universalTcpListener->addHandler<QnRestConnectionProcessor>("HTTP", "api");
    m_universalTcpListener->addHandler<QnRestConnectionProcessor>("HTTP", "ec2");
    m_universalTcpListener->addHandler<QnProgressiveDownloadingConsumer>("HTTP", "media");
    m_universalTcpListener->addHandler<QnDefaultTcpConnectionProcessor>("HTTP", "*");

    if( !MSSettings::roSettings()->value("authenticationEnabled", "true").toBool() )
        m_universalTcpListener->disableAuth();

#ifdef ENABLE_DESKTOP_CAMERA
    m_universalTcpListener->addHandler<QnDesktopCameraRegistrator>("HTTP", "desktop_camera");
#endif   //ENABLE_DESKTOP_CAMERA

    m_universalTcpListener->start();

#else
    int apiPort = MSSettings::roSettings()->value("apiPort", DEFAULT_REST_PORT).toInt();
    int streamingPort = MSSettings::roSettings()->value("streamingPort", DEFAULT_STREAMING_PORT).toInt();

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

    if (MSSettings::roSettings()->value("publicIPEnabled").isNull())
        MSSettings::roSettings()->setValue("publicIPEnabled", 1);

    int publicIPEnabled = MSSettings::roSettings()->value("publicIPEnabled").toInt();

    if (publicIPEnabled == 0)
        return QHostAddress(); // disabled
    else if (publicIPEnabled > 1)
        return QHostAddress(MSSettings::roSettings()->value("staticPublicIP").toString()); // manually added

    QStringList urls = MSSettings::roSettings()->value("publicIPServers", DEFAULT_URL_LIST).toString().split(";");

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
    QFile f(QLatin1String(":/cert.pem"));
    if (!f.open(QIODevice::ReadOnly)) 
    {
        qWarning() << "No SSL sertificate for mediaServer!";
    }
    else {
        QByteArray certData = f.readAll();
        QnSSLSocket::initSSLEngine(certData);
    }

    QScopedPointer<QnServerMessageProcessor> messageProcessor(new QnServerMessageProcessor());

    // Create SessionManager
    QnSessionManager::instance()->start();

    ec2::DummyHandler dummyEcResponseHandler;
    
#ifdef ENABLE_ONVIF
    //starting soap server to accept event notifications from onvif servers
    QnSoapServer::initStaticInstance( new QnSoapServer(8083) ); //TODO/IMPL get port from settings or use any unused port?
    QnSoapServer::instance()->start();
#endif //ENABLE_ONVIF

    QnResourcePool::initStaticInstance( new QnResourcePool() );

    QScopedPointer<QnGlobalSettings> globalSettings(new QnGlobalSettings());

    QnAuthHelper::initStaticInstance(new QnAuthHelper());

    QnBusinessRuleProcessor::init(new QnMServerBusinessRuleProcessor());
    QnEventsDB::init();

    QnVideoCameraPool::initStaticInstance( new QnVideoCameraPool() );

    QnMotionHelper::initStaticInstance( new QnMotionHelper() );

    QnBusinessEventConnector::initStaticInstance( new QnBusinessEventConnector() );
    auto stopQThreadFunc = []( QThread* obj ){ obj->quit(); obj->wait(); delete obj; };
    std::unique_ptr<QThread, decltype(stopQThreadFunc)> connectorThread( new QThread(), stopQThreadFunc );
    connectorThread->start();
    qnBusinessRuleConnector->moveToThread(connectorThread.get());

    CameraDriverRestrictionList cameraDriverRestrictionList;

    QnResourceDiscoveryManager::init(new QnMServerResourceDiscoveryManager(cameraDriverRestrictionList));
    bool directConnectTried = true;
    initAppServerConnection(*MSSettings::roSettings(), directConnectTried);

    QnMulticodecRtpReader::setDefaultTransport( MSSettings::roSettings()->value(QLatin1String("rtspTransport"), RtpTransport::_auto).toString().toUpper() );

    QScopedPointer<QnServerPtzControllerPool> ptzPool(new QnServerPtzControllerPool());

    //QnAppServerConnectionPtr appServerConnection = QnAppServerConnectionFactory::createConnection();


    connect(QnResourceDiscoveryManager::instance(), SIGNAL(CameraIPConflict(QHostAddress, QStringList)), this, SLOT(at_cameraIPConflict(QHostAddress, QStringList)));
    connect(QnStorageManager::instance(), SIGNAL(noStoragesAvailable()), this, SLOT(at_noStorages()));
    connect(QnStorageManager::instance(), SIGNAL(storageFailure(QnResourcePtr, QnBusiness::EventReason)), this, SLOT(at_storageFailure(QnResourcePtr, QnBusiness::EventReason)));

    std::unique_ptr<ec2::AbstractECConnectionFactory> ec2ConnectionFactory(getConnectionFactory());

    ec2::ResourceContext resCtx(
        QnResourceDiscoveryManager::instance(),
        qnResPool,
        qnResTypePool );
    ec2ConnectionFactory->setContext(resCtx);
    ec2::AbstractECConnectionPtr ec2Connection;
    QnConnectionInfoPtr connectInfo(new QnConnectionInfo());
    while (!needToStop())
    {
        const ec2::ErrorCode errorCode = ec2ConnectionFactory->connectSync( QUrl(), &ec2Connection );
        if( errorCode == ec2::ErrorCode::ok )
        {
            *connectInfo = ec2Connection->connectionInfo();
            NX_LOG( QString::fromLatin1("Connected to local EC2"), cl_logWARNING );
            break;
        }

        NX_LOG( QString::fromLatin1("Can't connect to local EC2. %1").arg(ec2::toString(errorCode)), cl_logERROR );
    }

    QnAppServerConnectionFactory::setEC2ConnectionFactory( ec2ConnectionFactory.get() );


#ifdef OLD_EC
    while (!needToStop())
    {
        if (appServerConnection->connect(connectInfo) == 0)
            break;

        if (directConnectTried) {
            directConnectTried = false;
            initAppServerConnection(*MSSettings::roSettings(), directConnectTried);
            appServerConnection->setUrl(QnAppServerConnectionFactory::defaultUrl ());
            continue;
        }

        cl_log.log("Can't connect to Enterprise Controller: ", appServerConnection->getLastError(), cl_logWARNING);
        if (!needToStop())
            QnSleep::msleep(1000);
    }
#endif

    QnAppServerConnectionFactory::setDefaultMediaProxyPort(connectInfo->proxyPort);
    QnAppServerConnectionFactory::setPublicIp(connectInfo->publicIp);

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

    if (!compatibilityChecker->isCompatible(COMPONENT_NAME, QnSoftwareVersion(QN_ENGINE_VERSION), "ECS", connectInfo->version))
    {
        cl_log.log(cl_logERROR, "Incompatible Enterprise Controller version detected! Giving up.");
        return;
    }

#ifdef OLD_EC
    while (!needToStop() && !initResourceTypes(appServerConnection))
#else
    while (!needToStop() && !initResourceTypes(ec2Connection))
#endif
    {
        QnSleep::msleep(1000);
    }


#ifdef OLD_EC
    while (!needToStop() && !initLicenses(appServerConnection))
    {
        QnSleep::msleep(1000);
    }
#endif

    if (needToStop())
        return;


    QnResource::startCommandProc();

    QnResourcePool::instance(); // to initialize net state;

    QString appserverHostString = MSSettings::roSettings()->value("appserverHost", QLatin1String(DEFAULT_APPSERVER_HOST)).toString();

    QHostAddress appserverHost;
    do
    {
        appserverHost = resolveHost(appserverHostString);
    } while (appserverHost.toIPv4Address() == 0);

    QnRestProcessorPool restProcessorPool;
    QnRestProcessorPool::initStaticInstance( &restProcessorPool );

    ec2ConnectionFactory->registerRestHandlers( &restProcessorPool );
    ec2ConnectionFactory->registerTransactionListener( m_universalTcpListener );

    initTcpListener();

    QUrl proxyServerUrl = ec2Connection->connectionInfo().ecUrl;
    proxyServerUrl.setPort(connectInfo->proxyPort);
    m_universalTcpListener->setProxyParams(proxyServerUrl, serverGuid());
    m_universalTcpListener->addProxySenderConnections(PROXY_POOL_SIZE);

    QHostAddress publicAddress = getPublicAddress();

    Qn::PanicMode pm;
    while (m_mediaServer.isNull())
    {
        QnMediaServerResourcePtr server = findServer(ec2Connection, &pm);

        if (!server) {
            server = QnMediaServerResourcePtr(new QnMediaServerResource(qnResTypePool));
            server->setGuid(serverGuid());
            server->setPanicMode(pm);
        }

        setServerNameAndUrls(server, defaultLocalAddress(appserverHost), m_universalTcpListener->getPort());

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

        m_mediaServer = registerServer(ec2Connection, server);
        if (m_mediaServer.isNull())
            QnSleep::msleep(1000);
    }


    syncStoragesToSettings(m_mediaServer);

    int status;
    do {
    } while (ec2Connection->getResourceManager()->setResourceStatusSync(m_mediaServer->getId(), QnResource::Online) != ec2::ErrorCode::ok);


    QStringList usedPathList;
    foreach (QnAbstractStorageResourcePtr storage, m_mediaServer->getStorages())
    {
        qnResPool->addResource(storage);
        usedPathList << closeDirPath(storage->getUrl());
        QnStorageResourcePtr physicalStorage = qSharedPointerDynamicCast<QnStorageResource>(storage);
        if (physicalStorage)
            qnStorageMan->addStorage(physicalStorage);
    }
    
    
    // check old storages, absent in the DB
    QStringList allStoragePathList = qnStorageMan->getAllStoragePathes();
    foreach (const QString& path, allStoragePathList)
    {
        if (usedPathList.contains(path))
            continue;
        QnStorageResourcePtr newStorage = QnStorageResourcePtr(new QnFileStorageResource());
        newStorage->setId(QnId::generateSpecialId());
        newStorage->setUrl(path);
        newStorage->setSpaceLimit(QnStorageManager::DEFAULT_SPACE_LIMIT);
        newStorage->setUsedForWriting(false);
        newStorage->addFlags(QnResource::deprecated);

        qnStorageMan->addStorage(newStorage);
    }

    

    qnStorageMan->loadFullFileCatalog();

    initAppServerEventConnection(*MSSettings::roSettings(), m_mediaServer);
    QnServerMessageProcessor::instance()->run();

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


    QString disabledVendors = MSSettings::roSettings()->value("disabledVendors").toString();
    QStringList disabledVendorList;
    if (disabledVendors.contains(";"))
        disabledVendorList = disabledVendors.split(";");
    else
        disabledVendorList = disabledVendors.split(" ");
    QStringList updatedVendorList;        
    for (int i = 0; i < disabledVendorList.size(); ++i) {
    if (!disabledVendorList[i].trimmed().isEmpty())
            updatedVendorList << disabledVendorList[i].trimmed();
    }
    qWarning() << "disabled vendors amount" << updatedVendorList.size();
    qWarning() << disabledVendorList;        
    QnResourceDiscoveryManager::instance()->setDisabledVendors(updatedVendorList);

    //NOTE plugins have higher priority than built-in drivers
    ThirdPartyResourceSearcher::initStaticInstance( new ThirdPartyResourceSearcher( &cameraDriverRestrictionList ) );
    QnResourceDiscoveryManager::instance()->addDeviceServer(ThirdPartyResourceSearcher::instance());
#ifdef ENABLE_ARECONT
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnPlArecontResourceSearcher::instance());
#endif
#ifdef ENABLE_DLINK
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnPlDlinkResourceSearcher::instance());
#endif
#ifdef ENABLE_DROID
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnPlIpWebCamResourceSearcher::instance());
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnPlDroidResourceSearcher::instance());
#endif
#ifdef ENABLE_TEST_CAMERA
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnTestCameraResourceSearcher::instance());
#endif
#ifdef ENABLE_PULSE_CAMERA
    //QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlPulseSearcher::instance()); native driver does not support dual streaming! new pulse cameras works via onvif
#endif
#ifdef ENABLE_AXIS
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnPlAxisResourceSearcher::instance());
#endif
#ifdef ENABLE_ACTI
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnActiResourceSearcher::instance());
#endif
#ifdef ENABLE_STARDOT
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnStardotResourceSearcher::instance());
#endif
#ifdef ENABLE_IQE
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnPlIqResourceSearcher::instance());
#endif
#ifdef ENABLE_ISD
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnPlISDResourceSearcher::instance());
#endif
#ifdef ENABLE_DESKTOP_CAMERA
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnDesktopCameraResourceSearcher::instance());
#endif  //ENABLE_DESKTOP_CAMERA

#if defined(Q_OS_WIN) && defined(ENABLE_VMAX)
    QnPlVmax480ResourceSearcher::initStaticInstance( new QnPlVmax480ResourceSearcher() );
    QnResourceDiscoveryManager::instance()->addDeviceServer(QnPlVmax480ResourceSearcher::instance());
#endif

    //Onvif searcher should be the last:
#ifdef ENABLE_ONVIF
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnFlexWatchResourceSearcher::instance());
    QnResourceDiscoveryManager::instance()->addDeviceServer(&OnvifResourceSearcher::instance());
#endif //ENABLE_ONVIF

    

    // Roman asked Ivan to comment it for Brian
    // QnResourceDiscoveryManager::instance()->addDTSServer(&QnColdStoreDTSSearcher::instance());

    //QnResourceDiscoveryManager::instance().addDeviceServer(&DwDvrResourceSearcher::instance());

    //

    //CLDeviceManager::instance().getDeviceSearcher().addDeviceServer(&FakeDeviceServer::instance());
    //CLDeviceSearcher::instance()->addDeviceServer(&IQEyeDeviceServer::instance());

    loadResourcesFromECS();

    connect(QnServerMessageProcessor::instance(), SIGNAL(connectionReset()), this, SLOT(loadResourcesFromECS()));

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

    m_firstRunningTime = MSSettings::roSettings()->value("lastRunningTime").toLongLong();

    at_timer();
    QTimer timer;
    connect(&timer, SIGNAL(timeout()), this, SLOT(at_timer()), Qt::DirectConnection);
    connect(QnServerMessageProcessor::instance(), SIGNAL(connectionOpened()), this, SLOT(at_connectionOpened()), Qt::DirectConnection);
    timer.start(60 * 1000);


    exec();

    stopObjects();

    QnResource::stopCommandProc();

    delete QnRecordingManager::instance();
    QnRecordingManager::initStaticInstance( NULL );

    QnRestProcessorPool::initStaticInstance( nullptr );

    delete QnMServerResourceSearcher::instance();
    QnMServerResourceSearcher::initStaticInstance( NULL );

    delete QnVideoCameraPool::instance();
    QnVideoCameraPool::initStaticInstance( NULL );

    QnResourceDiscoveryManager::instance()->stop();
    QnResource::stopAsyncTasks();

    delete QnResourceDiscoveryManager::instance();
    QnResourceDiscoveryManager::init( NULL );

    m_processor.reset();

    delete ThirdPartyResourceSearcher::instance();
    ThirdPartyResourceSearcher::initStaticInstance( NULL );

#if defined(Q_OS_WIN) && defined(ENABLE_VMAX)
    delete QnPlVmax480ResourceSearcher::instance();
    QnPlVmax480ResourceSearcher::initStaticInstance( NULL );
#endif

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

    delete QnResourcePool::instance();
    QnResourcePool::initStaticInstance( NULL );

    QnStorageManager::instance()->stopAsyncTasks();

    ptzPool.reset();

#ifdef ENABLE_ONVIF
    delete QnSoapServer::instance();
    QnSoapServer::initStaticInstance( NULL );
#endif //ENABLE_ONVIF

    av_lockmgr_register(NULL);

    // First disconnect eventManager from all slots, to not try to reconnect on connection close
    disconnect(QnServerMessageProcessor::instance());

    // This method will set flag on message channel to threat next connection close as normal
    //appServerConnection->disconnectSync();
    MSSettings::runTimeSettings()->setValue("lastRunningTime", 0);

    QnSSLSocket::releaseSSLEngine();
    QnAuthHelper::initStaticInstance(NULL);
}

class QnVideoService : public QtService<QtSingleCoreApplication>
{
public:
    QnVideoService(int argc, char **argv): 
        QtService<QtSingleCoreApplication>(argc, argv, SERVICE_NAME),
        m_argc(argc),
        m_argv(argv)
    {
        setServiceDescription(SERVICE_NAME);
    }

protected:
    virtual int executeApplication() override { 
        QScopedPointer<QnCorePlatformAbstraction> platform(new QnCorePlatformAbstraction());
        QScopedPointer<QnLongRunnablePool> runnablePool(new QnLongRunnablePool());
        QScopedPointer<QnMediaServerModule> module(new QnMediaServerModule(m_argc, m_argv));
        
        m_main.reset(new QnMain(m_argc, m_argv));

        return application()->exec();
    }

    virtual void start() override
    {
        QtSingleCoreApplication *application = this->application();

        // check if local or remote EC. MServer changes guid depend of this fact
        bool primaryGuidAbsent = MSSettings::roSettings()->value(lit("serverGuid")).isNull();
        if (primaryGuidAbsent)
            MSSettings::roSettings()->setValue("separateGuidForRemoteEC", 1);

        QString ECHost = resolveHost(MSSettings::roSettings()->value("appserverHost").toString()).toString();
        bool isLocalAddr = (ECHost == lit("127.0.0.1") || ECHost == lit("localhost"));
        foreach(const QHostAddress& addr, allLocalAddresses())
        {
            if (addr.toString() == ECHost)
                isLocalAddr = true;
        }
        if (!isLocalAddr && MSSettings::roSettings()->value("separateGuidForRemoteEC").toBool())
            setUseAlternativeGuid(true);

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
        m_main->start();
    }

    virtual void stop() override
    {
        if (serviceMainInstance)
            serviceMainInstance->stopSync();
    }

private:
    int m_argc;
    char **m_argv;
    QScopedPointer<QnMain> m_main;
};

void stopServer(int signal)
{
    if (serviceMainInstance) {
        qWarning() << "got signal" << signal << "stop server!";
        serviceMainInstance->stopAsync();
    }
}

static void printVersion();
static void printHelp();


int testDigest();

int main(int argc, char* argv[])
{
    //return testDigest();




#if __arm__
#if defined(__GNUC__)
# if defined(__i386__)
        /* Enable Alignment Checking on x86 */
        __asm__("pushf\norl $0x40000,(%esp)\npopf");
# elif defined(__x86_64__) 
             /* Enable Alignment Checking on x86_64 */
            __asm__("pushf\norl $0x40000,(%rsp)\npopf");
# endif
#endif
#endif //__arm__

    ::srand( ::time(NULL) );
#ifdef _WIN32
    win32_exception::installGlobalUnhandledExceptionHandler();
#endif

    //parsing command-line arguments
    QString configFilePath;
    QString rwConfigFilePath;
    bool showVersion = false;
    bool showHelp = false;

    QnCommandLineParser commandLineParser;
    commandLineParser.addParameter(&cmdLineArguments.logLevel, "--log-level", NULL, QString());
    commandLineParser.addParameter(&cmdLineArguments.rebuildArchive, "--rebuild", NULL, QString(), "all");
    commandLineParser.addParameter(&configFilePath, "--conf-file", NULL, QString());
    commandLineParser.addParameter(&rwConfigFilePath, "--runtime-conf-file", NULL, QString());
    commandLineParser.addParameter(&showVersion, "--version", NULL, QString(), true);
    commandLineParser.addParameter(&showHelp, "--help", NULL, QString(), true);
    commandLineParser.parse(argc, argv, stderr);

    if( showVersion )
    {
        printVersion();
        return 0;
    }

    if( showHelp )
    {
        printHelp();
        return 0;
    }

    if( !configFilePath.isEmpty() )
        MSSettings::initializeROSettingsFromConfFile( configFilePath );
    if( !rwConfigFilePath.isEmpty() )
        MSSettings::initializeRunTimeSettingsFromConfFile( rwConfigFilePath );

    QnVideoService service( argc, argv );
    return service.exec();
}

static void printVersion()
{
    std::cout<<"  "<<QN_APPLICATION_NAME" v."<<QN_APPLICATION_VERSION<<std::endl;
}

static void printHelp()
{
    printVersion();

    std::cout<<"\n"
        "  --help                   This help message\n"
        "  --version                Print version info and exit\n"
        "  -e                       Start as console application\n"
        "  --log-level              Supported values: none (no logging), ALWAYS, ERROR, WARNING, INFO, DEBUG, DEBUG2. Default value is "
#ifdef _DEBUG
            "DEBUG\n"
#else
            "INFO\n"
#endif
        "  --rebuild                Rebuild archive index. Supported values: all (high & low quality), hq (only high), lq (only low)\n"
        "  --conf-file              Path to config file. By default "<<MSSettings::defaultROSettingsFilePath().toStdString()<<"\n"
        "  --runtime-conf-file      Path to config file which is used to save some. By default "<<MSSettings::defaultRunTimeSettingsFilePath().toStdString()<<"\n"
        ;
}


namespace nx_http
{
extern bool calcDigestResponse(
    const QString& method,
    const QString& userName,
    const QString& userPassword,
    const QUrl& url,
    const nx_http::Header::WWWAuthenticate& wwwAuthenticateHeader,
    nx_http::Header::DigestAuthorization* const digestAuthorizationHeader );
}

//int testDigest()
//{
//    {
//    nx_http::Header::WWWAuthenticate wwwAuthenticateHeader;
//    wwwAuthenticateHeader.authScheme = nx_http::Header::AuthScheme::digest;
//    wwwAuthenticateHeader.params["realm"] = "Surveillance Server";
//    wwwAuthenticateHeader.params["nonce"] = "06737538";
//    nx_http::Header::DigestAuthorization digestAuthorizationHeader;
//
//    bool res = nx_http::calcDigestResponse(
//        lit("DESCRIBE"),
//        lit("admin"), lit("admin"),
//        QUrl(lit("rtsp://192.168.1.104:554/0")),
//        wwwAuthenticateHeader,
//        &digestAuthorizationHeader );
//
//    int x = 0;
//
//    }
//
//
//    {
//    nx_http::Header::WWWAuthenticate wwwAuthenticateHeader;
//    wwwAuthenticateHeader.authScheme = nx_http::Header::AuthScheme::digest;
//    wwwAuthenticateHeader.params["realm"] = "Surveillance Server";
//    wwwAuthenticateHeader.params["nonce"] = "41261936";
//    nx_http::Header::DigestAuthorization digestAuthorizationHeader;
//
//    bool res = nx_http::calcDigestResponse(
//        lit("DESCRIBE"),
//        lit("admin"), lit("admin"),
//        QUrl(lit("rtsp://192.168.1.104:554/0")),
//        wwwAuthenticateHeader,
//        &digestAuthorizationHeader );
//
//    int x = 0;
//    }
//
//    return 0;
//}
