#include "media_server_process.h"

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <functional>
#include <signal.h>
#ifdef __linux__
#include <signal.h>
#endif

#include <qtsinglecoreapplication.h>
#include <qtservice.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <QtCore/QUrl>
#include <utils/common/uuid.h>
#include <utils/common/ldap.h>
#include <QtCore/QThreadPool>

#include <QtNetwork/QUdpSocket>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QHostInfo>
#include <QtNetwork/QNetworkInterface>

#include <api/app_server_connection.h>
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
#include <core/resource_management/server_additional_addresses_dictionary.h>
#include <core/resource/camera_user_attribute_pool.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_user_attributes.h>
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
#include <nx_ec/ec_proto_version.h>

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
#include <plugins/storage/dts/vmax480/vmax480_resource_searcher.h>
#include <plugins/storage/file_storage/file_storage_resource.h>
#include <plugins/storage/third_party_storage_resource/third_party_storage_resource.h>

#include <recorder/file_deletor.h>
#include <recorder/recording_manager.h>
#include <recorder/storage_manager.h>

#include <rest/handlers/acti_event_rest_handler.h>
#include <rest/handlers/business_event_log_rest_handler.h>
#include <rest/handlers/get_system_name_rest_handler.h>
#include <rest/handlers/camera_diagnostics_rest_handler.h>
#include <rest/handlers/camera_settings_rest_handler.h>
#include <rest/handlers/camera_bookmarks_rest_handler.h>
#include <rest/handlers/crash_server_handler.h>
#include <rest/handlers/external_business_event_rest_handler.h>
#include <rest/handlers/favicon_rest_handler.h>
#include <rest/handlers/image_rest_handler.h>
#include <rest/handlers/log_rest_handler.h>
#include <rest/handlers/manual_camera_addition_rest_handler.h>
#include <rest/handlers/ping_rest_handler.h>
#include <rest/handlers/audit_log_rest_handler.h>
#include <rest/handlers/recording_stats_rest_handler.h>
#include <rest/handlers/ping_system_rest_handler.h>
#include <rest/handlers/ptz_rest_handler.h>
#include <rest/handlers/can_accept_cameras_rest_handler.h>
#include <rest/handlers/rebuild_archive_rest_handler.h>
#include <rest/handlers/recorded_chunks_rest_handler.h>
#include <rest/handlers/statistics_rest_handler.h>
#include <rest/handlers/storage_space_rest_handler.h>
#include <rest/handlers/storage_status_rest_handler.h>
#include <rest/handlers/time_rest_handler.h>
#include <rest/handlers/activate_license_rest_handler.h>
#include <rest/handlers/test_email_rest_handler.h>
#include <rest/handlers/test_ldap_rest_handler.h>
#include <rest/handlers/update_rest_handler.h>
#include <rest/handlers/restart_rest_handler.h>
#include <rest/handlers/module_information_rest_handler.h>
#include <rest/handlers/iflist_rest_handler.h>
#include <rest/handlers/json_aggregator_rest_handler.h>
#include <rest/handlers/ifconfig_rest_handler.h>
#include <rest/handlers/settime_rest_handler.h>
#include <rest/handlers/configure_rest_handler.h>
#include <rest/handlers/merge_systems_rest_handler.h>
#include <rest/handlers/current_user_rest_handler.h>
#include <rest/handlers/backup_db_rest_handler.h>
#include <rest/handlers/discovered_peers_rest_handler.h>
#include <rest/server/rest_connection_processor.h>

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
#include <utils/network/router.h>
#include <utils/common/ssl_gen_cert.h>

#include <media_server/mserver_status_watcher.h>
#include <media_server/server_message_processor.h>
#include <media_server/settings.h>
#include <media_server/serverutil.h>
#include <media_server/server_update_tool.h>
#include <media_server/server_connector.h>
#include <utils/common/app_info.h>
#include <transcoding/ffmpeg_video_transcoder.h>

#ifdef _WIN32
#include "common/systemexcept_win32.h"
#endif

#ifdef __linux__
#include "common/systemexcept_linux.h"
#endif

#include "platform/hardware_information.h"
#include "platform/platform_abstraction.h"
#include "core/ptz/server_ptz_controller_pool.h"
#include "plugins/resource/acti/acti_resource.h"
#include "transaction/transaction_message_bus.h"
#include "common/common_module.h"
#include "proxy/proxy_receiver_connection_processor.h"
#include "proxy/proxy_connection.h"
#include "compatibility.h"
#include "media_server/file_connection_processor.h"
#include "streaming/hls/hls_session_pool.h"
#include "streaming/hls/hls_server.h"
#include "streaming/streaming_chunk_transcoder.h"
#include "llutil/hardware_id.h"
#include "api/runtime_info_manager.h"
#include "rest/handlers/old_client_connect_rest_handler.h"
#include "nx_ec/data/api_conversion_functions.h"
#include "media_server/resource_status_watcher.h"
#include "nx_ec/dummy_handler.h"
#include "ec2_statictics_reporter.h"
#include "server/host_system_password_synchronizer.h"

#include "version.h"
#include "core/resource_management/resource_properties.h"
#include "core/resource_management/status_dictionary.h"
#include "network/universal_request_processor.h"
#include "utils/network/nettools.h"
#include "http/iomonitor_tcp_server.h"
#include "ldap/ldap_manager.h"
#include "rest/handlers/multiserver_chunks_rest_handler.h"
#include "rest/handlers/merge_ldap_users_rest_handler.h"
#include "audit/mserver_audit_manager.h"
#include "utils/common/waiting_for_qthread_to_empty_event_queue.h"


#ifdef __arm__
#include "nx1/info.h"
#endif

// This constant is used while checking for compatibility.
// Do not change it until you know what you're doing.
static const char COMPONENT_NAME[] = "MediaServer";

static QString SERVICE_NAME = lit("%1 Server").arg(QnAppInfo::organizationName());
static const quint64 DEFAULT_MAX_LOG_FILE_SIZE = 10*1024*1024;
static const quint64 DEFAULT_LOG_ARCHIVE_SIZE = 25;
//static const quint64 DEFAULT_MSG_LOG_ARCHIVE_SIZE = 5;
static const unsigned int APP_SERVER_REQUEST_ERROR_TIMEOUT_MS = 5500;
static const QString REMOVE_DB_PARAM_NAME(lit("removeDbOnStartup"));
static const QByteArray SYSTEM_IDENTITY_TIME("sysIdTime");
static const QByteArray AUTH_KEY("authKey");
static const QByteArray APPSERVER_PASSWORD("appserverPassword");
static const QByteArray LOW_PRIORITY_ADMIN_PASSWORD("lowPriorityPassword");
static const QByteArray SYSTEM_NAME_KEY("systemName");
static const QByteArray AUTO_GEN_SYSTEM_NAME("__auto__");

class MediaServerProcess;
static MediaServerProcess* serviceMainInstance = 0;
void stopServer(int signal);
bool restartFlag = false;
void restartServer(int restartTimeout);

namespace {
    const QString YES = lit("yes");
    const QString NO = lit("no");
    const QString GUID_IS_HWID = lit("guidIsHWID");
    const QString SERVER_GUID = lit("serverGuid");
    const QString SERVER_GUID2 = lit("serverGuid2");
    const QString OBSOLETE_SERVER_GUID = lit("obsoleteServerGuid");
    const QString PENDING_SWITCH_TO_CLUSTER_MODE = lit("pendingSwitchToClusterMode");
    const QString ADMIN_PSWD_HASH = lit("adminMd5Hash");
    const QString ADMIN_PSWD_DIGEST = lit("adminMd5Digest");
};

//#include "device_plugins/arecontvision/devices/av_device_server.h"

//#define TEST_RTSP_SERVER

//static const int PROXY_POOL_SIZE = 8;
#ifdef EDGE_SERVER
static const int DEFAULT_MAX_CAMERAS = 1;
#else
static const int DEFAULT_MAX_CAMERAS = 128;
#endif

//TODO #ak have to do something with settings
class CmdLineArguments
{
public:
    QString logLevel;
    //!Log level of http requests log
    QString msgLogLevel;
    QString ec2TranLogLevel;
    QString rebuildArchive;
    QString devModeKey;
    QString allowedDiscoveryPeers;

    CmdLineArguments()
    :
        logLevel(
#ifdef _DEBUG
            lit("DEBUG")
#else
            lit("INFO")
#endif
        )
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
    for (const QHostAddress &address: info.addresses())
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
}

QnStorageResourcePtr createStorage(const QnUuid& serverId, const QString& path)
{
    QnStorageResourcePtr storage(QnStoragePluginFactory::instance()->createStorage("ufile"));
    storage->setName("Initial");
    storage->setParentId(serverId);
    storage->setUrl(path);
    storage->setSpaceLimit( MSSettings::roSettings()->value(nx_ms_conf::MIN_STORAGE_SPACE, nx_ms_conf::DEFAULT_MIN_STORAGE_SPACE).toLongLong() );
    storage->setUsedForWriting(storage->getCapabilities() & QnAbstractStorageResource::cap::WriteFile);

    QnResourceTypePtr resType = qnResTypePool->getResourceTypeByName("Storage");
    Q_ASSERT(resType);
    if (resType)
        storage->setTypeId(resType->getId());
    storage->setParentId(serverGuid());

    const auto storagePath = QnStorageResource::toNativeDirPath(storage->getPath());
    const auto partitions = qnPlatform->monitor()->totalPartitionSpaceInfo();
    const auto it = std::find_if(partitions.begin(), partitions.end(),
                                 [&](const QnPlatformMonitor::PartitionSpace& part)
        { return storagePath.startsWith(QnStorageResource::toNativeDirPath(part.path)); });

    const auto storageType = (it != partitions.end()) ? it->type : QnPlatformMonitor::NetworkPartition;
    if (storage->getStorageType().isEmpty())
        storage->setStorageType(QnLexical::serialized(storageType));

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

    for (const QFileInfo& drive: QDir::drives()) {
        if (!drive.isWritable())
            continue;


        QString path = drive.absolutePath();

        if (GetDriveType(path.toStdWString().c_str()) != DRIVE_FIXED)
            continue;

        folderPaths.append(QDir::toNativeSeparators(path) + QnAppInfo::mediaFolderName());
        /*
        int freeSpace = freeGB(path);

        if (maxFreeSpaceDrive.isEmpty() || freeSpace > maxFreeSpace) {
            maxFreeSpaceDrive = path;
            maxFreeSpace = freeSpace;
        }

        if (freeSpace >= 100) {
            NX_LOG(QString("Drive %1 has more than 100GB free space. Using it for storage.").arg(path), cl_logINFO);
            folderPaths.append(path + QnAppInfo::mediaFolderName());
        }
        */
    }
    /*
    if (folderPaths.isEmpty()) {
        NX_LOG(QString("There are no drives with more than 100GB free space. Using drive %1 as it has the most free space: %2 GB").arg(maxFreeSpaceDrive).arg(maxFreeSpace), cl_logINFO);
        folderPaths.append(maxFreeSpaceDrive + QnAppInfo::mediaFolderName());
    }
    */
#endif

#ifdef Q_OS_LINUX
    QList<QnPlatformMonitor::PartitionSpace> partitions =
        qnPlatform->monitor()->QnPlatformMonitor::totalPartitionSpaceInfo(
            QnPlatformMonitor::LocalDiskPartition);

    //always adding storage in data dir
    const QString& dataDirStorage = QDir::cleanPath(getDataDirectory() + "/data");
    for(int i = 0; i < partitions.size(); ++i)
    {
        if( dataDirStorage.startsWith(partitions[i].path) )
            folderPaths.append( dataDirStorage );
        else
            folderPaths.append( QDir::cleanPath( QDir::toNativeSeparators(partitions[i].path) + lit("/") + QnAppInfo::mediaFolderName() ) );
    }
#endif

    return folderPaths;
}

