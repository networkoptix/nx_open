#include "media_server_process.h"

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <functional>
#include <signal.h>
#if defined(__linux__)
    #include <signal.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif

#include <boost/optional.hpp>

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

#include <nx/vms/event/rule.h>
#include <nx/vms/event/events/reasoned_event.h>
#include <nx/mediaserver/event/event_connector.h>
#include <nx/mediaserver/event/rule_processor.h>
#include <nx/mediaserver/event/extended_rule_processor.h>

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
#include <nx/network/upnp/upnp_device_searcher.h>

#include <plugins/native_sdk/common_plugin_container.h>
#include <plugins/plugin_manager.h>
#include <plugins/resource/avi/avi_resource.h>
#include <plugins/resource/flir/flir_io_executor.h>

#include <plugins/resource/desktop_camera/desktop_camera_registrator.h>

#include <plugins/resource/mserver_resource_searcher.h>
#include <plugins/resource/mdns/mdns_listener.h>
#include <plugins/resource/upnp/global_settings_to_device_searcher_settings_adapter.h>

#include <plugins/storage/file_storage/file_storage_resource.h>
#include <plugins/storage/file_storage/db_storage_resource.h>
#include <plugins/storage/third_party_storage_resource/third_party_storage_resource.h>

#include <recorder/file_deletor.h>
#include <recorder/recording_manager.h>
#include <recorder/storage_manager.h>
#include <recorder/schedule_sync.h>

#include <rest/handlers/acti_event_rest_handler.h>
#include <rest/handlers/event_log_rest_handler.h>
#include <rest/handlers/event_log2_rest_handler.h>
#include <rest/handlers/get_system_name_rest_handler.h>
#include <rest/handlers/camera_diagnostics_rest_handler.h>
#include <rest/handlers/camera_settings_rest_handler.h>
#include <rest/handlers/crash_server_handler.h>
#include <rest/handlers/external_event_rest_handler.h>
#include <rest/handlers/favicon_rest_handler.h>
#include <rest/handlers/image_rest_handler.h>
#include <rest/handlers/log_rest_handler.h>
#include <rest/handlers/manual_camera_addition_rest_handler.h>
#include <rest/handlers/ping_rest_handler.h>
#include <rest/handlers/p2p_stats_rest_handler.h>
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
#include <rest/handlers/detach_from_cloud_rest_handler.h>
#include <rest/handlers/detach_from_system_rest_handler.h>
#include <rest/handlers/restore_state_rest_handler.h>
#include <rest/handlers/setup_local_rest_handler.h>
#include <rest/handlers/setup_cloud_rest_handler.h>
#include <rest/handlers/merge_systems_rest_handler.h>
#include <rest/handlers/current_user_rest_handler.h>
#include <rest/handlers/backup_db_rest_handler.h>
#include <rest/handlers/discovered_peers_rest_handler.h>
#include <rest/handlers/log_level_rest_handler.h>
#include <rest/handlers/multiserver_chunks_rest_handler.h>
#include <rest/handlers/multiserver_time_rest_handler.h>
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
#include <rest/handlers/downloads_rest_handler.h>
#include <rest/handlers/get_hardware_ids_rest_handler.h>
#include <rest/handlers/multiserver_get_hardware_ids_rest_handler.h>
#ifdef _DEBUG
#include <rest/handlers/debug_events_rest_handler.h>
#endif

#include <rtsp/rtsp_connection.h>

#include <nx/vms/discovery/manager.h>
#include <network/multicodec_rtp_reader.h>
#include <network/router.h>

#include <utils/common/command_line_parser.h>
#include <nx/utils/app_info.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/cpp14.h>
#include <utils/common/sleep.h>
#include <utils/common/synctime.h>
#include <utils/common/system_information.h>
#include <utils/common/util.h>
#include <nx/network/simple_http_client.h>
#include <nx/network/ssl_socket.h>
#include <nx/network/socket_global.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/cloud/tunnel/tunnel_acceptor_factory.h>

#include <utils/common/app_info.h>
#include <transcoding/ffmpeg_video_transcoder.h>

#include <nx/utils/crash_dump/systemexcept.h>

#include "platform/hardware_information.h"
#include "platform/platform_abstraction.h"
#include "core/ptz/server_ptz_controller_pool.h"
#include "plugins/resource/acti/acti_resource.h"
#include "common/common_module.h"
#include "proxy/proxy_receiver_connection_processor.h"
#include "proxy/proxy_connection.h"
#include "streaming/hls/hls_session_pool.h"
#include "streaming/hls/hls_server.h"
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
#include <nx/mediaserver/unused_wallpapers_watcher.h>
#include <nx/mediaserver/license_watcher.h>
#include <media_server/connect_to_cloud_watcher.h>
#include <rest/helpers/permissions_helper.h>
#include "misc/migrate_oldwin_dir.h"
#include "media_server_process_aux.h"
#include <common/static_common_module.h>
#include <recorder/storage_db_pool.h>
#include <transaction/message_bus_adapter.h>
#include <managers/discovery_manager.h>
#include <rest/helper/p2p_statistics.h>
#include <recorder/remote_archive_synchronizer.h>
#include <recorder/archive_integrity_watcher.h>
#include <nx/utils/std/cpp14.h>
#include <nx/mediaserver/metadata/manager_pool.h>
#include <rest/handlers/change_camera_password_rest_handler.h>

#if !defined(EDGE_SERVER)
    #include <nx_speech_synthesizer/text_to_wav.h>
    #include <nx/utils/file_system.h>
#endif

#include <streaming/audio_streamer_pool.h>
#include <proxy/2wayaudio/proxy_audio_receiver.h>

#if defined(__arm__)
    #include "nx1/info.h"
#endif


using namespace nx;

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
static QString kLogTag = toString(typeid(MediaServerProcess));

bool initResourceTypes(const ec2::AbstractECConnectionPtr& ec2Connection)
{
    QList<QnResourceTypePtr> resourceTypeList;
    auto manager = ec2Connection->getResourceManager(Qn::kSystemAccess);
    const ec2::ErrorCode errorCode = manager->getResourceTypesSync(&resourceTypeList);
    if (errorCode != ec2::ErrorCode::ok)
    {
        NX_LOG(QString::fromLatin1("Failed to load resource types. %1").arg(ec2::toString(errorCode)), cl_logERROR);
        return false;
    }

    qnResTypePool->replaceResourceTypeList(resourceTypeList);
    return true;
}

void addFakeVideowallUser(QnCommonModule* commonModule)
{
    ec2::ApiUserData fakeUserData;
    fakeUserData.permissions = Qn::GlobalVideoWallModePermissionSet;
    fakeUserData.typeId = qnResTypePool->getFixedResourceTypeId(QnResourceTypePool::kUserTypeId);
    auto fakeUser = ec2::fromApiToResource(fakeUserData);
    fakeUser->setId(Qn::kVideowallUserAccess.userId);
    fakeUser->setName(lit("Video wall"));
    commonModule->resourcePool()->addResource(fakeUser);
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

void calculateSpaceLimitOrLoadFromConfig(
    QnCommonModule* commonModule,
    const QnFileStorageResourcePtr& fileStorage)
{
    const BeforeRestoreDbData& beforeRestoreData = commonModule->beforeRestoreDbData();
    if (!beforeRestoreData.isEmpty() && beforeRestoreData.hasInfoForStorage(fileStorage->getUrl()))
    {
        fileStorage->setSpaceLimit(beforeRestoreData.getSpaceLimitForStorage(fileStorage->getUrl()));
        return;
    }

    fileStorage->setSpaceLimit(fileStorage->calcInitialSpaceLimit());
}

QnStorageResourcePtr createStorage(
    QnCommonModule* commonModule,
    const QnUuid& serverId,
    const QString& path)
{
    NX_VERBOSE(kLogTag, lm("Attempting to create storage %1").arg(path));
    QnStorageResourcePtr storage(QnStoragePluginFactory::instance()->createStorage(commonModule,"ufile"));
    storage->setName("Initial");
    storage->setParentId(serverId);
    storage->setUrl(path);

    const QString storagePath = QnStorageResource::toNativeDirPath(storage->getPath());
    const auto partitions = qnPlatform->monitor()->totalPartitionSpaceInfo();
    const auto it = std::find_if(partitions.begin(), partitions.end(),
        [&](const QnPlatformMonitor::PartitionSpace& part)
        { return storagePath.startsWith(QnStorageResource::toNativeDirPath(part.path)); });

    const auto storageType = (it != partitions.end())
        ? it->type
        : QnPlatformMonitor::NetworkPartition;
    storage->setStorageType(QnLexical::serialized(storageType));

    if (auto fileStorage = storage.dynamicCast<QnFileStorageResource>())
    {
        const qint64 totalSpace = fileStorage->calculateAndSetTotalSpaceWithoutInit();
        calculateSpaceLimitOrLoadFromConfig(commonModule, fileStorage);

        if (totalSpace < fileStorage->getSpaceLimit())
        {
            NX_DEBUG(kLogTag, lm(
                "Storage with this path %1 total space is unknown or totalSpace < spaceLimit. "
                "Total space: %2, Space limit: %3").args(path, totalSpace, storage->getSpaceLimit()));
            return QnStorageResourcePtr();
        }
    }
    else
    {
        NX_ASSERT(false, lm("Failed to create to storage: %1").arg(path));
        return QnStorageResourcePtr();
    }

    storage->setUsedForWriting(storage->initOrUpdate() == Qn::StorageInit_Ok && storage->isWritable());
    NX_DEBUG(kLogTag, lm("Storage %1 is operational: %2").args(path, storage->isUsedForWriting()));

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
    using namespace nx::utils::file_system;
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

    if (qnServerModule->roSettings()->value(nx_ms_conf::ENABLE_MULTIPLE_INSTANCES).toInt() != 0)
    {
        for (auto& path: folderPaths)
            path = closeDirPath(path) + serverGuid().toString();
    }

    NX_VERBOSE(kLogTag, lm("Record folders: %1").container(folderPaths));
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

        NX_VERBOSE(kLogTag,
            lm("Small storage %1, isFileStorage=%2, totalSpace=%3, spaceLimit=%4, toDelete").args(
                storage->getUrl(), (bool)fileStorage, totalSpace, storage->getSpaceLimit()));
    }
    return result;
}


