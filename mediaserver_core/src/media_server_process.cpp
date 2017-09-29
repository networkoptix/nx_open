#include "media_server_process.h"

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <functional>
#include <signal.h>
#ifdef __linux__
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <qtsinglecoreapplication.h>
#include <qtservice.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <QtCore/QUrl>
#include <QtConcurrent/QtConcurrent>
#include <nx/utils/uuid.h>
#include <utils/common/ldap.h>
#include <utils/call_counter/call_counter.h>
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

#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/providers/resource_access_provider.h>

#include <core/resource_management/camera_driver_restriction_list.h>
#include <core/resource_management/mserver_resource_discovery_manager.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/server_additional_addresses_dictionary.h>

#include <core/resource/storage_plugin_factory.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_user_attributes.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/camera_resource.h>

#include <events/mserver_business_rule_processor.h>

#include <media_server/media_server_app_info.h>
#include <media_server/mserver_status_watcher.h>
#include <media_server/server_message_processor.h>
#include <media_server/settings.h>
#include <media_server/server_update_tool.h>
#include <media_server/server_connector.h>
#include <media_server/file_connection_processor.h>
#include <media_server/crossdomain_connection_processor.h>
#include <media_server/resource_status_watcher.h>
#include <media_server/media_server_resource_searchers.h>

#include <motion/motion_helper.h>

#include <network/auth/time_based_nonce_provider.h>
#include <network/authenticate_helper.h>
#include <network/connection_validator.h>
#include <network/default_tcp_connection_processor.h>
#include <network/system_helpers.h>

#include <nx_ec/ec2_lib.h>
#include <nx_ec/ec_api.h>
#include <nx_ec/ec_proto_version.h>
#include <nx_ec/data/api_user_data.h>
#include <nx_ec/managers/abstract_user_manager.h>
#include <nx_ec/managers/abstract_layout_manager.h>
#include <nx_ec/managers/abstract_videowall_manager.h>
#include <nx_ec/managers/abstract_webpage_manager.h>
#include <nx_ec/managers/abstract_camera_manager.h>
#include <nx_ec/managers/abstract_server_manager.h>
#include <nx/network/socket.h>
#include <nx/network/udt/udt_socket.h>

#include <plugins/native_sdk/common_plugin_container.h>
#include <plugins/plugin_manager.h>
#include <plugins/resource/avi/avi_resource.h>

#include <plugins/resource/desktop_camera/desktop_camera_registrator.h>

#include <plugins/resource/mserver_resource_searcher.h>
#include <plugins/resource/mdns/mdns_listener.h>

#include <plugins/storage/file_storage/file_storage_resource.h>
#include <plugins/storage/file_storage/db_storage_resource.h>
#include <plugins/storage/third_party_storage_resource/third_party_storage_resource.h>

#include <recorder/file_deletor.h>
#include <recorder/recording_manager.h>
#include <recorder/storage_manager.h>
#include <recorder/schedule_sync.h>

#include <rest/handlers/acti_event_rest_handler.h>
#include <rest/handlers/business_event_log_rest_handler.h>
#include "rest/handlers/business_log2_rest_handler.h"
#include <rest/handlers/get_system_name_rest_handler.h>
#include <rest/handlers/camera_diagnostics_rest_handler.h>
#include <rest/handlers/camera_settings_rest_handler.h>
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
#include <rest/handlers/timezones_rest_handler.h>
#include <rest/handlers/get_nonce_rest_handler.h>
#include <rest/handlers/cookie_login_rest_handler.h>
#include <rest/handlers/cookie_logout_rest_handler.h>
#include <rest/handlers/activate_license_rest_handler.h>
#include <rest/handlers/test_email_rest_handler.h>
#include <rest/handlers/test_ldap_rest_handler.h>
#include <rest/handlers/update_rest_handler.h>
#include <rest/handlers/update_unauthenticated_rest_handler.h>
#include <rest/handlers/update_information_rest_handler.h>
#include <rest/handlers/restart_rest_handler.h>
#include <rest/handlers/module_information_rest_handler.h>
#include <rest/handlers/iflist_rest_handler.h>
#include <rest/handlers/json_aggregator_rest_handler.h>
#include <rest/handlers/ifconfig_rest_handler.h>
#include <rest/handlers/settime_rest_handler.h>
#include <rest/handlers/configure_rest_handler.h>
#include <rest/handlers/detach_rest_handler.h>
#include <rest/handlers/restore_state_rest_handler.h>
#include <rest/handlers/setup_local_rest_handler.h>
#include <rest/handlers/setup_cloud_rest_handler.h>
#include <rest/handlers/merge_systems_rest_handler.h>
#include <rest/handlers/current_user_rest_handler.h>
#include <rest/handlers/backup_db_rest_handler.h>
#include <rest/handlers/discovered_peers_rest_handler.h>
#include <rest/handlers/log_level_rest_handler.h>
#include <rest/handlers/multiserver_chunks_rest_handler.h>
#include <rest/handlers/camera_history_rest_handler.h>
#include <rest/handlers/multiserver_bookmarks_rest_handler.h>
#include <rest/handlers/save_cloud_system_credentials.h>
#include <rest/handlers/multiserver_thumbnail_rest_handler.h>
#include <rest/handlers/multiserver_statistics_rest_handler.h>
#include <rest/server/rest_connection_processor.h>
#include <rest/handlers/get_hardware_info_rest_handler.h>
#include <rest/handlers/system_settings_handler.h>
#include <rest/handlers/audio_transmission_rest_handler.h>
#include <rest/handlers/start_lite_client_rest_handler.h>
#include <rest/handlers/runtime_info_rest_handler.h>
#ifdef _DEBUG
#include <rest/handlers/debug_events_rest_handler.h>
#endif

#include <rtsp/rtsp_connection.h>

#include <network/module_finder.h>
#include <network/multicodec_rtp_reader.h>
#include <network/router.h>

#include <utils/common/command_line_parser.h>
#include <nx/utils/app_info.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <utils/common/sleep.h>
#include <utils/common/synctime.h>
#include <utils/common/system_information.h>
#include <utils/common/util.h>
#include <nx/network/simple_http_client.h>
#include <nx/network/ssl_socket.h>
#include <nx/network/socket_global.h>
#include <nx/network/cloud/mediator_connector.h>

#include <utils/common/app_info.h>
#include <transcoding/ffmpeg_video_transcoder.h>

#include <nx/utils/crash_dump/systemexcept.h>

#include "platform/hardware_information.h"
#include "platform/platform_abstraction.h"
#include "core/ptz/server_ptz_controller_pool.h"
#include "plugins/resource/acti/acti_resource.h"
#include "transaction/transaction_message_bus.h"
#include "common/common_module.h"
#include "proxy/proxy_receiver_connection_processor.h"
#include "proxy/proxy_connection.h"
#include "streaming/hls/hls_session_pool.h"
#include "streaming/hls/hls_server.h"
#include "streaming/streaming_chunk_transcoder.h"
#include "llutil/hardware_id.h"
#include "api/runtime_info_manager.h"
#include "rest/handlers/old_client_connect_rest_handler.h"
#include "nx_ec/data/api_conversion_functions.h"
#include "nx_ec/dummy_handler.h"
#include "ec2_statictics_reporter.h"
#include "server/host_system_password_synchronizer.h"

#include "core/resource_management/resource_properties.h"
#include "core/resource/network_resource.h"
#include "network/universal_request_processor.h"
#include "core/resource/camera_history.h"
#include <nx/network/nettools.h>
#include "http/iomonitor_tcp_server.h"
#include "ldap/ldap_manager.h"
#include "rest/handlers/multiserver_chunks_rest_handler.h"
#include "rest/handlers/merge_ldap_users_rest_handler.h"
#include "audit/mserver_audit_manager.h"
#include "utils/common/waiting_for_qthread_to_empty_event_queue.h"
#include "core/multicast/multicast_http_server.h"
#include "crash_reporter.h"
#include "rest/handlers/exec_script_rest_handler.h"
#include "rest/handlers/script_list_rest_handler.h"
#include "cloud/cloud_manager_group.h"
#include "rest/handlers/backup_control_rest_handler.h"
#include <database/server_db.h>
#include <server/server_globals.h>
#include <media_server/master_server_status_watcher.h>
#include <media_server/connect_to_cloud_watcher.h>
#include <rest/helpers/permissions_helper.h>
#include "misc/migrate_oldwin_dir.h"
#include "media_server_process_aux.h"

#if !defined(EDGE_SERVER)
#include <nx_speech_synthesizer/text_to_wav.h>
#include <nx/utils/file_system.h>
#endif

#include <streaming/audio_streamer_pool.h>
#include <proxy/2wayaudio/proxy_audio_receiver.h>

#ifdef __arm__
#include "nx1/info.h"
#endif

// This constant is used while checking for compatibility.
// Do not change it until you know what you're doing.
static const char COMPONENT_NAME[] = "MediaServer";
static const QByteArray NO_SETUP_WIZARD("noSetupWizard");

static QString SERVICE_NAME = lit("%1 Server").arg(QnAppInfo::organizationName());
static const quint64 DEFAULT_MAX_LOG_FILE_SIZE = 10*1024*1024;
static const quint64 DEFAULT_LOG_ARCHIVE_SIZE = 25;
static const int UDT_INTERNET_TRAFIC_TIMER = 24 * 60 * 60 * 1000; //< Once a day;
//static const quint64 DEFAULT_MSG_LOG_ARCHIVE_SIZE = 5;
static const unsigned int APP_SERVER_REQUEST_ERROR_TIMEOUT_MS = 5500;
static const QByteArray APPSERVER_PASSWORD("appserverPassword");
static const QByteArray LOW_PRIORITY_ADMIN_PASSWORD("lowPriorityPassword");

class MediaServerProcess;
static MediaServerProcess* serviceMainInstance = 0;
void stopServer(int signal);
bool restartFlag = false;


namespace {
const QString YES = lit("yes");
const QString NO = lit("no");
const QString GUID_IS_HWID = lit("guidIsHWID");
const QString SERVER_GUID = lit("serverGuid");
const QString SERVER_GUID2 = lit("serverGuid2");
const QString OBSOLETE_SERVER_GUID = lit("obsoleteServerGuid");
const QString PENDING_SWITCH_TO_CLUSTER_MODE = lit("pendingSwitchToClusterMode");
const QString MEDIATOR_ADDRESS_UPDATE = lit("mediatorAddressUpdate");

static const int kPublicIpUpdateTimeoutMs = 60 * 2 * 1000;

bool initResourceTypes(const ec2::AbstractECConnectionPtr& ec2Connection)
{
    QList<QnResourceTypePtr> resourceTypeList;
    const ec2::ErrorCode errorCode = ec2Connection->getResourceManager(Qn::kSystemAccess)->getResourceTypesSync(&resourceTypeList);
    if (errorCode != ec2::ErrorCode::ok)
    {
        NX_LOG(QString::fromLatin1("Failed to load resource types. %1").arg(ec2::toString(errorCode)), cl_logERROR);
        return false;
    }

    qnResTypePool->replaceResourceTypeList(resourceTypeList);
    return true;
}

void addFakeVideowallUser()
{
    ec2::ApiUserData fakeUserData;
    fakeUserData.permissions = Qn::GlobalVideoWallModePermissionSet;
    fakeUserData.typeId = qnResTypePool->getFixedResourceTypeId(QnResourceTypePool::kUserTypeId);
    auto fakeUser = ec2::fromApiToResource(fakeUserData);
    fakeUser->setId(Qn::kVideowallUserAccess.userId);
    fakeUser->setName(lit("Video wall"));
    qnResPool->addResource(fakeUser);
}

} // namespace

#ifdef EDGE_SERVER
static const int DEFAULT_MAX_CAMERAS = 1;
#else
static const int DEFAULT_MAX_CAMERAS = 128;
#endif

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

            NX_ASSERT(result.length() > 0 );

            return result;
        }
    }

    return "127.0.0.1";

}

void ffmpegInit()
{
    // TODO: #Elric we need comments about true/false at call site => bad api design, use flags instead
    // true means use it plugin if no <protocol>:// prefix
    QnStoragePluginFactory::instance()->registerStoragePlugin("file", QnFileStorageResource::instance, true);
    QnStoragePluginFactory::instance()->registerStoragePlugin("dbfile", QnDbStorageResource::instance, false);
}

void calculateSpaceLimitOrLoadFromConfig(const QnFileStorageResourcePtr& fileStorage)
{
    const BeforeRestoreDbData& beforeRestoreData = qnCommon->beforeRestoreDbData();
    if (!beforeRestoreData.isEmpty() && beforeRestoreData.hasInfoForStorage(fileStorage->getUrl()))
    {
        fileStorage->setSpaceLimit(beforeRestoreData.getSpaceLimitForStorage(fileStorage->getUrl()));
        return;
    }

    fileStorage->setSpaceLimit(fileStorage->calcInitialSpaceLimit());
}

