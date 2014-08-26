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

#include <platform/platform_abstraction.h>

#include <plugins/plugin_manager.h>
#include <plugins/resource/acti/acti_resource_searcher.h>
#include <plugins/resource/avi/avi_resource.h>
#include <plugins/resource/arecontvision/resource/av_resource_searcher.h>
#include <plugins/resource/axis/axis_resource_searcher.h>
#include <plugins/resource/desktop_camera/desktop_camera_registrator.h>
#include <plugins/resource/desktop_camera/desktop_camera_resource_searcher.h>
#include <plugins/resource/desktop_camera/desktop_camera_deleter.h>
#include <plugins/resource/d-link/dlink_resource_searcher.h>
#include <plugins/resource/droid/droid_resource_searcher.h>
#include <plugins/resource/droid_ipwebcam/ipwebcam_droid_resource_searcher.h>
#include <plugins/resource/flex_watch/flexwatch_resource_searcher.h>
#include <plugins/resource/iqinvision/iqinvision_resource_searcher.h>
#include <plugins/resource/isd/isd_resource_searcher.h>
#include <plugins/resource/mserver_resource_searcher.h>
#include <plugins/resource/mdns/mdns_listener.h>
#include <plugins/resource/onvif/onvif_resource_searcher.h>
#include <plugins/resource/pulse/pulse_resource_searcher.h>
#include <plugins/resource/stardot/stardot_resource_searcher.h>
#include <plugins/resource/test_camera/testcamera_resource_searcher.h>
#include <plugins/resource/third_party/third_party_resource_searcher.h>
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
#include <rest/handlers/get_system_name_rest_handler.h>
#include <rest/handlers/camera_diagnostics_rest_handler.h>
#include <rest/handlers/camera_settings_rest_handler.h>
#include <rest/handlers/camera_bookmarks_rest_handler.h>
#include <rest/handlers/external_business_event_rest_handler.h>
#include <rest/handlers/favicon_rest_handler.h>
#include <rest/handlers/image_rest_handler.h>
#include <rest/handlers/log_rest_handler.h>
#include <rest/handlers/manual_camera_addition_rest_handler.h>
#include <rest/handlers/ping_rest_handler.h>
#include <rest/handlers/ping_system_rest_handler.h>
#include <rest/handlers/ptz_rest_handler.h>
#include <rest/handlers/rebuild_archive_rest_handler.h>
#include <rest/handlers/recorded_chunks_rest_handler.h>
#include <rest/handlers/statistics_rest_handler.h>
#include <rest/handlers/storage_space_rest_handler.h>
#include <rest/handlers/storage_status_rest_handler.h>
#include <rest/handlers/time_rest_handler.h>
#include <rest/handlers/activate_license_rest_handler.h>
#include <rest/handlers/test_email_rest_handler.h>
#include <rest/handlers/update_rest_handler.h>
#include <rest/handlers/restart_rest_handler.h>
#include <rest/handlers/module_information_rest_handler.h>
#include <rest/handlers/routing_information_rest_handler.h>
#include <rest/handlers/configure_rest_handler.h>
#include <rest/handlers/join_system_rest_handler.h>
#include <rest/handlers/backup_db_rest_handler.h>
#include <rest/server/rest_connection_processor.h>

#include <streaming/hls/hls_server.h>

#include <rtsp/rtsp_connection.h>

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
#include <utils/network/module_finder.h>
#include <utils/network/global_module_finder.h>
#include <utils/network/router.h>
#include <media_server/server_update_tool.h>


#include <media_server/server_message_processor.h>
#include <media_server/settings.h>
#include <media_server/serverutil.h>
#include "version.h"

#ifdef _WIN32
#include "common/systemexcept_win32.h"
#endif
#include "core/ptz/server_ptz_controller_pool.h"
#include "plugins/resource/acti/acti_resource.h"
#include "transaction/transaction_message_bus.h"
#include "common/common_module.h"
#include "proxy/proxy_receiver_connection_processor.h"
#include "proxy/proxy_connection.h"
#include "compatibility.h"
#include "media_server/file_connection_processor.h"
#include "streaming/hls/hls_session_pool.h"
#include "llutil/hardware_id.h"
#include "api/runtime_info_manager.h"
#include "rest/handlers/old_client_connect_rest_handler.h"
#include "nx_ec/data/api_conversion_functions.h"
#include "media_server/resource_status_watcher.h"


// This constant is used while checking for compatibility.
// Do not change it until you know what you're doing.
static const char COMPONENT_NAME[] = "MediaServer";

static QString SERVICE_NAME = lit("%1 Server").arg(lit(QN_ORGANIZATION_NAME));
static const quint64 DEFAULT_MAX_LOG_FILE_SIZE = 10*1024*1024;
static const quint64 DEFAULT_LOG_ARCHIVE_SIZE = 25;
static const quint64 DEFAULT_MSG_LOG_ARCHIVE_SIZE = 5;

class QnMain;
static QnMain* serviceMainInstance = 0;
void stopServer(int signal);
bool restartFlag = false;
void restartServer();

namespace {
    const QString YES = lit("yes");
    const QString NO = lit("no");
    const QString GUID_IS_HWID = lit("guidIsHWID");
    const QString SERVER_GUID = lit("serverGuid");
    const QString SERVER_GUID2 = lit("serverGuid2");
    const QString OBSOLETE_SERVER_GUID = lit("obsoleteServerGuid");
    const QString PENDING_SWITCH_TO_CLUSTER_MODE = lit("pendingSwitchToClusterMode");
    const QString ADMIN_PASSWORD = lit("adminPassword");
};

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
    QString allowedDiscoveryPeers;

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

    NX_LOG( lit("FFMPEG %1").arg(QString::fromLocal8Bit(szMsg)), cl_logERROR);
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
        NX_LOG(lit("Couldn't resolve host %1").arg(hostString), cl_logERROR);
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
        NX_LOG( lit("No ipv4 address associated with host %1").arg(hostString), cl_logERROR);

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
#ifdef ENABLE_COLDSTORE
    QnStoragePluginFactory::instance()->registerStoragePlugin("coldstore", QnPlColdStoreStorage::instance, false); // true means use it plugin if no <protocol>:// prefix