QnStorageResourceList createStorages(
    QnCommonModule* commonModule,
    const QnMediaServerResourcePtr& mServer)
{
    QnStorageResourceList storages;
    QStringList availablePaths;
    //bool isBigStorageExist = false;
    qint64 bigStorageThreshold = 0;

    availablePaths = listRecordFolders();

    NX_DEBUG(kLogTag, lm("Available paths count: %1").arg(availablePaths.size()));
    for(const QString& folderPath: availablePaths)
    {
        NX_DEBUG(kLogTag, lm("Available path: %1").arg(folderPath));
        if (!mServer->getStorageByUrl(folderPath).isNull())
        {
            NX_DEBUG(kLogTag,
                lm("Storage with this path %1 already exists. Won't be added.").arg(folderPath));
            continue;
        }
        // Create new storage because of new partition found that missing in the database
        QnStorageResourcePtr storage = createStorage(commonModule, mServer->getId(), folderPath);
        if (!storage)
            continue;

        qint64 available = storage->getTotalSpace() - storage->getSpaceLimit();
        bigStorageThreshold = qMax(bigStorageThreshold, available);
        storages.append(storage);
        NX_DEBUG(kLogTag, lm("Creating new storage: %1").arg(folderPath));
    }
    bigStorageThreshold /= QnStorageManager::BIG_STORAGE_THRESHOLD_COEFF;

    for (int i = 0; i < storages.size(); ++i) {
        QnStorageResourcePtr storage = storages[i].dynamicCast<QnStorageResource>();
        qint64 available = storage->getTotalSpace() - storage->getSpaceLimit();
        if (available < bigStorageThreshold)
            storage->setUsedForWriting(false);
    }

    QString logMessage = lit("Storage new candidates:\n");
    for (const auto& storage : storages)
    {
        logMessage.append(
            lit("\t\turl: %1, totalSpace: %2, spaceLimit: %3")
                .arg(storage->getUrl())
                .arg(storage->getTotalSpace())
                .arg(storage->getSpaceLimit()));
    }

    NX_DEBUG(kLogTag, logMessage);
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
    {
        logMesssage.append(
            lit("\t\turl: %1, totalSpace: %2, spaceLimit: %3\n")
                .arg(storage->getUrl())
                .arg(storage->getTotalSpace())
                .arg(storage->getSpaceLimit()));
    }

    NX_DEBUG(kLogTag, logMesssage);
    return result.values();
}