QnStorageResourcePtr createStorage(const QnUuid& serverId, const QString& path)
{
    NX_LOG(lit("%1 Attempting to create storage %2")
            .arg(Q_FUNC_INFO)
            .arg(path), cl_logDEBUG1);

    QnStorageResourcePtr storage(QnStoragePluginFactory::instance()->createStorage("ufile"));
    storage->setName("Initial");
    storage->setParentId(serverId);
    storage->setUrl(path);

    const auto storagePath = QnStorageResource::toNativeDirPath(storage->getPath());
    const auto partitions = qnPlatform->monitor()->totalPartitionSpaceInfo();
    const auto it = std::find_if(partitions.begin(), partitions.end(),
        [&](const QnPlatformMonitor::PartitionSpace& part)
    { return storagePath.startsWith(QnStorageResource::toNativeDirPath(part.path)); });

    const auto storageType = (it != partitions.end()) ? it->type : QnPlatformMonitor::NetworkPartition;
    storage->setStorageType(QnLexical::serialized(storageType));

    if (auto fileStorage = storage.dynamicCast<QnFileStorageResource>())
    {
        const qint64 totalSpace = fileStorage->calculateAndSetTotalSpaceWithoutInit();
        calculateSpaceLimitOrLoadFromConfig(fileStorage);

        if (totalSpace < fileStorage->getSpaceLimit())
        {
            NX_LOG(lit("%1 Storage with this path %2 total space is unknown or totalSpace < spaceLimit. \n\t Total space: %3, Space limit: %4")
                .arg(Q_FUNC_INFO)
                .arg(path)
                .arg(totalSpace)
                .arg(storage->getSpaceLimit()), cl_logDEBUG1);
            return QnStorageResourcePtr(); // if storage size isn't known or small do not add it by default
        }
    }
    else
    {
        NX_ASSERT(false);
        NX_LOG(lit("%1 Failed to create to storage %2").arg(Q_FUNC_INFO).arg(path), cl_logWARNING);
        return QnStorageResourcePtr();
    }

    storage->setUsedForWriting(storage->initOrUpdate() == Qn::StorageInit_Ok && storage->isWritable());

    NX_LOG(lit("%1 Storage %2 is operational: %3")
            .arg(Q_FUNC_INFO)
            .arg(path)
            .arg(storage->isUsedForWriting()), cl_logDEBUG1);

    QnResourceTypePtr resType = qnResTypePool->getResourceTypeByName("Storage");
    NX_ASSERT(resType);
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

static QStringList listRecordFolders(bool includeNetwork = false)
{
    QStringList folderPaths;

#ifdef Q_OS_WIN
    (void)includeNetwork;
    for (const WinDriveInfo& drive: getWinDrivesInfo())
    {
        if (!(drive.access | WinDriveInfo::Writable) || drive.type != DRIVE_FIXED)
            continue;

        folderPaths.append(QDir::toNativeSeparators(drive.path) + QnAppInfo::mediaFolderName());
     }

#endif

#ifdef Q_OS_LINUX
    QnPlatformMonitor::PartitionTypes searchFlags = QnPlatformMonitor::LocalDiskPartition;
    if (includeNetwork)
        searchFlags |= QnPlatformMonitor::NetworkPartition;

    auto partitions = qnPlatform->monitor()->QnPlatformMonitor::totalPartitionSpaceInfo(searchFlags);

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

    if (MSSettings::roSettings()->value(nx_ms_conf::ENABLE_MULTIPLE_INSTANCES).toInt() != 0)
    {
        for (auto& path: folderPaths)
            path = closeDirPath(path) + serverGuid().toString();
    }
    return folderPaths;
}

QnStorageResourceList getSmallStorages(const QnStorageResourceList& storages)
{
    QnStorageResourceList result;
    for (const auto& storage: storages)
    {
        qint64 totalSpace = -1;
        auto fileStorage = storage.dynamicCast<QnFileStorageResource>();
        if (fileStorage)
            totalSpace = fileStorage->calculateAndSetTotalSpaceWithoutInit();
        else
        {
            storage->initOrUpdate();
            totalSpace = storage->getTotalSpace();
        }
        if (totalSpace != QnStorageResource::kUnknownSize && totalSpace < storage->getSpaceLimit())
            result << storage; // if storage size isn't known do not delete it

        NX_LOG(lit("%1 Candidate %2, isFileStorage = %3, totalSpace = %4, spaceLimit = %5")
               .arg(Q_FUNC_INFO)
               .arg(storage->getUrl())
               .arg((bool)fileStorage)
               .arg(totalSpace)
               .arg(storage->getSpaceLimit()), cl_logDEBUG2);
    }
    return result;
}


QnStorageResourceList createStorages(const QnMediaServerResourcePtr& mServer)
{
    QnStorageResourceList storages;
    QStringList availablePaths;
    //bool isBigStorageExist = false;
    qint64 bigStorageThreshold = 0;

    availablePaths = listRecordFolders();

    NX_LOG(lit("%1 Available paths count: %2")
            .arg(Q_FUNC_INFO)
            .arg(availablePaths.size()), cl_logDEBUG1);

    for(const QString& folderPath: availablePaths)
    {
        NX_LOG(lit("%1 Available path: %2")
                .arg(Q_FUNC_INFO)
                .arg(folderPath), cl_logDEBUG1);

        if (!mServer->getStorageByUrl(folderPath).isNull())
        {
            NX_LOG(lit("%1 Storage with this path %2 already exists. Won't be added.")
                    .arg(Q_FUNC_INFO)
                    .arg(folderPath), cl_logDEBUG1);
            continue;
        }
        // Create new storage because of new partition found that missing in the database
        QnStorageResourcePtr storage = createStorage(mServer->getId(), folderPath);
        if (!storage)
            continue;

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

    QString logMessage = lit("%1 Storage new candidates:\n").arg(Q_FUNC_INFO);
    for (const auto& storage : storages)
        logMessage.append(
            lit("\t\turl: %1, totalSpace: %2, spaceLimit: %3")
                .arg(storage->getUrl())
                .arg(storage->getTotalSpace())
                .arg(storage->getSpaceLimit()));
    NX_LOG(logMessage, cl_logDEBUG1);

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
            storage->setStorageType(
                    storageType.isEmpty()
                    ? QnLexical::serialized(QnPlatformMonitor::UnknownPartition)
                    : storageType);
            modified = true;
        }
        if (modified)
            result.insert(storage->getId(), storage);
    }

    QString logMesssage = lit("%1 Modified storages:\n").arg(Q_FUNC_INFO);
    for (const auto& storage : result.values())
        logMesssage.append(
            lit("\t\turl: %1, totalSpace: %2, spaceLimit: %3\n")
                .arg(storage->getUrl())
                .arg(storage->getTotalSpace())
                .arg(storage->getSpaceLimit()));
    NX_LOG(logMesssage, cl_logDEBUG1);

    return result.values();
}

void MediaServerProcess::initStoragesAsync(QnCommonMessageProcessor* messageProcessor)
{
    m_initStoragesAsyncPromise.reset(new nx::utils::promise<void>());
    QtConcurrent::run([messageProcessor, this]
    {
        const auto setPromiseGuardFunc = makeScopedGuard([&]() { m_initStoragesAsyncPromise->set_value(); });

        //read server's storages
        ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();
        ec2::ErrorCode rez;
        ec2::ApiStorageDataList storages;

        while ((rez = ec2Connection->getMediaServerManager(Qn::kSystemAccess)->getStoragesSync(QnUuid(), &storages)) != ec2::ErrorCode::ok)
        {
            NX_LOG( lit("QnMain::run(): Can't get storage list. Reason: %1").arg(ec2::toString(rez)), cl_logDEBUG1 );
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }

        for(const auto& storage: storages)
        {
            NX_LOG(lit("%1 Existing storage: %2, spaceLimit = %3")
                   .arg(Q_FUNC_INFO)
                   .arg(storage.url)
                   .arg(storage.spaceLimit), cl_logDEBUG2);
            messageProcessor->updateResource(storage, ec2::NotificationSource::Local);
        }

        QnStorageResourceList storagesToRemove = getSmallStorages(m_mediaServer->getStorages());

        nx::mserver_aux::UnmountedLocalStoragesFilter unmountedLocalStoragesFilter(QnAppInfo::mediaFolderName());
        auto unMountedStorages = unmountedLocalStoragesFilter.getUnmountedStorages(
                [this]()
                {
                    QnStorageResourceList result;
                    for (const auto& storage: m_mediaServer->getStorages())
                        if (!storage->isExternal())
                            result.push_back(storage);

                    return result;
                }(),
                listRecordFolders(true));

        storagesToRemove.append(unMountedStorages);

        NX_LOG(lit("%1 Found %2 storages to remove").arg(Q_FUNC_INFO).arg(storagesToRemove.size()), cl_logDEBUG2);
        for (const auto& storage: storagesToRemove)
            NX_LOG(lit("%1 Storage to remove: %2, id: %3")
                   .arg(Q_FUNC_INFO)
                   .arg(storage->getUrl())
                   .arg(storage->getId().toString()), cl_logDEBUG2);

        if (!storagesToRemove.isEmpty())
        {
            ec2::ApiIdDataList idList;
            for (const auto& value: storagesToRemove)
                idList.push_back(value->getId());
            if (ec2Connection->getMediaServerManager(Qn::kSystemAccess)->removeStoragesSync(idList) != ec2::ErrorCode::ok)
                qWarning() << "Failed to remove deprecated storage on startup. Postpone removing to the next start...";
            qnResPool->removeResources(storagesToRemove);
        }

        QnStorageResourceList modifiedStorages = createStorages(m_mediaServer);
        modifiedStorages.append(updateStorages(m_mediaServer));
        saveStorages(ec2Connection, modifiedStorages);
        for(const QnStorageResourcePtr &storage: modifiedStorages)
            messageProcessor->updateResource(storage, ec2::NotificationSource::Local);

        qnNormalStorageMan->initDone();
        qnBackupStorageMan->initDone();
    });
}

QString getComputerName()
{
#if defined(Q_OS_WIN)
    ushort tmpBuffer[1024];
    DWORD  tmpBufferSize = sizeof(tmpBuffer);
    if (GetComputerName((LPTSTR) tmpBuffer, &tmpBufferSize))
        return QString::fromUtf16(tmpBuffer);
#elif defined(Q_OS_LINUX)
    char tmpBuffer[1024];
    if (gethostname(tmpBuffer, sizeof(tmpBuffer)) == 0)
        return QString::fromUtf8(tmpBuffer);
#endif
    return QString();
}

QString getDefaultServerName()
{
    QString id = getComputerName();
    if (id.isEmpty())
        id = getMacFromPrimaryIF();
    return lit("Server %1").arg(id);
}

QnMediaServerResourcePtr MediaServerProcess::findServer(ec2::AbstractECConnectionPtr ec2Connection)
{
    ec2::ApiMediaServerDataList servers;

    while (servers.empty() && !needToStop())
    {
        ec2::ErrorCode rez = ec2Connection->getMediaServerManager(Qn::kSystemAccess)->getServersSync(&servers);
        if( rez == ec2::ErrorCode::ok )
            break;

        qDebug() << "findServer(): Call to getServers failed. Reason: " << ec2::toString(rez);
        QnSleep::msleep(1000);
    }

    for(const auto& server: servers)
    {
        if (server.id == serverGuid())
        {
            QnMediaServerResourcePtr qnServer(new QnMediaServerResource());
            fromApiToResource(server, qnServer);
            return qnServer;
        }
    }

    return QnMediaServerResourcePtr();
}

QnMediaServerResourcePtr registerServer(ec2::AbstractECConnectionPtr ec2Connection, const QnMediaServerResourcePtr &server, bool isNewServerInstance)
{
    ec2::ApiMediaServerData apiServer;
    fromResourceToApi(server, apiServer);

    ec2::ErrorCode rez = ec2Connection->getMediaServerManager(Qn::kSystemAccess)->saveSync(apiServer);
    if (rez != ec2::ErrorCode::ok)
    {
        qWarning() << "registerServer(): Call to registerServer failed. Reason: " << ec2::toString(rez);
        return QnMediaServerResourcePtr();
    }

    if (!isNewServerInstance)
        return server;

    // insert server user attributes if defined
    QString dir = MSSettings::roSettings()->value("staticDataDir", getDataDirectory()).toString();
    QFile f(closeDirPath(dir) + lit("server_settings.json"));
    if (!f.open(QFile::ReadOnly))
        return server;
    QByteArray data = f.readAll();
    ec2::ApiMediaServerUserAttributesData userAttrsData;
    if (!QJson::deserialize(data, &userAttrsData))
        return server;
    userAttrsData.serverId = server->getId();

    ec2::ApiMediaServerUserAttributesDataList attrsList;
    attrsList.push_back(userAttrsData);
    rez =  QnAppServerConnectionFactory::getConnection2()->getMediaServerManager(Qn::kSystemAccess)->saveUserAttributesSync(attrsList);
    if (rez != ec2::ErrorCode::ok)
    {
        qWarning() << "registerServer(): Call to registerServer failed. Reason: " << ec2::toString(rez);
        return QnMediaServerResourcePtr();
    }

    return server;
}

void MediaServerProcess::saveStorages(ec2::AbstractECConnectionPtr ec2Connection, const QnStorageResourceList& storages)
{
    ec2::ApiStorageDataList apiStorages;
    fromResourceListToApi(storages, apiStorages);

    ec2::ErrorCode rez;
    while((rez = ec2Connection->getMediaServerManager(Qn::kSystemAccess)->saveStoragesSync(apiStorages)) != ec2::ErrorCode::ok && !needToStop())
    {
        qWarning() << "updateStorages(): Call to change server's storages failed. Reason: " << ec2::toString(rez);
        QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
    }
}

static const int SYSTEM_USAGE_DUMP_TIMEOUT = 7*60*1000;

void MediaServerProcess::dumpSystemUsageStats()
{
    if (!qnPlatform->monitor())
        return;

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

    QnMutexLocker lk( &m_mutex );
    if( m_dumpSystemResourceUsageTaskID == 0 )  //monitoring cancelled
        return;
    m_dumpSystemResourceUsageTaskID = nx::utils::TimerManager::instance()->addTimer(
        std::bind( &MediaServerProcess::dumpSystemUsageStats, this ),
        std::chrono::milliseconds(SYSTEM_USAGE_DUMP_TIMEOUT) );
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

    QnLog::initLog(logLevel);
    const QString& dataLocation = getDataDirectory();
    const QString& logFileLocation = MSSettings::roSettings()->value( "logDir", dataLocation + QLatin1String("/log/") ).toString();
    if (!QDir().mkpath(logFileLocation))
        NX_LOG(lit("Could not create log folder: ") + logFileLocation, cl_logALWAYS);
    const QString& logFileName = logFileLocation + QLatin1String("log_file");
    if (!cl_log.create(
            logFileName,
            MSSettings::roSettings()->value( "maxLogFileSize", DEFAULT_MAX_LOG_FILE_SIZE ).toULongLong(),
            MSSettings::roSettings()->value( "logArchiveSize", DEFAULT_LOG_ARCHIVE_SIZE ).toULongLong(),
            QnLog::logLevelFromString(logLevel)))
        NX_LOG(lit("Could not create log file ") + logFileName, cl_logALWAYS);
    MSSettings::roSettings()->setValue("logFile", logFileName);
    NX_LOG(QLatin1String("================================================================================="), cl_logALWAYS);
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
        if (MSSettings::roSettings()->value(QnServer::kRemoveDbParamName).toBool())
            params.addQueryItem("cleanupDb", QString());
    }

    // TODO: #rvasilenko Actually appserverPassword is always empty. Remove?
    QString userName = settings.value("appserverLogin", helpers::kFactorySystemUser).toString();
    QString password = settings.value(APPSERVER_PASSWORD, QLatin1String("")).toString();
    QByteArray authKey = nx::ServerSetting::getAuthKey();
    QString appserverHostString = settings.value("appserverHost").toString();
    if (!authKey.isEmpty() && !isLocalAppServer(appserverHostString))
    {
        userName = serverGuid().toString();
        password = authKey;
    }

    appServerUrl.setUserName(userName);
    appServerUrl.setPassword(password);
    appServerUrl.setQuery(params);

    NX_LOG(lit("Connect to server %1").arg(appServerUrl.toString(QUrl::RemovePassword)), cl_logINFO);
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

    parseCommandLineParameters(argc, argv);

    if (!m_cmdLineArguments.configFilePath.isEmpty())
        MSSettings::initializeROSettingsFromConfFile(m_cmdLineArguments.configFilePath);
    else
        MSSettings::initializeROSettings();

    if (!m_cmdLineArguments.rwConfigFilePath.isEmpty())
        MSSettings::initializeRunTimeSettingsFromConfFile(m_cmdLineArguments.rwConfigFilePath);
    else
        MSSettings::initializeRunTimeSettings();

    addCommandLineParametersFromConfig();

    const bool isStatisticsDisabled =
        MSSettings::roSettings()->value(QnServer::kNoMonitorStatistics, false).toBool();

    m_platform.reset(new QnPlatformAbstraction(
        isStatisticsDisabled ? 0 : QnGlobalMonitor::kDefaultUpdatePeridMs));
}