#endif
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

        folderPaths.append(QDir::toNativeSeparators(path) + QN_MEDIA_FOLDER_NAME);
        /*
        int freeSpace = freeGB(path);

        if (maxFreeSpaceDrive.isEmpty() || freeSpace > maxFreeSpace) {
            maxFreeSpaceDrive = path;
            maxFreeSpace = freeSpace;
        }

        if (freeSpace >= 100) {
            NX_LOG(QString("Drive %1 has more than 100GB free space. Using it for storage.").arg(path), cl_logINFO);
            folderPaths.append(path + QN_MEDIA_FOLDER_NAME);
        }
        */
    }
    /*
    if (folderPaths.isEmpty()) {
        NX_LOG(QString("There are no drives with more than 100GB free space. Using drive %1 as it has the most free space: %2 GB").arg(maxFreeSpaceDrive).arg(maxFreeSpace), cl_logINFO);
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
        NX_LOG(QString("Creating new storage: %1").arg(folderPath), cl_logINFO);
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

bool updateStorages(QnMediaServerResourcePtr mServer)
{
    bool isModified = false;
    // I've switched all patches to native separator to fix network patches like \\computer\share
    foreach(QnAbstractStorageResourcePtr abstractStorage, mServer->getStorages()) 
    {
        QnStorageResourcePtr storage = abstractStorage.dynamicCast<QnStorageResource>();
        if (!storage)
            continue;
        if (!storage->getUrl().contains("://")) {
            QString updatedURL = QDir::toNativeSeparators(storage->getUrl());
            if (updatedURL.endsWith(QDir::separator()))
                updatedURL.chop(1);
            if (storage->getUrl() != updatedURL) {
                storage->setUrl(updatedURL);
                isModified = true;
            }
        }

    }

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
                isModified = true;
                qWarning() << "Disable writing to storage" << storage->getPath() << "because of low storage size";
            }
        }
    }
    return isModified;
}

void setServerNameAndUrls(QnMediaServerResourcePtr server, const QString& myAddress, int port)
{
    if (server->getName().isEmpty())
        server->setName(QString("Server ") + myAddress);

    server->setUrl(QString("rtsp://%1:%2").arg(myAddress).arg(port));
    server->setApiUrl(QString("http://%1:%2").arg(myAddress).arg(port));
}

QnMediaServerResourcePtr QnMain::findServer(ec2::AbstractECConnectionPtr ec2Connection, Qn::PanicMode* pm)
{
    QnMediaServerResourceList servers;
    *pm = Qn::PM_None;

    while (servers.isEmpty() && !needToStop())
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
    serverPtr->setStatus(Qn::Online, true);

    ec2::ErrorCode rez = ec2Connection->getMediaServerManager()->saveSync(serverPtr, &savedServer);
    if (rez != ec2::ErrorCode::ok)
    {
        qDebug() << "registerServer(): Call to registerServer failed. Reason: " << ec2::toString(rez);
        return QnMediaServerResourcePtr();
    }

    /*
    rez = ec2Connection->getResourceManager()->setResourceStatusSync(serverPtr->getId(), Qn::Online);
    if (rez != ec2::ErrorCode::ok)
    {
        qDebug() << "registerServer(): Call to change server status failed. Reason: " << ec2::toString(rez);
        return QnMediaServerResourcePtr();
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
        NX_LOG(lit("Could not create log folder: ") + logFileLocation, cl_logALWAYS);
    const QString& logFileName = logFileLocation + QLatin1String("/log_file");
    if (!cl_log.create(
            logFileName,
            MSSettings::roSettings()->value( "maxLogFileSize", DEFAULT_MAX_LOG_FILE_SIZE ).toULongLong(),
            MSSettings::roSettings()->value( "logArchiveSize", DEFAULT_LOG_ARCHIVE_SIZE ).toULongLong(),
            cl_logDEBUG1))
        NX_LOG(lit("Could not create log file") + logFileName, cl_logALWAYS);
    MSSettings::roSettings()->setValue("logFile", logFileName);
    NX_LOG(QLatin1String("================================================================================="), cl_logALWAYS);
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
    if (QCoreApplication::applicationVersion().isEmpty())
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
    
    NX_LOG(lit("%1 started").arg(QN_APPLICATION_NAME), cl_logALWAYS);
    NX_LOG(lit("Software version: %1").arg(QCoreApplication::applicationVersion()), cl_logALWAYS);
    NX_LOG(lit("Software revision: %1").arg(QN_APPLICATION_REVISION), cl_logALWAYS);
    NX_LOG(lit("binary path: %1").arg(QFile::decodeName(argv[0])), cl_logALWAYS);

    if( cmdLineArguments.logLevel != lit("none") )
        defaultMsgHandler = qInstallMessageHandler(myMsgHandler);

    qnPlatform->process(NULL)->setPriority(QnPlatformProcess::HighPriority);

    ffmpegInit();

    // ------------------------------------------
#ifdef TEST_RTSP_SERVER
    addTestData();
#endif

    return 0;
}

void initAppServerConnection(QSettings &settings)
{
    QUrl appServerUrl;
    QUrlQuery params;
    
    // ### remove
    QString host = settings.value("appserverHost").toString();
    if( QUrl( host ).scheme() == "file" )
    {
        appServerUrl = QUrl( host ); // it is a completed URL
    }
    else if (host.isEmpty() || host == "localhost") 
    {
        appServerUrl = QUrl::fromLocalFile( closeDirPath( getDataDirectory() ) );
    }
    else {
        appServerUrl.setScheme(settings.value("secureAppserverConnection", true).toBool() ? QLatin1String("https") : QLatin1String("http"));
        int port = settings.value("appserverPort", DEFAULT_APPSERVER_PORT).toInt();
        appServerUrl.setHost(host);
        appServerUrl.setPort(port);
    }
    if (appServerUrl.scheme() == "file")
    {
        QString staticDBPath = settings.value("staticDataDir").toString();
        if (!staticDBPath.isEmpty()) {
            params.addQueryItem("staticdb_path", staticDBPath);
		}
    }

    // TODO: Actually appserverPassword is always empty. Remove?
    QString userName = settings.value("appserverLogin", QLatin1String("admin")).toString();
    QString password = settings.value("appserverPassword", QLatin1String("")).toString();
    QByteArray authKey = settings.value("authKey").toByteArray();
    if (!authKey.isEmpty())
    {
        // convert from v2.2 format and encode value
        QByteArray prefix("SK_");
        if (!authKey.startsWith(prefix))
        {
            QByteArray authKeyBin = QUuid(authKey).toRfc4122();
            QByteArray authKeyEncoded = QnAuthHelper::symmetricalEncode(authKeyBin).toHex();
            settings.setValue(lit("authKey"), prefix + authKeyEncoded); // encode and update in settings
        }
        else {
            QByteArray authKeyEncoded = QByteArray::fromHex(authKey.mid(prefix.length()));
            QByteArray authKeyDecoded = QnAuthHelper::symmetricalEncode(authKeyEncoded);
            authKey = QUuid::fromRfc4122(authKeyDecoded).toByteArray();
        }

        userName = serverGuid().toString();
        password = authKey;
    }

    appServerUrl.setUserName(userName);
    appServerUrl.setPassword(password);
    appServerUrl.setQuery(params);

    QUrl urlNoPassword(appServerUrl);
    urlNoPassword.setPassword("");
    NX_LOG(lit("Connect to server %1").arg(urlNoPassword.toString()), cl_logINFO);
    QnAppServerConnectionFactory::setClientGuid(serverGuid().toString());
    QnAppServerConnectionFactory::setUrl(appServerUrl);
    QnAppServerConnectionFactory::setDefaultFactory(QnResourceDiscoveryManager::instance());
}

QnMain::QnMain(int argc, char* argv[])
:
    m_argc(argc),
    m_argv(argv),
    m_startMessageSent(false),
    m_firstRunningTime(0),
    m_moduleFinder(0),
    m_universalTcpListener(0)
{
    serviceMainInstance = this;
}

QnMain::~QnMain()
{
    quit();
    stop();
}

void QnMain::at_restartServerRequired()
{
    restartServer();
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
    if (qnFileDeletor)
        qnFileDeletor->pleaseStop();

    if (m_universalTcpListener)
        m_universalTcpListener->pleaseStop();
    if (m_moduleFinder)
        m_moduleFinder->pleaseStop();

    if (m_universalTcpListener) {
        m_universalTcpListener->stop();
        delete m_universalTcpListener;
        m_universalTcpListener = 0;
    }

    if (m_moduleFinder)
    {
        m_moduleFinder->stop();
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

void QnMain::updateAllowCameraCHangesIfNeed()
{
    static const QString DV_PROPERTY = QLatin1String("allowCameraChanges");

    QString allowCameraChanges = MSSettings::roSettings()->value(DV_PROPERTY).toString();
    if (!allowCameraChanges.isEmpty())
    {
        QnGlobalSettings *settings = QnGlobalSettings::instance();
        settings->setCameraSettingsOptimizationEnabled(allowCameraChanges.toLower() == lit("true") || allowCameraChanges == lit("1"));
        MSSettings::roSettings()->remove(DV_PROPERTY);
    }
}

void QnMain::loadResourcesFromECS(QnCommonMessageProcessor* messageProcessor)
{
    ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();

    ec2::ErrorCode rez;

    {
        //reading servers list
        QnMediaServerResourceList mediaServerList;
        while( ec2Connection->getMediaServerManager()->getServersSync( &mediaServerList) != ec2::ErrorCode::ok )
        {
            NX_LOG( lit("QnMain::run(). Can't get servers."), cl_logERROR );
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
        while ((rez = ec2Connection->getCameraManager()->getCamerasSync(QUuid(), &cameras)) != ec2::ErrorCode::ok)
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

void QnMain::at_storageManager_storageFailure(const QnResourcePtr& storage, QnBusiness::EventReason reason) {
    qnBusinessRuleConnector->at_storageFailure(m_mediaServer, qnSyncTime->currentUSecsSinceEpoch(), reason, storage);
}

void QnMain::at_storageManager_rebuildFinished() {
    qnBusinessRuleConnector->at_archiveRebuildFinished(m_mediaServer);
}

void QnMain::at_cameraIPConflict(const QHostAddress& host, const QStringList& macAddrList)
{
    qnBusinessRuleConnector->at_cameraIPConflict(
        m_mediaServer,
        host,
        macAddrList,
        qnSyncTime->currentUSecsSinceEpoch());
}

void QnMain::at_peerFound(const QnModuleInformation &moduleInformation, const QString &remoteAddress) {
    ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();

    if (isCompatible(moduleInformation.version, qnCommon->engineVersion()) && moduleInformation.systemName == qnCommon->localSystemName()) {
        QString url = QString( lit( "%1://%2:%3" ) ).arg( moduleInformation.sslAllowed ? lit( "https" ) : lit( "http" ) ).arg( remoteAddress ).arg( moduleInformation.port );
        ec2Connection->addRemotePeer(url, moduleInformation.id);
    } else {
        at_peerLost(moduleInformation);
    }
}
void QnMain::at_peerLost(const QnModuleInformation &moduleInformation) {
    ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();

    foreach (const QString &remoteAddress, moduleInformation.remoteAddresses) {
        QString url = QString(lit("http://%1:%2")).arg(remoteAddress).arg(moduleInformation.port);
        ec2Connection->deleteRemotePeer(url);
    }
}

bool QnMain::initTcpListener()
{
    const int rtspPort = MSSettings::roSettings()->value(nx_ms_conf::SERVER_PORT, nx_ms_conf::DEFAULT_SERVER_PORT).toInt();
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
    QnRestProcessorPool::instance()->registerHandler("api/activateLicense", new QnActivateLicenseRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/testEmailSettings", new QnTestEmailSettingsHandler());
    QnRestProcessorPool::instance()->registerHandler("api/ping", new QnPingRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/pingSystem", new QnPingSystemRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/rebuildArchive", new QnRebuildArchiveRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/events", new QnBusinessEventLogRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/showLog", new QnLogRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/getSystemName", new QnGetSystemNameRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/doCameraDiagnosticsStep", new QnCameraDiagnosticsRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/installUpdate", new QnUpdateRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/restart", new QnRestartRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/connect", new QnOldClientConnectRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/moduleInformation", new QnModuleInformationRestHandler() );
    QnRestProcessorPool::instance()->registerHandler("api/moduleInformationAuthenticated", new QnModuleInformationRestHandler() );
    QnRestProcessorPool::instance()->registerHandler("api/routingInformation", new QnRoutingInformationRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/configure", new QnConfigureRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/joinSystem", new QnJoinSystemRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/backupDatabase", new QnBackupDbRestHandler());
#ifdef QN_ENABLE_BOOKMARKS
    QnRestProcessorPool::instance()->registerHandler("api/cameraBookmarks", new QnCameraBookmarksRestHandler());
#endif
#ifdef ENABLE_ACTI
    QnActiResource::setEventPort(rtspPort);
    QnRestProcessorPool::instance()->registerHandler("api/camera_event", new QnActiEventRestHandler());  //used to receive event from acti camera. TODO: remove this from api
#endif
    QnRestProcessorPool::instance()->registerHandler("favicon.ico", new QnFavIconRestHandler());

    m_universalTcpListener = new QnUniversalTcpListener(
        QHostAddress::Any,
        rtspPort,
        QnTcpListener::DEFAULT_MAX_CONNECTIONS,
        MSSettings::roSettings()->value( nx_ms_conf::ALLOW_SSL_CONNECTIONS, nx_ms_conf::DEFAULT_ALLOW_SSL_CONNECTIONS ).toBool() );
    if( !m_universalTcpListener->bindToLocalAddress() )
        return false;
    m_universalTcpListener->setDefaultPage("/static/index.html");
    m_universalTcpListener->addHandler<QnRtspConnectionProcessor>("RTSP", "*");
    m_universalTcpListener->addHandler<QnRestConnectionProcessor>("HTTP", "api");
    m_universalTcpListener->addHandler<QnRestConnectionProcessor>("HTTP", "ec2");
    m_universalTcpListener->addHandler<QnFileConnectionProcessor>("HTTP", "static");
    m_universalTcpListener->addHandler<QnProgressiveDownloadingConsumer>("HTTP", "media");
    m_universalTcpListener->addHandler<nx_hls::QnHttpLiveStreamingProcessor>("HTTP", "hls");
    //m_universalTcpListener->addHandler<QnDefaultTcpConnectionProcessor>("HTTP", "*");
    //m_universalTcpListener->addHandler<QnProxyConnectionProcessor>("HTTP", "*");

    m_universalTcpListener->addHandler<QnProxyConnectionProcessor>("*", "proxy");
    //m_universalTcpListener->addHandler<QnProxyReceiverConnection>("PROXY", "*");

    if( !MSSettings::roSettings()->value("authenticationEnabled", "true").toBool() )
        m_universalTcpListener->disableAuth();

#ifdef ENABLE_DESKTOP_CAMERA
    m_universalTcpListener->addHandler<QnDesktopCameraRegistrator>("HTTP", "desktop_camera");
#endif   //ENABLE_DESKTOP_CAMERA

    m_universalTcpListener->start();

    return true;
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
    QFile f( MSSettings::roSettings()->value( nx_ms_conf::SSL_CERTIFICATE_PATH, getDataDirectory() + lit( "/ssl/cert.pem" ) ).toString() );
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "Could not find SSL certificate at "<<f.fileName()<<". Using built-in certificate";
        f.setFileName( ":cert.pem" );
        if( !f.open( QIODevice::ReadOnly ) )
            qWarning() << "Could not load built-in SSL certificate "<<f.fileName();
    }
    if( f.isOpen() )
    {
        const QByteArray& certData = f.readAll();
        QnSSLSocket::initSSLEngine( certData );
    }

    QnSyncTime syncTime;

    QScopedPointer<QnServerMessageProcessor> messageProcessor(new QnServerMessageProcessor());
    QScopedPointer<QnRuntimeInfoManager> runtimeInfoManager(new QnRuntimeInfoManager());

    // Create SessionManager
    QnSessionManager::instance()->start();

#ifdef ENABLE_ONVIF
    QnSoapServer soapServer;    //starting soap server to accept event notifications from onvif cameras
    soapServer.bind();
    soapServer.start();
#endif //ENABLE_ONVIF

    QnResourcePool::initStaticInstance( new QnResourcePool() );

    QScopedPointer<QnGlobalSettings> globalSettings(new QnGlobalSettings());

    QnAuthHelper::initStaticInstance(new QnAuthHelper());
    connect(QnAuthHelper::instance(), &QnAuthHelper::emptyDigestDetected, this, &QnMain::at_emptyDigestDetected);
    QnAuthHelper::instance()->restrictionList()->allow( lit("*/api/ping"), AuthMethod::noAuth );
    QnAuthHelper::instance()->restrictionList()->allow( lit("*/api/camera_event*"), AuthMethod::noAuth );
    QnAuthHelper::instance()->restrictionList()->allow( lit("*/api/showLog*"), AuthMethod::urlQueryParam );
    QnAuthHelper::instance()->restrictionList()->allow( lit("*/api/moduleInformation"), AuthMethod::noAuth );

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

    QnStorageManager storageManager;
    QnFileDeletor fileDeletor;

    connect(QnResourceDiscoveryManager::instance(), &QnResourceDiscoveryManager::CameraIPConflict, this, &QnMain::at_cameraIPConflict);
    connect(QnStorageManager::instance(), &QnStorageManager::noStoragesAvailable, this, &QnMain::at_storageManager_noStoragesAvailable);
    connect(QnStorageManager::instance(), &QnStorageManager::storageFailure, this, &QnMain::at_storageManager_storageFailure);
    connect(QnStorageManager::instance(), &QnStorageManager::rebuildFinished, this, &QnMain::at_storageManager_rebuildFinished);

    QString dataLocation = getDataDirectory();
    QDir stateDirectory;
    stateDirectory.mkpath(dataLocation + QLatin1String("/state"));
    fileDeletor.init(dataLocation + QLatin1String("/state")); // constructor got root folder for temp files


    // If adminPassword is set by installer save it and create admin user with it if not exists yet
    qnCommon->setDefaultAdminPassword(settings->value(ADMIN_PASSWORD, QLatin1String("")).toString());
    connect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoAdded, this, &QnMain::at_runtimeInfoChanged);
    connect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoChanged, this, &QnMain::at_runtimeInfoChanged);

    qnCommon->setModuleGUID(serverGuid());
    qnCommon->setLocalSystemName(settings->value("systemName").toString());

    std::unique_ptr<ec2::AbstractECConnectionFactory> ec2ConnectionFactory(getConnectionFactory( Qn::PT_Server ));

    ec2::ApiRuntimeData runtimeData;
    runtimeData.peer.id = qnCommon->moduleGUID();
    runtimeData.peer.peerType = Qn::PT_Server;
    runtimeData.box = lit(QN_ARM_BOX);
    runtimeData.brand = lit(QN_PRODUCT_NAME_SHORT);
	runtimeData.platform = QN_APPLICATION_PLATFORM;
    int guidCompatibility = 0;
    runtimeData.mainHardwareIds = LLUtil::getMainHardwareIds(guidCompatibility);
    runtimeData.compatibleHardwareIds = LLUtil::getCompatibleHardwareIds(guidCompatibility);
    QnRuntimeInfoManager::instance()->items()->addItem(runtimeData);    // initializing localInfo

    ec2::ResourceContext resCtx(
        QnResourceDiscoveryManager::instance(),
        qnResPool,
        qnResTypePool );
    ec2ConnectionFactory->setContext(resCtx);
    ec2::AbstractECConnectionPtr ec2Connection;
    QnConnectionInfo connectInfo;
    while (!needToStop())
    {
        const ec2::ErrorCode errorCode = ec2ConnectionFactory->connectSync( QnAppServerConnectionFactory::url(), &ec2Connection );
        if( errorCode == ec2::ErrorCode::ok )
        {
            connectInfo = ec2Connection->connectionInfo();
            NX_LOG( QString::fromLatin1("Connected to local EC2"), cl_logWARNING );
            break;
        }

        NX_LOG( QString::fromLatin1("Can't connect to local EC2. %1").arg(ec2::toString(errorCode)), cl_logERROR );
        QnSleep::msleep(3000);
    }
    connect(ec2Connection.get(), &ec2::AbstractECConnection::databaseDumped, this, &QnMain::at_restartServerRequired);
    qnCommon->setRemoteGUID(QUuid(connectInfo.ecsGuid));
    MSSettings::roSettings()->sync();
    if (MSSettings::roSettings()->value(PENDING_SWITCH_TO_CLUSTER_MODE).toString() == "yes") {
        NX_LOG( QString::fromLatin1("Switching to cluster mode and restarting..."), cl_logWARNING );
        MSSettings::roSettings()->setValue("systemName", connectInfo.systemName);
        MSSettings::roSettings()->remove("appserverHost");
        MSSettings::roSettings()->remove("appserverPort");
        MSSettings::roSettings()->remove("appserverLogin");
        MSSettings::roSettings()->remove("appserverPassword");
        MSSettings::roSettings()->remove(PENDING_SWITCH_TO_CLUSTER_MODE);
        MSSettings::roSettings()->sync();

        QFile::remove(closeDirPath(getDataDirectory()) + "/ecs.sqlite");

        // kill itself to restart
        abort();
        return;
    }
      
    QnAppServerConnectionFactory::setEc2Connection( ec2Connection );
    QnAppServerConnectionFactory::setEC2ConnectionFactory( ec2ConnectionFactory.get() );
    
    QString publicIP = getPublicAddress().toString();
    if (!publicIP.isEmpty()) {
        QnPeerRuntimeInfo localInfo = QnRuntimeInfoManager::instance()->localInfo();
        localInfo.data.publicIP = publicIP;
        QnRuntimeInfoManager::instance()->items()->updateItem(localInfo.uuid, localInfo);
    }
    connect( ec2Connection.get(), &ec2::AbstractECConnection::timeChanged,
             QnSyncTime::instance(), (void(QnSyncTime::*)(qint64))&QnSyncTime::updateTime );

    QnMServerResourceSearcher::initStaticInstance( new QnMServerResourceSearcher() );
    QnMServerResourceSearcher::instance()->setAppPServerGuid(connectInfo.ecsGuid.toUtf8());
    QnMServerResourceSearcher::instance()->start();

    //Initializing plugin manager
    PluginManager::instance()->loadPlugins();

    if (needToStop())
        return;

    QnCompatibilityChecker remoteChecker(connectInfo.compatibilityItems);
    QnCompatibilityChecker localChecker(localCompatibilityItems());

    QnCompatibilityChecker* compatibilityChecker;
    if (remoteChecker.size() > localChecker.size())
        compatibilityChecker = &remoteChecker;
    else
        compatibilityChecker = &localChecker;

    if (!compatibilityChecker->isCompatible(COMPONENT_NAME, qnCommon->engineVersion(), "ECS", connectInfo.version))
    {
        NX_LOG(lit("Incompatible Server version detected! Giving up."), cl_logERROR);
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

    if( QnAppServerConnectionFactory::url().scheme().toLower() == lit("file") )
        ec2ConnectionFactory->registerRestHandlers( &restProcessorPool );

    nx_hls::HLSSessionPool hlsSessionPool;

    if( !initTcpListener() )
    {
        qCritical() << "Failed to bind to local port. Terminating...";
        QCoreApplication::quit();
        return;
    }

    using namespace std::placeholders;
    m_universalTcpListener->setProxyHandler<QnProxyConnectionProcessor>( std::bind( &QnServerMessageProcessor::isProxy, messageProcessor.data(), _1 ) );

    ec2ConnectionFactory->registerTransactionListener( m_universalTcpListener );

    QUrl proxyServerUrl = ec2Connection->connectionInfo().ecUrl;
    //proxyServerUrl.setPort(connectInfo->proxyPort);
    m_universalTcpListener->setProxyParams(proxyServerUrl, serverGuid().toString());
    //m_universalTcpListener->addProxySenderConnections(PROXY_POOL_SIZE);


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
        server->setVersion(qnCommon->engineVersion());
        server->setSystemInfo(QnSystemInformation::currentSystemInformation());

        QString appserverHostString = MSSettings::roSettings()->value("appserverHost").toString();
        bool isLocal = appserverHostString.isEmpty() || appserverHostString == "localhost" || QUrl(appserverHostString).scheme() == "file";

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

        bool isModified = false;
        if (server->getNetAddrList() != serverIfaceList) {
            server->setNetAddrList(serverIfaceList);
            isModified = true;
        }

        QnAbstractStorageResourceList storages = server->getStorages();
        if (storages.isEmpty()) {
            server->setStorages(createStorages());
            isModified = true;
        }
        else if (updateStorages(server)) {
            isModified = true;
        }

        if (server->getSystemName() != qnCommon->localSystemName()) {
            server->setSystemName(qnCommon->localSystemName());
            isModified = true;
        }
        
        if (isModified)
            m_mediaServer = registerServer(ec2Connection, server);
        else
            m_mediaServer = server;

        if (m_mediaServer.isNull())
            QnSleep::msleep(1000);
    }

    MSSettings::roSettings()->remove(OBSOLETE_SERVER_GUID);
    MSSettings::roSettings()->remove(ADMIN_PASSWORD);

    if (needToStop()) {
        stopObjects();
        return;
    }

    do {
        if (needToStop())
            return;
    } while (ec2Connection->getResourceManager()->setResourceStatusSync(m_mediaServer->getId(), Qn::Online) != ec2::ErrorCode::ok);

    QStringList usedPathList;
    foreach (QnAbstractStorageResourcePtr storage, m_mediaServer->getStorages())
    {
        qnResPool->addResource(storage);
        usedPathList << storage->getPath();
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
        newStorage->setId(QUuid::createUuid());
        newStorage->setUrl(path);
        newStorage->setSpaceLimit( settings->value(nx_ms_conf::MIN_STORAGE_SPACE, nx_ms_conf::DEFAULT_MIN_STORAGE_SPACE).toLongLong() );
        newStorage->setUsedForWriting(false);
        newStorage->addFlags(Qn::deprecated);

        qnStorageMan->addStorage(newStorage);
    }

    

    qnStorageMan->doMigrateCSVCatalog();

    QnRecordingManager::initStaticInstance( new QnRecordingManager() );
    QnRecordingManager::instance()->start();
    qnResPool->addResource(m_mediaServer);

    QString moduleName = qApp->applicationName();
    if( moduleName.startsWith( qApp->organizationName() ) )
        moduleName = moduleName.mid( qApp->organizationName().length() ).trimmed();

    QnModuleInformation selfInformation;
    selfInformation.type = moduleName;
    selfInformation.customization = lit( QN_CUSTOMIZATION_NAME );
    selfInformation.version = qnCommon->engineVersion();
    selfInformation.systemInformation = QnSystemInformation::currentSystemInformation();
    selfInformation.systemName = qnCommon->localSystemName();
    selfInformation.port = MSSettings::roSettings()->value( nx_ms_conf::SERVER_PORT, nx_ms_conf::DEFAULT_SERVER_PORT ).toInt();
    //selfInformation.remoteAddresses = ;
    //selfInformation.isLocal = ; //!< true if at least one address from \a remoteHostAddress is a local address
    selfInformation.id = serverGuid();
    selfInformation.sslAllowed = MSSettings::roSettings()->value( nx_ms_conf::ALLOW_SSL_CONNECTIONS, nx_ms_conf::DEFAULT_ALLOW_SSL_CONNECTIONS ).toBool();

    qnCommon->setModuleInformation(selfInformation);

    m_moduleFinder = new QnModuleFinder( false );
    if (cmdLineArguments.devModeKey == lit("razrazraz")) {
        m_moduleFinder->setCompatibilityMode(true);
        ec2ConnectionFactory->setCompatibilityMode(true);
    }
    if (!cmdLineArguments.allowedDiscoveryPeers.isEmpty()) {
        QList<QUuid> allowedPeers;
        foreach (const QString &peer, cmdLineArguments.allowedDiscoveryPeers.split(";")) {
            QUuid peerId(peer);
            if (!peerId.isNull())
                allowedPeers << peerId;
        }
        if (!allowedPeers.isEmpty())
            m_moduleFinder->setAllowedPeers(allowedPeers);
    }

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

    QScopedPointer<QnGlobalModuleFinder> globalModuleFinder(new QnGlobalModuleFinder());
    globalModuleFinder->setConnection(ec2Connection);
    globalModuleFinder->setModuleFinder(m_moduleFinder);

    QScopedPointer<QnRouter> router(new QnRouter());
    router->setConnection(ec2Connection);
    router->setModuleFinder(m_moduleFinder);

    QScopedPointer<QnServerUpdateTool> serverUpdateTool(new QnServerUpdateTool());

    //===========================================================================
    QnResource::initAsyncPoolInstance()->setMaxThreadCount( MSSettings::roSettings()->value(
        nx_ms_conf::RESOURCE_INIT_THREADS_COUNT,
        nx_ms_conf::DEFAULT_RESOURCE_INIT_THREADS_COUNT ).toInt() );

    //============================
    std::unique_ptr<UPNPDeviceSearcher> upnpDeviceSearcher(new UPNPDeviceSearcher());
    std::unique_ptr<QnMdnsListener> mdnsListener(new QnMdnsListener());

    std::unique_ptr<QnAppserverResourceProcessor> serverResourceProcessor( new QnAppserverResourceProcessor(m_mediaServer->getId()) );
    serverResourceProcessor->moveToThread( mserverResourceDiscoveryManager.get() );
    QnResourceDiscoveryManager::instance()->setResourceProcessor(serverResourceProcessor.get());

    std::unique_ptr<QnResourceStatusWatcher> statusWatcher( new QnResourceStatusWatcher());

    //NOTE plugins have higher priority than built-in drivers
    ThirdPartyResourceSearcher thirdPartyResourceSearcher;
    QnResourceDiscoveryManager::instance()->addDeviceServer( &thirdPartyResourceSearcher );

