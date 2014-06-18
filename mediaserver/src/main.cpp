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
#include <core/resource/videowall_resource.h>

#include <events/mserver_business_rule_processor.h>

#include <media_server/media_server_module.h>

#include <motion/motion_helper.h>

#include <network/authenticate_helper.h>
#include <network/default_tcp_connection_processor.h>
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

#include <rest/handlers/acti_event_rest_handler.h>
#include <rest/handlers/business_event_log_rest_handler.h>
#include <rest/handlers/business_action_rest_handler.h>
#include <rest/handlers/camera_diagnostics_rest_handler.h>
#include <rest/handlers/camera_settings_rest_handler.h>
#include <rest/handlers/camera_bookmarks_rest_handler.h>
#include <rest/handlers/external_business_event_rest_handler.h>
#include <rest/handlers/favicon_rest_handler.h>
#include <rest/handlers/image_rest_handler.h>
#include <rest/handlers/log_rest_handler.h>
#include <rest/handlers/manual_camera_addition_rest_handler.h>
#include <rest/handlers/ping_rest_handler.h>
#include <rest/handlers/ptz_rest_handler.h>
#include <rest/handlers/rebuild_archive_rest_handler.h>
#include <rest/handlers/recorded_chunks_rest_handler.h>
#include <rest/handlers/statistics_rest_handler.h>
#include <rest/handlers/storage_space_rest_handler.h>
#include <rest/handlers/storage_status_rest_handler.h>
#include <rest/handlers/time_rest_handler.h>
#include <rest/handlers/update_rest_handler.h>
#include <rest/server/rest_connection_processor.h>
#include <rest/server/rest_server.h>

#include <streaming/hls/hls_server.h>

#include <rtsp/rtsp_connection.h>
#include <rtsp/rtsp_listener.h>

#include <soap/soapserver.h>

#include <utils/common/command_line_parser.h>
#include <utils/common/log.h>
#include <utils/common/sleep.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include <utils/common/system_information.h>
#include <utils/network/multicodec_rtp_reader.h>
#include <utils/network/simple_http_client.h>
#include <utils/network/ssl_socket.h>
#include <utils/network/modulefinder.h>


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
#include "common/common_module.h"
#include "utils/network/modulefinder.h"
#include "proxy/proxy_receiver_connection_processor.h"
#include "proxy/proxy_connection.h"
#include "compatibility.h"
#include "media_server/file_connection_processor.h"

#define USE_SINGLE_STREAMING_PORT

//#include "plugins/resources/digitalwatchdog/dvr/dw_dvr_resource_searcher.h"

// This constant is used while checking for compatibility.
// Do not change it until you know what you're doing.
static const char COMPONENT_NAME[] = "MediaServer";

static QString SERVICE_NAME = QString(QLatin1String(VER_COMPANYNAME_STR)) + QString(QLatin1String(" Media Server"));
static const quint64 DEFAULT_MAX_LOG_FILE_SIZE = 10*1024*1024;
static const quint64 DEFAULT_LOG_ARCHIVE_SIZE = 25;
static const quint64 DEFAULT_MSG_LOG_ARCHIVE_SIZE = 5;

class QnMain;
static QnMain* serviceMainInstance = 0;
void stopServer(int signal);

//#include "device_plugins/arecontvision/devices/av_device_server.h"

//#define TEST_RTSP_SERVER

static const int PROXY_POOL_SIZE = 8;
#ifdef EDGE_SERVER
static const int DEFAULT_MAX_CAMERAS = 1;
#else
static const int DEFAULT_MAX_CAMERAS = 128;
#endif

//!TODO: #ak have to do something with settings
class CmdLineArguments
{
public:
    QString logLevel;
    //!Log level of http requests log
    QString msgLogLevel;
    QString rebuildArchive;
    QString devModeKey;

    CmdLineArguments()
    :
        msgLogLevel( lit("none") )
    {
    }
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
    if (!target.isNull())
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

    // TODO: #Elric we need comments about true/false at call site => bad api design, use flags instead
    QnStoragePluginFactory::instance()->registerStoragePlugin("file", QnFileStorageResource::instance, true); // true means use it plugin if no <protocol>:// prefix
    QnStoragePluginFactory::instance()->registerStoragePlugin("coldstore", QnPlColdStoreStorage::instance, false); // true means use it plugin if no <protocol>:// prefix
}