void MediaServerProcess::parseCommandLineParameters(int argc, char* argv[])
{
    QnCommandLineParser commandLineParser;
    commandLineParser.addParameter(&m_cmdLineArguments.logLevel, "--log-level", NULL,
        "Supported values: none (no logging), ALWAYS, ERROR, WARNING, INFO, DEBUG, DEBUG2. Default value is "
#ifdef _DEBUG
        "DEBUG"
#else
        "INFO"
#endif
    );
    commandLineParser.addParameter(&m_cmdLineArguments.msgLogLevel, "--http-log-level", NULL,
        "Log value for http_log.log. Supported values same as above. Default is none (no logging)", "none");
    commandLineParser.addParameter(&m_cmdLineArguments.ec2TranLogLevel, "--ec2-tran-log-level", NULL,
        "Log value for ec2_tran.log. Supported values same as above. Default is none (no logging)", "none");
    commandLineParser.addParameter(&m_cmdLineArguments.permissionsLogLevel, "--permissions-log-level", NULL,
        "Log value for permissions.log. Supported values same as above. Default is none (no logging)", "none");
    commandLineParser.addParameter(&m_cmdLineArguments.rebuildArchive, "--rebuild", NULL,
        lit("Rebuild archive index. Supported values: all (high & low quality), hq (only high), lq (only low)"), "all");
    commandLineParser.addParameter(&m_cmdLineArguments.devModeKey, "--dev-mode-key", NULL, QString());
    commandLineParser.addParameter(&m_cmdLineArguments.allowedDiscoveryPeers, "--allowed-peers", NULL, QString());
    commandLineParser.addParameter(&m_cmdLineArguments.ifListFilter, "--if", NULL,
        "Strict media server network interface list (comma delimited list)");
    commandLineParser.addParameter(&m_cmdLineArguments.configFilePath, "--conf-file", NULL,
        "Path to config file. By default " + MSSettings::defaultROSettingsFilePath());
    commandLineParser.addParameter(&m_cmdLineArguments.rwConfigFilePath, "--runtime-conf-file", NULL,
        "Path to config file which is used to save some. By default " + MSSettings::defaultRunTimeSettingsFilePath());
    commandLineParser.addParameter(&m_cmdLineArguments.showVersion, "--version", NULL,
        lit("Print version info and exit"), true);
    commandLineParser.addParameter(&m_cmdLineArguments.showHelp, "--help", NULL,
        lit("This help message"), true);
    commandLineParser.addParameter(&m_cmdLineArguments.engineVersion, "--override-version", NULL,
        lit("Force the other engine version"), QString());
    commandLineParser.addParameter(&m_cmdLineArguments.enforceSocketType, "--enforce-socket", NULL,
        lit("Enforces stream socket type (TCP, UDT)"), QString());
    commandLineParser.addParameter(&m_cmdLineArguments.enforcedMediatorEndpoint, "--enforce-mediator", NULL,
        lit("Enforces mediator address"), QString());
    commandLineParser.addParameter(&m_cmdLineArguments.ipVersion, "--ip-version", NULL,
        lit("Force ip version"), QString());
    commandLineParser.addParameter(&m_cmdLineArguments.cleanupDb, "--cleanup-db", NULL,
        lit("Deletes resources with NULL ids, "
            "cleans dangling cameras' and servers' user attributes, "
            "kvpairs and resourceStatuses, also cleans and rebuilds transaction log"), true);

    commandLineParser.addParameter(&m_cmdLineArguments.moveHandlingCameras, "--move-handling-cameras", NULL,
        lit("Move handling cameras to itself, "
            "In some rare scenarios cameras can be assigned to removed server, "
            "This startup parameter force server to move these cameras to itself"), true);

    commandLineParser.parse(argc, (const char**) argv, stderr);
    if (m_cmdLineArguments.showHelp)
    {
        QTextStream stream(stdout);
        commandLineParser.print(stream);
    }
    if (m_cmdLineArguments.showVersion)
        std::cout << nx::utils::AppInfo::applicationFullVersion().toStdString() << std::endl;
}

void MediaServerProcess::addCommandLineParametersFromConfig()
{
    // move arguments from conf file / registry

    if (m_cmdLineArguments.rebuildArchive.isEmpty())
        m_cmdLineArguments.rebuildArchive = MSSettings::runTimeSettings()->value("rebuild").toString();

    if (m_cmdLineArguments.msgLogLevel.isEmpty())
        m_cmdLineArguments.msgLogLevel = MSSettings::roSettings()->value(
            nx_ms_conf::HTTP_MSG_LOG_LEVEL,
            nx_ms_conf::DEFAULT_HTTP_MSG_LOG_LEVEL).toString();

    if (m_cmdLineArguments.ec2TranLogLevel.isEmpty())
        m_cmdLineArguments.ec2TranLogLevel = MSSettings::roSettings()->value(
            nx_ms_conf::EC2_TRAN_LOG_LEVEL,
            nx_ms_conf::DEFAULT_EC2_TRAN_LOG_LEVEL).toString();
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
    QnMutexLocker lock( &m_stopMutex );
    return m_stopping;
}

void MediaServerProcess::at_databaseDumped()
{
    if (isStopping())
        return;

    nx::mserver_aux::savePersistentDataBeforeDbRestore(
                qnResPool->getAdministrator(),
                m_mediaServer,
                nx::mserver_aux::createServerSettingsProxy().get()
            ).saveToSettings(MSSettings::roSettings());
    restartServer(500);
}

void MediaServerProcess::at_systemIdentityTimeChanged(qint64 value, const QnUuid& sender)
{
    if (isStopping())
        return;

    nx::ServerSetting::setSysIdTime(value);
    if (sender != qnCommon->moduleGUID())
    {
        MSSettings::roSettings()->setValue(QnServer::kRemoveDbParamName, "1");
        // If system Id has been changed, reset 'database restore time' variable
        nx::mserver_aux::savePersistentDataBeforeDbRestore(
                    qnResPool->getAdministrator(),
                    m_mediaServer,
                    nx::mserver_aux::createServerSettingsProxy().get()
                ).saveToSettings(MSSettings::roSettings());
        restartServer(0);
    }
}

void MediaServerProcess::stopSync()
{
    qWarning()<<"Stopping server";
    NX_LOG( lit("Stopping server"), cl_logALWAYS );

    const int kStopTimeoutMs = 100 * 1000;

    if (serviceMainInstance) {
        {
            QnMutexLocker lock( &m_stopMutex );
            m_stopping = true;
        }
        serviceMainInstance->pleaseStop();
        serviceMainInstance->quit();
        if (!serviceMainInstance->wait(kStopTimeoutMs))
        {
            serviceMainInstance->terminate();
            serviceMainInstance->wait();
        }
        serviceMainInstance = 0;
    }
    qApp->quit();
}

void MediaServerProcess::stopAsync()
{
    QTimer::singleShot(0, this, SLOT(stopSync()));
}


int MediaServerProcess::getTcpPort() const
{
    return m_universalTcpListener ? m_universalTcpListener->getPort() : 0;
}

void MediaServerProcess::stopObjects()
{
    qWarning() << "QnMain::stopObjects() called";

    qnBackupStorageMan->scheduleSync()->stop();
    NX_LOG("QnScheduleSync::stop() done", cl_logINFO);

    qnNormalStorageMan->cancelRebuildCatalogAsync();
    qnBackupStorageMan->cancelRebuildCatalogAsync();

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
    // migration from old version. move setting from registry to the DB
    static const QString DV_PROPERTY = QLatin1String("disabledVendors");
    QString disabledVendors = MSSettings::roSettings()->value(DV_PROPERTY).toString();
    if (!disabledVendors.isNull())
    {
        qnGlobalSettings->setDisabledVendors(disabledVendors);
        MSSettings::roSettings()->remove(DV_PROPERTY);
    }
}

void MediaServerProcess::updateAllowCameraCHangesIfNeed()
{
    static const QString DV_PROPERTY = QLatin1String("cameraSettingsOptimization");

    QString allowCameraChanges = MSSettings::roSettings()->value(DV_PROPERTY).toString();
    if (!allowCameraChanges.isEmpty())
    {
        qnGlobalSettings->setCameraSettingsOptimizationEnabled(allowCameraChanges.toLower() == lit("yes") || allowCameraChanges.toLower() == lit("true") || allowCameraChanges == lit("1"));
        MSSettings::roSettings()->setValue(DV_PROPERTY, "");
    }
}

template< typename Container>
QString containerToQString( const Container& container )
{
    QStringList list;
    for (const auto& it : container)
        list << it.toString();

    return list.join( lit(", ") );
}

void MediaServerProcess::updateAddressesList()
{
    if (isStopping())
        return;

    ec2::ApiMediaServerData prevValue;
    fromResourceToApi(m_mediaServer, prevValue);


    QList<SocketAddress> serverAddresses;

    const auto port = m_universalTcpListener->getPort();
    for (const auto& host: allLocalAddresses())
        serverAddresses << SocketAddress(host.toString(), port);

    for (const auto& host : m_forwardedAddresses )
        serverAddresses << SocketAddress(host.first, host.second);

    if (!m_ipDiscovery->publicIP().isNull())
        serverAddresses << SocketAddress(m_ipDiscovery->publicIP().toString(), port);

    m_mediaServer->setNetAddrList(serverAddresses);
    NX_LOGX(lit("Update mediaserver addresses: %1")
            .arg(containerToQString(serverAddresses)), cl_logDEBUG1);

    const QUrl defaultUrl(m_mediaServer->getApiUrl());
    const SocketAddress defaultAddress(defaultUrl.host(), defaultUrl.port());
    if (std::find(serverAddresses.begin(), serverAddresses.end(),
                  defaultAddress) == serverAddresses.end())
    {
        SocketAddress newAddress;
        if (!serverAddresses.isEmpty())
            newAddress = serverAddresses.front();

        m_mediaServer->setPrimaryAddress(newAddress);
    }

    ec2::ApiMediaServerData server;
    fromResourceToApi(m_mediaServer, server);
    if (server != prevValue)
        QnAppServerConnectionFactory::getConnection2()->getMediaServerManager(Qn::kSystemAccess)->save(server, this, &MediaServerProcess::at_serverSaved);

    nx::network::SocketGlobals::addressPublisher().updateAddresses(std::list<SocketAddress>(
        serverAddresses.begin(), serverAddresses.end()));
}