QnStorageResourceList createStorages(const QnMediaServerResourcePtr mServer)
{
    QnStorageResourceList storages;
    //bool isBigStorageExist = false;
    qint64 bigStorageThreshold = 0;
    for(const QString& folderPath: listRecordFolders())
    {
        if (!mServer->getStorageByUrl(folderPath).isNull())
            continue;
        QnStorageResourcePtr storage = createStorage(mServer->getId(), folderPath);
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

QnStorageResourceList updateStorages(QnMediaServerResourcePtr mServer)
{
    const auto partitions = qnPlatform->monitor()->totalPartitionSpaceInfo();

    QMap<QnUuid, QnStorageResourcePtr> result;
    // I've switched all patches to native separator to fix network patches like \\computer\share
    for(const QnStorageResourcePtr& abstractStorage: mServer->getStorages())
    {
        QnStorageResourcePtr storage = abstractStorage.dynamicCast<QnStorageResource>();
        if (!storage)
            continue;
        bool modified = false;
        if (!storage->getUrl().contains("://")) {
            QString updatedURL = QDir::toNativeSeparators(storage->getUrl());
            if (updatedURL.endsWith(QDir::separator()))
                updatedURL.chop(1);
            if (storage->getUrl() != updatedURL) {
                storage->setUrl(updatedURL);
                modified = true;
            }
        }

        QString storageType = storage->getStorageType();
        if (storageType.isEmpty())
        {
            if (storage->getUrl().contains(lit("://")))
                storageType = QUrl(storage->getUrl()).scheme();
            if (storageType.isEmpty())
            {
                storageType = QnLexical::serialized(QnPlatformMonitor::LocalDiskPartition);
                const auto storagePath = QnStorageResource::toNativeDirPath(storage->getPath());
                const auto it = std::find_if(partitions.begin(), partitions.end(),
                    [&](const QnPlatformMonitor::PartitionSpace& partition)
                { return storagePath.startsWith(QnStorageResource::toNativeDirPath(partition.path)); });
                if (it != partitions.end())
                    storageType = QnLexical::serialized(it->type);
            }
            storage->setStorageType(storageType);
            modified = true;
        }
        if (modified)
            result.insert(storage->getId(), storage);
    }

    return result.values();
}

void setServerNameAndUrls(QnMediaServerResourcePtr server, const QString& myAddress, int port)
{
    if (server->getName().isEmpty())
        server->setName(QString("Server ") + myAddress);

    server->setUrl(QString("rtsp://%1:%2").arg(myAddress).arg(port));
    server->setApiUrl(QString("http://%1:%2").arg(myAddress).arg(port));
}

QnMediaServerResourcePtr MediaServerProcess::findServer(ec2::AbstractECConnectionPtr ec2Connection)
{
    QnMediaServerResourceList servers;

    while (servers.isEmpty() && !needToStop())
    {
        ec2::ErrorCode rez = ec2Connection->getMediaServerManager()->getServersSync(QnUuid(), &servers);
        if( rez == ec2::ErrorCode::ok )
            break;

        qDebug() << "findServer(): Call to getServers failed. Reason: " << ec2::toString(rez);
        QnSleep::msleep(1000);
    }

    for(const QnMediaServerResourcePtr& server: servers)
    {
        if (server->getId() == serverGuid())
            return server;
    }

    return QnMediaServerResourcePtr();
}

QnMediaServerResourcePtr registerServer(ec2::AbstractECConnectionPtr ec2Connection, QnMediaServerResourcePtr serverPtr, bool isNewServerInstance)
{
    QnMediaServerResourcePtr savedServer;
    serverPtr->setStatus(Qn::Online, true);

    ec2::ErrorCode rez = ec2Connection->getMediaServerManager()->saveSync(serverPtr, &savedServer);
    if (rez != ec2::ErrorCode::ok)
    {
        qWarning() << "registerServer(): Call to registerServer failed. Reason: " << ec2::toString(rez);
        return QnMediaServerResourcePtr();
    }

    if (!isNewServerInstance)
        return savedServer;

    // insert server user attributes if defined
    QString dir = MSSettings::roSettings()->value("staticDataDir", getDataDirectory()).toString();
    QFile f(closeDirPath(dir) + lit("server_settings.json"));
    if (!f.open(QFile::ReadOnly))
        return savedServer;
    QByteArray data = f.readAll();
    ec2::ApiMediaServerUserAttributesData userAttrsData;
    if (!QJson::deserialize(data, &userAttrsData))
        return savedServer;
    userAttrsData.serverID = savedServer->getId();
    auto defaultServerAttrs = QnMediaServerUserAttributesPtr(new QnMediaServerUserAttributes());
    fromApiToResource(userAttrsData, defaultServerAttrs);
    rez =  QnAppServerConnectionFactory::getConnection2()->getMediaServerManager()->saveUserAttributesSync(QnMediaServerUserAttributesList() << defaultServerAttrs);
    if (rez != ec2::ErrorCode::ok)
    {
        qWarning() << "registerServer(): Call to registerServer failed. Reason: " << ec2::toString(rez);
        return QnMediaServerResourcePtr();
    }

    return savedServer;
}

void MediaServerProcess::saveStorages(ec2::AbstractECConnectionPtr ec2Connection, const QnStorageResourceList& storages)
{
    ec2::ErrorCode rez;
    while((rez = ec2Connection->getMediaServerManager()->saveStoragesSync(storages)) != ec2::ErrorCode::ok && !needToStop())
    {
        qWarning() << "updateStorages(): Call to change server's storages failed. Reason: " << ec2::toString(rez);
        QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
    }
}

static const int SYSTEM_USAGE_DUMP_TIMEOUT = 7*60*1000;

void MediaServerProcess::dumpSystemUsageStats()
{
    qnPlatform->monitor()->totalCpuUsage();
    qnPlatform->monitor()->totalRamUsage();
    qnPlatform->monitor()->totalHddLoad();

    // TODO: #mu
    //  - Add some more fields that might be interesting
    //  - Make and use JSON serializable struct rather than just a string
    QStringList networkIfList;
    for (const auto& iface : qnPlatform->monitor()->totalNetworkLoad())
        if (iface.type != QnPlatformMonitor::LoopbackInterface)
            networkIfList.push_back(lit("%1: %2 bps").arg(iface.interfaceName)
                                                     .arg(iface.bytesPerSecMax));

    const auto networkIfInfo = networkIfList.join(lit(", "));
    if (m_mediaServer->setProperty(Qn::NETWORK_INTERFACES, networkIfInfo))
        propertyDictionary->saveParams(m_mediaServer->getId());

    QMutexLocker lk( &m_mutex );
    if( m_dumpSystemResourceUsageTaskID == 0 )  //monitoring cancelled
        return;
    m_dumpSystemResourceUsageTaskID = TimerManager::instance()->addTimer(
        std::bind( &MediaServerProcess::dumpSystemUsageStats, this ),
        SYSTEM_USAGE_DUMP_TIMEOUT );
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
void initLog(const QString& _logLevel)
{
    QString logLevel = _logLevel;
    const QString& configLogLevel = MSSettings::roSettings()->value("logLevel").toString();
    if (!configLogLevel.isEmpty())
        logLevel = configLogLevel;

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
            QnLog::logLevelFromString(logLevel)))
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
    signal(SIGTERM, stopServer);

//    av_log_set_callback(decoderLogCallback);



    const QString& dataLocation = getDataDirectory();
    const QString& logDir = MSSettings::roSettings()->value( "logDir", dataLocation + QLatin1String("/log/") ).toString();

    QDir::setCurrent(qApp->applicationDirPath());

    if (cmdLineArguments.rebuildArchive.isEmpty()) {
        cmdLineArguments.rebuildArchive = MSSettings::runTimeSettings()->value("rebuild").toString();
    }
    MSSettings::runTimeSettings()->remove("rebuild");

    initLog(cmdLineArguments.logLevel);

    if( cmdLineArguments.msgLogLevel.isEmpty() )
        cmdLineArguments.msgLogLevel = MSSettings::roSettings()->value(
            nx_ms_conf::HTTP_MSG_LOG_LEVEL,
            nx_ms_conf::DEFAULT_HTTP_MSG_LOG_LEVEL ).toString();

    if( cmdLineArguments.msgLogLevel != lit("none") )
        QnLog::instance(QnLog::HTTP_LOG_INDEX)->create(
            logDir + QLatin1String("/http_log"),
            MSSettings::roSettings()->value( "maxLogFileSize", DEFAULT_MAX_LOG_FILE_SIZE ).toULongLong(),
            MSSettings::roSettings()->value( "logArchiveSize", DEFAULT_LOG_ARCHIVE_SIZE ).toULongLong(),
            QnLog::logLevelFromString(cmdLineArguments.msgLogLevel) );

    //preparing transaction log
    if( cmdLineArguments.ec2TranLogLevel.isEmpty() )
        cmdLineArguments.ec2TranLogLevel = MSSettings::roSettings()->value(
            nx_ms_conf::EC2_TRAN_LOG_LEVEL,
            nx_ms_conf::DEFAULT_EC2_TRAN_LOG_LEVEL ).toString();

    if( cmdLineArguments.ec2TranLogLevel != lit("none") )
    {
        QnLog::instance(QnLog::EC2_TRAN_LOG)->create(
            logDir + QLatin1String("/ec2_tran"),
            MSSettings::roSettings()->value( "maxLogFileSize", DEFAULT_MAX_LOG_FILE_SIZE ).toULongLong(),
            MSSettings::roSettings()->value( "logArchiveSize", DEFAULT_LOG_ARCHIVE_SIZE ).toULongLong(),
            QnLog::logLevelFromString(cmdLineArguments.ec2TranLogLevel) );
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("================================================================================="), cl_logALWAYS);
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("================================================================================="), cl_logALWAYS);
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("================================================================================="), cl_logALWAYS);
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("%1 started").arg(qApp->applicationName()), cl_logALWAYS );
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("Software version: %1").arg(QCoreApplication::applicationVersion()), cl_logALWAYS);
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("Software revision: %1").arg(QnAppInfo::applicationRevision()), cl_logALWAYS);
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("binary path: %1").arg(QFile::decodeName(argv[0])), cl_logALWAYS);
    }

    NX_LOG(lit("%1 started").arg(qApp->applicationName()), cl_logALWAYS);
    NX_LOG(lit("Software version: %1").arg(QCoreApplication::applicationVersion()), cl_logALWAYS);
    NX_LOG(lit("Software revision: %1").arg(QnAppInfo::applicationRevision()), cl_logALWAYS);
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