#ifdef ENABLE_DESKTOP_CAMERA
    QnDesktopCameraResourceSearcher desktopCameraResourceSearcher;
    QnResourceDiscoveryManager::instance()->addDeviceServer(&desktopCameraResourceSearcher);
    QnDesktopCameraDeleter autoDeleter;
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
    updateAllowCameraCHangesIfNeed();
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
        NX_LOG(str, cl_logALWAYS);
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
    QnResourceDiscoveryManager::instance()->pleaseStop();
    QnResource::pleaseStopAsyncTasks();
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

    mdnsListener.reset();
    upnpDeviceSearcher.reset();

    connectorThread->quit();
    connectorThread->wait();

    //deleting object from wrong thread, but its no problem, since object's thread has been stopped and no event can be delivered to the object
    delete QnBusinessEventConnector::instance();
    QnBusinessEventConnector::initStaticInstance( NULL );

    QnBusinessRuleProcessor::fini();
    QnEventsDB::fini();

    delete QnMotionHelper::instance();
    QnMotionHelper::initStaticInstance( NULL );


    QnStorageManager::instance()->stopAsyncTasks();

    ptzPool.reset();

    QnAppServerConnectionFactory::setEc2Connection( ec2::AbstractECConnectionPtr() );

    av_lockmgr_register(NULL);

    // First disconnect eventManager from all slots, to not try to reconnect on connection close
    disconnect(QnServerMessageProcessor::instance());

    // This method will set flag on message channel to threat next connection close as normal
    //appServerConnection->disconnectSync();
    MSSettings::runTimeSettings()->setValue("lastRunningTime", 0);

    QnSSLSocket::releaseSSLEngine();
    QnAuthHelper::initStaticInstance(NULL);

    globalSettings.reset();

    delete QnResourcePool::instance();
    QnResourcePool::initStaticInstance( NULL );
}