void MediaServerProcess::loadResourcesFromECS(QnCommonMessageProcessor* messageProcessor)
{
    ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();

    ec2::ErrorCode rez;

    {
        //reading servers list
        ec2::ApiMediaServerDataList mediaServerList;
        while( ec2Connection->getMediaServerManager(Qn::kSystemAccess)->getServersSync(&mediaServerList) != ec2::ErrorCode::ok )
        {
            NX_LOG( lit("QnMain::run(). Can't get servers."), cl_logERROR );
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }

        ec2::ApiDiscoveryDataList discoveryDataList;
        while( ec2Connection->getDiscoveryManager(Qn::kSystemAccess)->getDiscoveryDataSync(&discoveryDataList) != ec2::ErrorCode::ok )
        {
            NX_LOG( lit("QnMain::run(). Can't get discovery data."), cl_logERROR );
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }

        QMultiHash<QnUuid, QUrl> additionalAddressesById;
        QMultiHash<QnUuid, QUrl> ignoredAddressesById;
        for (const ec2::ApiDiscoveryData &data: discoveryDataList)
        {
            additionalAddressesById.insert(data.id, data.url);
            if (data.ignore)
                ignoredAddressesById.insert(data.id, data.url);
        }

        for(const auto &mediaServer: mediaServerList)
        {
            const auto defaultPort = QUrl(mediaServer.url).port();
            QList<SocketAddress> addresses;
            ec2::deserializeNetAddrList(mediaServer.networkAddresses, addresses, defaultPort);

            QList<QUrl> additionalAddresses = additionalAddressesById.values(mediaServer.id);
            for (auto it = additionalAddresses.begin(); it != additionalAddresses.end(); /* no inc */) {
                const SocketAddress addr(it->host(), it->port(defaultPort));
                if (addresses.contains(addr))
                    it = additionalAddresses.erase(it);
                else
                    ++it;
            }
            qnServerAdditionalAddressesDictionary->setAdditionalUrls(mediaServer.id, additionalAddresses);
            qnServerAdditionalAddressesDictionary->setIgnoredUrls(mediaServer.id, ignoredAddressesById.values(mediaServer.id));
            messageProcessor->updateResource(mediaServer, ec2::NotificationSource::Local);
        }
        do {
            if (needToStop())
                return;
        } while (ec2Connection->getResourceManager(Qn::kSystemAccess)->setResourceStatusSync(m_mediaServer->getId(), Qn::Online) != ec2::ErrorCode::ok);


        // read resource status
        ec2::ApiResourceStatusDataList statusList;
        while ((rez = ec2Connection->getResourceManager(Qn::kSystemAccess)->getStatusListSync(QnUuid(), &statusList)) != ec2::ErrorCode::ok)
        {
            NX_LOG( lit("QnMain::run(): Can't get properties dictionary. Reason: %1").arg(ec2::toString(rez)), cl_logDEBUG1 );
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }
        messageProcessor->resetStatusList( statusList );

        //reading server attributes
        ec2::ApiMediaServerUserAttributesDataList mediaServerUserAttributesList;
        while ((rez = ec2Connection->getMediaServerManager(Qn::kSystemAccess)->getUserAttributesSync(QnUuid(), &mediaServerUserAttributesList)) != ec2::ErrorCode::ok)
        {
            NX_LOG( lit("QnMain::run(): Can't get server user attributes list. Reason: %1").arg(ec2::toString(rez)), cl_logDEBUG1 );
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }
        messageProcessor->resetServerUserAttributesList( mediaServerUserAttributesList );

    }


    {
        // read camera list
        ec2::ApiCameraDataList cameras;
        while ((rez = ec2Connection->getCameraManager(Qn::kSystemAccess)->getCamerasSync(&cameras)) != ec2::ErrorCode::ok)
        {
            NX_LOG(lit("QnMain::run(): Can't get cameras. Reason: %1").arg(ec2::toString(rez)), cl_logDEBUG1);
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }

        //reading camera attributes
        ec2::ApiCameraAttributesDataList cameraUserAttributesList;
        while ((rez = ec2Connection->getCameraManager(Qn::kSystemAccess)->getUserAttributesSync(&cameraUserAttributesList)) != ec2::ErrorCode::ok)
        {
            NX_LOG(lit("QnMain::run(): Can't get camera user attributes list. Reason: %1").arg(ec2::toString(rez)), cl_logDEBUG1);
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }
        messageProcessor->resetCameraUserAttributesList(cameraUserAttributesList);

        // read properties dictionary
        ec2::ApiResourceParamWithRefDataList kvPairs;
        while ((rez = ec2Connection->getResourceManager(Qn::kSystemAccess)->getKvPairsSync(QnUuid(), &kvPairs)) != ec2::ErrorCode::ok)
        {
            NX_LOG(lit("QnMain::run(): Can't get properties dictionary. Reason: %1").arg(ec2::toString(rez)), cl_logDEBUG1);
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }
        messageProcessor->resetPropertyList(kvPairs);

        /* Properties and attributes must be read before processing cameras because of getAuth() method */
        QnManualCameraInfoMap manualCameras;
        for (const auto &camera : cameras)
        {
            messageProcessor->updateResource(camera, ec2::NotificationSource::Local);
            if (camera.manuallyAdded)
            {
                QnResourceTypePtr resType = qnResTypePool->getResourceType(camera.typeId);
                if (resType)
                {
                    const auto auth = QnNetworkResource::getResourceAuth(camera.id, camera.typeId);
                    manualCameras.insert(camera.url,
                        QnManualCameraInfo(QUrl(camera.url), auth, resType->getName()));
                }
                else
                {
                    NX_ASSERT(false, lm("No resourse type in the pool %1").str(camera.typeId));
                }
            }
        }
        QnResourceDiscoveryManager::instance()->registerManualCameras(manualCameras);
    }

    {
        ec2::ApiServerFootageDataList serverFootageData;
        while (( rez = ec2Connection->getCameraManager(Qn::kSystemAccess)->getServerFootageDataSync(&serverFootageData)) != ec2::ErrorCode::ok)
        {
            qDebug() << "QnMain::run(): Can't get cameras history. Reason: " << ec2::toString(rez);
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }
        qnCameraHistoryPool->resetServerFootageData(serverFootageData);
        qnCameraHistoryPool->setHistoryCheckDelay(1000);
    }

    {
        //loading users
        ec2::ApiUserDataList users;
        while(( rez = ec2Connection->getUserManager(Qn::kSystemAccess)->getUsersSync(&users))  != ec2::ErrorCode::ok)
        {
            qDebug() << "QnMain::run(): Can't get users. Reason: " << ec2::toString(rez);
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }

        for(const auto &user: users)
            messageProcessor->updateResource(user, ec2::NotificationSource::Local);
    }

    {
        //loading videowalls
        ec2::ApiVideowallDataList videowalls;
        while(( rez = ec2Connection->getVideowallManager(Qn::kSystemAccess)->getVideowallsSync(&videowalls))  != ec2::ErrorCode::ok)
        {
            qDebug() << "QnMain::run(): Can't get videowalls. Reason: " << ec2::toString(rez);
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }

        for (const ec2::ApiVideowallData& videowall: videowalls)
            messageProcessor->updateResource(videowall, ec2::NotificationSource::Local);
    }

    {
        //loading layouts
        ec2::ApiLayoutDataList layouts;
        while(( rez = ec2Connection->getLayoutManager(Qn::kSystemAccess)->getLayoutsSync(&layouts))  != ec2::ErrorCode::ok)
        {
            qDebug() << "QnMain::run(): Can't get layouts. Reason: " << ec2::toString(rez);
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }

        for(const auto &layout: layouts)
            messageProcessor->updateResource(layout, ec2::NotificationSource::Local);
    }

    {
        //loading webpages
        ec2::ApiWebPageDataList webpages;
        while ((rez = ec2Connection->getWebPageManager(Qn::kSystemAccess)->getWebPagesSync(&webpages)) != ec2::ErrorCode::ok)
        {
            qDebug() << "QnMain::run(): Can't get webpages. Reason: " << ec2::toString(rez);
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }

        for (const auto &webpage : webpages)
            messageProcessor->updateResource(webpage, ec2::NotificationSource::Local);
    }

    {
        //loading accessible resources
        ec2::ApiAccessRightsDataList accessRights;
        while ((rez = ec2Connection->getUserManager(Qn::kSystemAccess)->getAccessRightsSync(&accessRights)) != ec2::ErrorCode::ok)
        {
            qDebug() << "QnMain::run(): Can't get accessRights. Reason: " << ec2::toString(rez);
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }
        messageProcessor->resetAccessRights(accessRights);
    }

    {
        //loading user roles
        ec2::ApiUserRoleDataList userRoles;
        while ((rez = ec2Connection->getUserManager(Qn::kSystemAccess)->getUserRolesSync(&userRoles)) != ec2::ErrorCode::ok)
        {
            qDebug() << "QnMain::run(): Can't get roles. Reason: " << ec2::toString(rez);
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }
        messageProcessor->resetUserRoles(userRoles);
    }

    {
        //loading business rules
        QnBusinessEventRuleList rules;
        while( (rez = ec2Connection->getBusinessEventManager(Qn::kSystemAccess)->getBusinessRulesSync(&rules)) != ec2::ErrorCode::ok )
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
        while( (rez = ec2Connection->getLicenseManager(Qn::kSystemAccess)->getLicensesSync(&licenses)) != ec2::ErrorCode::ok )
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

    // Start receiving local notifications
    auto connection = QnAppServerConnectionFactory::getConnection2();
    auto processor = dynamic_cast<QnServerMessageProcessor*> (QnServerMessageProcessor::instance());
    processor->startReceivingLocalNotifications(connection);
}

void MediaServerProcess::saveServerInfo(const QnMediaServerResourcePtr& server)
{
    const auto hwInfo = HardwareInformation::instance();
    server->setProperty(Qn::CPU_ARCHITECTURE, hwInfo.cpuArchitecture);
    server->setProperty(Qn::CPU_MODEL_NAME, hwInfo.cpuModelName);
    server->setProperty(Qn::PHYSICAL_MEMORY, QString::number(hwInfo.physicalMemory));

    server->setProperty(Qn::PRODUCT_NAME_SHORT, QnAppInfo::productNameShort());
    server->setProperty(Qn::FULL_VERSION, nx::utils::AppInfo::applicationFullVersion());
    server->setProperty(Qn::BETA, QString::number(QnAppInfo::beta() ? 1 : 0));
    server->setProperty(Qn::PUBLIC_IP, m_ipDiscovery->publicIP().toString());
    server->setProperty(Qn::SYSTEM_RUNTIME, QnSystemInformation::currentSystemRuntime());

    propertyDictionary->saveParams(server->getId());
}

void MediaServerProcess::at_updatePublicAddress(const QHostAddress& publicIP)
{
    if (isStopping())
        return;

    QnPeerRuntimeInfo localInfo = QnRuntimeInfoManager::instance()->localInfo();
    localInfo.data.publicIP = publicIP.toString();
    QnRuntimeInfoManager::instance()->updateLocalItem(localInfo);

    QnMediaServerResourcePtr server = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
    if (server)
    {
        Qn::ServerFlags serverFlags = server->getServerFlags();
        if (publicIP.isNull())
            serverFlags &= ~Qn::SF_HasPublicIP;
        else
            serverFlags |= Qn::SF_HasPublicIP;
        if (serverFlags != server->getServerFlags())
        {
            server->setServerFlags(serverFlags);
            ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();

            ec2::ApiMediaServerData apiServer;
            fromResourceToApi(server, apiServer);
            ec2Connection->getMediaServerManager(Qn::kSystemAccess)->save(apiServer, this, [] {});
        }

        if (server->setProperty(Qn::PUBLIC_IP, publicIP.toString(), QnResource::NO_ALLOW_EMPTY))
            propertyDictionary->saveParams(server->getId());

        updateAddressesList(); //< update interface list to add/remove publicIP
    }
}

void MediaServerProcess::at_portMappingChanged(QString address)
{
    if (isStopping())
        return;

    SocketAddress mappedAddress(address);
    if (mappedAddress.port)
    {
        NX_LOGX(lit("New external address %1 has been mapped")
                .arg(address), cl_logALWAYS);

        auto it = m_forwardedAddresses.emplace(mappedAddress.address, 0).first;
        if (it->second != mappedAddress.port)
        {
            it->second = mappedAddress.port;
            updateAddressesList();
        }
    }
    else
    {
        const auto oldIp = m_forwardedAddresses.find(mappedAddress.address);
        if (oldIp != m_forwardedAddresses.end())
        {
            NX_LOGX(lit("External address %1:%2 has been unmapped")
                   .arg(oldIp->first.toString()).arg(oldIp->second), cl_logALWAYS);

            m_forwardedAddresses.erase(oldIp);
            updateAddressesList();
        }
    }
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

void MediaServerProcess::at_storageManager_rebuildFinished(QnSystemHealth::MessageType msgType) {
    if (isStopping())
        return;
    qnBusinessRuleConnector->at_archiveRebuildFinished(m_mediaServer, msgType);
}

void MediaServerProcess::at_archiveBackupFinished(
    qint64                      backedUpToMs,
    QnBusiness::EventReason     code
)
{
    if (isStopping())
        return;

    qnBusinessRuleConnector->at_archiveBackupFinished(
        m_mediaServer,
        qnSyncTime->currentUSecsSinceEpoch(),
        code,
        QString::number(backedUpToMs)
    );
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

void MediaServerProcess::registerRestHandlers(
    CloudManagerGroup* cloudManagerGroup)
{
    const auto welcomePage = lit("/static/index.html");
    QnRestProcessorPool::instance()->registerRedirectRule(lit(""), welcomePage);
    QnRestProcessorPool::instance()->registerRedirectRule(lit("/"), welcomePage);
    QnRestProcessorPool::instance()->registerRedirectRule(lit("/static"), welcomePage);
    QnRestProcessorPool::instance()->registerRedirectRule(lit("/static/"), welcomePage);

    auto reg =
        [](const QString& path, QnRestRequestHandler* handler,
            Qn::GlobalPermission permissions = Qn::NoGlobalPermissions)
        {
            QnRestProcessorPool::instance()->registerHandler(path, handler, permissions);
        };

    // TODO: When supported by apidoctool, the comment to these constants should be parsed.
    const auto kAdmin = Qn::GlobalAdminPermission;

    reg("api/storageStatus", new QnStorageStatusRestHandler());
    reg("api/storageSpace", new QnStorageSpaceRestHandler());
    reg("api/statistics", new QnStatisticsRestHandler());
    reg("api/getCameraParam", new QnCameraSettingsRestHandler());
    reg("api/setCameraParam", new QnCameraSettingsRestHandler());
    reg("api/manualCamera", new QnManualCameraAdditionRestHandler());
    reg("api/ptz", new QnPtzRestHandler());
    reg("api/image", new QnImageRestHandler()); //< deprecated
    reg("api/createEvent", new QnExternalBusinessEventRestHandler());
    reg("api/gettime", new QnTimeRestHandler());
    reg("api/getTimeZones", new QnGetTimeZonesRestHandler());
    reg("api/getNonce", new QnGetNonceRestHandler());
    reg("api/cookieLogin", new QnCookieLoginRestHandler());
    reg("api/cookieLogout", new QnCookieLogoutRestHandler());
    reg("api/getCurrentUser", new QnCurrentUserRestHandler());
    reg("api/activateLicense", new QnActivateLicenseRestHandler());
    reg("api/testEmailSettings", new QnTestEmailSettingsHandler());
    reg("api/getHardwareInfo", new QnGetHardwareInfoHandler());
    reg("api/testLdapSettings", new QnTestLdapSettingsHandler());
    reg("api/ping", new QnPingRestHandler());
    reg("api/recStats", new QnRecordingStatsRestHandler());
    reg("api/auditLog", new QnAuditLogRestHandler(), kAdmin);
    reg("api/checkDiscovery", new QnCanAcceptCameraRestHandler());
    reg("api/pingSystem", new QnPingSystemRestHandler());
    reg("api/rebuildArchive", new QnRebuildArchiveRestHandler());
    reg("api/backupControl", new QnBackupControlRestHandler());
    reg("api/events", new QnBusinessEventLogRestHandler(), Qn::GlobalViewLogsPermission); //< deprecated
    reg("api/getEvents", new QnBusinessLog2RestHandler(), Qn::GlobalViewLogsPermission); //< new version
    reg("api/showLog", new QnLogRestHandler());
    reg("api/getSystemId", new QnGetSystemIdRestHandler());
    reg("api/doCameraDiagnosticsStep", new QnCameraDiagnosticsRestHandler());
    reg("api/installUpdate", new QnUpdateRestHandler());
    reg("api/installUpdateUnauthenticated", new QnUpdateUnauthenticatedRestHandler());
    reg("api/restart", new QnRestartRestHandler(), kAdmin);
    reg("api/connect", new QnOldClientConnectRestHandler());
    reg("api/moduleInformation", new QnModuleInformationRestHandler());
    reg("api/iflist", new QnIfListRestHandler());
    reg("api/aggregator", new QnJsonAggregatorRestHandler());
    reg("api/ifconfig", new QnIfConfigRestHandler(), kAdmin);

    reg("api/settime", new QnSetTimeRestHandler(), kAdmin); //< deprecated
    reg("api/setTime", new QnSetTimeRestHandler(), kAdmin); //< new version

    reg("api/moduleInformationAuthenticated", new QnModuleInformationRestHandler());
    reg("api/configure", new QnConfigureRestHandler(), kAdmin);
    reg("api/detachFromCloud", new QnDetachFromCloudRestHandler(&cloudManagerGroup->connectionManager), kAdmin);
    reg("api/restoreState", new QnRestoreStateRestHandler(), kAdmin);
    reg("api/setupLocalSystem", new QnSetupLocalSystemRestHandler(), kAdmin);
    reg("api/setupCloudSystem", new QnSetupCloudSystemRestHandler(cloudManagerGroup), kAdmin);
    reg("api/mergeSystems", new QnMergeSystemsRestHandler(), kAdmin);
    reg("api/backupDatabase", new QnBackupDbRestHandler());
    reg("api/discoveredPeers", new QnDiscoveredPeersRestHandler());
    reg("api/logLevel", new QnLogLevelRestHandler());
    reg("api/execute", new QnExecScript(), kAdmin);
    reg("api/scriptList", new QnScriptListRestHandler(), kAdmin);
    reg("api/systemSettings", new QnSystemSettingsHandler());

    reg("api/transmitAudio", new QnAudioTransmissionRestHandler());

    // TODO: Introduce constants for API methods registered here, also use them in
    // media_server_connection.cpp. Get rid of static/global urlPath passed to some handler ctors.

    reg("api/RecordedTimePeriods", new QnRecordedChunksRestHandler()); //< deprecated
    reg("ec2/recordedTimePeriods", new QnMultiserverChunksRestHandler("ec2/recordedTimePeriods")); //< new version

    reg("ec2/cameraHistory", new QnCameraHistoryRestHandler());
    reg("ec2/bookmarks", new QnMultiserverBookmarksRestHandler("ec2/bookmarks"));
    reg("api/mergeLdapUsers", new QnMergeLdapUsersRestHandler());
    reg("ec2/updateInformation", new QnUpdateInformationRestHandler());

    reg("ec2/cameraThumbnail", new QnMultiserverThumbnailRestHandler("ec2/cameraThumbnail"));
    reg("ec2/statistics", new QnMultiserverStatisticsRestHandler("ec2/statistics"));

    reg("api/saveCloudSystemCredentials", new QnSaveCloudSystemCredentialsHandler(cloudManagerGroup));

    reg("favicon.ico", new QnFavIconRestHandler());
    reg("api/dev-mode-key", new QnCrashServerHandler(), kAdmin);

    reg("api/startLiteClient", new QnStartLiteClientRestHandler());

    #ifdef _DEBUG
        reg("api/debugEvent", new QnDebugEventsRestHandler());
    #endif

    reg("ec2/runtimeInfo", new QnRuntimeInfoRestHandler());

}

bool MediaServerProcess::initTcpListener(
    CloudManagerGroup* const cloudManagerGroup)
{
    m_httpModManager.reset( new nx_http::HttpModManager() );
    m_autoRequestForwarder.reset( new QnAutoRequestForwarder() );
    m_autoRequestForwarder->addPathToIgnore(lit("/ec2/*"));
    m_httpModManager->addCustomRequestMod( std::bind(
        &QnAutoRequestForwarder::processRequest,
        m_autoRequestForwarder.get(),
        std::placeholders::_1 ) );

    registerRestHandlers(cloudManagerGroup);

    const int rtspPort = MSSettings::roSettings()->value(nx_ms_conf::SERVER_PORT, nx_ms_conf::DEFAULT_SERVER_PORT).toInt();
#ifdef ENABLE_ACTI
    QnActiResource::setEventPort(rtspPort);
    QnRestProcessorPool::instance()->registerHandler("api/camera_event", new QnActiEventRestHandler());  //used to receive event from acti camera. TODO: remove this from api
#endif

    // Accept SSL connections in all cases as it is always in use by cloud modules and old clients,
    // config value only affects server preference listed in moduleInformation.
    bool acceptSslConnections = true;
    int maxConnections = MSSettings::roSettings()->value("maxConnections", QnTcpListener::DEFAULT_MAX_CONNECTIONS).toInt();
    NX_LOG(QString("Using maxConnections = %1.").arg(maxConnections), cl_logINFO);

    m_universalTcpListener = new QnUniversalTcpListener(
        cloudManagerGroup->connectionManager,
        QHostAddress::Any,
        rtspPort,
        maxConnections,
        acceptSslConnections );
    if( !m_universalTcpListener->bindToLocalAddress() )
        return false;
    m_universalTcpListener->setDefaultPage("/static/index.html");

    // Server return code 403 (forbidden) instead of 401 if user isn't authorized for requests starting with 'web' path
    m_universalTcpListener->setPathIgnorePrefix("web/");
    QnAuthHelper::instance()->restrictionList()->deny(lit("/web/*"), AuthMethod::http);

    AuthMethod::Values methods = (AuthMethod::Values)(AuthMethod::cookie | AuthMethod::urlQueryParam | AuthMethod::tempUrlQueryParam);
    QnUniversalRequestProcessor::setUnauthorizedPageBody(QnFileConnectionProcessor::readStaticFile("static/login.html"), methods);
    m_universalTcpListener->addHandler<QnRtspConnectionProcessor>("RTSP", "*");
    m_universalTcpListener->addHandler<QnRestConnectionProcessor>("HTTP", "api");
    m_universalTcpListener->addHandler<QnRestConnectionProcessor>("HTTP", "ec2");
    m_universalTcpListener->addHandler<QnFileConnectionProcessor>("HTTP", "static");
    m_universalTcpListener->addHandler<QnCrossdomainConnectionProcessor>("HTTP", "crossdomain.xml");
    m_universalTcpListener->addHandler<QnProgressiveDownloadingConsumer>("HTTP", "media");
    m_universalTcpListener->addHandler<QnIOMonitorConnectionProcessor>("HTTP", "api/iomonitor");

    nx_hls::QnHttpLiveStreamingProcessor::setMinPlayListSizeToStartStreaming(
        MSSettings::roSettings()->value(
        nx_ms_conf::HLS_PLAYLIST_PRE_FILL_CHUNKS,
        nx_ms_conf::DEFAULT_HLS_PLAYLIST_PRE_FILL_CHUNKS).toInt());
    m_universalTcpListener->addHandler<nx_hls::QnHttpLiveStreamingProcessor>("HTTP", "hls");
    //m_universalTcpListener->addHandler<QnDefaultTcpConnectionProcessor>("HTTP", "*");
    //m_universalTcpListener->addHandler<QnProxyConnectionProcessor>("HTTP", "*");

    m_universalTcpListener->addHandler<QnProxyConnectionProcessor>("*", "proxy");
    //m_universalTcpListener->addHandler<QnProxyReceiverConnection>("PROXY", "*");
    m_universalTcpListener->addHandler<QnProxyReceiverConnection>("HTTP", "proxy-reverse");
    m_universalTcpListener->addHandler<QnAudioProxyReceiver>("HTTP", "proxy-2wayaudio");

    if( !MSSettings::roSettings()->value("authenticationEnabled", "true").toBool() )
        m_universalTcpListener->disableAuth();

#ifdef ENABLE_DESKTOP_CAMERA
    m_universalTcpListener->addHandler<QnDesktopCameraRegistrator>("HTTP", "desktop_camera");
#endif   //ENABLE_DESKTOP_CAMERA

    return true;
}

std::unique_ptr<nx_upnp::PortMapper> MediaServerProcess::initializeUpnpPortMapper()
{
    auto mapper = std::make_unique<nx_upnp::PortMapper>(/*isEnabled*/ false);
    auto updateEnabled =
        [mapper = mapper.get()]()
        {
            const auto isCloudSystem = !qnGlobalSettings->cloudSystemId().isEmpty();
            mapper->setIsEnabled(isCloudSystem && qnGlobalSettings->isUpnpPortMappingEnabled());
        };

    connect(qnGlobalSettings, &QnGlobalSettings::upnpPortMappingEnabledChanged, updateEnabled);
    connect(qnGlobalSettings, &QnGlobalSettings::cloudSettingsChanged, updateEnabled);
    updateEnabled();

    mapper->enableMapping(
        m_mediaServer->getPort(), nx_upnp::PortMapper::Protocol::TCP,
        [this](SocketAddress address)
        {
            const auto result = QMetaObject::invokeMethod(
                this, "at_portMappingChanged", Qt::AutoConnection,
                Q_ARG(QString, address.toString()));

            NX_ASSERT(result, "Could not call at_portMappingChanged(...)");
        });

    return mapper;
}

Qn::ServerFlags MediaServerProcess::calcServerFlags()
{
    Qn::ServerFlags serverFlags = Qn::SF_None; // TODO: #Elric #EC2 type safety has just walked out of the window.

#ifdef EDGE_SERVER
    serverFlags |= Qn::SF_Edge;
#endif
    if (QnAppInfo::isBpi())
    {
        serverFlags |= Qn::SF_IfListCtrl | Qn::SF_timeCtrl;
        serverFlags |= Qn::SF_HasLiteClient;
    }

    bool compatibilityMode = m_cmdLineArguments.devModeKey == lit("razrazraz");
    if (compatibilityMode) // check compatibilityMode here for testing purpose
    {
        serverFlags |= Qn::SF_HasLiteClient;
    }

#ifdef __arm__
    serverFlags |= Qn::SF_ArmServer;

    struct stat st;
    memset(&st, 0, sizeof(st));
    const bool hddPresent =
        ::stat("/dev/sda", &st) == 0 ||
        ::stat("/dev/sdb", &st) == 0 ||
        ::stat("/dev/sdc", &st) == 0 ||
        ::stat("/dev/sdd", &st) == 0;
    if (hddPresent)
        serverFlags |= Qn::SF_Has_HDD;
#else
    serverFlags |= Qn::SF_Has_HDD;
#endif

    if (!(serverFlags & (Qn::SF_ArmServer | Qn::SF_Edge)))
        serverFlags |= Qn::SF_SupportsTranscoding;

    const QString appserverHostString = MSSettings::roSettings()->value("appserverHost").toString();
    bool isLocal = isLocalAppServer(appserverHostString);
    if (!isLocal)
        serverFlags |= Qn::SF_RemoteEC;

    initPublicIpDiscovery();
    if (!m_ipDiscovery->publicIP().isNull())
        serverFlags |= Qn::SF_HasPublicIP;

    return serverFlags;
}

void MediaServerProcess::initPublicIpDiscovery()
{
    m_ipDiscovery.reset(new QnPublicIPDiscovery(
        MSSettings::roSettings()->value(nx_ms_conf::PUBLIC_IP_SERVERS).toString().split(";", QString::SkipEmptyParts)));

    if (MSSettings::roSettings()->value("publicIPEnabled").isNull())
        MSSettings::roSettings()->setValue("publicIPEnabled", 1);

    int publicIPEnabled = MSSettings::roSettings()->value("publicIPEnabled").toInt();
    if (publicIPEnabled == 0)
        return; // disabled
    else if (publicIPEnabled > 1)
    {
        auto staticIp = MSSettings::roSettings()->value("staticPublicIP").toString();
        at_updatePublicAddress(QHostAddress(staticIp)); // manually added
        return;
    }
    m_ipDiscovery->update();
    m_ipDiscovery->waitForFinished();
    at_updatePublicAddress(m_ipDiscovery->publicIP());

    m_updatePiblicIpTimer.reset(new QTimer());
    connect(m_updatePiblicIpTimer.get(), &QTimer::timeout, m_ipDiscovery.get(), &QnPublicIPDiscovery::update);
    connect(m_ipDiscovery.get(), &QnPublicIPDiscovery::found, this, &MediaServerProcess::at_updatePublicAddress);
    m_updatePiblicIpTimer->start(kPublicIpUpdateTimeoutMs);
}

void MediaServerProcess::setHardwareGuidList(const QVector<QString>& hardwareGuidList)
{
    m_hardwareGuidList = hardwareGuidList;
}

void MediaServerProcess::resetSystemState(CloudConnectionManager& cloudConnectionManager)
{
    for (;;)
    {
        if (!cloudConnectionManager.resetCloudData())
        {
            qWarning() << "Error while clearing cloud information. Trying again...";
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            continue;
        }

        if (!resetSystemToStateNew())
        {
            qWarning() << "Error while resetting system to state \"new \". Trying again...";
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            continue;
        }

        break;
    }
}

namespace {

static const char* const kOnExitScriptName = "mediaserver_on_exit";

} // namespace

void MediaServerProcess::performActionsOnExit()
{
    // Call the script if it exists.

    QString fileName = getDataDirectory() + "/scripts/" + kOnExitScriptName;
    if (!QFile::exists(fileName))
    {
        NX_LOG(lit("Script '%1' is missing at the server").arg(fileName), cl_logDEBUG2);
        return;
    }

    // Currently, no args are needed, hence the empty list.
    QStringList args{};

    NX_LOG(lit("Calling the script: %1 %2").arg(fileName).arg(args.join(" ")), cl_logDEBUG2);
    if (!QProcess::startDetached(fileName, args))
    {
        NX_LOG(lit("Unable to start script '%1' because of a system error").arg(kOnExitScriptName),
            cl_logDEBUG2);
    }
}

void MediaServerProcess::moveHandlingCameras()
{
    QSet<QnUuid> servers;
    for (const auto& server : qnResPool->getResources<QnMediaServerResource>())
        servers << server->getId();
    ec2::ApiCameraDataList camerasToUpdate;
    for (const auto& camera: qnResPool->getAllCameras(/*all*/ QnResourcePtr()))
    {
        if (!servers.contains(camera->getParentId()))
        {
            ec2::ApiCameraData apiCameraData;
            fromResourceToApi(camera, apiCameraData);
            apiCameraData.parentId = qnCommon->moduleGUID(); //< move camera
            camerasToUpdate.push_back(apiCameraData);
        }
    }

    auto errCode = QnAppServerConnectionFactory::getConnection2()
        ->getCameraManager(Qn::kSystemAccess)
        ->addCamerasSync(camerasToUpdate);

    if (errCode != ec2::ErrorCode::ok)
        qWarning() << "Failed to move handling cameras due to database error. errCode=" << toString(errCode);
}

void MediaServerProcess::updateAllowedInterfaces()
{
    // check registry
    QString ifList = MSSettings::roSettings()->value(lit("if")).toString();
    // check startup parameter
    if (ifList.isEmpty())
        ifList = m_cmdLineArguments.ifListFilter;

    QList<QHostAddress> allowedInterfaces;
    for (const QString& s : ifList.split(QLatin1Char(';'), QString::SkipEmptyParts))
        allowedInterfaces << QHostAddress(s);

    if (!allowedInterfaces.isEmpty())
        qWarning() << "Using net IF filter:" << allowedInterfaces;
    setInterfaceListFilter(allowedInterfaces);
}

void MediaServerProcess::run()
{

    if (m_needInitHardwareId && !initHardwareId())
        return;

    updateAllowedInterfaces();

    if (!m_cmdLineArguments.enforceSocketType.isEmpty())
        SocketFactory::enforceStreamSocketType(m_cmdLineArguments.enforceSocketType);
    auto ipVersion = m_cmdLineArguments.ipVersion;
    if (ipVersion.isEmpty())
        ipVersion = MSSettings::roSettings()->value(QLatin1String("ipVersion")).toString();

    SocketFactory::setIpVersion(ipVersion);

    std::unique_ptr<QnMediaServerModule> module(new QnMediaServerModule(m_cmdLineArguments.enforcedMediatorEndpoint));

    if (!m_obsoleteGuid.isNull())
        qnCommon->setObsoleteServerGuid(m_obsoleteGuid);

    if (!m_cmdLineArguments.engineVersion.isNull())
    {
        qWarning() << "Starting with overridden version: " << m_cmdLineArguments.engineVersion;
        qnCommon->setEngineVersion(QnSoftwareVersion(m_cmdLineArguments.engineVersion));
    }

    QnCallCountStart(std::chrono::milliseconds(5000));
#ifdef Q_OS_WIN32
    nx::misc::ServerDataMigrateHandler migrateHandler;
    switch (nx::misc::migrateFilesFromWindowsOldDir(&migrateHandler))
    {
        case nx::misc::MigrateDataResult::WinDirNotFound:
            NX_LOG(lit("Moving data from the old windows dir. Windows dir not found."), cl_logWARNING);
            break;
        case nx::misc::MigrateDataResult::NoNeedToMigrate:
            NX_LOG(lit("Moving data from the old windows dir. Nothing to move"), cl_logDEBUG2);
            break;
        case nx::misc::MigrateDataResult::MoveDataFailed:
            NX_LOG(lit("Moving data from the old windows dir. Old data found but move failed."), cl_logWARNING);
            break;
        case nx::misc::MigrateDataResult::Ok:
            NX_LOG(lit("Moving data from the old windows dir. Old data found and successfully moved."), cl_logINFO);
            break;
    }
#endif
    ffmpegInit();

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

    const auto allowedSslVersions = MSSettings::roSettings()->value(
        nx_ms_conf::ALLOWED_SSL_VERSIONS, QString()).toString();
    if (!allowedSslVersions.isEmpty())
        nx::network::SslEngine::setAllowedServerVersions(allowedSslVersions.toUtf8());

    const auto allowedSslCiphers = MSSettings::roSettings()->value(
        nx_ms_conf::ALLOWED_SSL_CIPHERS, QString()).toString();
    if (!allowedSslCiphers.isEmpty())
        nx::network::SslEngine::setAllowedServerCiphers(allowedSslCiphers.toUtf8());

    nx::network::SslEngine::useOrCreateCertificate(
        MSSettings::roSettings()->value(
            nx_ms_conf::SSL_CERTIFICATE_PATH,
            getDataDirectory() + lit( "/ssl/cert.pem")).toString(),
        QnAppInfo::productName().toUtf8(), "US",
        QnAppInfo::organizationName().toUtf8());

    QScopedPointer<QnServerMessageProcessor> messageProcessor(new QnServerMessageProcessor());
    QScopedPointer<QnCameraHistoryPool> historyPool(new QnCameraHistoryPool());
    QScopedPointer<QnRuntimeInfoManager> runtimeInfoManager(new QnRuntimeInfoManager());
    QScopedPointer<QnMasterServerStatusWatcher> masterServerWatcher(new QnMasterServerStatusWatcher());
    QScopedPointer<QnConnectToCloudWatcher> connectToCloudWatcher(new QnConnectToCloudWatcher());
    std::unique_ptr<HostSystemPasswordSynchronizer> hostSystemPasswordSynchronizer( new HostSystemPasswordSynchronizer() );
    std::unique_ptr<QnServerDb> serverDB(new QnServerDb());
    std::unique_ptr<QnMServerAuditManager> auditManager( new QnMServerAuditManager() );

    TimeBasedNonceProvider timeBasedNonceProvider;
    CloudManagerGroup cloudManagerGroup(&timeBasedNonceProvider);
    auto authHelper = std::make_unique<QnAuthHelper>(&timeBasedNonceProvider, &cloudManagerGroup);
    connect(QnAuthHelper::instance(), &QnAuthHelper::emptyDigestDetected, this, &MediaServerProcess::at_emptyDigestDetected);

    //TODO #ak following is to allow "OPTIONS * RTSP/1.0" without authentication
    QnAuthHelper::instance()->restrictionList()->allow( lit( "?" ), AuthMethod::noAuth );

    QnAuthHelper::instance()->restrictionList()->allow(lit("*/api/ping"), AuthMethod::noAuth);
    QnAuthHelper::instance()->restrictionList()->allow(lit("*/api/camera_event*"), AuthMethod::noAuth);
    QnAuthHelper::instance()->restrictionList()->allow(lit("*/api/showLog*"), AuthMethod::urlQueryParam);   //allowed by default for now
    QnAuthHelper::instance()->restrictionList()->allow(lit("*/api/moduleInformation"), AuthMethod::noAuth);
    QnAuthHelper::instance()->restrictionList()->allow(lit("*/api/gettime"), AuthMethod::noAuth);
    QnAuthHelper::instance()->restrictionList()->allow(lit("*/api/getTimeZones"), AuthMethod::noAuth);
    QnAuthHelper::instance()->restrictionList()->allow(lit("*/api/getNonce"), AuthMethod::noAuth);
    QnAuthHelper::instance()->restrictionList()->allow(lit("*/api/cookieLogin"), AuthMethod::noAuth);
    QnAuthHelper::instance()->restrictionList()->allow(lit("*/api/cookieLogout"), AuthMethod::noAuth);
    QnAuthHelper::instance()->restrictionList()->allow(lit("*/api/getCurrentUser"), AuthMethod::noAuth);
    QnAuthHelper::instance()->restrictionList()->allow(lit("*/static/*"), AuthMethod::noAuth);
    QnAuthHelper::instance()->restrictionList()->allow(lit("/crossdomain.xml"), AuthMethod::noAuth);
    QnAuthHelper::instance()->restrictionList()->allow(lit("*/api/startLiteClient"), AuthMethod::noAuth);
    // TODO: #3.1 Remove this method and use /api/installUpdate in client when offline cloud authentication is implemented.
    QnAuthHelper::instance()->restrictionList()->allow(lit("*/api/installUpdateUnauthenticated"), AuthMethod::noAuth);

    //by following delegating hls authentication to target server
    QnAuthHelper::instance()->restrictionList()->allow( lit("*/proxy/*/hls/*"), AuthMethod::noAuth );

    QnBusinessRuleProcessor::init(new QnMServerBusinessRuleProcessor());

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

    std::unique_ptr<QnStorageManager> normalStorageManager(
        new QnStorageManager(
            QnServer::StoragePool::Normal
        )
    );

    std::unique_ptr<QnStorageManager> backupStorageManager(
        new QnStorageManager(
            QnServer::StoragePool::Backup
        )
    );

    std::unique_ptr<QnFileDeletor> fileDeletor( new QnFileDeletor() );

    connect(QnResourceDiscoveryManager::instance(), &QnResourceDiscoveryManager::CameraIPConflict, this, &MediaServerProcess::at_cameraIPConflict);
    connect(qnNormalStorageMan, &QnStorageManager::noStoragesAvailable, this, &MediaServerProcess::at_storageManager_noStoragesAvailable);
    connect(qnNormalStorageMan, &QnStorageManager::storageFailure, this, &MediaServerProcess::at_storageManager_storageFailure);
    connect(qnNormalStorageMan, &QnStorageManager::rebuildFinished, this, &MediaServerProcess::at_storageManager_rebuildFinished);

    connect(qnBackupStorageMan, &QnStorageManager::storageFailure, this, &MediaServerProcess::at_storageManager_storageFailure);
    connect(qnBackupStorageMan, &QnStorageManager::rebuildFinished, this, &MediaServerProcess::at_storageManager_rebuildFinished);
    connect(qnBackupStorageMan, &QnStorageManager::backupFinished, this, &MediaServerProcess::at_archiveBackupFinished);

    QString dataLocation = getDataDirectory();
    QDir stateDirectory;
    stateDirectory.mkpath(dataLocation + QLatin1String("/state"));
    fileDeletor->init(dataLocation + QLatin1String("/state")); // constructor got root folder for temp files


    // If adminPassword is set by installer save it and create admin user with it if not exists yet
    qnCommon->setDefaultAdminPassword(settings->value(APPSERVER_PASSWORD, QLatin1String("")).toString());
    qnCommon->setUseLowPriorityAdminPasswordHack(settings->value(LOW_PRIORITY_ADMIN_PASSWORD, false).toBool());

    BeforeRestoreDbData beforeRestoreDbData;
    beforeRestoreDbData.loadFromSettings(settings);
    qnCommon->setBeforeRestoreData(beforeRestoreDbData);

    qnCommon->setModuleGUID(serverGuid());
    nx::network::SocketGlobals::outgoingTunnelPool().assignOwnPeerId("ms", qnCommon->moduleGUID());

    bool compatibilityMode = m_cmdLineArguments.devModeKey == lit("razrazraz");
    const QString appserverHostString = MSSettings::roSettings()->value("appserverHost").toString();

    qnCommon->setSystemIdentityTime(nx::ServerSetting::getSysIdTime(), qnCommon->moduleGUID());
    qnCommon->setLocalPeerType(Qn::PT_Server);
    connect(qnCommon, &QnCommonModule::systemIdentityTimeChanged, this, &MediaServerProcess::at_systemIdentityTimeChanged, Qt::QueuedConnection);

    ec2::ApiRuntimeData runtimeData;
    runtimeData.peer.id = qnCommon->moduleGUID();
    runtimeData.peer.instanceId = qnCommon->runningInstanceGUID();
    runtimeData.peer.peerType = Qn::PT_Server;
    runtimeData.box = QnAppInfo::armBox();
    runtimeData.brand = QnAppInfo::productNameShort();
    runtimeData.customization = compatibilityMode ? QString() : QnAppInfo::customizationName();
    runtimeData.platform = QnAppInfo::applicationPlatform();

#ifdef __arm__
    if (QnAppInfo::isBpi() || QnAppInfo::isNx1())
    {
        runtimeData.nx1mac = Nx1::getMac();
        runtimeData.nx1serial = Nx1::getSerial();
    }
#endif

    runtimeData.hardwareIds = m_hardwareGuidList;
    QnRuntimeInfoManager::instance()->updateLocalItem(runtimeData);    // initializing localInfo

    std::unique_ptr<ec2::AbstractECConnectionFactory> ec2ConnectionFactory(
        getConnectionFactory(
            Qn::PT_Server,
            nx::utils::TimerManager::instance()));

    connect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoAdded, this, &MediaServerProcess::at_runtimeInfoChanged);
    connect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoChanged, this, &MediaServerProcess::at_runtimeInfoChanged);

    MediaServerStatusWatcher mediaServerStatusWatcher;

    //passing settings
    std::map<QString, QVariant> confParams;
    for( const auto& paramName: MSSettings::roSettings()->allKeys() )
    {
        if( paramName.startsWith( lit("ec") ) )
            confParams.emplace( paramName, MSSettings::roSettings()->value( paramName ) );
    }
    ec2ConnectionFactory->setConfParams(std::move(confParams));
    ec2::AbstractECConnectionPtr ec2Connection;
    QnConnectionInfo connectInfo;

    while (!needToStop())
    {
        const ec2::ErrorCode errorCode = ec2ConnectionFactory->connectSync(
            QnAppServerConnectionFactory::url(), ec2::ApiClientInfoData(), &ec2Connection );
        if (ec2Connection)
        {
            connectInfo = ec2Connection->connectionInfo();
            auto connectionResult = QnConnectionValidator::validateConnection(connectInfo, errorCode);
            if (connectionResult == Qn::SuccessConnectionResult)
            {
                NX_LOG(QString::fromLatin1("Connected to local EC2"), cl_logWARNING);
                break;
            }

            switch (connectionResult)
            {
            case Qn::IncompatibleInternalConnectionResult:
            case Qn::IncompatibleCloudHostConnectionResult:
            case Qn::IncompatibleVersionConnectionResult:
            case Qn::IncompatibleProtocolConnectionResult:
                NX_LOG(lit("Incompatible Server version detected! Giving up."), cl_logERROR);
                return;
            default:
                break;
            }
        }

        NX_LOG( QString::fromLatin1("Can't connect to local EC2. %1")
            .arg(ec2::toString(errorCode)), cl_logERROR );
        QnSleep::msleep(3000);
    }

    if (needToStop())
        return; //TODO #ak correctly deinitialize what has been initialised

    MSSettings::roSettings()->setValue(QnServer::kRemoveDbParamName, "0");

    connect(ec2Connection.get(), &ec2::AbstractECConnection::databaseDumped, this, &MediaServerProcess::at_databaseDumped);
    qnCommon->setRemoteGUID(connectInfo.serverId());
    MSSettings::roSettings()->sync();
    if (MSSettings::roSettings()->value(PENDING_SWITCH_TO_CLUSTER_MODE).toString() == "yes")
    {
        NX_LOG( QString::fromLatin1("Switching to cluster mode and restarting..."), cl_logWARNING );
        nx::SystemName systemName(connectInfo.systemName);
        systemName.saveToConfig(); //< migrate system name from foreign database via config
        nx::ServerSetting::setSysIdTime(0);
        MSSettings::roSettings()->remove("appserverHost");
        MSSettings::roSettings()->remove("appserverLogin");
        MSSettings::roSettings()->setValue(APPSERVER_PASSWORD, "");
        MSSettings::roSettings()->remove(PENDING_SWITCH_TO_CLUSTER_MODE);
        MSSettings::roSettings()->sync();

        QFile::remove(closeDirPath(getDataDirectory()) + "/ecs.sqlite");

        // kill itself to restart
#ifdef Q_OS_WIN
        HANDLE hProcess = GetCurrentProcess();
        TerminateProcess(hProcess, ERROR_SERVICE_SPECIFIC_ERROR);
        WaitForSingleObject(hProcess, 10*1000);
#endif
        abort();
        return;
    }

    settings->setValue(LOW_PRIORITY_ADMIN_PASSWORD, "");

    QnAppServerConnectionFactory::setEc2Connection( ec2Connection );
    auto clearEc2ConnectionGuardFunc = [](MediaServerProcess*){
        QnAppServerConnectionFactory::setEc2Connection(ec2::AbstractECConnectionPtr()); };
    std::unique_ptr<MediaServerProcess, decltype(clearEc2ConnectionGuardFunc)>
        clearEc2ConnectionGuard(this, clearEc2ConnectionGuardFunc);

    if (m_cmdLineArguments.cleanupDb)
    {
        const bool kCleanupDbObjects = true;
        const bool kCleanupTransactionLog = true;
        auto miscManager = ec2Connection->getMiscManager(Qn::kSystemAccess);
        miscManager->cleanupDatabaseSync(kCleanupDbObjects, kCleanupTransactionLog);
    }

    QnAppServerConnectionFactory::setEC2ConnectionFactory( ec2ConnectionFactory.get() );

    connect( ec2Connection->getTimeNotificationManager().get(), &ec2::AbstractTimeNotificationManager::timeChanged,
             QnSyncTime::instance(), (void(QnSyncTime::*)(qint64))&QnSyncTime::updateTime );

    QnMServerResourceSearcher::initStaticInstance( new QnMServerResourceSearcher() );

    CommonPluginContainer pluginContainer;

    //Initializing plugin manager
    PluginManager pluginManager(QString(), &pluginContainer);
    PluginManager::instance()->loadPlugins( MSSettings::roSettings() );

    for (const auto storagePlugin :
         PluginManager::instance()->findNxPlugins<nx_spl::StorageFactory>(nx_spl::IID_StorageFactory))
    {
        QnStoragePluginFactory::instance()->registerStoragePlugin(
            storagePlugin->storageType(),
            std::bind(
                &QnThirdPartyStorageResource::instance,
                std::placeholders::_1,
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

    if (!initTcpListener(&cloudManagerGroup))
    {
        qCritical() << "Failed to bind to local port. Terminating...";
        QCoreApplication::quit();
        return;
    }

    std::unique_ptr<QnMulticast::HttpServer> multicastHttp(new QnMulticast::HttpServer(qnCommon->moduleGUID().toQUuid(), m_universalTcpListener));

    using namespace std::placeholders;
    m_universalTcpListener->setProxyHandler<QnProxyConnectionProcessor>(&QnUniversalRequestProcessor::isProxy);
    messageProcessor->registerProxySender(m_universalTcpListener);

    ec2ConnectionFactory->registerTransactionListener( m_universalTcpListener );

    const bool sslAllowed =
        MSSettings::roSettings()->value(
            nx_ms_conf::ALLOW_SSL_CONNECTIONS,
            nx_ms_conf::DEFAULT_ALLOW_SSL_CONNECTIONS).toBool();

    bool foundOwnServerInDb = false;
    m_moduleFinder = new QnModuleFinder(false);
    std::unique_ptr<QnModuleFinder> moduleFinderScopedPointer( m_moduleFinder );
    while (m_mediaServer.isNull() && !needToStop())
    {
        QnMediaServerResourcePtr server = findServer(ec2Connection);
        ec2::ApiMediaServerData prevServerData;
        if (server)
        {
            fromResourceToApi(server, prevServerData);
            foundOwnServerInDb = true;
        }
        else
        {
            server = QnMediaServerResourcePtr(new QnMediaServerResource());
            server->setId(serverGuid());
            server->setMaxCameras(DEFAULT_MAX_CAMERAS);

            if (!beforeRestoreDbData.serverName.isEmpty())
                server->setName(QString::fromLocal8Bit(beforeRestoreDbData.serverName));
            else
                server->setName(getDefaultServerName());
        }

        server->setServerFlags((Qn::ServerFlags) calcServerFlags());

        QHostAddress appserverHost;
        bool isLocal = isLocalAppServer(appserverHostString);
        if (!isLocal) {
            do
            {
                appserverHost = resolveHost(appserverHostString);
            } while (appserverHost.toIPv4Address() == 0);
        }


        server->setPrimaryAddress(
            SocketAddress(defaultLocalAddress(appserverHost), m_universalTcpListener->getPort()));
        server->setSslAllowed(sslAllowed);
        cloudManagerGroup.connectionManager.setProxyVia(
            SocketAddress(HostAddress::localhost, m_universalTcpListener->getPort()));


        // used for statistics reported
        server->setSystemInfo(QnSystemInformation::currentSystemInformation());
        server->setVersion(qnCommon->engineVersion());

        QByteArray settingsAuthKey = nx::ServerSetting::getAuthKey();
        QByteArray authKey = settingsAuthKey;
        if (authKey.isEmpty())
            authKey = server->getAuthKey().toLatin1();
        if (authKey.isEmpty())
            authKey = QnUuid::createUuid().toString().toLatin1();
        server->setAuthKey(authKey);

        // Keep server auth key in registry. Server MUST be able pass authorization after deleting database in database restore process
        if (settingsAuthKey != authKey)
            nx::ServerSetting::setAuthKey(authKey);

        ec2::ApiMediaServerData newServerData;
        fromResourceToApi(server, newServerData);
        if (prevServerData != newServerData)
        {
            m_mediaServer = registerServer(
                ec2Connection,
                server,
                nx::mserver_aux::isNewServerInstance(
                    qnCommon->beforeRestoreDbData(),
                    foundOwnServerInDb,
                    MSSettings::roSettings()->value(NO_SETUP_WIZARD).toInt() > 0));
        }
        else
        {
            m_mediaServer = server;
        }

        #ifdef ENABLE_EXTENDED_STATISTICS
            qnServerDb->setBookmarkCountController([server](size_t count){
                server->setProperty(Qn::BOOKMARK_COUNT, QString::number(count));
                propertyDictionary->saveParams(server->getId());
            });
        #endif

        QFile hddList(Qn::HDD_LIST_FILE);
        if (hddList.open(QFile::ReadOnly))
        {
            const auto content = QString::fromUtf8(hddList.readAll());
            if (content.size())
            {
                auto hhds = content.split(lit("\n"), QString::SkipEmptyParts);
                for (auto& hdd : hhds) hdd = hdd.trimmed();
                server->setProperty(Qn::HDD_LIST, hhds.join(", "),
                                    QnResource::NO_ALLOW_EMPTY);
            }
        }

        propertyDictionary->saveParams(server->getId());

        if (m_mediaServer.isNull())
            QnSleep::msleep(1000);
    }

    /* This key means that password should be forcibly changed in the database. */
    MSSettings::roSettings()->remove(OBSOLETE_SERVER_GUID);
    MSSettings::roSettings()->setValue(APPSERVER_PASSWORD, "");
#ifdef _DEBUG
    MSSettings::roSettings()->sync();
    NX_ASSERT(MSSettings::roSettings()->value(APPSERVER_PASSWORD).toString().isEmpty(), Q_FUNC_INFO, "appserverPassword is not emptyu in registry. Restart the server as Administrator");
#endif

    if (needToStop())
    {
        stopObjects();
        m_ipDiscovery.reset();
        return;
    }

    QnRecordingManager::initStaticInstance( new QnRecordingManager() );
    qnResPool->addResource(m_mediaServer);

    QString moduleName = qApp->applicationName();
    if( moduleName.startsWith( qApp->organizationName() ) )
        moduleName = moduleName.mid( qApp->organizationName().length() ).trimmed();

    QnModuleInformation selfInformation;
    selfInformation.id = qnCommon->moduleGUID();
    selfInformation.type = QnModuleInformation::nxMediaServerId();
    selfInformation.protoVersion = nx_ec::EC2_PROTO_VERSION;
    selfInformation.systemInformation = QnSystemInformation::currentSystemInformation();

    selfInformation.brand = compatibilityMode ? QString() : QnAppInfo::productNameShort();
    selfInformation.customization = compatibilityMode ? QString() : QnAppInfo::customizationName();
    selfInformation.version = qnCommon->engineVersion();
    selfInformation.sslAllowed = sslAllowed;
    selfInformation.runtimeId = qnCommon->runningInstanceGUID();
    selfInformation.serverFlags = m_mediaServer->getServerFlags();
    selfInformation.ecDbReadOnly = ec2Connection->connectionInfo().ecDbReadOnly;

    qnCommon->setModuleInformation(selfInformation);
    qnCommon->bindModuleinformation(m_mediaServer);

    // show our cloud host value in registry in case of installer will check it
    MSSettings::roSettings()->setValue(QnServer::kIsConnectedToCloudKey,
        qnGlobalSettings->cloudSystemId().isEmpty() ? "no" : "yes");
    MSSettings::roSettings()->setValue("cloudHost", selfInformation.cloudHost);

    if (!m_cmdLineArguments.allowedDiscoveryPeers.isEmpty()) {
        QSet<QnUuid> allowedPeers;
        for (const QString &peer: m_cmdLineArguments.allowedDiscoveryPeers.split(";")) {
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
    serverUpdateTool->removeUpdateFiles(m_mediaServer->getVersion().toString());

    // ===========================================================================
    QnResource::initAsyncPoolInstance()->setMaxThreadCount( MSSettings::roSettings()->value(
        nx_ms_conf::RESOURCE_INIT_THREADS_COUNT,
        nx_ms_conf::DEFAULT_RESOURCE_INIT_THREADS_COUNT ).toInt() );
    QnResource::initAsyncPoolInstance();

    // ============================
    std::unique_ptr<nx_upnp::DeviceSearcher> upnpDeviceSearcher(new nx_upnp::DeviceSearcher(qnGlobalSettings));
    std::unique_ptr<QnMdnsListener> mdnsListener(new QnMdnsListener());

    std::unique_ptr<QnAppserverResourceProcessor> serverResourceProcessor( new QnAppserverResourceProcessor(m_mediaServer->getId()) );
    serverResourceProcessor->moveToThread( mserverResourceDiscoveryManager.get() );
    QnResourceDiscoveryManager::instance()->setResourceProcessor(serverResourceProcessor.get());

    std::unique_ptr<QnResourceStatusWatcher> statusWatcher( new QnResourceStatusWatcher());

    nx::network::SocketGlobals::addressPublisher().setRetryInterval(
        nx::utils::parseTimerDuration(
            MSSettings::roSettings()->value(MEDIATOR_ADDRESS_UPDATE).toString(),
            nx::network::cloud::MediatorAddressPublisher::kDefaultRetryInterval));

    /* Searchers must be initialized before the resources are loaded as resources instances are created by searchers. */
    QnMediaServerResourceSearchers searchers;

    std::unique_ptr<QnAudioStreamerPool> audioStreamerPool(new QnAudioStreamerPool());

    auto upnpPortMapper = initializeUpnpPortMapper();

    qDebug() << "start loading resources";
    QElapsedTimer tt;
    tt.start();
    qnResourceAccessManager->beginUpdate();
    qnResourceAccessProvider->beginUpdate();
    loadResourcesFromECS(messageProcessor.data());
    saveServerInfo(m_mediaServer);
    m_mediaServer->setStatus(Qn::Online);

    if (m_cmdLineArguments.moveHandlingCameras)
        moveHandlingCameras();

    qDebug() << "resources loaded for" << tt.elapsed();
    qnResourceAccessProvider->endUpdate();
    qDebug() << "access ready" << tt.elapsed();
    qnResourceAccessManager->endUpdate();
    qDebug() << "permissions ready" << tt.elapsed();

    qnGlobalSettings->initialize();

    updateAddressesList();

    auto settingsProxy = nx::mserver_aux::createServerSettingsProxy();
    auto systemNameProxy = nx::mserver_aux::createServerSystemNameProxy();

    nx::mserver_aux::setUpSystemIdentity(qnCommon->beforeRestoreDbData(), settingsProxy.get(), std::move(systemNameProxy));

    BeforeRestoreDbData::clearSettings(settings);

    addFakeVideowallUser();

    if (!MSSettings::roSettings()->value(QnServer::kNoInitStoragesOnStartup, false).toBool())
        initStoragesAsync(messageProcessor.data());

    if (!QnPermissionsHelper::isSafeMode())
    {
        if (nx::mserver_aux::needToResetSystem(
                    nx::mserver_aux::isNewServerInstance(
                        qnCommon->beforeRestoreDbData(),
                        foundOwnServerInDb,
                        MSSettings::roSettings()->value(NO_SETUP_WIZARD).toInt() > 0),
                    settingsProxy.get()))
        {
            if (settingsProxy->isCloudInstanceChanged())
                qWarning() << "Cloud instance changed from" << qnGlobalSettings->cloudHost() <<
                    "to" << QnAppInfo::defaultCloudHost() << ". Server goes to the new state";

            resetSystemState(cloudManagerGroup.connectionManager);
        }
        if (settingsProxy->isCloudInstanceChanged())
        {
            ec2::ErrorCode errCode;
            do
            {
                const bool kCleanupDbObjects = false;
                const bool kCleanupTransactionLog = true;

                errCode = QnAppServerConnectionFactory::getConnection2()
                    ->getMiscManager(Qn::kSystemAccess)
                    ->cleanupDatabaseSync(kCleanupDbObjects, kCleanupTransactionLog);

                if (errCode != ec2::ErrorCode::ok)
                {
                    qWarning() << "Error while rebuild transaction log. Trying again...";
                        msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
                }

            } while (errCode != ec2::ErrorCode::ok && !m_needStop);
        }
        qnGlobalSettings->setCloudHost(QnAppInfo::defaultCloudHost());
        qnGlobalSettings->synchronizeNow();
    }

    qnGlobalSettings->takeFromSettings(MSSettings::roSettings(), m_mediaServer);

    if (QnUserResourcePtr adminUser = qnResPool->getAdministrator())
    {
        //todo: root password for NX1 should be updated in case of cloud owner
        hostSystemPasswordSynchronizer->syncLocalHostRootPasswordWithAdminIfNeeded( adminUser );
    }
    MSSettings::roSettings()->sync();

#ifndef EDGE_SERVER
    //TODO: #GDM make this the common way with other settings
    updateDisabledVendorsIfNeeded();
    updateAllowCameraCHangesIfNeed();
    qnGlobalSettings->synchronizeNowSync(); //TODO: #GDM double sync
#endif

    std::unique_ptr<QnLdapManager> ldapManager(new QnLdapManager());


    QnResourceDiscoveryManager::instance()->setReady(true);
    const bool isDiscoveryDisabled =
        MSSettings::roSettings()->value(QnServer::kNoResourceDiscovery, false).toBool();
    if( !ec2Connection->connectionInfo().ecDbReadOnly && !isDiscoveryDisabled)
        QnResourceDiscoveryManager::instance()->start();
    //else
    //    we are not able to add cameras to DB anyway, so no sense to do discover


    connect(
        QnResourceDiscoveryManager::instance(),
        &QnResourceDiscoveryManager::localInterfacesChanged,
        this,
        &MediaServerProcess::updateAddressesList);

    connect(
        m_universalTcpListener,
        &QnTcpListener::portChanged,
        this,
        [this, &cloudManagerGroup]()
        {
            updateAddressesList();
            cloudManagerGroup.connectionManager.setProxyVia(
                SocketAddress(HostAddress::localhost, m_universalTcpListener->getPort()));
        });

    m_firstRunningTime = MSSettings::runTimeSettings()->value("lastRunningTime").toLongLong();

    m_crashReporter.reset(new ec2::CrashReporter);

    QTimer timer;
    connect(&timer, SIGNAL(timeout()), this, SLOT(at_timer()), Qt::DirectConnection);
    timer.start(QnVirtualCameraResource::issuesTimeoutMs());
    at_timer();

    QTimer udtInternetTrafficTimer;
    connect(&udtInternetTrafficTimer, &QTimer::timeout,
        []()
        {
            QnResourcePtr server = qnResPool->getResourceById(qnCommon->moduleGUID());
            const auto old = server->getProperty(Qn::UDT_INTERNET_TRFFIC).toULongLong();
            const auto current = nx::network::UdtStatistics::global.internetBytesTransfered.load();
            const auto update = old + (qulonglong) current;
            if (server->setProperty(Qn::UDT_INTERNET_TRFFIC, QString::number(update))
                && propertyDictionary->saveParams(server->getId()))
            {
                NX_LOG(lm("%1 is updated to %2").strs(Qn::UDT_INTERNET_TRFFIC, update), cl_logDEBUG1);
                nx::network::UdtStatistics::global.internetBytesTransfered -= current;
            }
        });
    udtInternetTrafficTimer.start(UDT_INTERNET_TRAFIC_TIMER);

    QTimer::singleShot(3000, this, SLOT(at_connectionOpened()));
    QTimer::singleShot(0, this, SLOT(at_appStarted()));

    m_dumpSystemResourceUsageTaskID = nx::utils::TimerManager::instance()->addTimer(
        std::bind( &MediaServerProcess::dumpSystemUsageStats, this ),
        std::chrono::milliseconds(SYSTEM_USAGE_DUMP_TIMEOUT));

    QnRecordingManager::instance()->start();
    if (!isDiscoveryDisabled)
        QnMServerResourceSearcher::instance()->start();
    m_universalTcpListener->start();
    serverConnector->start();
#if 1
    if (ec2Connection->connectionInfo().ecUrl.scheme() == "file") {
        // Connect to local database. Start peer-to-peer sync (enter to cluster mode)
        qnCommon->setCloudMode(true);
        if (!isDiscoveryDisabled)
            m_moduleFinder->start();
    }
#endif
    qnBackupStorageMan->scheduleSync()->start();
    emit started();
    exec();


    disconnect(QnAuthHelper::instance(), 0, this, 0);
    disconnect(QnResourceDiscoveryManager::instance(), 0, this, 0);
    disconnect(qnNormalStorageMan, 0, this, 0);
    disconnect(qnBackupStorageMan, 0, this, 0);
    disconnect(qnCommon, 0, this, 0);
    disconnect(QnRuntimeInfoManager::instance(), 0, this, 0);
    disconnect(ec2Connection->getTimeNotificationManager().get(), 0, this, 0);
    disconnect(ec2Connection.get(), 0, this, 0);
    disconnect(m_updatePiblicIpTimer.get(), 0, this, 0);
    disconnect(m_ipDiscovery.get(), 0, this, 0);
    disconnect(m_moduleFinder, 0, this, 0);

    WaitingForQThreadToEmptyEventQueue waitingForObjectsToBeFreed( QThread::currentThread(), 3 );
    waitingForObjectsToBeFreed.join();

    qWarning()<<"QnMain event loop has returned. Destroying objects...";

    m_crashReporter.reset();

    //cancelling dumping system usage
    quint64 dumpSystemResourceUsageTaskID = 0;
    {
        QnMutexLocker lk( &m_mutex );
        dumpSystemResourceUsageTaskID = m_dumpSystemResourceUsageTaskID;
        m_dumpSystemResourceUsageTaskID = 0;
    }
    nx::utils::TimerManager::instance()->joinAndDeleteTimer( dumpSystemResourceUsageTaskID );

    m_ipDiscovery.reset(); // stop it before IO deinitialized
    QnResourceDiscoveryManager::instance()->pleaseStop();
    QnResource::pleaseStopAsyncTasks();
    multicastHttp.reset();
    stopObjects();

    QnResource::stopCommandProc();
    if (m_initStoragesAsyncPromise)
        m_initStoragesAsyncPromise->get_future().wait();
    // todo: #rvasilenko some undeleted resources left in the QnMain event loop. I stopped TimerManager as temporary solution for it.
    nx::utils::TimerManager::instance()->stop();

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

    qnNormalStorageMan->stopAsyncTasks();
    qnBackupStorageMan->stopAsyncTasks();

    ptzPool.reset();

    QnServerMessageProcessor::instance()->init(NULL); // stop receiving notifications
    messageProcessor.reset();

    //disconnecting from EC2
    clearEc2ConnectionGuard.reset();

    ec2Connection.reset();
    QnAppServerConnectionFactory::setEC2ConnectionFactory( nullptr );
    ec2ConnectionFactory.reset();

    mserverResourceDiscoveryManager.reset();

    // This method will set flag on message channel to threat next connection close as normal
    //appServerConnection->disconnectSync();
    MSSettings::runTimeSettings()->setValue("lastRunningTime", 0);

    authHelper.reset();
    fileDeletor.reset();
    normalStorageManager.reset();
    backupStorageManager.reset();

    if (m_mediaServer)
        m_mediaServer->beforeDestroy();
    m_mediaServer.clear();

    performActionsOnExit();

    nx::network::SocketGlobals::outgoingTunnelPool().clearOwnPeerId();
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
        user->setName(login);
        user->setPassword(password);
        user->generateHash();

        ec2::ApiUserData userData;
        fromResourceToApi(user, userData);

        QnUuid userId = user->getId();
        m_updateUserRequests << userId;
        appServerConnection->getUserManager(Qn::kSystemAccess)->save(userData, password, this, [this, userId]( int reqID, ec2::ErrorCode errorCode )
        {
            QN_UNUSED(reqID, errorCode);
            m_updateUserRequests.remove(userId);
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

protected:
    virtual int executeApplication() override
    {
        m_main.reset(new MediaServerProcess(m_argc, m_argv));

        const auto cmdParams = m_main->cmdLineArguments();
        if (cmdParams.showHelp || cmdParams.showVersion)
            return 0;

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
        QCoreApplication::setApplicationName(QnServerAppInfo::applicationName());
        if (QCoreApplication::applicationVersion().isEmpty())
            QCoreApplication::setApplicationVersion(QnAppInfo::applicationVersion());

        if (application->isRunning() &&
            MSSettings::roSettings()->value(nx_ms_conf::ENABLE_MULTIPLE_INSTANCES).toInt() == 0)
        {
            NX_LOG("Server already started", cl_logERROR);
            qApp->quit();
            return;
        }

#ifdef Q_OS_WIN
        SetConsoleCtrlHandler(stopServer_WIN, true);
#endif
        signal(SIGINT, stopServer);
        signal(SIGTERM, stopServer);

    //    av_log_set_callback(decoderLogCallback);



        const QString& dataLocation = getDataDirectory();
        const QString& logDir = MSSettings::roSettings()->value( "logDir", dataLocation + QLatin1String("/log/") ).toString();

        QDir::setCurrent(qApp->applicationDirPath());

        MSSettings::runTimeSettings()->remove("rebuild");

        const auto cmdLineArguments = m_main->cmdLineArguments();

        initLog(cmdLineArguments.logLevel);

        QnLog::instance(QnLog::HTTP_LOG_INDEX)->create(
            logDir + QLatin1String("/http_log"),
            MSSettings::roSettings()->value( "maxLogFileSize", DEFAULT_MAX_LOG_FILE_SIZE ).toULongLong(),
            MSSettings::roSettings()->value( "logArchiveSize", DEFAULT_LOG_ARCHIVE_SIZE ).toULongLong(),
            QnLog::logLevelFromString(cmdLineArguments.msgLogLevel));

        //preparing transaction log
        initTransactionLog(logDir, QnLog::logLevelFromString(cmdLineArguments.ec2TranLogLevel));

        QnLog::instance(QnLog::HWID_LOG)->create(
            logDir + QLatin1String("/hw_log"),
            MSSettings::roSettings()->value( "maxLogFileSize", DEFAULT_MAX_LOG_FILE_SIZE ).toULongLong(),
            MSSettings::roSettings()->value( "logArchiveSize", DEFAULT_LOG_ARCHIVE_SIZE ).toULongLong(),
            QnLogLevel::cl_logINFO );

        initPermissionsLog(logDir, QnLog::logLevelFromString(cmdLineArguments.permissionsLogLevel));

        NX_LOG(lit("%1 started").arg(qApp->applicationName()), cl_logALWAYS);
        NX_LOG(lit("Software version: %1").arg(QCoreApplication::applicationVersion()), cl_logALWAYS);
        NX_LOG(lit("Software revision: %1").arg(QnAppInfo::applicationRevision()), cl_logALWAYS);
        NX_LOG(lit("binary path: %1").arg(QFile::decodeName(m_argv[0])), cl_logALWAYS);

        defaultMsgHandler = qInstallMessageHandler(myMsgHandler);

        qnPlatform->process(NULL)->setPriority(QnPlatformProcess::HighPriority);

        m_main->setNeedInitHardwareId(true);
    // ------------------------------------------
#ifdef TEST_RTSP_SERVER
        addTestData();
#endif

        m_main->start();
    }

    virtual void stop() override
    {
        if (serviceMainInstance)
            serviceMainInstance->stopSync();
    }

private:
    void initTransactionLog(const QString& logDir, QnLogLevel level)
    {
        //on "always" log level only server start messages are logged, so using it instead of disabled
        QnLog::instance(QnLog::EC2_TRAN_LOG)->create(
            logDir + lit("ec2_tran"),
            MSSettings::roSettings()->value("maxLogFileSize", DEFAULT_MAX_LOG_FILE_SIZE).toULongLong(),
            MSSettings::roSettings()->value("logArchiveSize", DEFAULT_LOG_ARCHIVE_SIZE).toULongLong(),
            level);
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("================================================================================="), cl_logALWAYS);
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("================================================================================="), cl_logALWAYS);
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("================================================================================="), cl_logALWAYS);
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("%1 started").arg(qApp->applicationName()), cl_logALWAYS);
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("Software version: %1").arg(QCoreApplication::applicationVersion()), cl_logALWAYS);
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("Software revision: %1").arg(QnAppInfo::applicationRevision()), cl_logALWAYS);
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("binary path: %1").arg(QFile::decodeName(m_argv[0])), cl_logALWAYS);
    }

    void initPermissionsLog(const QString& logDir, QnLogLevel level)
    {
        QnLog::instance(QnLog::PERMISSIONS_LOG)->create(
            logDir + lit("permissions"),
            MSSettings::roSettings()->value("maxLogFileSize", DEFAULT_MAX_LOG_FILE_SIZE).toULongLong(),
            MSSettings::roSettings()->value("logArchiveSize", DEFAULT_LOG_ARCHIVE_SIZE).toULongLong(),
            level);
        NX_LOG(QnLog::PERMISSIONS_LOG, lit("================================================================================="), cl_logALWAYS);
        NX_LOG(QnLog::PERMISSIONS_LOG, lit("%1 started").arg(qApp->applicationName()), cl_logALWAYS);
        NX_LOG(QnLog::PERMISSIONS_LOG, lit("Software version: %1").arg(QCoreApplication::applicationVersion()), cl_logALWAYS);
        NX_LOG(QnLog::PERMISSIONS_LOG, lit("Software revision: %1").arg(QnAppInfo::applicationRevision()), cl_logALWAYS);
    }

private:
    int m_argc;
    char **m_argv;
    QScopedPointer<MediaServerProcess> m_main;
};

void MediaServerProcess::setNeedInitHardwareId(bool value)
{
    m_needInitHardwareId = value;
}

bool MediaServerProcess::initHardwareId()
{
    LLUtil::initHardwareId(MSSettings::roSettings());
    updateGuidIfNeeded();
    setHardwareGuidList(LLUtil::getAllHardwareIds().toVector());

    QnUuid guid = serverGuid();
    if (guid.isNull())
    {
        qDebug() << "Can't save guid. Run once as administrator.";
        NX_LOG("Can't save guid. Run once as administrator.", cl_logERROR);
        qApp->quit();
        return false;
    }
    return true;
}

QString MediaServerProcess::hardwareIdAsGuid() const
{
    auto hwId = LLUtil::getLatestHardwareId();
    auto hwIdString = QnUuid::fromHardwareId(hwId).toString();
    std::cout << "Got hwID \"" << hwIdString.toStdString() << "\"" << std::endl;
    return hwIdString;
}

void MediaServerProcess::updateGuidIfNeeded()
{
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
    }
    else if (guidIsHWID == NO) {
        if (serverGuid.isEmpty()) {
            // serverGuid remove from settings manually?
            MSSettings::roSettings()->setValue(SERVER_GUID, hwidGuid);
            MSSettings::roSettings()->setValue(GUID_IS_HWID, YES);
        }

        MSSettings::roSettings()->remove(SERVER_GUID2);
    }
    else if (guidIsHWID.isEmpty()) {
        if (!serverGuid2.isEmpty()) {
            MSSettings::roSettings()->setValue(SERVER_GUID, serverGuid2);
            MSSettings::roSettings()->setValue(GUID_IS_HWID, NO);
            MSSettings::roSettings()->remove(SERVER_GUID2);
        }
        else {
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
    if (!obsoleteGuid.isNull())
    {
        setObsoleteGuid(obsoleteGuid);
    }
}

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
#ifdef _WIN32
    win32_exception::installGlobalUnhandledExceptionHandler();
    _tzset();
#endif

#ifdef __linux__
    signal( SIGUSR1, SIGUSR1_handler );
#endif


#ifndef EDGE_SERVER
    std::unique_ptr<TextToWaveServer> textToWaveServer = std::make_unique<TextToWaveServer>(
        nx::utils::file_system::applicationDirPath(argc, argv));

    textToWaveServer->start();
    textToWaveServer->waitForStarted();
#endif

    QnVideoService service( argc, argv );
    int res = service.exec();
    if (restartFlag && res == 0)
        return 1;
    return 0;
}

const CmdLineArguments MediaServerProcess::cmdLineArguments() const
{
    return m_cmdLineArguments;
}