void encodeAndStoreAuthKey(const QByteArray& authKey)
{
    QByteArray prefix("SK_");
    QByteArray authKeyBin = QnUuid(authKey).toRfc4122();
    QByteArray authKeyEncoded = QnAuthHelper::symmetricalEncode(authKeyBin).toHex();
    MSSettings::roSettings()->setValue(AUTH_KEY, prefix + authKeyEncoded); // encode and update in settings
}

void initAppServerConnection(QSettings &settings)
{
    // migrate appserverPort settings from version 2.2 if exist
    if (!MSSettings::roSettings()->value("appserverPort").isNull())
    {
        MSSettings::roSettings()->setValue("port", MSSettings::roSettings()->value("appserverPort"));
        MSSettings::roSettings()->remove("appserverPort");
    }

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
        int port = settings.value("port", DEFAULT_APPSERVER_PORT).toInt();
        appServerUrl.setHost(host);
        appServerUrl.setPort(port);
    }
    if (appServerUrl.scheme() == "file")
    {
        QString staticDBPath = settings.value("staticDataDir").toString();
        if (!staticDBPath.isEmpty()) {
            params.addQueryItem("staticdb_path", staticDBPath);
        }
        if (MSSettings::roSettings()->value(REMOVE_DB_PARAM_NAME).toBool())
            params.addQueryItem("cleanupDb", QString());
    }

    // TODO: Actually appserverPassword is always empty. Remove?
    QString userName = settings.value("appserverLogin", QLatin1String("admin")).toString();
    QString password = settings.value(APPSERVER_PASSWORD, QLatin1String("")).toString();
    QByteArray authKey = settings.value(AUTH_KEY).toByteArray();
    QString appserverHostString = settings.value("appserverHost").toString();
    if (!authKey.isEmpty() && !isLocalAppServer(appserverHostString))
    {
        // convert from v2.2 format and encode value
        QByteArray prefix("SK_");
        if (!authKey.startsWith(prefix))
            encodeAndStoreAuthKey(authKey);
        else
            authKey = decodeAuthKey(authKey);

        userName = serverGuid().toString();
        password = authKey;
    }

    appServerUrl.setUserName(userName);
    appServerUrl.setPassword(password);
    appServerUrl.setQuery(params);

    QUrl urlNoPassword(appServerUrl);
    urlNoPassword.setPassword("");
    NX_LOG(lit("Connect to server %1").arg(urlNoPassword.toString()), cl_logINFO);
    QnAppServerConnectionFactory::setUrl(appServerUrl);
    QnAppServerConnectionFactory::setDefaultFactory(QnResourceDiscoveryManager::instance());
}


MediaServerProcess::MediaServerProcess(int argc, char* argv[])
:
    m_argc(argc),
    m_argv(argv),
    m_startMessageSent(false),
    m_firstRunningTime(0),
    m_moduleFinder(0),
    m_universalTcpListener(0),
    m_dumpSystemResourceUsageTaskID(0),
    m_stopping(false)
{
    serviceMainInstance = this;
}

MediaServerProcess::~MediaServerProcess()
{
    quit();
    stop();

    if( defaultMsgHandler )
        qInstallMessageHandler( defaultMsgHandler );
}

bool MediaServerProcess::isStopping() const
{
    QMutexLocker lock(&m_stopMutex);
    return m_stopping;
}

void MediaServerProcess::at_databaseDumped()
{
    if (isStopping())
        return;

    saveAdminPswdHash();
    restartServer(500);
}

void MediaServerProcess::at_systemIdentityTimeChanged(qint64 value, const QnUuid& sender)
{
    if (isStopping())
        return;

    MSSettings::roSettings()->setValue(SYSTEM_IDENTITY_TIME, value);
    if (sender != qnCommon->moduleGUID()) {
        MSSettings::roSettings()->setValue(REMOVE_DB_PARAM_NAME, "1");
        saveAdminPswdHash();
        restartServer(0);
    }
}

void MediaServerProcess::stopSync()
{
    qWarning()<<"Stopping server";
    NX_LOG( lit("Stopping server"), cl_logALWAYS );
    if (serviceMainInstance) {
        {
            QMutexLocker lock(&m_stopMutex);
            m_stopping = true;
        }
        serviceMainInstance->pleaseStop();
        serviceMainInstance->exit();
        serviceMainInstance->wait();
        serviceMainInstance = 0;
    }
    qApp->quit();
}

void MediaServerProcess::stopAsync()
{
    QTimer::singleShot(0, this, SLOT(stopSync()));
}


void MediaServerProcess::stopObjects()
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
}

void MediaServerProcess::updateDisabledVendorsIfNeeded()
{
    static const QString DV_PROPERTY = QLatin1String("disabledVendors");

    QString disabledVendors = MSSettings::roSettings()->value(DV_PROPERTY).toString();
    QnUserResourcePtr admin = qnResPool->getAdministrator();
    if (!admin)
        return;

    if (!disabledVendors.isNull()) {
        QnGlobalSettings* settings = QnGlobalSettings::instance();
        settings->setDisabledVendors(disabledVendors);
        MSSettings::roSettings()->remove(DV_PROPERTY);
    }
}

void MediaServerProcess::updateAllowCameraCHangesIfNeed()
{
    static const QString DV_PROPERTY = QLatin1String("cameraSettingsOptimization");

    QString allowCameraChanges = MSSettings::roSettings()->value(DV_PROPERTY).toString();
    if (!allowCameraChanges.isEmpty())
    {
        QnGlobalSettings *settings = QnGlobalSettings::instance();
        settings->setCameraSettingsOptimizationEnabled(allowCameraChanges.toLower() == lit("yes") || allowCameraChanges.toLower() == lit("true") || allowCameraChanges == lit("1"));
        MSSettings::roSettings()->setValue(DV_PROPERTY, "");
    }
}