void MediaServerProcess::initStoragesAsync(QnCommonMessageProcessor* messageProcessor)
{
    m_initStoragesAsyncPromise.reset(new nx::utils::promise<void>());
    QtConcurrent::run([messageProcessor, this]
    {
        NX_VERBOSE(this, "Init storages begin");
        const auto setPromiseGuardFunc = makeScopeGuard(
            [&]()
            {
                NX_VERBOSE(this, "Init storages end");
                m_initStoragesAsyncPromise->set_value();
            });

        //read server's storages
        ec2::AbstractECConnectionPtr ec2Connection = messageProcessor->commonModule()->ec2Connection();
        ec2::ErrorCode rez;
        ec2::ApiStorageDataList storages;

        while ((rez = ec2Connection->getMediaServerManager(Qn::kSystemAccess)->getStoragesSync(QnUuid(), &storages)) != ec2::ErrorCode::ok)
        {
            NX_DEBUG(this, lm("Can't get storage list. Reason: %1").arg(rez));
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }

        for(const auto& storage: storages)
        {
            NX_DEBUG(this, lm("Existing storage: %1, spaceLimit = %2")
                .args(storage.url, storage.spaceLimit));
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

        NX_DEBUG(this, lm("Found %1 storages to remove").arg(storagesToRemove.size()));
        for (const auto& storage: storagesToRemove)
        {
            NX_DEBUG(this, lm("Storage to remove: %2, id: %3").args(
                storage->getUrl(), storage->getId()));
        }

        if (!storagesToRemove.isEmpty())
        {
            ec2::ApiIdDataList idList;
            for (const auto& value: storagesToRemove)
                idList.push_back(value->getId());
            if (ec2Connection->getMediaServerManager(Qn::kSystemAccess)->removeStoragesSync(idList) != ec2::ErrorCode::ok)
                qWarning() << "Failed to remove deprecated storage on startup. Postpone removing to the next start...";
            commonModule()->resourcePool()->removeResources(storagesToRemove);
        }

        QnStorageResourceList modifiedStorages = createStorages(messageProcessor->commonModule(), m_mediaServer);
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
            QnMediaServerResourcePtr qnServer(new QnMediaServerResource(commonModule()));
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
    QString dir = qnServerModule->roSettings()->value("staticDataDir", getDataDirectory()).toString();
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
    rez = ec2Connection->getMediaServerManager(Qn::kSystemAccess)->saveUserAttributesSync(attrsList);
    if (rez != ec2::ErrorCode::ok)
    {
        qWarning() << "registerServer(): Call to registerServer failed. Reason: " << ec2::toString(rez);
        return QnMediaServerResourcePtr();
    }

    return server;
}

void MediaServerProcess::saveStorages(
    ec2::AbstractECConnectionPtr ec2Connection,
    const QnStorageResourceList& storages)
{
    ec2::ApiStorageDataList apiStorages;
    fromResourceListToApi(storages, apiStorages);

    ec2::ErrorCode rez;
    while((rez = ec2Connection->getMediaServerManager(Qn::kSystemAccess)->saveStoragesSync(apiStorages)) != ec2::ErrorCode::ok && !needToStop())
    {
        NX_WARNING(this) "Call to change server's storages failed. Reason: " << rez;
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
        m_mediaServer->saveParams();

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

    NX_EXPECT(!msg.contains(lit("QString:")), msg);
    NX_EXPECT(!msg.contains(lit("QObject:")), msg);
    qnLogMsgHandler(type, ctx, msg);
}

QUrl appServerConnectionUrl(QSettings &settings)
{
    // migrate appserverPort settings from version 2.2 if exist
    if (!qnServerModule->roSettings()->value("appserverPort").isNull())
    {
        qnServerModule->roSettings()->setValue("port", qnServerModule->roSettings()->value("appserverPort"));
        qnServerModule->roSettings()->remove("appserverPort");
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
        if (qnServerModule->roSettings()->value(QnServer::kRemoveDbParamName).toBool())
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
    return appServerUrl;
}

MediaServerProcess::MediaServerProcess(int argc, char* argv[], bool serviceMode)
:
    m_argc(argc),
    m_argv(argv),
    m_startMessageSent(false),
    m_firstRunningTime(0),
    m_universalTcpListener(0),
    m_dumpSystemResourceUsageTaskID(0),
    m_stopping(false),
    m_serviceMode(serviceMode)
{
    serviceMainInstance = this;

    parseCommandLineParameters(argc, argv);

    m_settings.reset(new MSSettings(
        m_cmdLineArguments.configFilePath,
        m_cmdLineArguments.rwConfigFilePath));

    addCommandLineParametersFromConfig(m_settings.get());

    const bool isStatisticsDisabled =
        m_settings->roSettings()->value(QnServer::kNoMonitorStatistics, false).toBool();

    m_platform.reset(new QnPlatformAbstraction());
    m_platform->process(NULL)->setPriority(QnPlatformProcess::HighPriority);
    m_platform->setUpdatePeriodMs(
        isStatisticsDisabled ? 0 : QnGlobalMonitor::kDefaultUpdatePeridMs);
}

void MediaServerProcess::parseCommandLineParameters(int argc, char* argv[])
{
    QnCommandLineParser commandLineParser;
    commandLineParser.addParameter(&m_cmdLineArguments.logLevel, "--log-level", NULL,
        lit("Supported values: none (no logging), always, error, warning, info, debug, verbose. Default: ")
        + toString(nx::utils::log::Settings::kDefaultLevel));
    commandLineParser.addParameter(&m_cmdLineArguments.exceptionFilters, "--exception-filters", NULL, "Log filters.");
    commandLineParser.addParameter(&m_cmdLineArguments.httpLogLevel, "--http-log-level", NULL, "Log value for http_log.log.");
    commandLineParser.addParameter(&m_cmdLineArguments.hwLogLevel, "--hw-log-level", NULL, "Log value for hw_log.log.");
    commandLineParser.addParameter(&m_cmdLineArguments.ec2TranLogLevel, "--ec2-tran-log-level", NULL, "Log value for ec2_tran.log.");
    commandLineParser.addParameter(&m_cmdLineArguments.permissionsLogLevel, "--permissions-log-level", NULL,"Log value for permissions.log.");

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
    commandLineParser.addParameter(&m_cmdLineArguments.createFakeData, "--create-fake-data", NULL,
        lit("Create fake data: users,cameras,propertiesPerCamera,camerasPerLayout,storageCount"), QString());
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

void MediaServerProcess::addCommandLineParametersFromConfig(MSSettings* settings)
{
    // move arguments from conf file / registry

    if (m_cmdLineArguments.rebuildArchive.isEmpty())
        m_cmdLineArguments.rebuildArchive = settings->runTimeSettings()->value("rebuild").toString();
}

MediaServerProcess::~MediaServerProcess()
{
    quit();
    stop();
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
                commonModule()->resourcePool()->getAdministrator(),
                m_mediaServer,
                nx::mserver_aux::createServerSettingsProxy(commonModule()).get()
            ).saveToSettings(qnServerModule->roSettings());
    restartServer(500);
}

void MediaServerProcess::at_systemIdentityTimeChanged(qint64 value, const QnUuid& sender)
{
    if (isStopping())
        return;

    nx::ServerSetting::setSysIdTime(value);
    if (sender != commonModule()->moduleGUID())
    {
        qnServerModule->roSettings()->setValue(QnServer::kRemoveDbParamName, "1");
        // If system Id has been changed, reset 'database restore time' variable
        nx::mserver_aux::savePersistentDataBeforeDbRestore(
                    commonModule()->resourcePool()->getAdministrator(),
                    m_mediaServer,
                    nx::mserver_aux::createServerSettingsProxy(commonModule()).get()
                ).saveToSettings(qnServerModule->roSettings());
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

    if (const auto manager = commonModule()->moduleDiscoveryManager())
        manager->stop();

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
    QString disabledVendors = qnServerModule->roSettings()->value(DV_PROPERTY).toString();
    if (!disabledVendors.isNull())
    {
        qnGlobalSettings->setDisabledVendors(disabledVendors);
        qnServerModule->roSettings()->remove(DV_PROPERTY);
    }
}

void MediaServerProcess::updateAllowCameraCHangesIfNeed()
{
    static const QString DV_PROPERTY = QLatin1String("cameraSettingsOptimization");

    QString allowCameraChanges = qnServerModule->roSettings()->value(DV_PROPERTY).toString();
    if (!allowCameraChanges.isEmpty())
    {
        qnGlobalSettings->setCameraSettingsOptimizationEnabled(allowCameraChanges.toLower() == lit("yes") || allowCameraChanges.toLower() == lit("true") || allowCameraChanges == lit("1"));
        qnServerModule->roSettings()->setValue(DV_PROPERTY, "");
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
        commonModule()->ec2Connection()->getMediaServerManager(Qn::kSystemAccess)->save(server, this, &MediaServerProcess::at_serverSaved);

    nx::network::SocketGlobals::addressPublisher().updateAddresses(std::list<SocketAddress>(
        serverAddresses.begin(), serverAddresses.end()));
}

void MediaServerProcess::loadResourcesFromECS(
    ec2::AbstractECConnectionPtr ec2Connection,
    QnCommonMessageProcessor* messageProcessor)
{
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
            const auto dictionary = commonModule()->serverAdditionalAddressesDictionary();
            dictionary->setAdditionalUrls(mediaServer.id, additionalAddresses);
            dictionary->setIgnoredUrls(mediaServer.id, ignoredAddressesById.values(mediaServer.id));
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
                    const auto auth = QnNetworkResource::getResourceAuth(commonModule(), camera.id, camera.typeId);
                    manualCameras.insert(camera.url,
                        QnManualCameraInfo(QUrl(camera.url), auth, resType->getName()));
                }
                else
                {
                    NX_ASSERT(false, lm("No resourse type in the pool %1").arg(camera.typeId));
                }
            }
        }
        commonModule()->resourceDiscoveryManager()->registerManualCameras(manualCameras);
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
        commonModule()->cameraHistoryPool()->resetServerFootageData(serverFootageData);
        commonModule()->cameraHistoryPool()->setHistoryCheckDelay(1000);
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
        vms::event::RuleList rules;
        while( (rez = ec2Connection->getBusinessEventManager(Qn::kSystemAccess)->getBusinessRulesSync(&rules)) != ec2::ErrorCode::ok )
        {
            qDebug() << "QnMain::run(): Can't get business rules. Reason: " << ec2::toString(rez);
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            if (m_needStop)
                return;
        }

        for (const auto& rule: rules)
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

    // Start receiving local notifications
    auto processor = dynamic_cast<QnServerMessageProcessor*> (commonModule()->messageProcessor());
    processor->startReceivingLocalNotifications(ec2Connection);
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

    if (m_mediaServer->getPanicMode() == Qn::PM_BusinessEvents)
        server->setPanicMode(Qn::PM_None);

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

    server->saveParams();

    #ifdef ENABLE_EXTENDED_STATISTICS
        qnServerDb->setBookmarkCountController(
            [server](size_t count)
            {
                server->setProperty(Qn::BOOKMARK_COUNT, QString::number(count));
                server->saveParams();
            });
    #endif
}

void MediaServerProcess::at_updatePublicAddress(const QHostAddress& publicIp)
{
    if (isStopping())
        return;

    QnPeerRuntimeInfo localInfo = commonModule()->runtimeInfoManager()->localInfo();
    localInfo.data.publicIP = publicIp.toString();
    commonModule()->runtimeInfoManager()->updateLocalItem(localInfo);

    const auto& resPool = commonModule()->resourcePool();
    QnMediaServerResourcePtr server = resPool->getResourceById<QnMediaServerResource>(commonModule()->moduleGUID());
    if (server)
    {
        Qn::ServerFlags serverFlags = server->getServerFlags();
        if (publicIp.isNull())
            serverFlags &= ~Qn::SF_HasPublicIP;
        else
            serverFlags |= Qn::SF_HasPublicIP;
        if (serverFlags != server->getServerFlags())
        {
            server->setServerFlags(serverFlags);
            ec2::AbstractECConnectionPtr ec2Connection = commonModule()->ec2Connection();

            ec2::ApiMediaServerData apiServer;
            fromResourceToApi(server, apiServer);
            ec2Connection->getMediaServerManager(Qn::kSystemAccess)->save(apiServer, this, [] {});
        }

        if (server->setProperty(Qn::PUBLIC_IP, publicIp.toString(), QnResource::NO_ALLOW_EMPTY))
            server->saveParams();

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
        auto it = m_forwardedAddresses.emplace(mappedAddress.address, 0).first;
        if (it->second != mappedAddress.port)
        {
            NX_LOGX(lit("New external address %1 has been mapped")
                    .arg(address), cl_logALWAYS);

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

    const auto& resPool = commonModule()->resourcePool();
    if (m_firstRunningTime)
    {
        qnEventRuleConnector->at_serverFailure(
            resPool->getResourceById<QnMediaServerResource>(serverGuid()),
            m_firstRunningTime * 1000,
            nx::vms::event::EventReason::serverStarted,
            QString());
    }
    if (!m_startMessageSent)
    {
        qnEventRuleConnector->at_serverStarted(
            resPool->getResourceById<QnMediaServerResource>(serverGuid()),
            qnSyncTime->currentUSecsSinceEpoch());
        m_startMessageSent = true;
    }
    m_firstRunningTime = 0;
}

void MediaServerProcess::at_serverModuleConflict(nx::vms::discovery::ModuleEndpoint module)
{
    const auto& resPool = commonModule()->resourcePool();
    qnEventRuleConnector->at_serverConflict(
        resPool->getResourceById<QnMediaServerResource>(commonModule()->moduleGUID()),
        qnSyncTime->currentUSecsSinceEpoch(),
        module,
        QUrl(lit("http://%1").arg(module.endpoint.toString())));
}

void MediaServerProcess::at_timer()
{
    if (isStopping())
        return;

    // TODO: #2.4 #GDM This timer make two totally different functions. Split it.
    qnServerModule->setLastRunningTime(
        std::chrono::milliseconds(qnSyncTime->currentMSecsSinceEpoch()));

    const auto& resPool = commonModule()->resourcePool();
    QnResourcePtr mServer = resPool->getResourceById(commonModule()->moduleGUID());
    if (!mServer)
        return;

    for(const auto& camera: resPool->getAllCameras(mServer, true))
        camera->cleanCameraIssues();
}

void MediaServerProcess::at_storageManager_noStoragesAvailable() {
    if (isStopping())
        return;
    qnEventRuleConnector->at_noStorages(m_mediaServer);
}

void MediaServerProcess::at_storageManager_storageFailure(const QnResourcePtr& storage,
    nx::vms::event::EventReason reason)
{
    if (isStopping())
        return;
    qnEventRuleConnector->at_storageFailure(m_mediaServer, qnSyncTime->currentUSecsSinceEpoch(), reason, storage);
}

void MediaServerProcess::at_storageManager_rebuildFinished(QnSystemHealth::MessageType msgType) {
    if (isStopping())
        return;
    qnEventRuleConnector->at_archiveRebuildFinished(m_mediaServer, msgType);
}

void MediaServerProcess::at_archiveBackupFinished(
    qint64                      backedUpToMs,
    nx::vms::event::EventReason code
)
{
    if (isStopping())
        return;

    qnEventRuleConnector->at_archiveBackupFinished(
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
    qnEventRuleConnector->at_cameraIPConflict(
        m_mediaServer,
        host,
        macAddrList,
        qnSyncTime->currentUSecsSinceEpoch());
}

void MediaServerProcess::registerRestHandlers(
    CloudManagerGroup* cloudManagerGroup,
    QnUniversalTcpListener* tcpListener,
    ec2::TransactionMessageBusAdapter* messageBus)
{
    auto processorPool = tcpListener->processorPool();
    const auto welcomePage = lit("/static/index.html");
    processorPool->registerRedirectRule(lit(""), welcomePage);
    processorPool->registerRedirectRule(lit("/"), welcomePage);
    processorPool->registerRedirectRule(lit("/static"), welcomePage);
    processorPool->registerRedirectRule(lit("/static/"), welcomePage);

    auto reg =
        [this, processorPool](
            const QString& path,
            QnRestRequestHandler* handler,
            Qn::GlobalPermission permissions = Qn::NoGlobalPermissions)
        {
            processorPool->registerHandler(path, handler, permissions);

            const auto& cameraIdUrlParams = handler->cameraIdUrlParams();
            if (!cameraIdUrlParams.isEmpty())
                m_autoRequestForwarder->addCameraIdUrlParams(path, cameraIdUrlParams);
        };

    // TODO: When supported by apidoctool, the comment to these constants should be parsed.
    const auto kAdmin = Qn::GlobalAdminPermission;
    const auto kViewLogs = Qn::GlobalViewLogsPermission;

    reg("api/storageStatus", new QnStorageStatusRestHandler());
    reg("api/storageSpace", new QnStorageSpaceRestHandler());
    reg("api/statistics", new QnStatisticsRestHandler());
    reg("api/getCameraParam", new QnCameraSettingsRestHandler());
    reg("api/setCameraParam", new QnCameraSettingsRestHandler());
    reg("api/manualCamera", new QnManualCameraAdditionRestHandler());
    reg("api/ptz", new QnPtzRestHandler());
    reg("api/image", new QnImageRestHandler()); //< deprecated
    reg("api/createEvent", new QnExternalEventRestHandler());
    static const char kGetTimePath[] = "api/gettime";
    reg(kGetTimePath, new QnTimeRestHandler());
    reg("ec2/getTimeOfServers", new QnMultiserverTimeRestHandler(QLatin1String("/") + kGetTimePath));
    reg("api/getTimeZones", new QnGetTimeZonesRestHandler());
    reg("api/getNonce", new QnGetNonceRestHandler());
    reg("api/getRemoteNonce", new QnGetNonceRestHandler(lit("/api/getNonce")));
    reg("api/cookieLogin", new QnCookieLoginRestHandler());
    reg("api/cookieLogout", new QnCookieLogoutRestHandler());
    reg("api/getCurrentUser", new QnCurrentUserRestHandler());
    reg("api/activateLicense", new QnActivateLicenseRestHandler());
    reg("api/testEmailSettings", new QnTestEmailSettingsHandler());
    reg("api/getHardwareInfo", new QnGetHardwareInfoHandler());
    reg("api/testLdapSettings", new QnTestLdapSettingsHandler());
    reg("api/ping", new QnPingRestHandler());
    reg(rest::helper::P2pStatistics::kUrlPath, new QnP2pStatsRestHandler());
    reg("api/recStats", new QnRecordingStatsRestHandler());
    reg("api/auditLog", new QnAuditLogRestHandler(), kAdmin);
    reg("api/checkDiscovery", new QnCanAcceptCameraRestHandler());
    reg("api/pingSystem", new QnPingSystemRestHandler());
    reg("api/changeCameraPassword", new QnChangeCameraPasswordRestHandler(), kAdmin);
    reg("api/rebuildArchive", new QnRebuildArchiveRestHandler());
    reg("api/backupControl", new QnBackupControlRestHandler());
    reg("api/events", new QnEventLogRestHandler(), kViewLogs); //< deprecated, still used in the client
    reg("api/getEvents", new QnEventLog2RestHandler(), kViewLogs); //< new version
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

    reg("api/downloads/", new QnDownloadsRestHandler());

    reg("api/settime", new QnSetTimeRestHandler(), kAdmin); //< deprecated
    reg("api/setTime", new QnSetTimeRestHandler(), kAdmin); //< new version

    reg("api/moduleInformationAuthenticated", new QnModuleInformationRestHandler());
    reg("api/configure", new QnConfigureRestHandler(messageBus), kAdmin);
    reg("api/detachFromCloud", new QnDetachFromCloudRestHandler(
        &cloudManagerGroup->connectionManager), kAdmin);
    reg("api/detachFromSystem", new QnDetachFromSystemRestHandler(
        &cloudManagerGroup->connectionManager, messageBus), kAdmin);
    reg("api/restoreState", new QnRestoreStateRestHandler(), kAdmin);
    reg("api/setupLocalSystem", new QnSetupLocalSystemRestHandler(), kAdmin);
    reg("api/setupCloudSystem", new QnSetupCloudSystemRestHandler(cloudManagerGroup), kAdmin);
    reg("api/mergeSystems", new QnMergeSystemsRestHandler(messageBus), kAdmin);
    reg("api/backupDatabase", new QnBackupDbRestHandler());
    reg("api/discoveredPeers", new QnDiscoveredPeersRestHandler());
    reg("api/logLevel", new QnLogLevelRestHandler());
    reg("api/execute", new QnExecScript(), kAdmin);
    reg("api/scriptList", new QnScriptListRestHandler(), kAdmin);
    reg("api/systemSettings", new QnSystemSettingsHandler());

    reg("api/transmitAudio", new QnAudioTransmissionRestHandler());

    // TODO: Introduce constants for API methods registered here, also use them in
    // media_server_connection.cpp. Get rid of static/global urlPath passed to some handler ctors,
    // except when it is the path of some other api method.

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

    #if defined(_DEBUG)
        reg("api/debugEvent", new QnDebugEventsRestHandler());
    #endif

    reg("ec2/runtimeInfo", new QnRuntimeInfoRestHandler());

    static const char kGetHardwareIdsPath[] = "api/getHardwareIds";
    reg(kGetHardwareIdsPath, new QnGetHardwareIdsRestHandler());
    reg("ec2/getHardwareIdsOfServers", new QnMultiserverGetHardwareIdsRestHandler(QLatin1String("/") + kGetHardwareIdsPath));
}

template<class TcpConnectionProcessor, typename... ExtraParam>
void MediaServerProcess::regTcp(
    const QByteArray& protocol, const QString& path, ExtraParam... extraParam)
{
    m_universalTcpListener->addHandler<TcpConnectionProcessor>(
        protocol, path, extraParam...);

    if (TcpConnectionProcessor::doesPathEndWithCameraId())
        m_autoRequestForwarder->addAllowedProtocolAndPathPart(protocol, path);
}

bool MediaServerProcess::initTcpListener(
    CloudManagerGroup* const cloudManagerGroup,
    ec2::TransactionMessageBusAdapter* messageBus)
{
    m_autoRequestForwarder.reset( new QnAutoRequestForwarder(commonModule()));
    m_autoRequestForwarder->addPathToIgnore(lit("/ec2/*"));

    const int rtspPort = qnServerModule->roSettings()->value(
        nx_ms_conf::SERVER_PORT, nx_ms_conf::DEFAULT_SERVER_PORT).toInt();

    // Accept SSL connections in all cases as it is always in use by cloud modules and old clients,
    // config value only affects server preference listed in moduleInformation.
    bool acceptSslConnections = true;
    int maxConnections = qnServerModule->roSettings()->value(
        "maxConnections", QnTcpListener::DEFAULT_MAX_CONNECTIONS).toInt();
    NX_INFO(this) lit("Using maxConnections = %1.").arg(maxConnections);

    m_universalTcpListener = new QnUniversalTcpListener(
        commonModule(),
        cloudManagerGroup->connectionManager,
        QHostAddress::Any,
        rtspPort,
        maxConnections,
        acceptSslConnections);

    m_universalTcpListener->httpModManager()->addCustomRequestMod(std::bind(
        &QnAutoRequestForwarder::processRequest,
        m_autoRequestForwarder.get(),
        std::placeholders::_1));

    #if defined(ENABLE_ACTI)
        QnActiResource::setEventPort(rtspPort);
        // Used to receive event from an acti camera.
        // TODO: Remove this from api.
        m_universalTcpListener->processorPool()->registerHandler(
            "api/camera_event", new QnActiEventRestHandler());
    #endif

    registerRestHandlers(cloudManagerGroup, m_universalTcpListener, messageBus);

    if (!m_universalTcpListener->bindToLocalAddress())
        return false;
    m_universalTcpListener->setDefaultPage("/static/index.html");

    // Server returns code 403 (forbidden) instead of 401 if the user isn't authorized for requests
    // starting with "web" path.
    m_universalTcpListener->setPathIgnorePrefix("web/");
    QnAuthHelper::instance()->restrictionList()->deny(lit("/web/.+"), nx_http::AuthMethod::http);

    nx_http::AuthMethod::Values methods = (nx_http::AuthMethod::Values) (
        nx_http::AuthMethod::cookie |
        nx_http::AuthMethod::urlQueryParam |
        nx_http::AuthMethod::tempUrlQueryParam);
    QnUniversalRequestProcessor::setUnauthorizedPageBody(
        QnFileConnectionProcessor::readStaticFile("static/login.html"), methods);
    regTcp<QnRtspConnectionProcessor>("RTSP", "*");
    regTcp<QnRestConnectionProcessor>("HTTP", "api");
    regTcp<QnRestConnectionProcessor>("HTTP", "ec2");
    regTcp<QnFileConnectionProcessor>("HTTP", "static");
    regTcp<QnCrossdomainConnectionProcessor>("HTTP", "crossdomain.xml");
    regTcp<QnProgressiveDownloadingConsumer>("HTTP", "media");
    regTcp<QnIOMonitorConnectionProcessor>("HTTP", "api/iomonitor");

    nx_hls::QnHttpLiveStreamingProcessor::setMinPlayListSizeToStartStreaming(
        qnServerModule->roSettings()->value(
        nx_ms_conf::HLS_PLAYLIST_PRE_FILL_CHUNKS,
        nx_ms_conf::DEFAULT_HLS_PLAYLIST_PRE_FILL_CHUNKS).toInt());
    regTcp<nx_hls::QnHttpLiveStreamingProcessor>("HTTP", "hls");

    // Our HLS uses implementation uses authKey (generated by target server) to skip authorization,
    // to keep this worning we should not ask for authrorization along the way.
    m_universalTcpListener->enableUnauthorizedForwarding("hls");

    //regTcp<QnDefaultTcpConnectionProcessor>("HTTP", "*");

    regTcp<QnProxyConnectionProcessor>("*", "proxy", messageBus);
    //regTcp<QnProxyReceiverConnection>("PROXY", "*");
    regTcp<QnProxyReceiverConnection>("HTTP", "proxy-reverse");
    regTcp<QnAudioProxyReceiver>("HTTP", "proxy-2wayaudio");

    if( !qnServerModule->roSettings()->value("authenticationEnabled", "true").toBool())
        m_universalTcpListener->disableAuth();

    #if defined(ENABLE_DESKTOP_CAMERA)
        regTcp<QnDesktopCameraRegistrator>("HTTP", "desktop_camera");
    #endif

    return true;
}

void MediaServerProcess::initializeCloudConnect()
{
    nx::network::SocketGlobals::outgoingTunnelPool()
        .assignOwnPeerId("ms", commonModule()->moduleGUID());

    nx::network::SocketGlobals::addressPublisher().setRetryInterval(
        nx::utils::parseTimerDuration(
            qnServerModule->roSettings()->value(MEDIATOR_ADDRESS_UPDATE).toString(),
            nx::network::cloud::MediatorAddressPublisher::kDefaultRetryInterval));

    connect(
        commonModule()->globalSettings(), &QnGlobalSettings::cloudConnectUdpHolePunchingEnabledChanged,
        [this]()
        {
            nx::network::cloud::TunnelAcceptorFactory::instance().setUdpHolePunchingEnabled(
                commonModule()->globalSettings()->cloudConnectUdpHolePunchingEnabled());
        });

    connect(
        commonModule()->globalSettings(), &QnGlobalSettings::cloudConnectRelayingEnabledChanged,
        [this]()
        {
            nx::network::cloud::TunnelAcceptorFactory::instance().setRelayingEnabled(
                commonModule()->globalSettings()->cloudConnectRelayingEnabled());
        });
}

std::unique_ptr<nx_upnp::PortMapper> MediaServerProcess::initializeUpnpPortMapper()
{
    auto mapper = std::make_unique<nx_upnp::PortMapper>(
        /*isEnabled*/ false,
        nx_upnp::PortMapper::DEFAULT_CHECK_MAPPINGS_INTERVAL,
        QnAppInfo::organizationName());
    auto updateEnabled =
        [mapper = mapper.get(), this]()
        {
            const auto& settings = commonModule()->globalSettings();
            const auto isCloudSystem = !settings->cloudSystemId().isEmpty();
            mapper->setIsEnabled(isCloudSystem && settings->isUpnpPortMappingEnabled());
        };

    const auto& settings = commonModule()->globalSettings();
    connect(settings, &QnGlobalSettings::upnpPortMappingEnabledChanged, updateEnabled);
    connect(settings, &QnGlobalSettings::cloudSettingsChanged, updateEnabled);
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

    const QString appserverHostString = qnServerModule->roSettings()->value("appserverHost").toString();
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
    m_ipDiscovery.reset(new nx::network::PublicIPDiscovery(
        qnServerModule->roSettings()->value(nx_ms_conf::PUBLIC_IP_SERVERS).toString().split(";", QString::SkipEmptyParts)));

    if (qnServerModule->roSettings()->value("publicIPEnabled").isNull())
        qnServerModule->roSettings()->setValue("publicIPEnabled", 1);

    int publicIPEnabled = qnServerModule->roSettings()->value("publicIPEnabled").toInt();
    if (publicIPEnabled == 0)
        return; // disabled
    else if (publicIPEnabled > 1)
    {
        auto staticIp = qnServerModule->roSettings()->value("staticPublicIP").toString();
        at_updatePublicAddress(QHostAddress(staticIp)); // manually added
        return;
    }
    m_ipDiscovery->update();
    m_ipDiscovery->waitForFinished();
    at_updatePublicAddress(m_ipDiscovery->publicIP());

    m_updatePiblicIpTimer.reset(new QTimer());
    connect(m_updatePiblicIpTimer.get(), &QTimer::timeout, m_ipDiscovery.get(), &nx::network::PublicIPDiscovery::update);
    connect(m_ipDiscovery.get(), &nx::network::PublicIPDiscovery::found, this, &MediaServerProcess::at_updatePublicAddress);
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
        if (!cloudConnectionManager.detachSystemFromCloud())
        {
            qWarning() << "Error while clearing cloud information. Trying again...";
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            continue;
        }

        if (!resetSystemToStateNew(commonModule()))
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
    const auto& resPool = commonModule()->resourcePool();
    for (const auto& server: resPool->getResources<QnMediaServerResource>())
        servers << server->getId();
    ec2::ApiCameraDataList camerasToUpdate;
    for (const auto& camera: resPool->getAllCameras(/*all*/ QnResourcePtr()))
    {
        if (!servers.contains(camera->getParentId()))
        {
            ec2::ApiCameraData apiCameraData;
            fromResourceToApi(camera, apiCameraData);
            apiCameraData.parentId = commonModule()->moduleGUID(); //< move camera
            camerasToUpdate.push_back(apiCameraData);
        }
    }

    auto errCode = commonModule()->ec2Connection()
        ->getCameraManager(Qn::kSystemAccess)
        ->addCamerasSync(camerasToUpdate);

    if (errCode != ec2::ErrorCode::ok)
        qWarning() << "Failed to move handling cameras due to database error. errCode=" << toString(errCode);
}

void MediaServerProcess::updateAllowedInterfaces()
{
    // check registry
    QString ifList = qnServerModule->roSettings()->value(lit("if")).toString();
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

QString MediaServerProcess::hardwareIdAsGuid() const
{
    auto hwId = LLUtil::getLatestHardwareId();
    auto hwIdString = QnUuid::fromHardwareId(hwId).toString();
    std::cout << "Got hwID \"" << hwIdString.toStdString() << "\"" << std::endl;
    return hwIdString;
}

void MediaServerProcess::updateGuidIfNeeded()
{
    QString guidIsHWID = qnServerModule->roSettings()->value(GUID_IS_HWID).toString();
    QString serverGuid = qnServerModule->roSettings()->value(SERVER_GUID).toString();
    QString serverGuid2 = qnServerModule->roSettings()->value(SERVER_GUID2).toString();
    QString pendingSwitchToClusterMode = qnServerModule->roSettings()->value(PENDING_SWITCH_TO_CLUSTER_MODE).toString();

    QString hwidGuid = hardwareIdAsGuid();

    if (guidIsHWID == YES) {
        if (serverGuid.isEmpty())
            qnServerModule->roSettings()->setValue(SERVER_GUID, hwidGuid);
        else if (serverGuid != hwidGuid)
            qnServerModule->roSettings()->setValue(GUID_IS_HWID, NO);

        qnServerModule->roSettings()->remove(SERVER_GUID2);
    }
    else if (guidIsHWID == NO) {
        if (serverGuid.isEmpty()) {
            // serverGuid remove from settings manually?
            qnServerModule->roSettings()->setValue(SERVER_GUID, hwidGuid);
            qnServerModule->roSettings()->setValue(GUID_IS_HWID, YES);
        }

        qnServerModule->roSettings()->remove(SERVER_GUID2);
    }
    else if (guidIsHWID.isEmpty()) {
        if (!serverGuid2.isEmpty()) {
            qnServerModule->roSettings()->setValue(SERVER_GUID, serverGuid2);
            qnServerModule->roSettings()->setValue(GUID_IS_HWID, NO);
            qnServerModule->roSettings()->remove(SERVER_GUID2);
        }
        else {
            // Don't reset serverGuid if we're in pending switch to cluster mode state.
            // As it's stored in the remote database.
            if (pendingSwitchToClusterMode == YES)
                return;

            qnServerModule->roSettings()->setValue(SERVER_GUID, hwidGuid);
            qnServerModule->roSettings()->setValue(GUID_IS_HWID, YES);

            if (!serverGuid.isEmpty()) {
                qnServerModule->roSettings()->setValue(OBSOLETE_SERVER_GUID, serverGuid);
            }
        }
    }

    QnUuid obsoleteGuid = QnUuid(qnServerModule->roSettings()->value(OBSOLETE_SERVER_GUID).toString());
    if (!obsoleteGuid.isNull())
        setObsoleteGuid(obsoleteGuid);
}

void MediaServerProcess::serviceModeInit()
{
    const auto settings = qnServerModule->roSettings();
    const auto binaryPath = QFile::decodeName(m_argv[0]);

    nx::utils::log::Settings logSettings;
    logSettings.maxBackupCount = settings->value("logArchiveSize", DEFAULT_LOG_ARCHIVE_SIZE).toUInt();
    logSettings.directory = settings->value("logDir").toString();
    logSettings.maxFileSize = settings->value("maxLogFileSize", DEFAULT_MAX_LOG_FILE_SIZE).toUInt();
    logSettings.updateDirectoryIfEmpty(getDataDirectory());

    // TODO: Generalize nx::utils::log::Settings parsing from QSetting and cmdLineArguments
    // for mediaserver and all clients.
    const auto makeLevel =
        [&](const QString& argValue, const QString& settingsKey, const QString& defaultValue)
        {
            const auto settingsValue = settings->value(settingsKey);
            const auto settingsLevel = nx::utils::log::levelFromString(settingsValue.toString());
            if (settingsLevel != nx::utils::log::Level::undefined)
                return settingsLevel;

            const auto argLevel = nx::utils::log::levelFromString(argValue);
            if (argLevel != nx::utils::log::Level::undefined)
                return argLevel;

            const auto defaultLevel = nx::utils::log::levelFromString(defaultValue);
            if (defaultLevel != nx::utils::log::Level::undefined)
                return defaultLevel;

            return nx::utils::log::Settings::kDefaultLevel;
        };

    for (const auto& filter: cmdLineArguments().exceptionFilters.split(L';', QString::SkipEmptyParts))
        logSettings.exceptionFilers.insert(filter);

    logSettings.level = makeLevel(cmdLineArguments().logLevel, "logLevel", "");
    nx::utils::log::initialize(logSettings, qApp->applicationName(), binaryPath);

    if (auto path = nx::utils::log::mainLogger()->filePath())
        settings->setValue("logFile", path->replace(lit(".log"), QString()));
    else
        settings->remove("logFile");

    logSettings.level = makeLevel(cmdLineArguments().httpLogLevel, "http-log-level", "none");
    nx::utils::log::initialize(
        logSettings, qApp->applicationName(), binaryPath,
        QLatin1String("http_log"), nx::utils::log::addLogger({QnLog::HTTP_LOG_INDEX}));

    logSettings.level = makeLevel(cmdLineArguments().hwLogLevel, "hwLoglevel", "info");
    nx::utils::log::initialize(
        logSettings, qApp->applicationName(), binaryPath,
        QLatin1String("hw_log"), nx::utils::log::addLogger({QnLog::HWID_LOG}));

    logSettings.level = makeLevel(cmdLineArguments().ec2TranLogLevel, "tranLogLevel", "none");
    nx::utils::log::initialize(
        logSettings, qApp->applicationName(), binaryPath,
        QLatin1String("ec2_tran"), nx::utils::log::addLogger({QnLog::EC2_TRAN_LOG}));

    logSettings.level = makeLevel(cmdLineArguments().permissionsLogLevel, "permissionsLogLevel", "none");
    nx::utils::log::initialize(
        logSettings, qApp->applicationName(), binaryPath,
        QLatin1String("permissions"), nx::utils::log::addLogger({QnLog::PERMISSIONS_LOG}));

    defaultMsgHandler = qInstallMessageHandler(myMsgHandler);

    LLUtil::initHardwareId(qnServerModule->roSettings());
    updateGuidIfNeeded();
    setHardwareGuidList(LLUtil::getAllHardwareIds().toVector());

    QnUuid guid = serverGuid();
    if (guid.isNull())
    {
        qDebug() << "Can't save guid. Run once as administrator.";
        NX_LOG("Can't save guid. Run once as administrator.", cl_logERROR);
        qApp->quit();
        return;
    }
}

void MediaServerProcess::connectArchiveIntegrityWatcher()
{
    using namespace nx::mediaserver;
    auto serverArchiveIntegrityWatcher = static_cast<ServerArchiveIntegrityWatcher*>(
        qnServerModule->archiveIntegrityWatcher());

    connect(
        serverArchiveIntegrityWatcher,
        &ServerArchiveIntegrityWatcher::fileIntegrityCheckFailed,
        qnEventRuleConnector,
        &event::EventConnector::atFileIntegrityCheckFailed);
}

void MediaServerProcess::run()
{
    std::shared_ptr<QnMediaServerModule> serverModule(new QnMediaServerModule(
        m_cmdLineArguments.enforcedMediatorEndpoint,
        m_cmdLineArguments.configFilePath,
        m_cmdLineArguments.rwConfigFilePath));

    qnServerModule->runTimeSettings()->remove("rebuild");

    if (m_serviceMode)
        serviceModeInit();

    updateAllowedInterfaces();

    if (!m_cmdLineArguments.enforceSocketType.isEmpty())
        SocketFactory::enforceStreamSocketType(m_cmdLineArguments.enforceSocketType);
    auto ipVersion = m_cmdLineArguments.ipVersion;
    if (ipVersion.isEmpty())
        ipVersion = qnServerModule->roSettings()->value(QLatin1String("ipVersion")).toString();

    SocketFactory::setIpVersion(ipVersion);

    m_serverModule = serverModule;

    if (!m_obsoleteGuid.isNull())
        commonModule()->setObsoleteServerGuid(m_obsoleteGuid);

    if (!m_cmdLineArguments.engineVersion.isNull())
    {
        qWarning() << "Starting with overridden version: " << m_cmdLineArguments.engineVersion;
        qnStaticCommon->setEngineVersion(QnSoftwareVersion(m_cmdLineArguments.engineVersion));
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
    win32_exception::setCreateFullCrashDump( qnServerModule->roSettings()->value(
        nx_ms_conf::CREATE_FULL_CRASH_DUMP,
        nx_ms_conf::DEFAULT_CREATE_FULL_CRASH_DUMP ).toBool() );
#endif

#ifdef __linux__
    linux_exception::setSignalHandlingDisabled( qnServerModule->roSettings()->value(
        nx_ms_conf::CREATE_FULL_CRASH_DUMP,
        nx_ms_conf::DEFAULT_CREATE_FULL_CRASH_DUMP ).toBool() );
#endif

    const auto allowedSslVersions = qnServerModule->roSettings()->value(
        nx_ms_conf::ALLOWED_SSL_VERSIONS, QString()).toString();
    if (!allowedSslVersions.isEmpty())
        nx::network::ssl::Engine::setAllowedServerVersions(allowedSslVersions.toUtf8());

    const auto allowedSslCiphers = qnServerModule->roSettings()->value(
        nx_ms_conf::ALLOWED_SSL_CIPHERS, QString()).toString();
    if (!allowedSslCiphers.isEmpty())
        nx::network::ssl::Engine::setAllowedServerCiphers(allowedSslCiphers.toUtf8());

    nx::network::ssl::Engine::useOrCreateCertificate(
        qnServerModule->roSettings()->value(
            nx_ms_conf::SSL_CERTIFICATE_PATH,
            getDataDirectory() + lit( "/ssl/cert.pem")).toString(),
        nx::utils::AppInfo::productName().toUtf8(), "US",
        nx::utils::AppInfo::organizationName().toUtf8());

    commonModule()->createMessageProcessor<QnServerMessageProcessor>();
    std::unique_ptr<HostSystemPasswordSynchronizer> hostSystemPasswordSynchronizer( new HostSystemPasswordSynchronizer(commonModule()) );
    std::unique_ptr<QnServerDb> serverDB(new QnServerDb(commonModule()));
    auto auditManager = std::make_unique<QnMServerAuditManager>(
        qnServerModule->lastRunningTime(), commonModule());

    TimeBasedNonceProvider timeBasedNonceProvider;
    CloudManagerGroup cloudManagerGroup(commonModule(), &timeBasedNonceProvider);
    auto authHelper = std::make_unique<QnAuthHelper>(
        commonModule(),
        &timeBasedNonceProvider,
        &cloudManagerGroup);
    connect(QnAuthHelper::instance(), &QnAuthHelper::emptyDigestDetected, this, &MediaServerProcess::at_emptyDigestDetected);

    configureApiRestrictions(QnAuthHelper::instance()->restrictionList());

    std::unique_ptr<mediaserver::event::RuleProcessor> eventRuleProcessor(
        new mediaserver::event::ExtendedRuleProcessor(commonModule()));

    std::unique_ptr<QnVideoCameraPool> videoCameraPool( new QnVideoCameraPool(commonModule()) );

    std::unique_ptr<QnMotionHelper> motionHelper(new QnMotionHelper());

    std::unique_ptr<mediaserver::event::EventConnector> eventConnector(
        new mediaserver::event::EventConnector(commonModule()) );
    auto stopQThreadFunc = []( QThread* obj ){ obj->quit(); obj->wait(); delete obj; };
    std::unique_ptr<QThread, decltype(stopQThreadFunc)> connectorThread( new QThread(), stopQThreadFunc );
    connectorThread->start();
    qnEventRuleConnector->moveToThread(connectorThread.get());

    CameraDriverRestrictionList cameraDriverRestrictionList;

    QSettings* settings = qnServerModule->roSettings();

    commonModule()->setResourceDiscoveryManager(new QnMServerResourceDiscoveryManager(commonModule()));
    QUrl appServerUrl = appServerConnectionUrl(*settings);

    QnMulticodecRtpReader::setDefaultTransport( qnServerModule->roSettings()->value(QLatin1String("rtspTransport"), RtpTransport::_auto).toString().toUpper() );

    connect(commonModule()->resourceDiscoveryManager(), &QnResourceDiscoveryManager::CameraIPConflict, this, &MediaServerProcess::at_cameraIPConflict);
    connect(qnNormalStorageMan, &QnStorageManager::noStoragesAvailable, this, &MediaServerProcess::at_storageManager_noStoragesAvailable);
    connect(qnNormalStorageMan, &QnStorageManager::storageFailure, this, &MediaServerProcess::at_storageManager_storageFailure);
    connect(qnNormalStorageMan, &QnStorageManager::rebuildFinished, this, &MediaServerProcess::at_storageManager_rebuildFinished);

    connect(qnBackupStorageMan, &QnStorageManager::storageFailure, this, &MediaServerProcess::at_storageManager_storageFailure);
    connect(qnBackupStorageMan, &QnStorageManager::rebuildFinished, this, &MediaServerProcess::at_storageManager_rebuildFinished);
    connect(qnBackupStorageMan, &QnStorageManager::backupFinished, this, &MediaServerProcess::at_archiveBackupFinished);

    connectArchiveIntegrityWatcher();

    auto remoteArchiveSynchronizer =
        std::make_unique<nx::mediaserver_core::recorder::RemoteArchiveSynchronizer>(qnServerModule);

    // If adminPassword is set by installer save it and create admin user with it if not exists yet
    commonModule()->setDefaultAdminPassword(settings->value(APPSERVER_PASSWORD, QLatin1String("")).toString());
    commonModule()->setUseLowPriorityAdminPasswordHack(settings->value(LOW_PRIORITY_ADMIN_PASSWORD, false).toBool());

    BeforeRestoreDbData beforeRestoreDbData;
    beforeRestoreDbData.loadFromSettings(settings);
    commonModule()->setBeforeRestoreData(beforeRestoreDbData);

    commonModule()->setModuleGUID(serverGuid());

    initializeCloudConnect();

    bool compatibilityMode = m_cmdLineArguments.devModeKey == lit("razrazraz");
    const QString appserverHostString = qnServerModule->roSettings()->value("appserverHost").toString();

    commonModule()->setSystemIdentityTime(nx::ServerSetting::getSysIdTime(), commonModule()->moduleGUID());
    connect(commonModule(), &QnCommonModule::systemIdentityTimeChanged, this, &MediaServerProcess::at_systemIdentityTimeChanged, Qt::QueuedConnection);

    ec2::ApiRuntimeData runtimeData;
    runtimeData.peer.id = commonModule()->moduleGUID();
    runtimeData.peer.instanceId = commonModule()->runningInstanceGUID();
    runtimeData.peer.persistentId = commonModule()->dbId();
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
    commonModule()->runtimeInfoManager()->updateLocalItem(runtimeData);    // initializing localInfo

    std::unique_ptr<ec2::AbstractECConnectionFactory> ec2ConnectionFactory(
        getConnectionFactory(
            Qn::PT_Server,
            nx::utils::TimerManager::instance(),
            commonModule()));

    MediaServerStatusWatcher mediaServerStatusWatcher(commonModule());
    QScopedPointer<QnConnectToCloudWatcher> connectToCloudWatcher(new QnConnectToCloudWatcher(ec2ConnectionFactory->messageBus()));

    //passing settings
    std::map<QString, QVariant> confParams;
    for( const auto& paramName: qnServerModule->roSettings()->allKeys() )
    {
        if( paramName.startsWith( lit("ec") ) )
            confParams.emplace( paramName, qnServerModule->roSettings()->value( paramName ) );
    }
    ec2ConnectionFactory->setConfParams(std::move(confParams));
    ec2::AbstractECConnectionPtr ec2Connection;
    QnConnectionInfo connectInfo;
    std::unique_ptr<ec2::QnDiscoveryMonitor> discoveryMonitor;

    while (!needToStop())
    {
        const ec2::ErrorCode errorCode = ec2ConnectionFactory->connectSync(
            appServerUrl, ec2::ApiClientInfoData(), &ec2Connection );
        if (ec2Connection)
        {
            connectInfo = ec2Connection->connectionInfo();
            auto connectionResult = QnConnectionValidator::validateConnection(connectInfo, errorCode);
            if (connectionResult == Qn::SuccessConnectionResult)
            {
                discoveryMonitor = std::make_unique<ec2::QnDiscoveryMonitor>(
                    ec2ConnectionFactory->messageBus());

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
    QnAppServerConnectionFactory::setEc2Connection(ec2Connection);

    const auto& runtimeManager = commonModule()->runtimeInfoManager();
    connect(
        runtimeManager, &QnRuntimeInfoManager::runtimeInfoAdded,
        this, &MediaServerProcess::at_runtimeInfoChanged, Qt::QueuedConnection);
    connect(
        runtimeManager, &QnRuntimeInfoManager::runtimeInfoChanged,
        this, &MediaServerProcess::at_runtimeInfoChanged, Qt::QueuedConnection);

    if (needToStop())
        return; //TODO #ak correctly deinitialize what has been initialised

    qnServerModule->roSettings()->setValue(QnServer::kRemoveDbParamName, "0");

    connect(ec2Connection.get(), &ec2::AbstractECConnection::databaseDumped, this, &MediaServerProcess::at_databaseDumped);
    commonModule()->setRemoteGUID(connectInfo.serverId());
    qnServerModule->roSettings()->sync();
    if (qnServerModule->roSettings()->value(PENDING_SWITCH_TO_CLUSTER_MODE).toString() == "yes")
    {
        NX_LOG( QString::fromLatin1("Switching to cluster mode and restarting..."), cl_logWARNING );
        nx::SystemName systemName(connectInfo.systemName);
        systemName.saveToConfig(); //< migrate system name from foreign database via config
        nx::ServerSetting::setSysIdTime(0);
        qnServerModule->roSettings()->remove("appserverHost");
        qnServerModule->roSettings()->remove("appserverLogin");
        qnServerModule->roSettings()->setValue(APPSERVER_PASSWORD, "");
        qnServerModule->roSettings()->remove(PENDING_SWITCH_TO_CLUSTER_MODE);
        qnServerModule->roSettings()->sync();

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

    connect(
        ec2Connection->getTimeNotificationManager().get(),
        &ec2::AbstractTimeNotificationManager::timeChanged,
        this,
        &MediaServerProcess::at_timeChanged,
        Qt::QueuedConnection);
    std::unique_ptr<QnMServerResourceSearcher> mserverResourceSearcher(new QnMServerResourceSearcher(commonModule()));

    auto pluginManager = qnServerModule->pluginManager();
    for (const auto storagePlugin :
         pluginManager->findNxPlugins<nx_spl::StorageFactory>(nx_spl::IID_StorageFactory))
    {
        QnStoragePluginFactory::instance()->registerStoragePlugin(
            storagePlugin->storageType(),
            std::bind(
                &QnThirdPartyStorageResource::instance,
                std::placeholders::_1,
                std::placeholders::_2,
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

    if (qnServerModule->roSettings()->value("disableTranscoding").toBool())
        commonModule()->setTranscodeDisabled(true);

    QnResource::startCommandProc();

    std::unique_ptr<nx_hls::HLSSessionPool> hlsSessionPool( new nx_hls::HLSSessionPool() );

    if (!initTcpListener(&cloudManagerGroup, ec2ConnectionFactory->messageBus()))
    {
        qCritical() << "Failed to bind to local port. Terminating...";
        QCoreApplication::quit();
        return;
    }

    if (appServerUrl.scheme().toLower() == lit("file"))
        ec2ConnectionFactory->registerRestHandlers(m_universalTcpListener->processorPool());

    std::unique_ptr<QnMulticast::HttpServer> multicastHttp(new QnMulticast::HttpServer(commonModule()->moduleGUID().toQUuid(), m_universalTcpListener));

    using namespace std::placeholders;
    m_universalTcpListener->setProxyHandler<QnProxyConnectionProcessor>(
        &QnUniversalRequestProcessor::isProxy,
        ec2ConnectionFactory->messageBus());
    auto processor = dynamic_cast<QnServerMessageProcessor*> (commonModule()->messageProcessor());
    processor->registerProxySender(m_universalTcpListener);

    ec2ConnectionFactory->registerTransactionListener( m_universalTcpListener );

    const bool sslAllowed =
        qnServerModule->roSettings()->value(
            nx_ms_conf::ALLOW_SSL_CONNECTIONS,
            nx_ms_conf::DEFAULT_ALLOW_SSL_CONNECTIONS).toBool();

    bool foundOwnServerInDb = false;

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
            server = QnMediaServerResourcePtr(new QnMediaServerResource(commonModule()));
            server->setId(serverGuid());
            server->setMaxCameras(DEFAULT_MAX_CAMERAS);

            QString serverName(getDefaultServerName());
            if (!beforeRestoreDbData.serverName.isEmpty())
                serverName = QString::fromLocal8Bit(beforeRestoreDbData.serverName);
            server->setName(serverName);
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
        server->setVersion(qnStaticCommon->engineVersion());

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
                    commonModule()->beforeRestoreDbData(),
                    foundOwnServerInDb,
                    qnServerModule->roSettings()->value(NO_SETUP_WIZARD).toInt() > 0));
        }
        else
        {
            m_mediaServer = server;
        }

        if (m_mediaServer.isNull())
            QnSleep::msleep(1000);
    }

    /* This key means that password should be forcibly changed in the database. */
    qnServerModule->roSettings()->remove(OBSOLETE_SERVER_GUID);
    qnServerModule->roSettings()->setValue(APPSERVER_PASSWORD, "");
#ifdef _DEBUG
    qnServerModule->roSettings()->sync();
    NX_ASSERT(qnServerModule->roSettings()->value(APPSERVER_PASSWORD).toString().isEmpty(), Q_FUNC_INFO, "appserverPassword is not emptyu in registry. Restart the server as Administrator");
#endif

    if (needToStop())
    {
        stopObjects();
        m_ipDiscovery.reset();
        return;
    }
    const auto& resPool = commonModule()->resourcePool();
    resPool->addResource(m_mediaServer);

    QString moduleName = qApp->applicationName();
    if( moduleName.startsWith( qApp->organizationName() ) )
        moduleName = moduleName.mid( qApp->organizationName().length() ).trimmed();

    QnModuleInformation selfInformation = commonModule()->moduleInformation();
    if (compatibilityMode)
    {
        selfInformation.brand = QString();
        selfInformation.customization = QString();
    }
    selfInformation.version = qnStaticCommon->engineVersion();
    selfInformation.sslAllowed = sslAllowed;
    selfInformation.serverFlags = m_mediaServer->getServerFlags();
    selfInformation.ecDbReadOnly = ec2Connection->connectionInfo().ecDbReadOnly;

    commonModule()->setModuleInformation(selfInformation);
    commonModule()->bindModuleinformation(m_mediaServer);

    // show our cloud host value in registry in case of installer will check it
    const auto& globalSettings = commonModule()->globalSettings();
    qnServerModule->roSettings()->setValue(QnServer::kIsConnectedToCloudKey,
        globalSettings->cloudSystemId().isEmpty() ? "no" : "yes");
    qnServerModule->roSettings()->setValue("cloudHost", selfInformation.cloudHost);

    if (!m_cmdLineArguments.allowedDiscoveryPeers.isEmpty()) {
        QSet<QnUuid> allowedPeers;
        for (const QString &peer: m_cmdLineArguments.allowedDiscoveryPeers.split(";")) {
            QnUuid peerId(peer);
            if (!peerId.isNull())
                allowedPeers << peerId;
        }
        commonModule()->setAllowedPeers(allowedPeers);
    }

    connect(commonModule()->moduleDiscoveryManager(), &nx::vms::discovery::Manager::conflict,
        this, &MediaServerProcess::at_serverModuleConflict);

    QScopedPointer<QnServerConnector> serverConnector(new QnServerConnector(commonModule()));

    // ------------------------------------------

    QScopedPointer<QnServerUpdateTool> serverUpdateTool(new QnServerUpdateTool(commonModule()));
    serverUpdateTool->removeUpdateFiles(m_mediaServer->getVersion().toString());

    // ===========================================================================
    QnResource::initAsyncPoolInstance()->setMaxThreadCount( qnServerModule->roSettings()->value(
        nx_ms_conf::RESOURCE_INIT_THREADS_COUNT,
        nx_ms_conf::DEFAULT_RESOURCE_INIT_THREADS_COUNT ).toInt() );
    QnResource::initAsyncPoolInstance();

    // ============================
    GlobalSettingsToDeviceSearcherSettingsAdapter upnpDeviceSearcherSettings(globalSettings);
    auto upnpDeviceSearcher = std::make_unique<nx_upnp::DeviceSearcher>(upnpDeviceSearcherSettings);
    std::unique_ptr<QnMdnsListener> mdnsListener(new QnMdnsListener());

    std::unique_ptr<QnAppserverResourceProcessor> serverResourceProcessor( new QnAppserverResourceProcessor(
        commonModule(),
        ec2ConnectionFactory->distributedMutex(),
        m_mediaServer->getId()) );
    std::unique_ptr<QnRecordingManager> recordingManager(new QnRecordingManager(
        commonModule(),
        ec2ConnectionFactory->distributedMutex()));
    serverResourceProcessor->moveToThread(commonModule()->resourceDiscoveryManager());
    commonModule()->resourceDiscoveryManager()->setResourceProcessor(serverResourceProcessor.get());

    std::unique_ptr<QnResourceStatusWatcher> statusWatcher( new QnResourceStatusWatcher(commonModule()));

    /* Searchers must be initialized before the resources are loaded as resources instances are created by searchers. */
    auto resourceSearchers = std::make_unique<QnMediaServerResourceSearchers>(commonModule());

    std::unique_ptr<QnAudioStreamerPool> audioStreamerPool(new QnAudioStreamerPool(commonModule()));
    auto flirExecutor = std::make_unique<nx::plugins::flir::IoExecutor>();

    auto upnpPortMapper = initializeUpnpPortMapper();

    commonModule()->resourceAccessManager()->beginUpdate();
    commonModule()->resourceAccessProvider()->beginUpdate();
    loadResourcesFromECS(ec2Connection, commonModule()->messageProcessor());
    qnServerModule->metadataManagerPool()->init();
    at_runtimeInfoChanged(runtimeManager->localInfo());

    saveServerInfo(m_mediaServer);
    m_mediaServer->setStatus(Qn::Online);

    if (m_cmdLineArguments.moveHandlingCameras)
        moveHandlingCameras();

    commonModule()->resourceAccessProvider()->endUpdate();
    commonModule()->resourceAccessManager()->endUpdate();

    globalSettings->initialize();

    updateAddressesList();

    auto settingsProxy = nx::mserver_aux::createServerSettingsProxy(commonModule());
    auto systemNameProxy = nx::mserver_aux::createServerSystemNameProxy();

    nx::mserver_aux::setUpSystemIdentity(commonModule()->beforeRestoreDbData(), settingsProxy.get(), std::move(systemNameProxy));

    BeforeRestoreDbData::clearSettings(settings);

    addFakeVideowallUser(commonModule());

    if (!qnServerModule->roSettings()->value(QnServer::kNoInitStoragesOnStartup, false).toBool())
        initStoragesAsync(commonModule()->messageProcessor());

    if (!QnPermissionsHelper::isSafeMode(commonModule()))
    {
        if (nx::mserver_aux::needToResetSystem(
                    nx::mserver_aux::isNewServerInstance(
                        commonModule()->beforeRestoreDbData(),
                        foundOwnServerInDb,
                        qnServerModule->roSettings()->value(NO_SETUP_WIZARD).toInt() > 0),
                    settingsProxy.get()))
        {
            if (settingsProxy->isCloudInstanceChanged())
                qWarning() << "Cloud instance changed from" << globalSettings->cloudHost() <<
                    "to" << nx::network::AppInfo::defaultCloudHost() << ". Server goes to the new state";

            resetSystemState(cloudManagerGroup.connectionManager);
        }
        if (settingsProxy->isCloudInstanceChanged())
        {
            ec2::ErrorCode errCode;
            do
            {
                const bool kCleanupDbObjects = false;
                const bool kCleanupTransactionLog = true;

                errCode = commonModule()->ec2Connection()
                    ->getMiscManager(Qn::kSystemAccess)
                    ->cleanupDatabaseSync(kCleanupDbObjects, kCleanupTransactionLog);

                if (errCode != ec2::ErrorCode::ok)
                {
                    qWarning() << "Error while rebuild transaction log. Trying again...";
                        msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
                }

            } while (errCode != ec2::ErrorCode::ok && !m_needStop);
        }
        globalSettings->setCloudHost(nx::network::AppInfo::defaultCloudHost());
        globalSettings->synchronizeNow();
    }

    globalSettings->takeFromSettings(qnServerModule->roSettings(), m_mediaServer);

    if (QnUserResourcePtr adminUser = resPool->getAdministrator())
    {
        //todo: root password for NX1 should be updated in case of cloud owner
        hostSystemPasswordSynchronizer->syncLocalHostRootPasswordWithAdminIfNeeded( adminUser );
    }
    qnServerModule->roSettings()->sync();

#ifndef EDGE_SERVER
    // TODO: #GDM make this the common way with other settings
    updateDisabledVendorsIfNeeded();
    updateAllowCameraCHangesIfNeed();
    qnGlobalSettings->synchronizeNowSync(); // TODO: #GDM double sync
#endif

    std::unique_ptr<QnLdapManager> ldapManager(new QnLdapManager(commonModule()));


    commonModule()->resourceDiscoveryManager()->setReady(true);
    const bool isDiscoveryDisabled =
        qnServerModule->roSettings()->value(QnServer::kNoResourceDiscovery, false).toBool();
    if( !ec2Connection->connectionInfo().ecDbReadOnly && !isDiscoveryDisabled)
        commonModule()->resourceDiscoveryManager()->start();
    //else
    //    we are not able to add cameras to DB anyway, so no sense to do discover


    connect(
        commonModule()->resourceDiscoveryManager(),
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

    m_firstRunningTime = qnServerModule->lastRunningTime().count();

    m_crashReporter.reset(new ec2::CrashReporter(commonModule()));

    QTimer timer;
    connect(&timer, SIGNAL(timeout()), this, SLOT(at_timer()), Qt::DirectConnection);
    timer.start(QnVirtualCameraResource::issuesTimeoutMs());
    at_timer();

    QTimer udtInternetTrafficTimer;
    connect(&udtInternetTrafficTimer, &QTimer::timeout,
        [common = commonModule()]()
        {
            QnResourcePtr server = common->resourcePool()->getResourceById(common->moduleGUID());
            const auto old = server->getProperty(Qn::UDT_INTERNET_TRFFIC).toULongLong();
            const auto current = nx::network::UdtStatistics::global.internetBytesTransfered.load();
            const auto update = old + (qulonglong) current;
            if (server->setProperty(Qn::UDT_INTERNET_TRFFIC, QString::number(update))
                && server->saveParams())
            {
                NX_LOG(lm("%1 is updated to %2").args(Qn::UDT_INTERNET_TRFFIC, update), cl_logDEBUG1);
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
        mserverResourceSearcher->start();
    m_universalTcpListener->start();
    serverConnector->start();
#if 1
    if (ec2Connection->connectionInfo().ecUrl.scheme() == "file") {
        // Connect to local database. Start peer-to-peer sync (enter to cluster mode)
        commonModule()->setCloudMode(true);
        if (!isDiscoveryDisabled)
        {
            // Should be called after global settings are initialized.
            commonModule()->moduleDiscoveryManager()->start();
        }
    }
#endif

    nx::mserver_aux::makeFakeData(
        cmdLineArguments().createFakeData, ec2Connection, commonModule()->moduleGUID());

    qnBackupStorageMan->scheduleSync()->start();
    serverModule->unusedWallpapersWatcher()->start();
    if (m_serviceMode)
        serverModule->licenseWatcher()->start();

    emit started();
    exec();

    disconnect(QnAuthHelper::instance(), 0, this, 0);
    disconnect(commonModule()->resourceDiscoveryManager(), 0, this, 0);
    disconnect(qnNormalStorageMan, 0, this, 0);
    disconnect(qnBackupStorageMan, 0, this, 0);
    disconnect(commonModule(), 0, this, 0);
    disconnect(runtimeManager, 0, this, 0);
    disconnect(ec2Connection->getTimeNotificationManager().get(), 0, this, 0);
    disconnect(ec2Connection.get(), 0, this, 0);
    if (m_updatePiblicIpTimer) {
        disconnect(m_updatePiblicIpTimer.get(), 0, this, 0);
        m_updatePiblicIpTimer.reset();
    }
    disconnect(m_ipDiscovery.get(), 0, this, 0);
    disconnect(commonModule()->moduleDiscoveryManager(), 0, this, 0);

    WaitingForQThreadToEmptyEventQueue waitingForObjectsToBeFreed( QThread::currentThread(), 3 );
    waitingForObjectsToBeFreed.join();

    qWarning()<<"QnMain event loop has returned. Destroying objects...";

    discoveryMonitor.reset();
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
    commonModule()->resourceDiscoveryManager()->pleaseStop();
    QnResource::pleaseStopAsyncTasks();
    multicastHttp.reset();
    stopObjects();

    QnResource::stopCommandProc();
    if (m_initStoragesAsyncPromise)
        m_initStoragesAsyncPromise->get_future().wait();
    // todo: #rvasilenko some undeleted resources left in the QnMain event loop. I stopped TimerManager as temporary solution for it.
    nx::utils::TimerManager::instance()->stop();

    hlsSessionPool.reset();

    recordingManager.reset();

    mserverResourceSearcher.reset();

    videoCameraPool.reset();

    commonModule()->resourceDiscoveryManager()->stop();
    QnResource::stopAsyncTasks();

    //since mserverResourceDiscoveryManager instance is dead no events can be delivered to serverResourceProcessor: can delete it now
        //TODO refactoring of discoveryManager <-> resourceProcessor interaction is required
    serverResourceProcessor.reset();

    mdnsListener.reset();
    resourceSearchers.reset();
    upnpDeviceSearcher.reset();

    connectorThread->quit();
    connectorThread->wait();

    //deleting object from wrong thread, but its no problem, since object's thread has been stopped and no event can be delivered to the object
    eventConnector.reset();

    eventRuleProcessor.reset();

    motionHelper.reset();
    remoteArchiveSynchronizer.reset();

    qnNormalStorageMan->stopAsyncTasks();
    qnBackupStorageMan->stopAsyncTasks();

    //ptzPool.reset();

    commonModule()->deleteMessageProcessor(); // stop receiving notifications

    //disconnecting from EC2
    clearEc2ConnectionGuard.reset();

    connectToCloudWatcher.reset();
    ec2Connection.reset();
    ec2ConnectionFactory.reset();

    commonModule()->setResourceDiscoveryManager(nullptr);

    // This method will set flag on message channel to threat next connection close as normal
    //appServerConnection->disconnectSync();
    qnServerModule->setLastRunningTime(std::chrono::milliseconds::zero());

    authHelper.reset();
    //fileDeletor.reset();
    //qnNormalStorageMan.reset();
    //qnBackupStorageMan.reset();

    if (m_mediaServer)
        m_mediaServer->beforeDestroy();
    m_mediaServer.clear();

    performActionsOnExit();

    nx::network::SocketGlobals::outgoingTunnelPool().clearOwnPeerId();

    m_autoRequestForwarder.reset();

    if (defaultMsgHandler)
        qInstallMessageHandler(defaultMsgHandler);
}

void MediaServerProcess::at_appStarted()
{
    if (isStopping())
        return;

    commonModule()->messageProcessor()->init(commonModule()->ec2Connection()); // start receiving notifications
    m_crashReporter->scanAndReportByTimer(qnServerModule->runTimeSettings());

    QString dataLocation = getDataDirectory();
    QDir stateDirectory;
    stateDirectory.mkpath(dataLocation + QLatin1String("/state"));
    qnFileDeletor->init(dataLocation + QLatin1String("/state")); // constructor got root folder for temp files
};

void MediaServerProcess::at_timeChanged(qint64 newTime)
{
    QnSyncTime::instance()->updateTime(newTime);

    using namespace ec2;
    QnTransaction<ApiPeerSyncTimeData> tran(
        ApiCommand::broadcastPeerSyncTime,
        commonModule()->moduleGUID());
    tran.params.syncTimeMs = newTime;
    if (auto connection = commonModule()->ec2Connection())
        connection->messageBus()->sendTransaction(tran);
}

void MediaServerProcess::at_runtimeInfoChanged(const QnPeerRuntimeInfo& runtimeInfo)
{
    if (isStopping())
        return;
    if (runtimeInfo.uuid != commonModule()->moduleGUID())
        return;
    auto connection = commonModule()->ec2Connection();
    if (connection)
    {
        ec2::QnTransaction<ec2::ApiRuntimeData> tran(
            ec2::ApiCommand::runtimeInfoChanged,
            commonModule()->moduleGUID());
        tran.params = runtimeInfo.data;
        commonModule()->ec2Connection()->messageBus()->sendTransaction(tran);
    }
}

void MediaServerProcess::at_emptyDigestDetected(const QnUserResourcePtr& user, const QString& login, const QString& password)
{
    if (isStopping())
        return;

    // fill authenticate digest here for compatibility with version 2.1 and below.
    const ec2::AbstractECConnectionPtr& appServerConnection = commonModule()->ec2Connection();
    if (user->getDigest().isEmpty() && !m_updateUserRequests.contains(user->getId()))
    {
        user->setName(login);
        user->setPasswordAndGenerateHash(password);

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
        m_main.reset(new MediaServerProcess(m_argc, m_argv, true));

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
            m_main->serverSettings()->roSettings()->value(nx_ms_conf::ENABLE_MULTIPLE_INSTANCES).toInt() == 0)
        {
            qWarning() << "Server already started";
            qApp->quit();
            return;
        }

#ifdef Q_OS_WIN
        SetConsoleCtrlHandler(stopServer_WIN, true);
#endif
        signal(SIGINT, stopServer);
        signal(SIGTERM, stopServer);

        QDir::setCurrent(qApp->applicationDirPath());

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
    int m_argc;
    char **m_argv;
    QScopedPointer<MediaServerProcess> m_main;
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

void MediaServerProcess::configureApiRestrictions(nx_http::AuthMethodRestrictionList* restrictions)
{
    // For "OPTIONS * RTSP/1.0"
    restrictions->allow(lit("."), nx_http::AuthMethod::noAuth);

    const auto webPrefix = lit("(/web)?(/proxy/[^/]*(/[^/]*)?)?");
    restrictions->allow(webPrefix + lit("/api/ping"), nx_http::AuthMethod::noAuth);
    restrictions->allow(webPrefix + lit("/api/camera_event.*"), nx_http::AuthMethod::noAuth);
    restrictions->allow(webPrefix + lit("/api/showLog.*"), nx_http::AuthMethod::urlQueryParam);
    restrictions->allow(webPrefix + lit("/api/moduleInformation"), nx_http::AuthMethod::noAuth);
    restrictions->allow(webPrefix + lit("/api/gettime"), nx_http::AuthMethod::noAuth);
    restrictions->allow(webPrefix + lit("/api/getTimeZones"), nx_http::AuthMethod::noAuth);
    restrictions->allow(webPrefix + lit("/api/getNonce"), nx_http::AuthMethod::noAuth);
    restrictions->allow(webPrefix + lit("/api/cookieLogin"), nx_http::AuthMethod::noAuth);
    restrictions->allow(webPrefix + lit("/api/cookieLogout"), nx_http::AuthMethod::noAuth);
    restrictions->allow(webPrefix + lit("/api/getCurrentUser"), nx_http::AuthMethod::noAuth);
    restrictions->allow(webPrefix + lit("/static/.*"), nx_http::AuthMethod::noAuth);
    restrictions->allow(lit("/crossdomain.xml"), nx_http::AuthMethod::noAuth);
    restrictions->allow(webPrefix + lit("/api/startLiteClient"), nx_http::AuthMethod::noAuth);

    // TODO: #3.1 Remove this method and use /api/installUpdate in client when offline cloud
    // authentication is implemented.
    // WARNING: This is severe vulnerability introduced in 3.0.
    restrictions->allow(webPrefix + lit("/api/installUpdateUnauthenticated"),
        nx_http::AuthMethod::noAuth);
}