QnStorageResourcePtr createStorage(const QString& path)
{
    QnStorageResourcePtr storage(QnStoragePluginFactory::instance()->createStorage("ufile"));
    storage->setName("Initial");
    storage->setUrl(path);
    storage->setSpaceLimit( MSSettings::roSettings()->value(nx_ms_conf::MIN_STORAGE_SPACE, nx_ms_conf::DEFAULT_MIN_STORAGE_SPACE).toLongLong() );
    storage->setUsedForWriting(storage->isStorageAvailableForWriting());
    QnResourceTypePtr resType = qnResTypePool->getResourceTypeByName("Storage");
    Q_ASSERT(resType);
    if (resType)
        storage->setTypeId(resType->getId());
    storage->setParentId(serverGuid());

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

void updateStorages(QnMediaServerResourcePtr mServer)
{
    qint64 bigStorageThreshold = 0;
    foreach(QnAbstractStorageResourcePtr abstractStorage, mServer->getStorages()) {
        QnStorageResourcePtr storage = abstractStorage.dynamicCast<QnStorageResource>();
        if (!storage)
            continue;
        qint64 available = storage->getTotalSpace() - storage->getSpaceLimit();
        bigStorageThreshold = qMax(bigStorageThreshold, available);
    }
    bigStorageThreshold /= QnStorageManager::BIG_STORAGE_THRESHOLD_COEFF;

    foreach(QnAbstractStorageResourcePtr abstractStorage, mServer->getStorages()) {
        QnStorageResourcePtr storage = abstractStorage.dynamicCast<QnStorageResource>();
        if (!storage)
            continue;
        qint64 available = storage->getTotalSpace() - storage->getSpaceLimit();
        if (available < bigStorageThreshold) {
            if (storage->isUsedForWriting()) {
                storage->setUsedForWriting(false);
                qWarning() << "Disable writing to storage" << storage->getUrl() << "because of low storage size";
            }
        }
    }
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
        if (server->getId() == serverGuid())
            return server;
    }

    return QnMediaServerResourcePtr();
}