void MediaServerProcess::loadResourcesFromECS(QnCommonMessageProcessor* messageProcessor)
{
    ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();

    ec2::ErrorCode rez;

    {
        //reading servers list
        QnMediaServerResourceList mediaServerList;
        while( ec2Connection->getMediaServerManager()->getServersSync(QnUuid(), &mediaServerList) != ec2::ErrorCode::ok )
        {
            NX_LOG( lit("QnMain::run(). Can't get servers."), cl_logERROR );
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }

        ec2::ApiDiscoveryDataList discoveryDataList;
        while( ec2Connection->getDiscoveryManager()->getDiscoveryDataSync(&discoveryDataList) != ec2::ErrorCode::ok )
        {
            NX_LOG( lit("QnMain::run(). Can't get discovery data."), cl_logERROR );
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }

        QMultiHash<QnUuid, QUrl> additionalAddressesById;
        QMultiHash<QnUuid, QUrl> ignoredAddressesById;
        for (const ec2::ApiDiscoveryData &data: discoveryDataList) {
            additionalAddressesById.insert(data.id, data.url);
            if (data.ignore)
                ignoredAddressesById.insert(data.id, data.url);
        }

        for(const QnMediaServerResourcePtr &mediaServer: mediaServerList) {
            QList<QHostAddress> addresses = mediaServer->getNetAddrList();
            QList<QUrl> additionalAddresses = additionalAddressesById.values(mediaServer->getId());
            for (auto it = additionalAddresses.begin(); it != additionalAddresses.end(); /* no inc */) {
                if (it->port() == -1 && addresses.contains(QHostAddress(it->host())))
                    it = additionalAddresses.erase(it);
                else
                    ++it;
            }
            mediaServer->setAdditionalUrls(additionalAddresses);
            mediaServer->setIgnoredUrls(ignoredAddressesById.values(mediaServer->getId()));
            messageProcessor->updateResource(mediaServer);
        }

        // read resource status
        ec2::ApiResourceStatusDataList statusList;
        while ((rez = ec2Connection->getResourceManager()->getStatusListSync(QnUuid(), &statusList)) != ec2::ErrorCode::ok)
        {
            NX_LOG( lit("QnMain::run(): Can't get properties dictionary. Reason: %1").arg(ec2::toString(rez)), cl_logDEBUG1 );
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }
        messageProcessor->resetStatusList( statusList );

        //reading server attributes
        QnMediaServerUserAttributesList mediaServerUserAttributesList;
        while ((rez = ec2Connection->getMediaServerManager()->getUserAttributesSync(QnUuid(), &mediaServerUserAttributesList)) != ec2::ErrorCode::ok)
        {
            NX_LOG( lit("QnMain::run(): Can't get server user attributes list. Reason: %1").arg(ec2::toString(rez)), cl_logDEBUG1 );
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }
        messageProcessor->resetServerUserAttributesList( mediaServerUserAttributesList );

        //read server's storages
        QnResourceList storages;
        while ((rez = ec2Connection->getMediaServerManager()->getStoragesSync(QnUuid(), &storages)) != ec2::ErrorCode::ok)
        {
            NX_LOG( lit("QnMain::run(): Can't get storage list. Reason: %1").arg(ec2::toString(rez)), cl_logDEBUG1 );
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }
        for(const QnResourcePtr& storage: storages)
            messageProcessor->updateResource( storage );
    }


    {
        // read camera list
        QnVirtualCameraResourceList cameras;
        while ((rez = ec2Connection->getCameraManager()->getCamerasSync(QnUuid(), &cameras)) != ec2::ErrorCode::ok)
        {
            NX_LOG( lit("QnMain::run(): Can't get cameras. Reason: %1").arg(ec2::toString(rez)), cl_logDEBUG1 );
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }

        //reading camera attributes
        QnCameraUserAttributesList cameraUserAttributesList;
        while ((rez = ec2Connection->getCameraManager()->getUserAttributesSync(QnUuid(), &cameraUserAttributesList)) != ec2::ErrorCode::ok)
        {
            NX_LOG( lit("QnMain::run(): Can't get camera user attributes list. Reason: %1").arg(ec2::toString(rez)), cl_logDEBUG1 );
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }
        messageProcessor->resetCameraUserAttributesList( cameraUserAttributesList );

        // read properties dictionary
        ec2::ApiResourceParamWithRefDataList kvPairs;
        while ((rez = ec2Connection->getResourceManager()->getKvPairsSync(QnUuid(), &kvPairs)) != ec2::ErrorCode::ok)
        {
            NX_LOG( lit("QnMain::run(): Can't get properties dictionary. Reason: %1").arg(ec2::toString(rez)), cl_logDEBUG1 );
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }
        messageProcessor->resetPropertyList( kvPairs );

        QnManualCameraInfoMap manualCameras;
        for(const QnSecurityCamResourcePtr &camera: cameras) {
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

        for(QnCameraHistoryPtr history: cameraHistoryList)
            QnCameraHistoryPool::instance()->addCameraHistory(history);
    }

    {
        //loading users
        QnUserResourceList users;
        while(( rez = ec2Connection->getUserManager()->getUsersSync(QnUuid(), &users))  != ec2::ErrorCode::ok)
        {
            qDebug() << "QnMain::run(): Can't get users. Reason: " << ec2::toString(rez);
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }

        for(const QnUserResourcePtr &user: users)
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

        for(const QnVideoWallResourcePtr &videowall: videowalls)
            messageProcessor->updateResource(videowall);
    }

    {
        //loading layouts
        QnLayoutResourceList layouts;
        while(( rez = ec2Connection->getLayoutManager()->getLayoutsSync(&layouts))  != ec2::ErrorCode::ok)
        {
            qDebug() << "QnMain::run(): Can't get layouts. Reason: " << ec2::toString(rez);
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }

        for(const auto &layout: layouts)
            messageProcessor->updateResource(layout);
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

        for(const QnBusinessEventRulePtr &rule: rules)
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

        for(const QnLicensePtr &license: licenses)
            messageProcessor->on_licenseChanged(license);
    }

    if (m_mediaServer->getPanicMode() == Qn::PM_BusinessEvents) {
        m_mediaServer->setPanicMode(Qn::PM_None);
        propertyDictionary->saveParams(m_mediaServer->getId());
    }
}

void MediaServerProcess::at_updatePublicAddress(const QHostAddress& publicIP)
{
    if (isStopping())
        return;
    m_publicAddress = publicIP;

    QnPeerRuntimeInfo localInfo = QnRuntimeInfoManager::instance()->localInfo();
    localInfo.data.publicIP = m_publicAddress.toString();
    QnRuntimeInfoManager::instance()->updateLocalItem(localInfo);

    at_localInterfacesChanged();

    QnMediaServerResourcePtr server = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
    if (server) {
        Qn::ServerFlags serverFlags = server->getServerFlags();
        if (m_publicAddress.isNull())
            serverFlags &= ~Qn::SF_HasPublicIP;
        else
            serverFlags |= Qn::SF_HasPublicIP;
        if (serverFlags != server->getServerFlags()) {
            server->setServerFlags(serverFlags);
            ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();
            ec2Connection->getMediaServerManager()->save(server, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
        }

        if (server->setProperty(Qn::PUBLIC_IP, m_publicAddress.toString(), QnResource::NO_ALLOW_EMPTY))
            propertyDictionary->saveParams(server->getId());
    }
}

void MediaServerProcess::at_localInterfacesChanged()
{
    if (isStopping())
        return;
    auto intfList = allLocalAddresses();
    if (!m_publicAddress.isNull())
        intfList << m_publicAddress;
    m_mediaServer->setNetAddrList(intfList);

    QString defaultAddress = QUrl(m_mediaServer->getApiUrl()).host();
    int port = m_mediaServer->getPort();
    bool found = false;
    for(const QHostAddress& addr: intfList) {
        if (addr.toString() == defaultAddress) {
            found = true;
            break;
        }
    }
    if (!found) {
        QHostAddress newAddress;
        if (!intfList.isEmpty())
            newAddress = intfList.first();
        setServerNameAndUrls(m_mediaServer, newAddress.toString(), port);
    }

    ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();
    ec2Connection->getMediaServerManager()->save(m_mediaServer, this, &MediaServerProcess::at_serverSaved);
}

void MediaServerProcess::at_serverSaved(int, ec2::ErrorCode err)
{
    if (isStopping())
        return;

    if (err != ec2::ErrorCode::ok)
        qWarning() << "Error saving server.";
}

void MediaServerProcess::at_connectionOpened()
{
    if (isStopping())
        return;
    if (m_firstRunningTime)
        qnBusinessRuleConnector->at_mserverFailure(qnResPool->getResourceById<QnMediaServerResource>(serverGuid()), m_firstRunningTime*1000, QnBusiness::ServerStartedReason, QString());
    if (!m_startMessageSent) {
        qnBusinessRuleConnector->at_mserverStarted(qnResPool->getResourceById<QnMediaServerResource>(serverGuid()), qnSyncTime->currentUSecsSinceEpoch());
        m_startMessageSent = true;
    }
    m_firstRunningTime = 0;
}

void MediaServerProcess::at_serverModuleConflict(const QnModuleInformation &moduleInformation, const SocketAddress &address)
{
    qnBusinessRuleConnector->at_mediaServerConflict(
                qnResPool->getResourceById<QnMediaServerResource>(serverGuid()),
                qnSyncTime->currentUSecsSinceEpoch(),
                moduleInformation,
                QUrl(lit("http://%1").arg(address.toString())));
}

void MediaServerProcess::at_timer()
{
    if (isStopping())
        return;

    //TODO: #2.4 #GDM This timer make two totally different functions. Split it.
    MSSettings::runTimeSettings()->setValue("lastRunningTime", qnSyncTime->currentMSecsSinceEpoch());

    QnResourcePtr mServer = qnResPool->getResourceById(qnCommon->moduleGUID());
    if (!mServer)
        return;

    for(const auto& camera: qnResPool->getAllCameras(mServer, true))
        camera->cleanCameraIssues();
}

void MediaServerProcess::at_storageManager_noStoragesAvailable() {
    if (isStopping())
        return;
    qnBusinessRuleConnector->at_NoStorages(m_mediaServer);
}

void MediaServerProcess::at_storageManager_storageFailure(const QnResourcePtr& storage, QnBusiness::EventReason reason) {
    if (isStopping())
        return;
    qnBusinessRuleConnector->at_storageFailure(m_mediaServer, qnSyncTime->currentUSecsSinceEpoch(), reason, storage);
}

void MediaServerProcess::at_storageManager_rebuildFinished() {
    if (isStopping())
        return;
    qnBusinessRuleConnector->at_archiveRebuildFinished(m_mediaServer);
}

void MediaServerProcess::at_cameraIPConflict(const QHostAddress& host, const QStringList& macAddrList)
{
    if (isStopping())
        return;
    qnBusinessRuleConnector->at_cameraIPConflict(
        m_mediaServer,
        host,
        macAddrList,
        qnSyncTime->currentUSecsSinceEpoch());
}


bool MediaServerProcess::initTcpListener()
{
    m_httpModManager.reset( new nx_http::HttpModManager() );
    m_httpModManager->addUrlRewriteExact( lit( "/crossdomain.xml" ), lit( "/static/crossdomain.xml" ) );
    m_autoRequestForwarder.reset( new QnAutoRequestForwarder() );
    m_httpModManager->addCustomRequestMod( std::bind(
        &QnAutoRequestForwarder::processRequest,
        m_autoRequestForwarder.get(),
        std::placeholders::_1 ) );

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
    QnRestProcessorPool::instance()->registerHandler("api/onEvent", new QnExternalBusinessEventRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/gettime", new QnTimeRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/getCurrentUser", new QnCurrentUserRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/activateLicense", new QnActivateLicenseRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/testEmailSettings", new QnTestEmailSettingsHandler());
    QnRestProcessorPool::instance()->registerHandler("api/testLdapSettings", new QnTestLdapSettingsHandler());
    QnRestProcessorPool::instance()->registerHandler("api/ping", new QnPingRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/auditLog", new QnAuditLogRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/recStats", new QnRecordingStatsRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/checkDiscovery", new QnCanAcceptCameraRestHandler());
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
    QnRestProcessorPool::instance()->registerHandler("api/iflist", new QnIfListRestHandler() );
    QnRestProcessorPool::instance()->registerHandler("api/aggregator", new QnJsonAggregatorRestHandler() );
    QnRestProcessorPool::instance()->registerHandler("api/ifconfig", new QnIfConfigRestHandler() );
    QnRestProcessorPool::instance()->registerHandler("api/settime", new QnSetTimeRestHandler() );
    QnRestProcessorPool::instance()->registerHandler("api/moduleInformationAuthenticated", new QnModuleInformationRestHandler() );
    QnRestProcessorPool::instance()->registerHandler("api/configure", new QnConfigureRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/mergeSystems", new QnMergeSystemsRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/backupDatabase", new QnBackupDbRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/discoveredPeers", new QnDiscoveredPeersRestHandler());
#ifdef QN_ENABLE_BOOKMARKS
    QnRestProcessorPool::instance()->registerHandler("api/cameraBookmarks", new QnCameraBookmarksRestHandler());
#endif
    QnRestProcessorPool::instance()->registerHandler("ec2/recordedTimePeriods", new QnMultiserverChunksRestHandler("ec2/recordedTimePeriods"));
    QnRestProcessorPool::instance()->registerHandler("api/mergeLdapUsers", new QnMergeLdapUsersRestHandler());
#ifdef ENABLE_ACTI
    QnActiResource::setEventPort(rtspPort);
    QnRestProcessorPool::instance()->registerHandler("api/camera_event", new QnActiEventRestHandler());  //used to receive event from acti camera. TODO: remove this from api
#endif
    QnRestProcessorPool::instance()->registerHandler("favicon.ico", new QnFavIconRestHandler());
    QnRestProcessorPool::instance()->registerHandler("api/dev-mode-key", new QnCrashServerHandler());

    m_universalTcpListener = new QnUniversalTcpListener(
        QHostAddress::Any,
        rtspPort,
        QnTcpListener::DEFAULT_MAX_CONNECTIONS,
        MSSettings::roSettings()->value( nx_ms_conf::ALLOW_SSL_CONNECTIONS, nx_ms_conf::DEFAULT_ALLOW_SSL_CONNECTIONS ).toBool() );
    if( !m_universalTcpListener->bindToLocalAddress() )
        return false;
    m_universalTcpListener->setDefaultPage("/static/index.html");
    AuthMethod::Values methods = (AuthMethod::Values)(AuthMethod::cookie | AuthMethod::urlQueryParam | AuthMethod::tempUrlQueryParam);
    QnUniversalRequestProcessor::setUnauthorizedPageBody(QnFileConnectionProcessor::readStaticFile("static/login.html"), methods);
    m_universalTcpListener->addHandler<QnRtspConnectionProcessor>("RTSP", "*");
    m_universalTcpListener->addHandler<QnRestConnectionProcessor>("HTTP", "api");
    m_universalTcpListener->addHandler<QnRestConnectionProcessor>("HTTP", "ec2");
    m_universalTcpListener->addHandler<QnFileConnectionProcessor>("HTTP", "static");
    m_universalTcpListener->addHandler<QnProgressiveDownloadingConsumer>("HTTP", "media");
    m_universalTcpListener->addHandler<QnIOMonitorConnectionProcessor>("HTTP", "api/iomonitor");
    m_universalTcpListener->addHandler<nx_hls::QnHttpLiveStreamingProcessor>("HTTP", "hls");
    //m_universalTcpListener->addHandler<QnDefaultTcpConnectionProcessor>("HTTP", "*");
    //m_universalTcpListener->addHandler<QnProxyConnectionProcessor>("HTTP", "*");

    m_universalTcpListener->addHandler<QnProxyConnectionProcessor>("*", "proxy");
    //m_universalTcpListener->addHandler<QnProxyReceiverConnection>("PROXY", "*");
    m_universalTcpListener->addHandler<QnProxyReceiverConnection>("HTTP", "proxy-reverse");

    if( !MSSettings::roSettings()->value("authenticationEnabled", "true").toBool() )
        m_universalTcpListener->disableAuth();

#ifdef ENABLE_DESKTOP_CAMERA
    m_universalTcpListener->addHandler<QnDesktopCameraRegistrator>("HTTP", "desktop_camera");
#endif   //ENABLE_DESKTOP_CAMERA

    return true;
}

void MediaServerProcess::saveAdminPswdHash()
{
    QnUserResourcePtr admin = qnResPool->getAdministrator();
    if (admin) {
        MSSettings::roSettings()->setValue(ADMIN_PSWD_HASH, admin->getHash());
        MSSettings::roSettings()->setValue(ADMIN_PSWD_DIGEST, admin->getDigest());
    }
}

QHostAddress MediaServerProcess::getPublicAddress()
{
    m_ipDiscovery.reset(new QnPublicIPDiscovery());

    if (MSSettings::roSettings()->value("publicIPEnabled").isNull())
        MSSettings::roSettings()->setValue("publicIPEnabled", 1);

    int publicIPEnabled = MSSettings::roSettings()->value("publicIPEnabled").toInt();
    if (publicIPEnabled == 0)
        return QHostAddress(); // disabled
    else if (publicIPEnabled > 1)
        return QHostAddress(MSSettings::roSettings()->value("staticPublicIP").toString()); // manually added
    m_ipDiscovery->update();
    m_ipDiscovery->waitForFinished();
    return m_ipDiscovery->publicIP();
}

void MediaServerProcess::run()
{

    QnFileStorageResource::removeOldDirs(); // cleanup temp folders;

#ifdef _WIN32
    win32_exception::setCreateFullCrashDump( MSSettings::roSettings()->value(
        nx_ms_conf::CREATE_FULL_CRASH_DUMP,
        nx_ms_conf::DEFAULT_CREATE_FULL_CRASH_DUMP ).toBool() );
#endif

#ifdef __linux__
    linux_exception::setSignalHandlingDisabled( MSSettings::roSettings()->value(
        nx_ms_conf::CREATE_FULL_CRASH_DUMP,
        nx_ms_conf::DEFAULT_CREATE_FULL_CRASH_DUMP ).toBool() );
#endif

    QString sslCertPath = MSSettings::roSettings()->value( nx_ms_conf::SSL_CERTIFICATE_PATH, getDataDirectory() + lit( "/ssl/cert.pem" ) ).toString();
    QFile f(sslCertPath);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "Could not find SSL certificate at "<<f.fileName()<<". Generating a new one";

        QDir parentDir = QFileInfo(f).absoluteDir();
        if (!parentDir.exists()) {
            if (!QDir().mkpath(parentDir.absolutePath())) {
                qWarning() << "Could not create directory " << parentDir.absolutePath();
            }
        }

        //TODO: #ivigasin sslCertPath can contain non-latin1 symbols
        if (generateSslCertificate(sslCertPath.toLatin1(), qApp->applicationName().toLatin1(), "US", QnAppInfo::organizationName().toLatin1())) {
            qWarning() << "Could not generate SSL certificate ";
        }

        if( !f.open( QIODevice::ReadOnly ) )
            qWarning() << "Could not load SSL certificate "<<f.fileName();
    }
    if( f.isOpen() )
    {
        const QByteArray& certData = f.readAll();
        QnSSLSocket::initSSLEngine( certData );
    }

    QScopedPointer<QnSyncTime> syncTime(new QnSyncTime());
    QScopedPointer<QnServerMessageProcessor> messageProcessor(new QnServerMessageProcessor());
    QScopedPointer<QnRuntimeInfoManager> runtimeInfoManager(new QnRuntimeInfoManager());

#ifdef ENABLE_ONVIF
    QnSoapServer soapServer;    //starting soap server to accept event notifications from onvif cameras
    soapServer.bind();
    soapServer.start();
#endif //ENABLE_ONVIF
    std::unique_ptr<QnCameraUserAttributePool> cameraUserAttributePool( new QnCameraUserAttributePool() );
    std::unique_ptr<QnMediaServerUserAttributesPool> mediaServerUserAttributesPool( new QnMediaServerUserAttributesPool() );
    std::unique_ptr<QnResourcePropertyDictionary> dictionary(new QnResourcePropertyDictionary());
    std::unique_ptr<QnResourceStatusDictionary> statusDict(new QnResourceStatusDictionary());
    std::unique_ptr<QnServerAdditionalAddressesDictionary> serverAdditionalAddressesDictionary(new QnServerAdditionalAddressesDictionary());

    std::unique_ptr<QnResourcePool> resourcePool( new QnResourcePool() );

    std::unique_ptr<HostSystemPasswordSynchronizer> hostSystemPasswordSynchronizer( new HostSystemPasswordSynchronizer() );

    QScopedPointer<QnGlobalSettings> globalSettings(new QnGlobalSettings());
    std::unique_ptr<QnMServerAuditManager> auditManager( new QnMServerAuditManager() );

    QnAuthHelper::initStaticInstance(new QnAuthHelper());
    connect(QnAuthHelper::instance(), &QnAuthHelper::emptyDigestDetected, this, &MediaServerProcess::at_emptyDigestDetected);

    //TODO #ak following is to allow "OPTIONS * RTSP/1.0" without authentication
    QnAuthHelper::instance()->restrictionList()->allow( lit( "?" ), AuthMethod::noAuth );

    QnAuthHelper::instance()->restrictionList()->allow( lit("*/api/ping"), AuthMethod::noAuth );
    QnAuthHelper::instance()->restrictionList()->allow( lit("*/api/camera_event*"), AuthMethod::noAuth );
    QnAuthHelper::instance()->restrictionList()->allow( lit("*/api/showLog*"), AuthMethod::urlQueryParam );   //allowed by default for now
    QnAuthHelper::instance()->restrictionList()->allow( lit("*/api/moduleInformation"), AuthMethod::noAuth );
    QnAuthHelper::instance()->restrictionList()->allow( lit("*/api/gettime"), AuthMethod::noAuth );
    QnAuthHelper::instance()->restrictionList()->allow( lit("/static/*"), AuthMethod::noAuth );
    //by following delegating hls authentication to target server
    QnAuthHelper::instance()->restrictionList()->allow( lit("/proxy/*/hls/*"), AuthMethod::noAuth );

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
    initAppServerConnection(*settings);

    QnMulticodecRtpReader::setDefaultTransport( MSSettings::roSettings()->value(QLatin1String("rtspTransport"), RtpTransport::_auto).toString().toUpper() );

    QScopedPointer<QnServerPtzControllerPool> ptzPool(new QnServerPtzControllerPool());

    //QnAppServerConnectionPtr appServerConnection = QnAppServerConnectionFactory::createConnection();

    std::unique_ptr<QnStorageManager> storageManager( new QnStorageManager() );
    std::unique_ptr<QnFileDeletor> fileDeletor( new QnFileDeletor() );

    connect(QnResourceDiscoveryManager::instance(), &QnResourceDiscoveryManager::CameraIPConflict, this, &MediaServerProcess::at_cameraIPConflict);
    connect(QnStorageManager::instance(), &QnStorageManager::noStoragesAvailable, this, &MediaServerProcess::at_storageManager_noStoragesAvailable);
    connect(QnStorageManager::instance(), &QnStorageManager::storageFailure, this, &MediaServerProcess::at_storageManager_storageFailure);
    connect(QnStorageManager::instance(), &QnStorageManager::rebuildFinished, this, &MediaServerProcess::at_storageManager_rebuildFinished);

    QString dataLocation = getDataDirectory();
    QDir stateDirectory;
    stateDirectory.mkpath(dataLocation + QLatin1String("/state"));
    fileDeletor->init(dataLocation + QLatin1String("/state")); // constructor got root folder for temp files


    // If adminPassword is set by installer save it and create admin user with it if not exists yet
    qnCommon->setDefaultAdminPassword(settings->value(APPSERVER_PASSWORD, QLatin1String("")).toString());
    qnCommon->setUseLowPriorityAdminPasswordHach(settings->value(LOW_PRIORITY_ADMIN_PASSWORD, false).toBool());

    qnCommon->setAdminPasswordData(settings->value(ADMIN_PSWD_HASH).toByteArray(), settings->value(ADMIN_PSWD_DIGEST).toByteArray());

    connect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoAdded, this, &MediaServerProcess::at_runtimeInfoChanged);
    connect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoChanged, this, &MediaServerProcess::at_runtimeInfoChanged);

    qnCommon->setModuleGUID(serverGuid());

    bool compatibilityMode = cmdLineArguments.devModeKey == lit("razrazraz");
    const QString appserverHostString = MSSettings::roSettings()->value("appserverHost").toString();
    bool isLocal = isLocalAppServer(appserverHostString);
    int serverFlags = Qn::SF_None; // TODO: #Elric #EC2 type safety has just walked out of the window.
#ifdef EDGE_SERVER
    serverFlags |= Qn::SF_Edge;
#endif
    if (QnAppInfo::armBox() == "bpi" || compatibilityMode) // check compatibilityMode here for testing purpose
        serverFlags |= Qn::SF_IfListCtrl | Qn::SF_timeCtrl;

    if (!isLocal)
        serverFlags |= Qn::SF_RemoteEC;
    m_publicAddress = getPublicAddress();
    if (!m_publicAddress.isNull())
        serverFlags |= Qn::SF_HasPublicIP;


    QString systemName = settings->value(SYSTEM_NAME_KEY).toString();
#ifdef __arm__
    if (systemName.isEmpty()) {
        systemName = QString(lit("%1system_%2")).arg(QString::fromLatin1(AUTO_GEN_SYSTEM_NAME)).arg(getMacFromPrimaryIF());
        settings->setValue(SYSTEM_NAME_KEY, systemName);
    }
#endif
    if (systemName.startsWith(AUTO_GEN_SYSTEM_NAME)) {
        serverFlags |= Qn::SF_AutoSystemName;
        systemName = systemName.mid(AUTO_GEN_SYSTEM_NAME.length());
    }

    qnCommon->setLocalSystemName(systemName);

    qint64 systemIdentityTime = MSSettings::roSettings()->value(SYSTEM_IDENTITY_TIME).toLongLong();
    qnCommon->setSystemIdentityTime(systemIdentityTime, qnCommon->moduleGUID());
    connect(qnCommon, &QnCommonModule::systemIdentityTimeChanged, this, &MediaServerProcess::at_systemIdentityTimeChanged, Qt::QueuedConnection);

    std::unique_ptr<ec2::AbstractECConnectionFactory> ec2ConnectionFactory(getConnectionFactory( Qn::PT_Server ));

    ec2::ApiRuntimeData runtimeData;
    runtimeData.peer.id = qnCommon->moduleGUID();
    runtimeData.peer.instanceId = qnCommon->runningInstanceGUID();
    runtimeData.peer.peerType = Qn::PT_Server;
    runtimeData.box = QnAppInfo::armBox();
    runtimeData.brand = QnAppInfo::productNameShort();
    runtimeData.platform = QnAppInfo::applicationPlatform();
    runtimeData.nx1mac = Nx1::getMac();
    runtimeData.nx1serial = Nx1::getSerial();
    int guidCompatibility = 0;
    runtimeData.mainHardwareIds = LLUtil::getMainHardwareIds(guidCompatibility, MSSettings::roSettings()).toVector();
    runtimeData.compatibleHardwareIds = LLUtil::getCompatibleHardwareIds(guidCompatibility, MSSettings::roSettings()).toVector();
    QnRuntimeInfoManager::instance()->updateLocalItem(runtimeData);    // initializing localInfo

    MediaServerStatusWatcher mediaServerStatusWatcher;

    ec2::ResourceContext resCtx(
        QnResourceDiscoveryManager::instance(),
        qnResPool,
        qnResTypePool );
    //passing settings
    std::map<QString, QVariant> confParams;
    for( const auto& paramName: MSSettings::roSettings()->allKeys() )
    {
        if( paramName.startsWith( lit("ec") ) )
            confParams.emplace( paramName, MSSettings::roSettings()->value( paramName ) );
    }
    ec2ConnectionFactory->setConfParams(std::move(confParams));
    ec2ConnectionFactory->setContext(resCtx);
    ec2::AbstractECConnectionPtr ec2Connection;
    QnConnectionInfo connectInfo;

    while (!needToStop())
    {
        const ec2::ErrorCode errorCode = ec2ConnectionFactory->connectSync(
            QnAppServerConnectionFactory::url(), ec2::ApiClientInfoData(), &ec2Connection );
        if( errorCode == ec2::ErrorCode::ok )
        {
            connectInfo = ec2Connection->connectionInfo();
            NX_LOG( QString::fromLatin1("Connected to local EC2"), cl_logWARNING );
            break;
        }

        NX_LOG( QString::fromLatin1("Can't connect to local EC2. %1").arg(ec2::toString(errorCode)), cl_logERROR );
        QnSleep::msleep(3000);
    }

    if (needToStop())
        return; //TODO #ak correctly deinitialize what has been initialised

    MSSettings::roSettings()->setValue(REMOVE_DB_PARAM_NAME, "0");

    connect(ec2Connection.get(), &ec2::AbstractECConnection::databaseDumped, this, &MediaServerProcess::at_databaseDumped);
    qnCommon->setRemoteGUID(QnUuid(connectInfo.ecsGuid));
    MSSettings::roSettings()->sync();
    if (MSSettings::roSettings()->value(PENDING_SWITCH_TO_CLUSTER_MODE).toString() == "yes") {
        NX_LOG( QString::fromLatin1("Switching to cluster mode and restarting..."), cl_logWARNING );
        MSSettings::roSettings()->setValue(SYSTEM_NAME_KEY, connectInfo.systemName);
        MSSettings::roSettings()->remove("appserverHost");
        MSSettings::roSettings()->remove("appserverLogin");
        MSSettings::roSettings()->setValue(APPSERVER_PASSWORD, "");
        MSSettings::roSettings()->remove(PENDING_SWITCH_TO_CLUSTER_MODE);
        MSSettings::roSettings()->sync();

        QFile::remove(closeDirPath(getDataDirectory()) + "/ecs.sqlite");

        // kill itself to restart
        abort();
        return;
    }
    settings->remove(ADMIN_PSWD_HASH);
    settings->remove(ADMIN_PSWD_DIGEST);
    settings->setValue(LOW_PRIORITY_ADMIN_PASSWORD, "");

    QnAppServerConnectionFactory::setEc2Connection( ec2Connection );
    auto clearEc2ConnectionGuardFunc = [](MediaServerProcess*){
        QnAppServerConnectionFactory::setEc2Connection(ec2::AbstractECConnectionPtr()); };
    std::unique_ptr<MediaServerProcess, decltype(clearEc2ConnectionGuardFunc)>
        clearEc2ConnectionGuard(this, clearEc2ConnectionGuardFunc);

    QnAppServerConnectionFactory::setEC2ConnectionFactory( ec2ConnectionFactory.get() );

    connect( ec2Connection->getTimeManager().get(), &ec2::AbstractTimeManager::timeChanged,
             QnSyncTime::instance(), (void(QnSyncTime::*)(qint64))&QnSyncTime::updateTime );

    QnMServerResourceSearcher::initStaticInstance( new QnMServerResourceSearcher() );

    //Initializing plugin manager
    PluginManager::instance()->loadPlugins( MSSettings::roSettings() );

    using namespace std::placeholders;
    for (const auto storagePlugin : 
         PluginManager::instance()->findNxPlugins<nx_spl::StorageFactory>(nx_spl::IID_StorageFactory))
    {
        QnStoragePluginFactory::instance()->registerStoragePlugin(
            storagePlugin->storageType(),
            std::bind(
                &QnThirdPartyStorageResource::instance,
                _1,
                storagePlugin
            ),
            false
        );                    
    }

    QnStoragePluginFactory::instance()->registerStoragePlugin(
        "smb",
        QnFileStorageResource::instance,
        false
    );

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

    if (MSSettings::roSettings()->value("disableTranscoding").toBool())
        qnCommon->setTranscodeDisabled(true);

    QnResource::startCommandProc();

    std::unique_ptr<QnRestProcessorPool> restProcessorPool( new QnRestProcessorPool() );

    if( QnAppServerConnectionFactory::url().scheme().toLower() == lit("file") )
        ec2ConnectionFactory->registerRestHandlers( restProcessorPool.get() );

    std::unique_ptr<StreamingChunkTranscoder> streamingChunkTranscoder(
        new StreamingChunkTranscoder( StreamingChunkTranscoder::fBeginOfRangeInclusive ) );
    std::unique_ptr<nx_hls::HLSSessionPool> hlsSessionPool( new nx_hls::HLSSessionPool() );

    if( !initTcpListener() )
    {
        qCritical() << "Failed to bind to local port. Terminating...";
        QCoreApplication::quit();
        return;
    }

    using namespace std::placeholders;
    m_universalTcpListener->setProxyHandler<QnProxyConnectionProcessor>(&QnUniversalRequestProcessor::isProxy);
    messageProcessor->registerProxySender(m_universalTcpListener);

    ec2ConnectionFactory->registerTransactionListener( m_universalTcpListener );

    qnCommon->setModuleUlr(QString("http://%1:%2").arg(m_publicAddress.toString()).arg(m_universalTcpListener->getPort()));
    bool isNewServerInstance = false;
    while (m_mediaServer.isNull() && !needToStop())
    {
        QnMediaServerResourcePtr server = findServer(ec2Connection);

        if (!server) {
            server = QnMediaServerResourcePtr(new QnMediaServerResource(qnResTypePool));
            server->setId(serverGuid());
            server->setMaxCameras(DEFAULT_MAX_CAMERAS);
            isNewServerInstance = true;
        }
        server->setSystemInfo(QnSystemInformation::currentSystemInformation());

        server->setServerFlags((Qn::ServerFlags) serverFlags);

        QHostAddress appserverHost;
        if (!isLocal) {
            do
            {
                appserverHost = resolveHost(appserverHostString);
            } while (appserverHost.toIPv4Address() == 0);
        }

        bool isModified = false;
        if (m_universalTcpListener->getPort() != server->getPort())
            isModified = true;

        setServerNameAndUrls(server, defaultLocalAddress(appserverHost), m_universalTcpListener->getPort());

        QList<QHostAddress> serverIfaceList = allLocalAddresses();
        if (!m_publicAddress.isNull()) {
            bool isExists = false;
            for (int i = 0; i < serverIfaceList.size(); ++i)
            {
                if (serverIfaceList[i] == m_publicAddress)
                    isExists = true;
            }
            if (!isExists)
                serverIfaceList << m_publicAddress;
        }

        if (server->getNetAddrList() != serverIfaceList) {
            server->setNetAddrList(serverIfaceList);
            isModified = true;
        }

        bool needUpdateAuthKey = false;
        if (server->getSystemName() != qnCommon->localSystemName())
        {
            if (!server->getSystemName().isEmpty())
                needUpdateAuthKey = true;
            server->setSystemName(qnCommon->localSystemName());
            isModified = true;
        }
        if (server->getVersion() != qnCommon->engineVersion()) {
            server->setVersion(qnCommon->engineVersion());
            isModified = true;
        }

        QByteArray settingsAuthKey = decodeAuthKey(MSSettings::roSettings()->value(AUTH_KEY).toString().toLatin1());
        QByteArray authKey = settingsAuthKey;
        if (authKey.isEmpty())
            authKey = server->getAuthKey().toLatin1();
        if (authKey.isEmpty() || needUpdateAuthKey)
            authKey = QnUuid::createUuid().toString().toLatin1();

        if (server->getAuthKey().toLatin1() != authKey) {
            server->setAuthKey(authKey);
            isModified = true;
        }
        // Keep server auth key in registry. Server MUST be able pass authorization after deleting database in database restore process
        if (settingsAuthKey != authKey)
            encodeAndStoreAuthKey(authKey);

        if (isModified)
            m_mediaServer = registerServer(ec2Connection, server, isNewServerInstance);
        else
            m_mediaServer = server;

        const auto hwInfo = HardwareInformation::instance();
        server->setProperty(Qn::CPU_ARCHITECTURE, hwInfo.cpuArchitecture);
        server->setProperty(Qn::CPU_MODEL_NAME, hwInfo.cpuModelName);
        server->setProperty(Qn::PHISICAL_MEMORY, QString::number(hwInfo.phisicalMemory));

        server->setProperty(Qn::FULL_VERSION, QnAppInfo::applicationFullVersion());
        server->setProperty(Qn::BETA, QString::number(QnAppInfo::beta() ? 1 : 0));
        server->setProperty(Qn::PUBLIC_IP, m_publicAddress.toString());
        server->setProperty(Qn::SYSTEM_RUNTIME, QnSystemInformation::currentSystemRuntime());

        const auto confStats = MSSettings::roSettings()->value(Qn::STATISTICS_REPORT_ALLOWED);
        if (!confStats.isNull()) // if present
        {
            server->setProperty(Qn::STATISTICS_REPORT_ALLOWED, QnLexical::serialized(confStats.toBool()));
            MSSettings::roSettings()->remove(Qn::STATISTICS_REPORT_ALLOWED);
            MSSettings::roSettings()->sync();
        }

        propertyDictionary->saveParams(server->getId());

        if (m_mediaServer.isNull())
            QnSleep::msleep(1000);
    }

    if (!m_publicAddress.isNull())
    {
        if (!m_ipDiscovery->publicIP().isNull()) {
            m_updatePiblicIpTimer.reset(new QTimer());
            connect(m_updatePiblicIpTimer.get(), &QTimer::timeout, m_ipDiscovery.get(), &QnPublicIPDiscovery::update);
            connect(m_ipDiscovery.get(), &QnPublicIPDiscovery::found, this, &MediaServerProcess::at_updatePublicAddress);
            m_updatePiblicIpTimer->start(60 * 1000 * 2);
        }

        QnPeerRuntimeInfo localInfo = QnRuntimeInfoManager::instance()->localInfo();
        localInfo.data.publicIP = m_publicAddress.toString();
        QnRuntimeInfoManager::instance()->updateLocalItem(localInfo);
    }

    /* This key means that password should be forcibly changed in the database. */
    MSSettings::roSettings()->remove(OBSOLETE_SERVER_GUID);
    MSSettings::roSettings()->setValue(APPSERVER_PASSWORD, "");
#ifdef _DEBUG
    MSSettings::roSettings()->sync();
    Q_ASSERT_X(MSSettings::roSettings()->value(APPSERVER_PASSWORD).toString().isEmpty(), Q_FUNC_INFO, "appserverPassword is not emptyu in registry. Restart the server as Administrator");
#endif

    if (needToStop()) {
        stopObjects();
        return;
    }

    do {
        if (needToStop())
            return;
    } while (ec2Connection->getResourceManager()->setResourceStatusLocalSync(m_mediaServer->getId(), Qn::Online) != ec2::ErrorCode::ok);


    QnRecordingManager::initStaticInstance( new QnRecordingManager() );
    qnResPool->addResource(m_mediaServer);

    QString moduleName = qApp->applicationName();
    if( moduleName.startsWith( qApp->organizationName() ) )
        moduleName = moduleName.mid( qApp->organizationName().length() ).trimmed();

    QnModuleInformation selfInformation = m_mediaServer->getModuleInformation();
    if (!compatibilityMode)
        selfInformation.customization = QnAppInfo::customizationName();
    selfInformation.version = qnCommon->engineVersion();
    selfInformation.sslAllowed = MSSettings::roSettings()->value( nx_ms_conf::ALLOW_SSL_CONNECTIONS, nx_ms_conf::DEFAULT_ALLOW_SSL_CONNECTIONS ).toBool();
    selfInformation.runtimeId = qnCommon->runningInstanceGUID();
    selfInformation.flags = m_mediaServer->getServerFlags();
    selfInformation.ecDbReadOnly = ec2Connection->connectionInfo().ecDbReadOnly;

    qnCommon->setModuleInformation(selfInformation);
    qnCommon->bindModuleinformation(m_mediaServer);

    m_moduleFinder = new QnModuleFinder(false, compatibilityMode);
    std::unique_ptr<QnModuleFinder> moduleFinderScopedPointer( m_moduleFinder );
    ec2ConnectionFactory->setCompatibilityMode(compatibilityMode);
    if (!cmdLineArguments.allowedDiscoveryPeers.isEmpty()) {
        QSet<QnUuid> allowedPeers;
        for (const QString &peer: cmdLineArguments.allowedDiscoveryPeers.split(";")) {
            QnUuid peerId(peer);
            if (!peerId.isNull())
                allowedPeers << peerId;
        }
        qnCommon->setAllowedPeers(allowedPeers);
    }

    connect(m_moduleFinder, &QnModuleFinder::moduleConflict, this, &MediaServerProcess::at_serverModuleConflict);

    QScopedPointer<QnServerConnector> serverConnector(new QnServerConnector(m_moduleFinder));

    // ------------------------------------------

    QScopedPointer<QnRouter> router(new QnRouter(m_moduleFinder));

    QScopedPointer<QnServerUpdateTool> serverUpdateTool(new QnServerUpdateTool());

    // ===========================================================================
    QnResource::initAsyncPoolInstance()->setMaxThreadCount( MSSettings::roSettings()->value(
        nx_ms_conf::RESOURCE_INIT_THREADS_COUNT,
        nx_ms_conf::DEFAULT_RESOURCE_INIT_THREADS_COUNT ).toInt() );
    QnResource::initAsyncPoolInstance()->setExpiryTimeout(-1); // default experation timeout is 30 second. But it has a bug in QT < v.5.3
    QThreadPool::globalInstance()->setExpiryTimeout(-1);

    // ============================
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
    if (QnUserResourcePtr adminUser = qnResPool->getAdministrator())
    {
        qnCommon->bindModuleinformation(adminUser);
        qnCommon->updateModuleInformation();

        hostSystemPasswordSynchronizer->syncLocalHostRootPasswordWithAdminIfNeeded( adminUser );

        typedef ec2::Ec2StaticticsReporter stats;
        bool adminParamsChanged = false;

        // TODO: fix, when VS supports init lists:
        //       for (const auto& param : { stats::SR_TIME_CYCLE, stats::SR_SERVER_API })
        const QString* statParams[] = { &stats::SR_TIME_CYCLE, &stats::SR_SERVER_API };
        for (auto it = &statParams[0]; it != &statParams[sizeof(statParams)/sizeof(statParams[0])]; ++it)
        {
            const QString& param = **it;
            const QString val = MSSettings::roSettings()->value(param, lit("")).toString();
            if (adminUser->setProperty(param, val, QnResource::NO_ALLOW_EMPTY))
            {
                MSSettings::roSettings()->remove(param);
                adminParamsChanged = true;
            }
        }

        if (adminParamsChanged)
        {
            propertyDictionary->saveParams(adminUser->getId());
            MSSettings::roSettings()->sync();
        }
    }

    QnStorageResourceList storages = m_mediaServer->getStorages();
    QnStorageResourceList modifiedStorages = createStorages(m_mediaServer);
    modifiedStorages.append(updateStorages(m_mediaServer));
    saveStorages(ec2Connection, modifiedStorages);
    for(const QnStorageResourcePtr &storage: modifiedStorages)
        messageProcessor->updateResource(storage);

    qnStorageMan->initDone();
#ifndef EDGE_SERVER
    updateDisabledVendorsIfNeeded();
    updateAllowCameraCHangesIfNeed();
    //QSet<QString> disabledVendors = QnGlobalSettings::instance()->disabledVendorsSet();
#endif

    std::unique_ptr<QnLdapManager> ldapManager(new QnLdapManager());

    //QnCommonMessageProcessor::instance()->init(ec2Connection); // start receiving notifications

    /*
    QnScheduleTaskList scheduleTasks;
    for (const QnScheduleTask &scheduleTask: scheduleTasks)
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

    m_firstRunningTime = MSSettings::runTimeSettings()->value("lastRunningTime").toLongLong();

    m_crashReporter.reset(new ec2::CrashReporter);

    QTimer timer;
    connect(&timer, SIGNAL(timeout()), this, SLOT(at_timer()), Qt::DirectConnection);
    timer.start(QnVirtualCameraResource::issuesTimeoutMs());
    at_timer();

    QTimer::singleShot(3000, this, SLOT(at_connectionOpened()));
    QTimer::singleShot(0, this, SLOT(at_appStarted()));

    m_dumpSystemResourceUsageTaskID = TimerManager::instance()->addTimer(
        std::bind( &MediaServerProcess::dumpSystemUsageStats, this ),
        SYSTEM_USAGE_DUMP_TIMEOUT );

    QnRecordingManager::instance()->start();
    QnMServerResourceSearcher::instance()->start();
    m_universalTcpListener->start();
    serverConnector->start();
#if 1
    if (ec2Connection->connectionInfo().ecUrl.scheme() == "file") {
        // Connect to local database. Start peer-to-peer sync (enter to cluster mode)
        qnCommon->setCloudMode(true);
        m_moduleFinder->start();
    }
#endif

    exec();
    disconnect(0,0, this, 0);
    WaitingForQThreadToEmptyEventQueue waitingForObjectsToBeFreed( QThread::currentThread(), 3 );
    waitingForObjectsToBeFreed.join();

    qWarning()<<"QnMain event loop has returned. Destroying objects...";

    m_crashReporter.reset();

    //cancelling dumping system usage
    quint64 dumpSystemResourceUsageTaskID = 0;
    {
        QMutexLocker lk( &m_mutex );
        dumpSystemResourceUsageTaskID = m_dumpSystemResourceUsageTaskID;
        m_dumpSystemResourceUsageTaskID = 0;
    }
    TimerManager::instance()->joinAndDeleteTimer( dumpSystemResourceUsageTaskID );


    QnResourceDiscoveryManager::instance()->pleaseStop();
    QnResource::pleaseStopAsyncTasks();
    stopObjects();

    QnResource::stopCommandProc();
    // todo: #rvasilenko some undeleted resources left in the QnMain event loop. I stopped TimerManager as temporary solution for it.
    TimerManager::instance()->stop();

    hlsSessionPool.reset();
    streamingChunkTranscoder.reset();

    delete QnRecordingManager::instance();
    QnRecordingManager::initStaticInstance( NULL );

    restProcessorPool.reset();

    delete QnMServerResourceSearcher::instance();
    QnMServerResourceSearcher::initStaticInstance( NULL );

    delete QnVideoCameraPool::instance();
    QnVideoCameraPool::initStaticInstance( NULL );

    QnResourceDiscoveryManager::instance()->stop();
    QnResource::stopAsyncTasks();

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

    delete QnMotionHelper::instance();
    QnMotionHelper::initStaticInstance( NULL );


    QnStorageManager::instance()->stopAsyncTasks();

    ptzPool.reset();

    QnServerMessageProcessor::instance()->init(NULL); // stop receiving notifications
    messageProcessor.reset();

    //disconnecting from EC2
    clearEc2ConnectionGuard.reset();

    ec2Connection.reset();
    QnAppServerConnectionFactory::setEC2ConnectionFactory( nullptr );
    ec2ConnectionFactory.reset();
    
    // destroy events db
    QnEventsDB::fini();

    mserverResourceDiscoveryManager.reset();

    av_lockmgr_register(NULL);

    // This method will set flag on message channel to threat next connection close as normal
    //appServerConnection->disconnectSync();
    MSSettings::runTimeSettings()->setValue("lastRunningTime", 0);

    QnSSLSocket::releaseSSLEngine();
    QnAuthHelper::initStaticInstance(NULL);

    globalSettings.reset();

    fileDeletor.reset();
    storageManager.reset();
    if (m_mediaServer)
        m_mediaServer->beforeDestroy();
    m_mediaServer.clear();
}


void MediaServerProcess::at_appStarted()
{
    if (isStopping())
        return;

    QnCommonMessageProcessor::instance()->init(QnAppServerConnectionFactory::getConnection2()); // start receiving notifications
    m_crashReporter->scanAndReportByTimer(MSSettings::runTimeSettings());
};

void MediaServerProcess::at_runtimeInfoChanged(const QnPeerRuntimeInfo& runtimeInfo)
{
    if (isStopping())
        return;
    if (runtimeInfo.uuid != qnCommon->moduleGUID())
        return;

    ec2::QnTransaction<ec2::ApiRuntimeData> tran(ec2::ApiCommand::runtimeInfoChanged);
    tran.params = runtimeInfo.data;
    qnTransactionBus->sendTransaction(tran);
}

void MediaServerProcess::at_emptyDigestDetected(const QnUserResourcePtr& user, const QString& login, const QString& password)
{
    if (isStopping())
        return;

    // fill authenticate digest here for compatibility with version 2.1 and below.
    const ec2::AbstractECConnectionPtr& appServerConnection = QnAppServerConnectionFactory::getConnection2();
    if (user->getDigest().isEmpty() && !m_updateUserRequests.contains(user->getId()))
    {
        user->setPassword(password);
        user->setName(login);
        user->generateHash();
        m_updateUserRequests << user->getId();
        appServerConnection->getUserManager()->save(
            user, this,
            [this, user]( int /*reqID*/, ec2::ErrorCode errorCode )
            {
                if (errorCode == ec2::ErrorCode::ok) {
                    ec2::ApiUserData userData;
                    fromResourceToApi(user, userData);
                    user->setDigest(userData.digest);
                    user->setCryptSha512Hash(userData.cryptSha512Hash);
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
        QScopedPointer<QnMediaServerModule> module(new QnMediaServerModule());

        if (!m_overrideVersion.isNull())
            qnCommon->setEngineVersion(m_overrideVersion);

        m_main.reset(new MediaServerProcess(m_argc, m_argv));

        int res = application()->exec();

        m_main.reset();

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

        QCoreApplication::setOrganizationName(QnAppInfo::organizationName());
        QCoreApplication::setApplicationName(lit(QN_APPLICATION_NAME));
        if (QCoreApplication::applicationVersion().isEmpty())
            QCoreApplication::setApplicationVersion(QnAppInfo::applicationVersion());

        updateGuidIfNeeded();

        QnUuid guid = serverGuid();
        if (guid.isNull())
        {
            qDebug() << "Can't save guid. Run once as administrator.";
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
    QString hardwareIdAsGuid() {
        auto hwId = LLUtil::getHardwareId(LLUtil::LATEST_HWID_VERSION, false, MSSettings::roSettings());
        auto hwIdString = QnUuid::fromHardwareId(hwId).toString();
        std::cout << "Got hwID \"" << hwIdString.toStdString() << "\"" << std::endl;
        return hwIdString;
    }

    void updateGuidIfNeeded() {
        QString guidIsHWID = MSSettings::roSettings()->value(GUID_IS_HWID).toString();
        QString serverGuid = MSSettings::roSettings()->value(SERVER_GUID).toString();
        QString serverGuid2 = MSSettings::roSettings()->value(SERVER_GUID2).toString();
        QString pendingSwitchToClusterMode = MSSettings::roSettings()->value(PENDING_SWITCH_TO_CLUSTER_MODE).toString();

        QString hwidGuid = hardwareIdAsGuid();

        if (guidIsHWID == YES) {
            if (serverGuid.isEmpty())
                MSSettings::roSettings()->setValue(SERVER_GUID, hwidGuid);
            else if (serverGuid != hwidGuid)
                MSSettings::roSettings()->setValue(GUID_IS_HWID, NO);

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

        QnUuid obsoleteGuid = QnUuid(MSSettings::roSettings()->value(OBSOLETE_SERVER_GUID).toString());
        if (!obsoleteGuid.isNull()) {
            qnCommon->setObsoleteServerGuid(obsoleteGuid);
        }
    }

private:
    int m_argc;
    char **m_argv;
    QScopedPointer<MediaServerProcess> m_main;
    QnSoftwareVersion m_overrideVersion;
};

void stopServer(int /*signal*/)
{
    restartFlag = false;
    if (serviceMainInstance) {
        //output to console from signal handler can cause deadlock
        //qWarning() << "got signal" << signal << "stop server!";
        serviceMainInstance->stopAsync();
    }
}

void restartServer(int restartTimeout)
{
    restartFlag = true;
    if (serviceMainInstance) {
        qWarning() << "restart requested!";
        QTimer::singleShot(restartTimeout, serviceMainInstance, SLOT(stopAsync()));
    }
}

/*
bool changePort(quint16 port)
{
    if (serviceMainInstance)
        return serviceMainInstance->changePort(port);
    else
        return false;
}
*/

#ifdef __linux__
void SIGUSR1_handler(int)
{
    //doing nothing. Need this signal only to interrupt some blocking calls
}
#endif

int MediaServerProcess::main(int argc, char* argv[])
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

#ifdef __linux__
    signal( SIGUSR1, SIGUSR1_handler );
#endif

    //parsing command-line arguments
    QString configFilePath;
    QString rwConfigFilePath;
    bool showVersion = false;
    bool showHelp = false;
    bool disableCrashHandler = false;
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
    commandLineParser.addParameter(&cmdLineArguments.msgLogLevel, "--http-log-level", NULL,
        "Log value for http_log.log. Supported values same as above. Default is none (no logging)", "none");
    commandLineParser.addParameter(&cmdLineArguments.ec2TranLogLevel, "--ec2-tran-log-level", NULL,
        "Log value for ec2_tran.log. Supported values same as above. Default is none (no logging)", "none");
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

    #ifdef __linux__
        commandLineParser.addParameter(&disableCrashHandler, "--disable-crash-handler", NULL,
            lit("Disables crash signal handler (linux only)"), true);
    #endif

    commandLineParser.parse(argc, argv, stderr, QnCommandLineParser::PreserveParsedParameters);

    #ifdef __linux__
        if( !disableCrashHandler )
            linux_exception::installCrashSignalHandler();
    #endif

    if( showVersion )
    {
        std::cout << QnAppInfo::applicationFullVersion().toStdString() << std::endl;
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
            qWarning() << "Starting with overridden version: " << version.toString();
            service.setOverrideVersion(version);
        }
    }

    int res = service.exec();
    if (restartFlag && res == 0)
        return 1;
    return 0;
}