void QnMain::at_appStarted()
{
    QnCommonMessageProcessor::instance()->init(QnAppServerConnectionFactory::getConnection2()); // start receiving notifications
};

void QnMain::at_runtimeInfoChanged(const QnPeerRuntimeInfo& runtimeInfo)
{
    if (runtimeInfo.uuid != qnCommon->moduleGUID())
        return;
    
    ec2::QnTransaction<ec2::ApiRuntimeData> tran(ec2::ApiCommand::runtimeInfoChanged);
    tran.params = runtimeInfo.data;
    qnTransactionBus->sendTransaction(tran);
}

void QnMain::at_emptyDigestDetected(const QnUserResourcePtr& user, const QString& login, const QString& password)
{
    // fill authenticate digest here for compatibility with version 2.1 and below.
    const ec2::AbstractECConnectionPtr& appServerConnection = QnAppServerConnectionFactory::getConnection2();
    if (user->getDigest().isEmpty() && !m_updateUserRequests.contains(user->getId()))
    {
        user->setPassword(password);
        user->setName(login);
        m_updateUserRequests << user->getId();
        appServerConnection->getUserManager()->save(
            user, this, 
            [this, user]( int /*reqID*/, ec2::ErrorCode errorCode ) 
            {
                if (errorCode == ec2::ErrorCode::ok) {
                    ec2::ApiUserData userData;
                    fromResourceToApi(user, userData);
                    user->setDigest(userData.digest);
                }
                m_updateUserRequests.remove(user->getId());
            } );
    }
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

    void setOverrideVersion(const QnSoftwareVersion &version) {
        m_overrideVersion = version;
    }

protected:
    virtual int executeApplication() override { 
        QScopedPointer<QnPlatformAbstraction> platform(new QnPlatformAbstraction());
        QScopedPointer<QnLongRunnablePool> runnablePool(new QnLongRunnablePool());
        QScopedPointer<QnMediaServerModule> module(new QnMediaServerModule(m_argc, m_argv));
        
        if (!m_overrideVersion.isNull())
            qnCommon->setEngineVersion(m_overrideVersion);

        m_main.reset(new QnMain(m_argc, m_argv));

        int res = application()->exec();
#ifdef Q_OS_WIN
        // stop the service unexpectedly to let windows service management system restart it
        if (restartFlag) {
            HANDLE hProcess = GetCurrentProcess();
            TerminateProcess(hProcess, ERROR_SERVICE_SPECIFIC_ERROR);
        }
#endif
        return res;
    }

    virtual void start() override
    {
        QtSingleCoreApplication *application = this->application();

        updateGuidIfNeeded();

        QUuid guid = serverGuid();
        if (guid.isNull())
        {
            NX_LOG("Can't save guid. Run once as administrator.", cl_logERROR);
            qApp->quit();
            return;
        }

        if (application->isRunning())
        {
            NX_LOG("Server already started", cl_logERROR);
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
    QUuid hardwareIdToUuid(const QByteArray& hardwareId) {
        if (hardwareId.length() != 34)
            return QUuid();

        QString hwid(hardwareId);
        QString uuidForm = QString("%1-%2-%3-%4-%5").arg(hwid.mid(2, 8)).arg(hwid.mid(10, 4)).arg(hwid.mid(14, 4)).arg(hwid.mid(18, 4)).arg(hwid.mid(22, 12));
        return QUuid(uuidForm);
    }

    QString hardwareIdAsGuid() {
        const QString& hwID = hardwareIdToUuid(LLUtil::getHardwareId(LLUtil::LATEST_HWID_VERSION, false)).toString();
        std::cout << "Got hwID \"" << hwID.toStdString() << "\"" << std::endl;
        return hwID;
    }

    void updateGuidIfNeeded() {
        QString guidIsHWID = MSSettings::roSettings()->value(GUID_IS_HWID).toString();
        QString serverGuid = MSSettings::roSettings()->value(SERVER_GUID).toString();
        QString serverGuid2 = MSSettings::roSettings()->value(SERVER_GUID2).toString();
        QString pendingSwitchToClusterMode = MSSettings::roSettings()->value(PENDING_SWITCH_TO_CLUSTER_MODE).toString();

        QString hwidGuid = hardwareIdAsGuid();

        if (guidIsHWID == YES) {
            MSSettings::roSettings()->setValue(SERVER_GUID, hwidGuid);
            MSSettings::roSettings()->remove(SERVER_GUID2);
        } else if (guidIsHWID == NO) {
            if (serverGuid.isEmpty()) {
                // serverGuid remove from settings manually?
                MSSettings::roSettings()->setValue(SERVER_GUID, hwidGuid);
                MSSettings::roSettings()->setValue(GUID_IS_HWID, YES);
            }

            MSSettings::roSettings()->remove(SERVER_GUID2);
        } else if (guidIsHWID.isEmpty()) {
            if (!serverGuid2.isEmpty()) {
                MSSettings::roSettings()->setValue(SERVER_GUID, serverGuid2);
                MSSettings::roSettings()->setValue(GUID_IS_HWID, NO);
                MSSettings::roSettings()->remove(SERVER_GUID2);
            } else {
                // Don't reset serverGuid if we're in pending switch to cluster mode state.
                // As it's stored in the remote database.
                if (pendingSwitchToClusterMode == YES)
                    return;

                MSSettings::roSettings()->setValue(SERVER_GUID, hwidGuid);
                MSSettings::roSettings()->setValue(GUID_IS_HWID, YES);

                if (!serverGuid.isEmpty()) {
                    MSSettings::roSettings()->setValue(OBSOLETE_SERVER_GUID, serverGuid);
                }
            }
        }

        QUuid obsoleteGuid = QUuid(MSSettings::roSettings()->value(OBSOLETE_SERVER_GUID).toString());
        if (!obsoleteGuid.isNull()) {
            qnCommon->setObsoleteServerGuid(obsoleteGuid);
        }
    }

private:
    int m_argc;
    char **m_argv;
    QScopedPointer<QnMain> m_main;
    QnSoftwareVersion m_overrideVersion;
};

void stopServer(int signal)
{
    restartFlag = false;
    if (serviceMainInstance) {
        qWarning() << "got signal" << signal << "stop server!";
        serviceMainInstance->stopAsync();
    }
}

void restartServer()
{
    restartFlag = true;
    if (serviceMainInstance) {
        qWarning() << "restart requested!";
        serviceMainInstance->stopAsync();
    }
}

static void printVersion();


int main(int argc, char* argv[])
{
#if 0
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
    _tzset();
#endif

    //parsing command-line arguments
    QString configFilePath;
    QString rwConfigFilePath;
    bool showVersion = false;
    bool showHelp = false;
    QString engineVersion;

    QnCommandLineParser commandLineParser;
    commandLineParser.addParameter(&cmdLineArguments.logLevel, "--log-level", NULL, 
        "Supported values: none (no logging), ALWAYS, ERROR, WARNING, INFO, DEBUG, DEBUG2. Default value is "
#ifdef _DEBUG
            "DEBUG"
#else
            "INFO"
#endif
            );
    commandLineParser.addParameter(&cmdLineArguments.msgLogLevel, "--msg-log-level", NULL,
        "Log value for msg_log.log. Supported values same as above. Default is none (no logging)", "none");
    commandLineParser.addParameter(&cmdLineArguments.rebuildArchive, "--rebuild", NULL, 
        lit("Rebuild archive index. Supported values: all (high & low quality), hq (only high), lq (only low)"), "all");
    commandLineParser.addParameter(&cmdLineArguments.devModeKey, "--dev-mode-key", NULL, QString());
    commandLineParser.addParameter(&cmdLineArguments.allowedDiscoveryPeers, "--allowed-peers", NULL, QString());
    commandLineParser.addParameter(&configFilePath, "--conf-file", NULL, 
        "Path to config file. By default "+MSSettings::defaultROSettingsFilePath());
    commandLineParser.addParameter(&rwConfigFilePath, "--runtime-conf-file", NULL, 
        "Path to config file which is used to save some. By default "+MSSettings::defaultRunTimeSettingsFilePath() );
    commandLineParser.addParameter(&showVersion, "--version", NULL,
        lit("Print version info and exit"), true);
    commandLineParser.addParameter(&showHelp, "--help", NULL,
        lit("This help message"), true);
    commandLineParser.addParameter(&engineVersion, "--override-version", NULL,
        lit("Force the other engine version"), QString());
    commandLineParser.parse(argc, argv, stderr, QnCommandLineParser::PreserveParsedParameters);

    if( showVersion )
    {
        printVersion();
        return 0;
    }

    if( showHelp )
    {
        QTextStream stream(stdout);
        commandLineParser.print(stream);
        return 0;
    }

    if( !configFilePath.isEmpty() )
        MSSettings::initializeROSettingsFromConfFile( configFilePath );
    if( !rwConfigFilePath.isEmpty() )
        MSSettings::initializeRunTimeSettingsFromConfFile( rwConfigFilePath );


    QnVideoService service( argc, argv );

    if (!engineVersion.isEmpty()) {
        QnSoftwareVersion version(engineVersion);
        if (!version.isNull()) {
            qWarning() << "Starting with overriden version: " << version.toString();
            service.setOverrideVersion(version);
        }
    }

    int res = service.exec();
    if (restartFlag && res == 0)
        return 1;
    return 0;
    //return res;
}

static void printVersion()
{
    std::cout << "  " << QN_APPLICATION_NAME << " v." << QCoreApplication::applicationVersion().toUtf8().data() << std::endl;
}