QnMediaServerResourcePtr registerServer(ec2::AbstractECConnectionPtr ec2Connection, QnMediaServerResourcePtr serverPtr)
{
    QnMediaServerResourcePtr savedServer;
    serverPtr->setStatus(QnResource::Online, true);

    ec2::ErrorCode rez = ec2Connection->getMediaServerManager()->saveSync(serverPtr, &savedServer);
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

    return savedServer;
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

/** Initialize log. */
void initLog(const QString &logLevel) {
    if(logLevel == lit("none"))
        return;

    QnLog::initLog(logLevel);
    const QString& dataLocation = getDataDirectory();
    const QString& logFileLocation = MSSettings::roSettings()->value( "logDir", dataLocation + QLatin1String("/log/") ).toString();
    if (!QDir().mkpath(logFileLocation))
        cl_log.log(lit("Could not create log folder: ") + logFileLocation, cl_logALWAYS);
    const QString& logFileName = logFileLocation + QLatin1String("/log_file");
    if (!cl_log.create(
            logFileName,
            MSSettings::roSettings()->value( "maxLogFileSize", DEFAULT_MAX_LOG_FILE_SIZE ).toULongLong(),
            MSSettings::roSettings()->value( "logArchiveSize", DEFAULT_LOG_ARCHIVE_SIZE ).toULongLong(),
            cl_logDEBUG1))
        cl_log.log(lit("Could not create log file") + logFileName, cl_logALWAYS);
    MSSettings::roSettings()->setValue("logFile", logFileName);
    cl_log.log(QLatin1String("================================================================================="), cl_logALWAYS);
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

    initLog(cmdLineArguments.logLevel);

    QString logLevel;
    QString rebuildArchive;

    QnCommandLineParser commandLineParser;
    commandLineParser.addParameter(&logLevel, "--log-level", NULL, QString());
    commandLineParser.addParameter(&rebuildArchive, "--rebuild", NULL, QString(), "all");
    commandLineParser.parse(argc, argv, stderr);

    QnLog::initLog(logLevel);

    if( cmdLineArguments.msgLogLevel != lit("none") )
        QnLog::instance(QnLog::HTTP_LOG_INDEX)->create(
            dataLocation + QLatin1String("/log/http_log"),
            DEFAULT_MAX_LOG_FILE_SIZE,
            DEFAULT_MSG_LOG_ARCHIVE_SIZE,
            QnLog::logLevelFromString(cmdLineArguments.msgLogLevel) );

    if (rebuildArchive == "all")
        DeviceFileCatalog::setRebuildArchive(DeviceFileCatalog::Rebuild_All);
    else if (cmdLineArguments.rebuildArchive == "hq")
        DeviceFileCatalog::setRebuildArchive(DeviceFileCatalog::Rebuild_HQ);
    else if (cmdLineArguments.rebuildArchive == "lq")
        DeviceFileCatalog::setRebuildArchive(DeviceFileCatalog::Rebuild_LQ);
    
    cl_log.log(QN_APPLICATION_NAME, " started", cl_logALWAYS);
    cl_log.log("Software version: ", QN_APPLICATION_VERSION, cl_logALWAYS);
    cl_log.log("Software revision: ", QN_APPLICATION_REVISION, cl_logALWAYS);
    cl_log.log("binary path: ", QFile::decodeName(argv[0]), cl_logALWAYS);

    if( cmdLineArguments.logLevel != lit("none") )
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

void initAppServerConnection(const QSettings &settings)
{
    QUrl appServerUrl;
    QUrlQuery params;
    
    // ### remove
    QString host = settings.value("appserverHost").toString();
    if (QUrl(host).scheme() == "file")
        appServerUrl = QUrl(host); // it is a completed URL
    else if (host.isEmpty() || host == "localhost") 
    {
        appServerUrl = QUrl(QString("file:///") + closeDirPath(getDataDirectory()));

        QString staticDBPath = settings.value("staticDataDir").toString();
        if (!staticDBPath.isEmpty())
            params.addQueryItem("staticdb_path", staticDBPath);
    }
    else {
        appServerUrl.setScheme(settings.value("secureAppserverConnection", true).toBool() ? QLatin1String("https") : QLatin1String("http"));
        int port = settings.value("appserverPort", DEFAULT_APPSERVER_PORT).toInt();
        appServerUrl.setHost(host);
        appServerUrl.setPort(port);
    }

    QString userName = settings.value("appserverLogin", QLatin1String("admin")).toString();
    QString password = settings.value("appserverPassword", QLatin1String("123")).toString();
    appServerUrl.setUserName(userName);
    appServerUrl.setPassword(password);
    appServerUrl.setQuery(params);

    QUrl urlNoPassword(appServerUrl);
    urlNoPassword.setPassword("");
    cl_log.log("Connect to enterprise controller server ", urlNoPassword.toString(), cl_logINFO);
    QnAppServerConnectionFactory::setAuthKey(authKey());
    QnAppServerConnectionFactory::setClientGuid(serverGuid().toString());
    QnAppServerConnectionFactory::setDefaultUrl(appServerUrl);
    QnAppServerConnectionFactory::setDefaultFactory(QnResourceDiscoveryManager::instance());
    QnAppServerConnectionFactory::setBox(lit(QN_ARM_BOX));
}

void initAppServerEventConnection(const QSettings &settings, const QnMediaServerResourcePtr& mediaServer)
{
    QUrl appServerEventsUrl;

    //TODO: #GDM #Common move to server_message_processor as in client or remove if it is not needed anymore
    // ### remove
   /* appServerEventsUrl.setScheme(settings.value("secureAppserverConnection", true).toBool() ? QLatin1String("https") : QLatin1String("http"));
    appServerEventsUrl.setHost(settings.value("appserverHost", QLatin1String(DEFAULT_APPSERVER_HOST)).toString());
    appServerEventsUrl.setPort(settings.value("appserverPort", DEFAULT_APPSERVER_PORT).toInt());
    appServerEventsUrl.setUserName(settings.value("appserverLogin").toString());
    appServerEventsUrl.setPassword(settings.value("appserverPassword").toString());
    appServerEventsUrl.setPath("/events/");
    QUrlQuery appServerEventsUrlQuery;
    appServerEventsUrlQuery.addQueryItem("xid", mediaServer->getId().toString());
    appServerEventsUrlQuery.addQueryItem("guid", QnAppServerConnectionFactory::clientGuid());
    appServerEventsUrlQuery.addQueryItem("version", QN_ENGINE_VERSION);
    appServerEventsUrlQuery.addQueryItem("format", "pb");
    appServerEventsUrl.setQuery( appServerEventsUrlQuery );*/

    //QnServerMessageProcessor::instance()->init(QnAppServerConnectionFactory::getConnection2());
}


QnMain::QnMain(int argc, char* argv[])
    : m_argc(argc),
    m_argv(argv),
    m_startMessageSent(false),
    m_firstRunningTime(0),
    m_moduleFinder(0),
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

    if (m_moduleFinder)
    {
        delete m_moduleFinder;
        m_moduleFinder = 0;
    }

}

static const unsigned int APP_SERVER_REQUEST_ERROR_TIMEOUT_MS = 5500;

void QnMain::updateDisabledVendorsIfNeeded()
{
    static const QString DV_PROPERTY = QLatin1String("disabledVendors");

    QString disabledVendors = MSSettings::roSettings()->value(DV_PROPERTY).toString();
    QnUserResourcePtr admin = qnResPool->getAdministrator();
    if (!admin)
        return;

    if (!admin->hasProperty(DV_PROPERTY)) {
        QnGlobalSettings *settings = QnGlobalSettings::instance();
        settings->setDisabledVendors(disabledVendors);
        MSSettings::roSettings()->remove(DV_PROPERTY);
    }
}

void QnMain::loadResourcesFromECS(QnCommonMessageProcessor* messageProcessor)
{
    ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();

    ec2::ErrorCode rez;

    {
        //reading media servers list
        QnMediaServerResourceList mediaServerList;
        while( ec2Connection->getMediaServerManager()->getServersSync( &mediaServerList) != ec2::ErrorCode::ok )
        {
            NX_LOG( lit("QnMain::run(). Can't get media servers."), cl_logERROR );
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }
        foreach(const QnMediaServerResourcePtr &mediaServer, mediaServerList) 
            messageProcessor->updateResource(mediaServer);
    }
    

    {
        // read camera list
        QnVirtualCameraResourceList cameras;
        while ((rez = ec2Connection->getCameraManager()->getCamerasSync(QnId(), &cameras)) != ec2::ErrorCode::ok)
        {
            qDebug() << "QnMain::run(): Can't get cameras. Reason: " << ec2::toString(rez);
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }

        QnManualCameraInfoMap manualCameras;
        foreach(const QnSecurityCamResourcePtr &camera, cameras) {
            messageProcessor->updateResource(camera);
            if (camera->isManuallyAdded()) {
                QnResourceTypePtr resType = qnResTypePool->getResourceType(camera->getTypeId());
                manualCameras.insert(camera->getUrl(), QnManualCameraInfo(QUrl(camera->getUrl()), camera->getAuth(), resType->getName()));
            }
        }
        QnResourceDiscoveryManager::instance()->registerManualCameras(manualCameras);
    }

    {
        QnCameraHistoryList cameraHistoryList;
        while (( rez = ec2Connection->getCameraManager()->getCameraHistoryListSync(&cameraHistoryList)) != ec2::ErrorCode::ok)
        {
            qDebug() << "QnMain::run(): Can't get cameras history. Reason: " << ec2::toString(rez);
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }

        foreach(QnCameraHistoryPtr history, cameraHistoryList)
            QnCameraHistoryPool::instance()->addCameraHistory(history);
    }

    {
        //loading users
        QnUserResourceList users;
        while(( rez = ec2Connection->getUserManager()->getUsersSync(&users))  != ec2::ErrorCode::ok)
        {
            qDebug() << "QnMain::run(): Can't get users. Reason: " << ec2::toString(rez);
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }

        foreach(const QnUserResourcePtr &user, users)
            messageProcessor->updateResource(user);
    }

    {
        //loading videowalls
        QnVideoWallResourceList videowalls;
        while(( rez = ec2Connection->getVideowallManager()->getVideowallsSync(&videowalls))  != ec2::ErrorCode::ok)
        {
            qDebug() << "QnMain::run(): Can't get videowalls. Reason: " << ec2::toString(rez);
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }

        foreach(const QnVideoWallResourcePtr &videowall, videowalls)
            messageProcessor->updateResource(videowall);
    }

    {
        //loading business rules
        QnBusinessEventRuleList rules;
        while( (rez = ec2Connection->getBusinessEventManager()->getBusinessRulesSync(&rules)) != ec2::ErrorCode::ok )
        {
            qDebug() << "QnMain::run(): Can't get business rules. Reason: " << ec2::toString(rez);
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }

        foreach(const QnBusinessEventRulePtr &rule, rules)
            messageProcessor->on_businessEventAddedOrUpdated(rule);
    }

    {
        // load licenses
        QnLicenseList licenses;
        while( (rez = ec2Connection->getLicenseManager()->getLicensesSync(&licenses)) != ec2::ErrorCode::ok )
        {
            qDebug() << "QnMain::run(): Can't get license list. Reason: " << ec2::toString(rez);
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }

        foreach(const QnLicensePtr &license, licenses)
            messageProcessor->on_licenseChanged(license);
    }

}

void QnMain::at_localInterfacesChanged()
{
    m_mediaServer->setNetAddrList(allLocalAddresses());
    ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();
    ec2Connection->getMediaServerManager()->save(m_mediaServer, this, &QnMain::at_serverSaved);
}

void QnMain::at_serverSaved(int, ec2::ErrorCode err)
{
    if (err != ec2::ErrorCode::ok)
        qWarning() << "Error saving server.";
}

void QnMain::at_connectionOpened()
{
    if (m_firstRunningTime)
        qnBusinessRuleConnector->at_mserverFailure(qnResPool->getResourceById(serverGuid()).dynamicCast<QnMediaServerResource>(), m_firstRunningTime*1000, QnBusiness::ServerStartedReason, QString());
    if (!m_startMessageSent) {
        qnBusinessRuleConnector->at_mserverStarted(qnResPool->getResourceById(serverGuid()).dynamicCast<QnMediaServerResource>(), qnSyncTime->currentUSecsSinceEpoch());
        m_startMessageSent = true;
    }
    m_firstRunningTime = 0;
}

void QnMain::at_timer()
{
    MSSettings::runTimeSettings()->setValue("lastRunningTime", qnSyncTime->currentMSecsSinceEpoch());
    QnResourcePtr mServer = qnResPool->getResourceById(qnCommon->moduleGUID());
    foreach(QnResourcePtr res, qnResPool->getAllCameras(mServer)) 
    {
        QnVirtualCameraResourcePtr cam = res.dynamicCast<QnVirtualCameraResource>();
        if (cam)
            cam->noCameraIssues(); // decrease issue counter
    }
    
}

void QnMain::at_storageManager_noStoragesAvailable() {
    qnBusinessRuleConnector->at_NoStorages(m_mediaServer);
}

void QnMain::at_storageManager_storageFailure(QnResourcePtr storage, QnBusiness::EventReason reason) {
    qnBusinessRuleConnector->at_storageFailure(m_mediaServer, qnSyncTime->currentUSecsSinceEpoch(), reason, storage);
}

void QnMain::at_storageManager_rebuildFinished() {
    qnBusinessRuleConnector->at_archiveRebuildFinished(m_mediaServer);
}

void QnMain::at_cameraIPConflict(QHostAddress host, QStringList macAddrList)
{
    qnBusinessRuleConnector->at_cameraIPConflict(
        m_mediaServer,
        host,
        macAddrList,
        qnSyncTime->currentUSecsSinceEpoch());
}

void QnMain::at_peerFound(const QnModuleInformation &moduleInformation, const QString &remoteAddress, const QString &localInterfaceAddress) {
    Q_UNUSED(localInterfaceAddress)

    if (isCompatible(moduleInformation.version, QnSoftwareVersion(QN_APPLICATION_VERSION)) && moduleInformation.systemName == qnCommon->localSystemName()) {
        int port = moduleInformation.parameters.value("port").toInt();
        QString url = QString(lit("http://%1:%2")).arg(remoteAddress).arg(port);
        ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();
        ec2Connection->addRemotePeer(url, moduleInformation.id);
    }
}
void QnMain::at_peerLost(const QnModuleInformation &moduleInformation) {
    int port = moduleInformation.parameters.value("port").toInt();
    foreach (const QString &remoteAddress, moduleInformation.remoteAddresses) {
        QString url = QString(lit("http://%1:%2")).arg(remoteAddress).arg(port);
        ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();
        ec2Connection->deleteRemotePeer(url);
    }
}

void QnMain::initTcpListener()
{
    int rtspPort = MSSettings::roSettings()->value("rtspPort", MSSettings::DEFAUT_RTSP_PORT).toInt();
#ifdef USE_SINGLE_STREAMING_PORT
    QnRestProcessorPool::instance()->registerHandler("api/RecordedTimePeriods", new QnRecordedChunksRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/storageStatus", new QnStorageStatusRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/storageSpace", new QnStorageSpaceRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/statistics", new QnStatisticsRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/getCameraParam", new QnGetCameraParamRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/setCameraParam", new QnSetCameraParamRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/manualCamera", new QnManualCameraAdditionRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/ptz", new QnPtzRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/image", new QnImageRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/execAction", new QnBusinessActionRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/onEvent", new QnExternalBusinessEventRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/gettime", new QnTimeRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/ping", new QnPingRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/rebuildArchive", new QnRebuildArchiveRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/events", new QnBusinessEventLogRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/showLog", new QnLogRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/doCameraDiagnosticsStep", new QnCameraDiagnosticsRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/installUpdate", new QnUpdateRestHandler());
#ifdef QN_ENABLE_BOOKMARKS
    QnRestProcessorPool::instance()->registerHandler("api/cameraBookmarks", new QnCameraBookmarksRestHandler());
#endif
#ifdef ENABLE_ACTI
    QnActiResource::setEventPort(rtspPort);
    QnRestProcessorPool::instance()->registerHandler("api/camera_event", new QnActiEventRestHandler());  //used to receive event from acti camera. TODO: remove this from api
#endif
    QnRestProcessorPool::instance()->registerHandler("favicon.ico", new QnFavIconRestHandler());

    m_universalTcpListener = new QnUniversalTcpListener(QHostAddress::Any, rtspPort);
    m_universalTcpListener->enableSSLMode();
    m_universalTcpListener->addHandler<QnRtspConnectionProcessor>("RTSP", "*");
    m_universalTcpListener->addHandler<QnRestConnectionProcessor>("HTTP", "api");
    m_universalTcpListener->addHandler<QnRestConnectionProcessor>("HTTP", "ec2");
    m_universalTcpListener->addHandler<QnFileConnectionProcessor>("HTTP", "web/static");
    m_universalTcpListener->addHandler<QnProgressiveDownloadingConsumer>("HTTP", "media");
    m_universalTcpListener->addHandler<nx_hls::QnHttpLiveStreamingProcessor>("HTTP", "hls");
    //m_universalTcpListener->addHandler<QnDefaultTcpConnectionProcessor>("HTTP", "*");
    //m_universalTcpListener->addHandler<QnProxyConnectionProcessor>("HTTP", "*");

    m_universalTcpListener->addHandler<QnProxyConnectionProcessor>("*", "proxy");
    m_universalTcpListener->addHandler<QnProxyReceiverConnection>("PROXY", "*");

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

#ifdef ENABLE_ONVIF
    //starting soap server to accept event notifications from onvif servers
    QnSoapServer::initStaticInstance( new QnSoapServer(8083) ); //TODO/IMPL get port from settings or use any unused port?
    QnSoapServer::instance()->start();
#endif //ENABLE_ONVIF

    QnResourcePool::initStaticInstance( new QnResourcePool() );

    QScopedPointer<QnGlobalSettings> globalSettings(new QnGlobalSettings());

    QnAuthHelper::initStaticInstance(new QnAuthHelper());
    QnAuthHelper::instance()->restrictionList()->allow( lit("*/api/ping*"), AuthMethod::noAuth );
    QnAuthHelper::instance()->restrictionList()->allow( lit("*/api/camera_event*"), AuthMethod::noAuth );
    QnAuthHelper::instance()->restrictionList()->allow( lit("*/api/showLog*"), AuthMethod::urlQueryParam );

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

    QSettings* settings = MSSettings::roSettings();

    std::unique_ptr<QnMServerResourceDiscoveryManager> mserverResourceDiscoveryManager( new QnMServerResourceDiscoveryManager() );
    QnResourceDiscoveryManager::init( mserverResourceDiscoveryManager.get() );
    initAppServerConnection(*settings);

    QnMulticodecRtpReader::setDefaultTransport( MSSettings::roSettings()->value(QLatin1String("rtspTransport"), RtpTransport::_auto).toString().toUpper() );

    QScopedPointer<QnServerPtzControllerPool> ptzPool(new QnServerPtzControllerPool());

    //QnAppServerConnectionPtr appServerConnection = QnAppServerConnectionFactory::createConnection();


    connect(QnResourceDiscoveryManager::instance(), SIGNAL(CameraIPConflict(QHostAddress, QStringList)), this, SLOT(at_cameraIPConflict(QHostAddress, QStringList)));
    connect(QnStorageManager::instance(), SIGNAL(noStoragesAvailable()), this, SLOT(at_storageManager_noStoragesAvailable()));
    connect(QnStorageManager::instance(), SIGNAL(storageFailure(QnResourcePtr, QnBusiness::EventReason)), this, SLOT(at_storageManager_storageFailure(QnResourcePtr, QnBusiness::EventReason)));
    connect(QnStorageManager::instance(), SIGNAL(rebuildFinished()), this, SLOT(at_storageManager_rebuildFinished()));

    qnCommon->setModuleGUID(serverGuid());
    qnCommon->setLocalSystemName(settings->value("systemName").toString());

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
        const ec2::ErrorCode errorCode = ec2ConnectionFactory->connectSync( QnAppServerConnectionFactory::defaultUrl(), &ec2Connection );
        if( errorCode == ec2::ErrorCode::ok )
        {
            *connectInfo = ec2Connection->connectionInfo();
            NX_LOG( QString::fromLatin1("Connected to local EC2"), cl_logWARNING );
            break;
        }

        NX_LOG( QString::fromLatin1("Can't connect to local EC2. %1").arg(ec2::toString(errorCode)), cl_logERROR );
        QnSleep::msleep(3000);
    }

    QnAppServerConnectionFactory::setEc2Connection( ec2Connection );
    QnAppServerConnectionFactory::setEC2ConnectionFactory( ec2ConnectionFactory.get() );


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

    while (!needToStop() && !initResourceTypes(ec2Connection))
    {
        QnSleep::msleep(1000);
    }

    if (needToStop())
        return;


    QnResource::startCommandProc();

    QnResourcePool::instance(); // to initialize net state;

    QnRestProcessorPool restProcessorPool;
    QnRestProcessorPool::initStaticInstance( &restProcessorPool );

    if( QnAppServerConnectionFactory::defaultUrl().scheme().toLower() == lit("file") )
        ec2ConnectionFactory->registerRestHandlers( &restProcessorPool );

    initTcpListener();
    m_universalTcpListener->setProxyHandler<QnProxyConnectionProcessor>(messageProcessor.data(), QnServerMessageProcessor::isProxy);

    ec2ConnectionFactory->registerTransactionListener( m_universalTcpListener );

    QUrl proxyServerUrl = ec2Connection->connectionInfo().ecUrl;
    proxyServerUrl.setPort(connectInfo->proxyPort);
    m_universalTcpListener->setProxyParams(proxyServerUrl, serverGuid().toString());
    m_universalTcpListener->addProxySenderConnections(PROXY_POOL_SIZE);


    QHostAddress publicAddress = getPublicAddress();
    qnCommon->setModuleUlr(QString("http://%1:%2").arg(publicAddress.toString()).arg(m_universalTcpListener->getPort()));

    Qn::PanicMode pm;
    while (m_mediaServer.isNull() && !needToStop())
    {
        QnMediaServerResourcePtr server = findServer(ec2Connection, &pm);

        if (!server) {
            server = QnMediaServerResourcePtr(new QnMediaServerResource(qnResTypePool));
            server->setId(serverGuid());
            server->setPanicMode(pm);
            server->setMaxCameras(DEFAULT_MAX_CAMERAS);
        }
        server->setVersion(QnSoftwareVersion(QN_ENGINE_VERSION));
        server->setSystemInfo(QnSystemInformation(QN_APPLICATION_PLATFORM, QN_APPLICATION_ARCH));

        QString appserverHostString = MSSettings::roSettings()->value("appserverHost").toString();
        bool isLocal = appserverHostString.isEmpty() || QUrl(appserverHostString).scheme() == "file";

        int serverFlags = Qn::SF_None; // TODO: #Elric #EC2 type safety has just walked out of the window.
#ifdef EDGE_SERVER
        serverFlags |= Qn::SF_Edge;
#endif
        if (!isLocal)
            serverFlags |= Qn::SF_RemoteEC;
        if (!publicAddress.isNull())
            serverFlags |= Qn::SF_HasPublicIP;

        server->setServerFlags((Qn::ServerFlags) serverFlags);

        QHostAddress appserverHost;
        if (!isLocal) {
            do
            {
                appserverHost = resolveHost(appserverHostString);
            } while (appserverHost.toIPv4Address() == 0);
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
        else
            updateStorages(server);

        m_mediaServer = registerServer(ec2Connection, server);
        if (m_mediaServer.isNull())
            QnSleep::msleep(1000);
    }

    if (needToStop())
        return;


    syncStoragesToSettings(m_mediaServer);

    do {
        if (needToStop())
            return;
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
        newStorage->setId(QnId::createUuid());
        newStorage->setUrl(path);
        newStorage->setSpaceLimit( settings->value(nx_ms_conf::MIN_STORAGE_SPACE, nx_ms_conf::DEFAULT_MIN_STORAGE_SPACE).toLongLong() );
        newStorage->setUsedForWriting(false);
        newStorage->addFlags(QnResource::deprecated);

        qnStorageMan->addStorage(newStorage);
    }

    

    qnStorageMan->doMigrateCSVCatalog();

    initAppServerEventConnection(*MSSettings::roSettings(), m_mediaServer);
    
    QnRecordingManager::initStaticInstance( new QnRecordingManager() );
    QnRecordingManager::instance()->start();
    qnResPool->addResource(m_mediaServer);

    m_moduleFinder = new QnModuleFinder(false);
    //if (cmdLineArguments.devModeKey == lit("raz-raz-raz"))
        m_moduleFinder->setCompatibilityMode(true);
    QObject::connect(m_moduleFinder,    &QnModuleFinder::moduleFound,     this,   &QnMain::at_peerFound,  Qt::DirectConnection);
    QObject::connect(m_moduleFinder,    &QnModuleFinder::moduleLost,      this,   &QnMain::at_peerLost,   Qt::DirectConnection);

    QUrl url = ec2Connection->connectionInfo().ecUrl;
#if 1
    if (url.scheme() == "file") {
        // Connect to local database. Start peer-to-peer sync (enter to cluster mode)
        qnCommon->setCloudMode(true);
        m_moduleFinder->start();
    }
#endif
    // ------------------------------------------


    //===========================================================================
    //IPPH264Decoder::dll.init();

    //============================
    UPNPDeviceSearcher::initGlobalInstance( new UPNPDeviceSearcher() );

    std::unique_ptr<QnAppserverResourceProcessor> serverResourceProcessor( new QnAppserverResourceProcessor(m_mediaServer->getId()) );
    serverResourceProcessor->moveToThread( mserverResourceDiscoveryManager.get() );
    QnResourceDiscoveryManager::instance()->setResourceProcessor(serverResourceProcessor.get());

    //NOTE plugins have higher priority than built-in drivers
    ThirdPartyResourceSearcher thirdPartyResourceSearcher;
    QnResourceDiscoveryManager::instance()->addDeviceServer( &thirdPartyResourceSearcher );

#ifdef ENABLE_DESKTOP_CAMERA
    QnDesktopCameraResourceSearcher desktopCameraResourceSearcher;
    QnResourceDiscoveryManager::instance()->addDeviceServer(&desktopCameraResourceSearcher);
#endif  //ENABLE_DESKTOP_CAMERA

#ifndef EDGE_SERVER
#ifdef ENABLE_ARECONT
    QnPlArecontResourceSearcher arecontResourceSearcher;
    QnResourceDiscoveryManager::instance()->addDeviceServer(&arecontResourceSearcher);
#endif
#ifdef ENABLE_DLINK
    QnPlDlinkResourceSearcher dlinkSearcher;
    QnResourceDiscoveryManager::instance()->addDeviceServer(&dlinkSearcher);
#endif
#ifdef ENABLE_DROID
    QnPlIpWebCamResourceSearcher plIpWebCamResourceSearcher;
    QnResourceDiscoveryManager::instance()->addDeviceServer(&plIpWebCamResourceSearcher);

    QnPlDroidResourceSearcher droidResourceSearcher;
    QnResourceDiscoveryManager::instance()->addDeviceServer(&droidResourceSearcher);
#endif
#ifdef ENABLE_TEST_CAMERA
    QnTestCameraResourceSearcher testCameraResourceSearcher;
    QnResourceDiscoveryManager::instance()->addDeviceServer(&testCameraResourceSearcher);
#endif
#ifdef ENABLE_PULSE_CAMERA
    //QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlPulseSearcher::instance()); native driver does not support dual streaming! new pulse cameras works via onvif
#endif
#ifdef ENABLE_AXIS
    QnPlAxisResourceSearcher axisResourceSearcher;
    QnResourceDiscoveryManager::instance()->addDeviceServer(&axisResourceSearcher);
#endif
#ifdef ENABLE_ACTI
    QnActiResourceSearcher actiResourceSearcherInstance;
    QnResourceDiscoveryManager::instance()->addDeviceServer(&actiResourceSearcherInstance);
#endif
#ifdef ENABLE_STARDOT
    QnStardotResourceSearcher stardotResourceSearcher;
    QnResourceDiscoveryManager::instance()->addDeviceServer(&stardotResourceSearcher);
#endif
#ifdef ENABLE_IQE
    QnPlIqResourceSearcher iqResourceSearcher;
    QnResourceDiscoveryManager::instance()->addDeviceServer(&iqResourceSearcher);
#endif
#ifdef ENABLE_ISD
    QnPlISDResourceSearcher isdResourceSearcher;
    QnResourceDiscoveryManager::instance()->addDeviceServer(&isdResourceSearcher);
#endif

#if defined(Q_OS_WIN) && defined(ENABLE_VMAX)
    QnPlVmax480ResourceSearcher::initStaticInstance( new QnPlVmax480ResourceSearcher() );
    QnResourceDiscoveryManager::instance()->addDeviceServer(QnPlVmax480ResourceSearcher::instance());
#endif

    //Onvif searcher should be the last:
#ifdef ENABLE_ONVIF
    QnFlexWatchResourceSearcher flexWatchResourceSearcher;
    QnResourceDiscoveryManager::instance()->addDeviceServer(&flexWatchResourceSearcher);

    OnvifResourceSearcher onvifResourceSearcher;
    QnResourceDiscoveryManager::instance()->addDeviceServer(&onvifResourceSearcher);
#endif //ENABLE_ONVIF
#endif


    // Roman asked Ivan to comment it for Brian
    // QnResourceDiscoveryManager::instance()->addDTSServer(&QnColdStoreDTSSearcher::instance());

    //QnResourceDiscoveryManager::instance().addDeviceServer(&DwDvrResourceSearcher::instance());

    //

    //CLDeviceManager::instance().getDeviceSearcher().addDeviceServer(&FakeDeviceServer::instance());
    //CLDeviceSearcher::instance()->addDeviceServer(&IQEyeDeviceServer::instance());

    loadResourcesFromECS(messageProcessor.data());
#ifndef EDGE_SERVER
    updateDisabledVendorsIfNeeded();
    //QSet<QString> disabledVendors = QnGlobalSettings::instance()->disabledVendorsSet();
#endif

    //QnCommonMessageProcessor::instance()->init(ec2Connection); // start receiving notifications

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
    at_connectionOpened();
    timer.start(60 * 1000);


    QTimer::singleShot(0, this, SLOT(at_appStarted()));
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

    QnResourceDiscoveryManager::init( NULL );
    mserverResourceDiscoveryManager.reset();

    //since mserverResourceDiscoveryManager instance is dead no events can be delivered to serverResourceProcessor: can delete it now
        //TODO refactoring of discoveryManager <-> resourceProcessor interaction is required
    serverResourceProcessor.reset();

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

    QnAppServerConnectionFactory::setEc2Connection( ec2::AbstractECConnectionPtr() );

    av_lockmgr_register(NULL);

    // First disconnect eventManager from all slots, to not try to reconnect on connection close
    disconnect(QnServerMessageProcessor::instance());

    // This method will set flag on message channel to threat next connection close as normal
    //appServerConnection->disconnectSync();
    MSSettings::runTimeSettings()->setValue("lastRunningTime", 0);

    QnSSLSocket::releaseSSLEngine();
    QnAuthHelper::initStaticInstance(NULL);
}

void QnMain::at_appStarted()
{
    QnCommonMessageProcessor::instance()->init(QnAppServerConnectionFactory::getConnection2()); // start receiving notifications
};

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

        QUuid guid = serverGuid();
        if (guid.isNull())
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


int main(int argc, char* argv[])
{

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
    commandLineParser.addParameter(&cmdLineArguments.msgLogLevel, "--msg-log-level", NULL, QString(), "none");
    commandLineParser.addParameter(&cmdLineArguments.rebuildArchive, "--rebuild", NULL, QString(), "all");
    //commandLineParser.addParameter(&cmdLineArguments.devModeKey, "--dev-mode-key", NULL, QString());
    commandLineParser.addParameter(&configFilePath, "--conf-file", NULL, QString());
    commandLineParser.addParameter(&rwConfigFilePath, "--runtime-conf-file", NULL, QString());
    commandLineParser.addParameter(&showVersion, "--version", NULL, QString(), true);
    commandLineParser.addParameter(&showHelp, "--help", NULL, QString(), true);
    commandLineParser.parse(argc, argv, stderr, QnCommandLineParser::PreserveParsedParameters);

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

// TODO: #Elric we have QnCommandLineParser::print for that.
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
        "  --msg-log-level          Log value for http_log.log. Supported values same as above. Default is none (no logging)\n"
        "  --rebuild                Rebuild archive index. Supported values: all (high & low quality), hq (only high), lq (only low)\n"
        "  --conf-file              Path to config file. By default "<<MSSettings::defaultROSettingsFilePath().toStdString()<<"\n"
        "  --runtime-conf-file      Path to config file which is used to save some. By default "<<MSSettings::defaultRunTimeSettingsFilePath().toStdString()<<"\n"
        ;
}
