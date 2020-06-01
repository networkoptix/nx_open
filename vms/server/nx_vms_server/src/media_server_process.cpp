#include "media_server_process.h"

#include <functional>
#include <signal.h>
#if defined(__linux__)
    #include <signal.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <sys/prctl.h>
    #include <unistd.h>
#endif

#include <boost/optional.hpp>

#include <qtsinglecoreapplication.h>
#include <qtservice.h>

#include <QtCore/QStringLiteral>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <QtCore/QUrl>
#include <QtCore/QThreadPool>
#include <QtConcurrent/QtConcurrent>
#include <nx/utils/uuid.h>
#include <utils/common/ldap.h>
#include <QtCore/QThreadPool>

#include <QtNetwork/QUdpSocket>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QHostInfo>
#include <QtNetwork/QNetworkInterface>

#include <api/global_settings.h>
#include <nx/analytics/db/analytics_db.h>

#include <appserver/processor.h>

#include <nx/vms/event/rule.h>
#include <nx/vms/event/events/reasoned_event.h>
#include <nx/vms/utils/vms_utils.h>

#include <core/misc/schedule_task.h>

#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/providers/resource_access_provider.h>

#include <core/resource_management/camera_driver_restriction_list.h>
#include <core/resource_management/mserver_resource_discovery_manager.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/server_additional_addresses_dictionary.h>

#include <core/resource/layout_resource.h>
#include <core/resource/media_server_user_attributes.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/camera_resource.h>

#include <media_server/media_server_app_info.h>
#include <media_server/server_message_processor.h>
#include <media_server/settings.h>
#include <api/app_server_connection.h>
#include <media_server/file_connection_processor.h>
#include <media_server/crossdomain_connection_processor.h>
#include <media_server/media_server_resource_searchers.h>
#include <media_server/media_server_module.h>

#include <nx/vms/auth/time_based_nonce_provider.h>
#include <nx/vms/server/authenticator.h>
#include <nx/vms/server/rest/get_merge_status_handler.h>
#include <network/connection_validator.h>
#include <network/default_tcp_connection_processor.h>
#include <network/system_helpers.h>

#include <nx_ec/ec_api.h>
#include <nx/vms/api/data/user_data.h>
#include <nx_ec/managers/abstract_user_manager.h>
#include <nx_ec/managers/abstract_layout_manager.h>
#include <nx_ec/managers/abstract_videowall_manager.h>
#include <nx_ec/managers/abstract_webpage_manager.h>
#include <nx_ec/managers/abstract_camera_manager.h>
#include <nx_ec/managers/abstract_server_manager.h>
#include <nx_ec/managers/abstract_analytics_manager.h>
#include <nx/network/socket.h>
#include <nx/network/ssl/ssl_engine.h>
#include <nx/network/udt/udt_socket.h>

#include <nx/kit/output_redirector.h>

#include <camera_vendors.h>

#include <plugins/plugin_manager.h>

#include <plugins/resource/desktop_camera/desktop_camera_registrator.h>

#include <plugins/storage/file_storage/file_storage_resource.h>
#include <core/storage/file_storage/db_storage_resource.h>
#include <plugins/storage/third_party_storage_resource/third_party_storage_resource.h>

#include <recorder/file_deletor.h>
#include <recorder/storage_manager.h>
#include <recorder/schedule_sync.h>

#include <nx/vms/server/rest/exec_event_action_rest_handler.h>
#include <rest/handlers/acti_event_rest_handler.h>
#include <rest/handlers/event_log_rest_handler.h>
#include <rest/handlers/event_log2_rest_handler.h>
#include <rest/handlers/multiserver_events_rest_handler.h>
#include <rest/handlers/get_system_name_rest_handler.h>
#include <rest/handlers/camera_diagnostics_rest_handler.h>
#include <rest/handlers/camera_settings_rest_handler.h>
#include <rest/handlers/debug_handler.h>
#include <rest/handlers/ini_config_handler.h>
#include <rest/handlers/external_event_rest_handler.h>
#include <rest/handlers/favicon_rest_handler.h>
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
#include <rest/handlers/update_information_rest_handler.h>
#include <rest/handlers/start_update_rest_handler.h>
#include <rest/handlers/finish_update_rest_handler.h>
#include <rest/handlers/update_status_rest_handler.h>
#include <rest/handlers/install_update_rest_handler.h>
#include <rest/handlers/cancel_update_rest_handler.h>
#include <rest/handlers/retry_update.h>
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
#include <rest/handlers/settings_documentation_handler.h>
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
#include <rest/handlers/multiserver_analytics_lookup_object_tracks.h>
#include <rest/handlers/execute_analytics_action_rest_handler.h>
#include <rest/handlers/get_analytics_actions_rest_handler.h>
#include <rest/server/rest_connection_processor.h>
#include <rest/server/options_request_handler.h>
#include <rest/handlers/get_hardware_info_rest_handler.h>
#include <rest/handlers/system_settings_handler.h>
#include <rest/handlers/audio_transmission_rest_handler.h>
#include <rest/handlers/start_lite_client_rest_handler.h>
#include <rest/handlers/runtime_info_rest_handler.h>
#include <rest/handlers/downloads_rest_handler.h>
#include <rest/handlers/get_hardware_ids_rest_handler.h>
#include <rest/handlers/multiserver_get_hardware_ids_rest_handler.h>
#include <rest/handlers/wearable_camera_rest_handler.h>
#include <rest/handlers/set_primary_time_server_rest_handler.h>
#ifdef _DEBUG
#include <rest/handlers/debug_events_rest_handler.h>
#endif
#include <nx/vms/server/rest/device_analytics_settings_handler.h>
#include <nx/vms/server/rest/analytics_engine_settings_handler.h>
#include <nx/vms/server/rest/get_time_handler.h>
#include <nx/vms/server/rest/server_time_handler.h>
#include <nx/vms/server/rest/plugin_info_handler.h>
#include <nx/vms/server/rest/multiserver_plugin_info_handler.h>
#include <nx/vms/server/rest/nvr_network_block_handler.h>
#include <nx/vms/server/rest/get_statistics_report_handler.h>
#include <nx/vms/server/rest/trigger_statistics_report_handler.h>
#include <nx/vms/server/nvr/i_service.h>

#include <rtsp/rtsp_connection.h>

#include <nx/vms/server/http_audio/request_processor.h>

#include <nx/vms/discovery/manager.h>
#include <nx/vms/utils/initial_data_loader.h>
#include <network/multicodec_rtp_reader.h>
#include <network/router.h>

#include <utils/common/command_line_parser.h>
#include <nx/utils/event_loop_timer.h>
#include <nx/utils/app_info.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/rlimit.h>
#include <utils/common/app_info.h>
#include <utils/common/sleep.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include <nx/network/socket_global.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/cloud/tunnel/outgoing_tunnel_pool.h>
#include <nx/network/cloud/tunnel/tunnel_acceptor_factory.h>
#include <nx/network/cloud/mediator_address_publisher.h>
#include <nx/vms/utils/system_helpers.h>

#include <utils/common/app_info.h>
#include <transcoding/ffmpeg_video_transcoder.h>

#include <nx/utils/crash_dump/systemexcept.h>

#include "platform/hardware_information.h"
#include "platform/platform_abstraction.h"
#include <nx/vms/server/ptz/server_ptz_controller_pool.h>
#include "plugins/resource/acti/acti_resource.h"
#include "common/common_module.h"
#include <nx/vms/network/reverse_connection_listener.h>
#include <nx/vms/network/proxy_connection.h>
#include <nx/vms/time_sync/server_time_sync_manager.h>
#include "llutil/hardware_id.h"
#include "api/runtime_info_manager.h"
#include "rest/handlers/old_client_connect_rest_handler.h"
#include "nx_ec/data/api_conversion_functions.h"
#include "nx_ec/dummy_handler.h"

#include "core/resource_management/resource_properties.h"
#include "core/resource/network_resource.h"
#include "network/universal_request_processor.h"
#include "core/resource/camera_history.h"
#include <nx/network/nettools.h>
#include "http/iomonitor_tcp_server.h"
#include "rest/handlers/multiserver_chunks_rest_handler.h"
#include "rest/handlers/merge_ldap_users_rest_handler.h"
#include "utils/common/waiting_for_qthread_to_empty_event_queue.h"
#include "crash_reporter.h"
#include "rest/handlers/exec_script_rest_handler.h"
#include "rest/handlers/script_list_rest_handler.h"
#include "cloud/cloud_integration_manager.h"
#include "rest/handlers/backup_control_rest_handler.h"
#include <server/server_globals.h>
#include <nx/vms/server/unused_wallpapers_watcher.h>
#include <nx/vms/server/license_watcher.h>
#include <nx/vms/server/videowall_license_watcher.h>
#include <rest/helpers/permissions_helper.h>
#include "misc/migrate_oldwin_dir.h"
#include <common/static_common_module.h>
#include <recorder/storage_db_pool.h>
#include <transaction/message_bus_adapter.h>
#include <rest/helper/p2p_statistics.h>
#include <recorder/archive_integrity_watcher.h>
#include <nx/utils/std/cpp14.h>
#include <nx/vms/server/analytics/manager.h>
#include <nx/vms/server/analytics/sdk_object_factory.h>
#include <nx/utils/platform/current_process.h>
#include <rest/handlers/change_camera_password_rest_handler.h>
#include <nx/vms/server/fs/media_paths/media_paths_filter_config.h>
#include <nx/vms/common/p2p/downloader/downloader.h>
#include <nx/vms/server/root_fs.h>
#include <system_log/raid_event_ini_config.h>
#include <nx/vms/utils/resource_params_data.h>

#include <nx/vms/server/update/update_manager.h>
#include <nx_vms_server_ini.h>
#include <proxy/2wayaudio/proxy_audio_receiver.h>
#include <local_connection_factory.h>
#include <core/resource/resource_command_processor.h>
#include <rest/handlers/sync_time_rest_handler.h>
#include <rest/handlers/metrics_rest_handler.h>
#include <nx/vms/server/event/event_connector.h>
#include <nx/vms/server/event/extended_rule_processor.h>
#include <nx/vms/server/event/server_runtime_event_manager.h>
#include <nx/network/http/http_client.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource/storage_plugin_factory.h>
#include <nx/analytics/utils.h>

#include <providers/speech_synthesis_data_provider.h>
#include <nx/utils/file_system.h>
#include <nx/network/url/url_builder.h>

#include <nx/vms/api/protocol_version.h>

#include "nx/vms/server/system/nx1/info.h"
#include <atomic>

#include <nx/vms/server/nvr/i_service.h>

#include <nx/vms/server/metrics/camera_controller.h>
#include <nx/vms/server/metrics/network_controller.h>
#include <nx/vms/server/metrics/rest_handlers.h>
#include <nx/vms/server/metrics/server_controller.h>
#include <nx/vms/server/metrics/storage_controller.h>
#include <nx/vms/server/metrics/system_controller.h>

using namespace nx::vms::server;

// This constant is used while checking for compatibility.
// Do not change it until you know what you're doing.
static const char COMPONENT_NAME[] = "MediaServer";

static QString SERVICE_NAME = QnServerAppInfo::serviceName();
static const int UDT_INTERNET_TRAFIC_TIMER = 24 * 60 * 60 * 1000; //< Once a day;
//static const quint64 DEFAULT_MSG_LOG_ARCHIVE_SIZE = 5;
static const unsigned int APP_SERVER_REQUEST_ERROR_TIMEOUT_MS = 5500;

class MediaServerProcess;
static std::atomic<MediaServerProcess*> serviceMainInstance = nullptr;
void stopServer(int signal);
static bool gRestartFlag = false;

namespace {

static const std::chrono::seconds kResourceDataReadingTimeout(5);
const QString YES = "yes";
const QString NO = "no";
const QString MEDIATOR_ADDRESS_UPDATE = "mediatorAddressUpdate";

static const int kPublicIpUpdateTimeoutMs = 60 * 2 * 1000;
static nx::utils::log::Tag kLogTag(typeid(MediaServerProcess));

static const int kMinimalGlobalThreadPoolSize = 4;
static const std::chrono::seconds kCheckAnalyticsUsedTimeout(5);

void addFakeVideowallUser(QnCommonModule* commonModule)
{
    nx::vms::api::UserData fakeUserData;
    fakeUserData.realm = nx::network::AppInfo::realm();
    fakeUserData.permissions = GlobalPermission::videowallModePermissions;

    auto fakeUser = ec2::fromApiToResource(fakeUserData);
    fakeUser->setIdUnsafe(Qn::kVideowallUserAccess.userId);
    fakeUser->setName("Video wall");

    commonModule->resourcePool()->addResource(fakeUser);
}

} // namespace

std::unique_ptr<QnStaticCommonModule> MediaServerProcess::m_staticCommonModule;

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

    NX_ERROR(kLogTag, "FFMPEG %1", QString::fromLocal8Bit(szMsg));
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
        NX_ERROR(kLogTag, "Couldn't resolve host %1", hostString);
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
        NX_ERROR(kLogTag, "No ipv4 address associated with host %1", hostString);

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

    // if nothing else works use first enabled hostaddr
    for (const auto& address: allLocalAddresses(nx::network::AddressFilter::onlyFirstIpV4))
    {
        QUdpSocket socket;
        if (!socket.bind(QHostAddress(address.toString()), 0))
            continue;
        const QString result = socket.localAddress().toString();
        NX_ASSERT(result.length() > 0);
        return result;
    }

    return "127.0.0.1";

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

#ifdef Q_OS_WIN
static int freeGB(QString drive)
{
    ULARGE_INTEGER freeBytes;

    GetDiskFreeSpaceEx(drive.toStdWString().c_str(), &freeBytes, 0, 0);

    return freeBytes.HighPart * 4 + (freeBytes.LowPart>> 30);
}
#endif

static StorageResourceList getSmallStorages(const StorageResourceList& storages)
{
    StorageResourceList result;
    for (const auto& storage: storages)
    {
        auto fileStorage = storage.dynamicCast<QnFileStorageResource>();
        if (fileStorage && !fileStorage->isMounted())
            continue;

        const qint64 totalSpace = storage->getTotalSpace();
        if (totalSpace != QnStorageResource::kUnknownSize && totalSpace < storage->getSpaceLimit())
        {
            result << storage;
            NX_VERBOSE(
                kLogTag,
                "Small storage %1, isFileStorage=%2, totalSpace=%3, spaceLimit=%4, toDelete",
                nx::utils::url::hidePassword(storage->getUrl()),
                static_cast<bool>(fileStorage),
                totalSpace,
                storage->getSpaceLimit());
        }
    }

    return result;
}

StorageResourcePtr MediaServerProcess::createStorage(
    const QnUuid& serverId,
    const nx::vms::server::fs::media_paths::Partition& partition)
{
    NX_VERBOSE(kLogTag, lm("Attempting to create storage %1").arg(partition.path));
    auto storage = QnStorageResourcePtr(serverModule()->storagePluginFactory()->createStorage(
        commonModule(),
        "ufile")).dynamicCast<StorageResource>();

    if (!NX_ASSERT(storage))
        return StorageResourcePtr();

    storage->setName("Initial");
    storage->setParentId(serverId);
    storage->setUrl(partition.path);
    storage->fillID();
    storage->setStorageType(QnLexical::serialized(partition.type));

    auto fileStorage = storage.dynamicCast<QnFileStorageResource>();
    if (!fileStorage || fileStorage->initOrUpdate() != Qn::StorageInit_Ok)
    {
        NX_WARNING(this, "Failed to initialize new storage %1", partition.path);
        return StorageResourcePtr();
    }

    calculateSpaceLimitOrLoadFromConfig(commonModule(), fileStorage);
    if (fileStorage->getTotalSpace() < fileStorage->getSpaceLimit())
    {
        NX_DEBUG(
            kLogTag, "Storage with this path %1 total space is unknown or totalSpace < spaceLimit. "
            "Total space: %2, Space limit: %3",
            partition.path, fileStorage->getTotalSpace(), storage->getSpaceLimit());

        return StorageResourcePtr();
    }

    storage->setUsedForWriting(storage->isWritable());
    NX_DEBUG(kLogTag, "Storage %1 is operational: %2", partition.path, storage->isUsedForWriting());

    QnResourceTypePtr resType = qnResTypePool->getResourceTypeByName("Storage");
    NX_ASSERT(resType);
    if (resType)
        storage->setTypeId(resType->getId());

    storage->setParentId(QnUuid(serverModule()->settings().serverGuid()));
    return storage;
}

StorageResourceList MediaServerProcess::createStorages()
{
    StorageResourceList storages;
    qint64 bigStorageThreshold = 0;

    auto partitions = getMediaPartitions(fs::media_paths::FilterConfig::createDefault(
        serverModule()->platform(), /*includeNonHdd*/false, &serverModule()->settings()));

    NX_VERBOSE(this, "Record folders: %1", containerString(partitions));
    for (const auto& p: partitions)
    {
        NX_DEBUG(kLogTag, lm("Available path: %1").arg(p.path));
        if (!m_mediaServer->getStorageByUrl(p.path).isNull())
        {
            NX_DEBUG(kLogTag,
                lm("Storage with this path %1 already exists. Won't be added.").arg(p.path));
            continue;
        }
        // Create new storage because of new partition found that missing in the database
        StorageResourcePtr storage = createStorage(m_mediaServer->getId(), p);
        if (!storage)
            continue;

        qint64 available = storage->getTotalSpace() - storage->getSpaceLimit();
        bigStorageThreshold = qMax(bigStorageThreshold, available);
        storages.append(storage);
        NX_DEBUG(kLogTag, lm("Creating new storage: %1").arg(p.path));
    }
    bigStorageThreshold /= QnStorageManager::kBigStorageTreshold;

    for (int i = 0; i < storages.size(); ++i) {
        QnStorageResourcePtr storage = storages[i].dynamicCast<QnStorageResource>();
        qint64 available = storage->getTotalSpace() - storage->getSpaceLimit();
        if (available < bigStorageThreshold)
            storage->setUsedForWriting(false);
    }

    QString logMessage = "Storage new candidates:\n";
    for (const auto& storage : storages)
    {
        logMessage.append(
            lm("\t\turl: %1, totalSpace: %2, spaceLimit: %3")
                .args(storage->getUrl(), storage->getTotalSpace(), storage->getSpaceLimit()));
    }

    NX_DEBUG(kLogTag, logMessage);
    return storages;
}

StorageResourceList MediaServerProcess::updateStorages(const StorageResourceList& storages)
{
    const auto partitions = serverModule()->platform()->monitor()->totalPartitionSpaceInfo();

    QMap<QnUuid, StorageResourcePtr> result;
    // I've switched all patches to native separator to fix network patches like \\computer\share
    for(const auto& storage: storages)
    {
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
            if (storage->getUrl().contains("://"))
                storageType = QUrl(storage->getUrl()).scheme();
            if (storageType.isEmpty())
            {
                storageType = QnLexical::serialized(nx::vms::server::PlatformMonitor::LocalDiskPartition);
                const auto storagePath = QnStorageResource::toNativeDirPath(storage->getPath());
                const auto it = std::find_if(partitions.begin(), partitions.end(),
                    [&](const nx::vms::server::PlatformMonitor::PartitionSpace& partition)
                { return storagePath.startsWith(QnStorageResource::toNativeDirPath(partition.path)); });
                if (it != partitions.end())
                    storageType = QnLexical::serialized(it->type);
            }
            storage->setStorageType(
                    storageType.isEmpty()
                    ? QnLexical::serialized(nx::vms::server::PlatformMonitor::UnknownPartition)
                    : storageType);
            modified = true;
        }
        if (modified)
            result.insert(storage->getId(), storage);
    }

    QString logMesssage = lm("%1 Modified storages:\n").arg(Q_FUNC_INFO);
    for (const auto& storage : result.values())
    {
        logMesssage.append(
            lm("\t\turl: %1, totalSpace: %2, spaceLimit: %3\n")
                .args(storage->getUrl(), storage->getTotalSpace(), storage->getSpaceLimit()));
    }

    NX_DEBUG(kLogTag, logMesssage);
    return result.values();
}

ec2::AbstractMediaServerManagerPtr MediaServerProcess::serverManager() const
{
    return serverModule()->ec2Connection()->getMediaServerManager(Qn::kSystemAccess);
}

nx::vms::api::StorageDataList MediaServerProcess::loadStorages() const
{
    nx::vms::api::StorageDataList result;
    ec2::ErrorCode rez;
    while ((rez = serverManager()->getStoragesSync(QnUuid(), &result)) != ec2::ErrorCode::ok)
    {
        NX_DEBUG(this, lm("[Storages init] Can't get storage list. Reason: %1").arg(rez));
        QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
        if (m_needStop)
            return nx::vms::api::StorageDataList();
    }

    return result;
}

void MediaServerProcess::removeStorages(const StorageResourceList& storages)
{
    if (storages.empty())
        return;

    nx::vms::api::IdDataList idList;
    for (const auto& s: storages)
        idList.push_back(s->getId());

    if (serverManager()->removeStoragesSync(idList) != ec2::ErrorCode::ok)
        NX_WARNING(this, "[Storages init] Failed to remove storages");
}

StorageResourceList MediaServerProcess::fromDataToStorageList(
    const nx::vms::api::StorageDataList& dataList) const
{
    auto messageProcessor = serverModule()->commonModule()->messageProcessor();
    for (const auto& entry: dataList)
        messageProcessor->updateResource(entry, ec2::NotificationSource::Local);
    const auto qnStorages = m_mediaServer->getStorages();
    StorageResourceList result;
    std::transform(
        qnStorages.cbegin(), qnStorages.cend(), std::back_inserter(result),
        [](const QnStorageResourcePtr& s)
        {
            const auto result = s.dynamicCast<StorageResource>();
            NX_ASSERT(result);
            return result;
        });
    return result;
}

static StorageResourceList subtractLists(
    const StorageResourceList& from,
    const StorageResourceList& what)
{
    StorageResourceList result;
    std::copy_if(
        from.cbegin(), from.cend(), std::back_inserter(result),
        [&what](const auto& s) { return !what.contains(s); });

    return result;
}

StorageResourceList MediaServerProcess::processExistingStorages()
{
    const auto existing = fromDataToStorageList(loadStorages());
    const auto modified = updateStorages(existing);
    const auto tooSmall = getSmallStorages(existing);
    removeStorages(tooSmall);
    saveStorages(subtractLists(modified, tooSmall));
    return subtractLists(existing, tooSmall);
}

class StorageManagerWatcher
{
public:
    StorageManagerWatcher(QnMediaServerModule* serverModule):
        m_serverModule(serverModule)
    {
        m_normalManagerConnection = QObject::connect(
            serverModule->normalStorageManager(),
            &QnStorageManager::storageAdded,
            [this](const QnStorageResourcePtr& storage) { onAdded(storage); });

        m_backupManagerConnection = QObject::connect(
            serverModule->backupStorageManager(),
            &QnStorageManager::storageAdded,
            [this](const QnStorageResourcePtr& storage) { onAdded(storage); });
    }

    void waitForPopulate(const StorageResourceList& expected) const
    {
        using namespace std::chrono;
        const auto expectedUrls = toUrlSet(expected);
        const auto startTime = steady_clock::now();
        while (true)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            NX_MUTEX_LOCKER lock(&m_mutex);
            if (toUrlSet(m_storages).contains(expectedUrls))
            {
                NX_DEBUG(
                    typeid(MediaServerProcess),
                    "[Storages init] All storages have been added to storage manager");
                break;
            }

            if (steady_clock::now() - startTime > 1min)
            {
                NX_ERROR(
                    typeid(MediaServerProcess),
                    "[Storages init] Failed to add all storages to storage manager");
                break;
            }
        }

        QObject::disconnect(m_normalManagerConnection);
        QObject::disconnect(m_backupManagerConnection);
        m_serverModule->normalStorageManager()->initDone();
        m_serverModule->backupStorageManager()->initDone();
    }

private:
    QnMediaServerModule* m_serverModule = nullptr;
    mutable QnMutex m_mutex;
    StorageResourceList m_storages;
    QMetaObject::Connection m_normalManagerConnection;
    QMetaObject::Connection m_backupManagerConnection;

    void onAdded(const QnStorageResourcePtr& storage)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        const auto storageResource = storage.dynamicCast<StorageResource>();
        NX_ASSERT(storageResource);
        if (!m_storages.contains(storageResource))
            m_storages.append(storageResource);
    }

    static QSet<QString> toUrlSet(const StorageResourceList& storages)
    {
        QSet<QString> result;
        for (const auto& s: storages)
            result.insert(s->getUrl());
        return result;
    }
};

void MediaServerProcess::initializeMetaDataStorage()
{
    connect(m_mediaServer.get(), &QnMediaServerResource::propertyChanged, this,
        &MediaServerProcess::at_serverPropertyChanged);

    if (!m_mediaServer->metadataStorageId().isNull() || QFile::exists(getMetadataDatabaseName()))
        initializeAnalyticsEvents();

    NX_DEBUG(this, "[Storages init] Analytics storage DB initialized");
}

void MediaServerProcess::initStoragesAsync(QnCommonMessageProcessor* messageProcessor)
{
    m_initStoragesAsyncPromise.reset(new nx::utils::promise<void>());
    QtConcurrent::run(
        [messageProcessor, this]()
        {
            NX_VERBOSE(this, "[Storages init] Init storages begin");
            const auto setPromiseGuardFunc = nx::utils::makeScopeGuard(
                [&]()
                {
                    NX_VERBOSE(this, "[Storages init] Init storages end");
                    m_initStoragesAsyncPromise->set_value();
                });

            const auto storageManagerWatcher = StorageManagerWatcher(serverModule());
            const auto existing = processExistingStorages();
            const auto newStorages = createStorages();
            saveStorages(newStorages);
            storageManagerWatcher.waitForPopulate(newStorages + existing);
            initializeMetaDataStorage();
            m_storageInitializationDone = true;
        });
}

void MediaServerProcess::startDeletor()
{
    QString dataLocation = serverModule()->settings().dataDir();
    QDir stateDirectory;
    stateDirectory.mkpath(dataLocation + QLatin1String("/state"));
    serverModule()->fileDeletor()->init(dataLocation + QLatin1String("/state")); // constructor got root folder for temp files
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
        id = nx::network::getMacFromPrimaryIF();
    return lm("Server %1").arg(id);
}

QnMediaServerResourcePtr MediaServerProcess::findServer(ec2::AbstractECConnectionPtr ec2Connection)
{
    nx::vms::api::MediaServerDataList servers;

    while (servers.empty() && !needToStop())
    {
        const ec2::ErrorCode res =
            ec2Connection->getMediaServerManager(Qn::kSystemAccess)->getServersSync(&servers);
        if (res == ec2::ErrorCode::ok)
            break;

        NX_DEBUG(this, "%1(): Call to getServers failed: %2", __func__, res);
        QnSleep::msleep(1000);
    }

    const QnUuid serverGuid(serverModule()->settings().serverGuid());
    for(const auto& server: servers)
    {
        if (server.id == serverGuid)
        {
            QnMediaServerResourcePtr qnServer(new QnMediaServerResource(commonModule()));
            ec2::fromApiToResource(server, qnServer);
            return qnServer;
        }
    }

    return QnMediaServerResourcePtr();
}

QnMediaServerResourcePtr MediaServerProcess::registerServer(
    ec2::AbstractECConnectionPtr ec2Connection,
    const QnMediaServerResourcePtr &server,
    bool isNewServerInstance)
{
    nx::vms::api::MediaServerData apiServer;
    ec2::fromResourceToApi(server, apiServer);

    ec2::ErrorCode rez = ec2Connection->getMediaServerManager(Qn::kSystemAccess)->saveSync(apiServer);
    if (rez != ec2::ErrorCode::ok)
    {
        qWarning() << "registerServer(): Call to registerServer failed. Reason: " << ec2::toString(rez);
        return QnMediaServerResourcePtr();
    }

    if (!isNewServerInstance)
        return server;

    // insert server user attributes if defined
    QString dir = serverModule()->settings().staticDataDir();

    nx::vms::api::MediaServerUserAttributesData userAttrsData;
    ec2::fromResourceToApi(server->userAttributes(), userAttrsData);

    QFile f(closeDirPath(dir) + "server_settings.json");
    if (!f.open(QFile::ReadOnly))
        return server;
    QByteArray data = f.readAll();
    if (QJson::deserialize(data, &userAttrsData))
    {
        userAttrsData.serverId = server->getId();
        saveMediaServerUserAttributes(ec2Connection, userAttrsData);
    }
    else
    {
        NX_WARNING(this, "Can not deserialize server_settings.json file");
    }
    return server;
}

void MediaServerProcess::saveMediaServerUserAttributes(
    ec2::AbstractECConnectionPtr ec2Connection,
    const nx::vms::api::MediaServerUserAttributesData& userAttrsData)
{
    nx::vms::api::MediaServerUserAttributesDataList attrsList;
    attrsList.push_back(userAttrsData);
    auto rez = ec2Connection->getMediaServerManager(Qn::kSystemAccess)->saveUserAttributesSync(attrsList);
    if (rez != ec2::ErrorCode::ok)
        qWarning() << "registerServer(): Call to registerServer failed. Reason: " << ec2::toString(rez);
}

void MediaServerProcess::saveStorages(const StorageResourceList& storages)
{
    nx::vms::api::StorageDataList apiStorages;
    ec2::fromResourceListToApi(storages, apiStorages);

    ec2::ErrorCode rez;
    while ((rez = serverManager()->saveStoragesSync(apiStorages))
        != ec2::ErrorCode::ok && !needToStop())
    {
        NX_WARNING(this) << "Call to change server's storages failed. Reason: " << rez;
        QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
    }
}

void MediaServerProcess::dumpSystemUsageStats()
{
    if (!serverModule()->platform()->monitor())
        return;

    if (!serverModule()->settings().noMonitorStatistics())
        serverModule()->platform()->monitor()->logStatistics();

    // TODO: #muskov
    //  - Add some more fields that might be interesting.
    //  - Make and use JSON serializable struct rather than just a string.
    QStringList networkIfList;
    for (const auto& iface: serverModule()->platform()->monitor()->totalNetworkLoad())
    {
        if (iface.type != nx::vms::server::PlatformMonitor::LoopbackInterface)
        {
            networkIfList.push_back(
                lm("%1: %2 bps").args(iface.interfaceName, iface.bytesPerSecMax));
        }
    }
    const auto networkIfInfo = networkIfList.join(", ");
    if (m_mediaServer->setProperty(ResourcePropertyKey::Server::kNetworkInterfaces, networkIfInfo))
        m_mediaServer->saveProperties();

    NX_MUTEX_LOCKER lk(&m_mutex);
    if (m_dumpSystemResourceUsageTaskId == 0)  // Monitoring cancelled
        return;
    m_dumpSystemResourceUsageTaskId = commonModule()->timerManager()->addTimer(
        std::bind(&MediaServerProcess::dumpSystemUsageStats, this),
        std::chrono::seconds(ini().systemUsageDumpTimeoutS));
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

nx::utils::Url MediaServerProcess::appServerConnectionUrl() const
{
    auto settings = serverModule()->mutableSettings();
    // migrate appserverPort settings from version 2.2 if exist
    if (settings->appserverPort.present())
    {
        settings->port.set(settings->appserverPort());
        settings->appserverPort.remove();
    }

    nx::utils::Url appServerUrl;
    QUrlQuery params;

    // ### remove
    QString host = settings->appserverHost();
    if (QUrl(host).scheme() == "file")
    {
        appServerUrl = nx::utils::Url(host); // it is a completed URL
    }
    else if (host.isEmpty() || host == "localhost")
    {
        appServerUrl = nx::utils::Url::fromLocalFile(closeDirPath(serverModule()->settings().dataDir()));
    }
    else
    {
        appServerUrl.setScheme(nx::network::http::urlSheme(settings->secureAppserverConnection()));
        appServerUrl.setHost(host);
        appServerUrl.setPort(settings->port());
    }
    if (appServerUrl.scheme() == "file")
    {
        QString staticDBPath = settings->staticDataDir();
        if (!staticDBPath.isEmpty()) {
            params.addQueryItem("staticdb_path", staticDBPath);
        }
        if (settings->removeDbOnStartup())
            params.addQueryItem("cleanupDb", QString());
    }

    // TODO: #rvasilenko Actually appserverPassword is always empty. Remove?
    QString userName = settings->appserverLogin();
    QString password = settings->appserverPassword();
    QByteArray authKey = SettingsHelper(serverModule()).getAuthKey();
    QString appserverHostString = settings->appserverHost();
    if (!authKey.isEmpty() && !Utils::isLocalAppServer(appserverHostString))
    {
        userName = serverModule()->settings().serverGuid();
        password = authKey;
    }

    appServerUrl.setUserName(userName);
    appServerUrl.setPassword(password);
    appServerUrl.setQuery(params);

    NX_INFO(this, "Connect to server %1", appServerUrl.toString(QUrl::RemovePassword));
    return appServerUrl;
}

MediaServerProcess::MediaServerProcess(int argc, char* argv[], bool serviceMode)
    :
    m_argc(argc),
    m_argv(argv),
    m_cmdLineArguments(argc, argv),
    m_serviceMode(serviceMode)
{
    serviceMainInstance = this;

    // TODO: Other platforms?
    #if defined(__linux__)
        if (!m_cmdLineArguments.crashDirectory.isEmpty())
            linux_exception::setCrashDirectory(m_cmdLineArguments.crashDirectory.toStdString());
    #endif

    auto settings = std::make_unique<MSSettings>(
        m_cmdLineArguments.configFilePath,
        m_cmdLineArguments.rwConfigFilePath);

    addCommandLineParametersFromConfig(settings.get());

    const QString raidEventLogName = system_log::ini().logName;
    const QString raidEventProviderName = system_log::ini().providerName;

    m_raidEventLogReader.reset(new RaidEventLogReader(
        raidEventLogName, raidEventProviderName));
    m_enableMultipleInstances = settings->settings().enableMultipleInstances();
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
    m_staticCommonModule.reset();
    serviceMainInstance = nullptr;
}

void MediaServerProcess::initResourceTypes()
{
    auto manager = m_ec2Connection->getResourceManager(Qn::kSystemAccess);
    while (!needToStop())
    {
        QList<QnResourceTypePtr> resourceTypeList;
        const ec2::ErrorCode errorCode = manager->getResourceTypesSync(&resourceTypeList);
        if (errorCode == ec2::ErrorCode::ok)
        {
            qnResTypePool->replaceResourceTypeList(resourceTypeList);
            break;
        }
        NX_ERROR(this, lm("Failed to load resource types. %1").arg(ec2::toString(errorCode)));
    }
}

bool MediaServerProcess::isStopping() const
{
    QnMutexLocker lock(&m_stopMutex);
    return m_stopping;
}

void MediaServerProcess::at_databaseDumped()
{
    if (isStopping())
        return;

    nx::mserver_aux::savePersistentDataBeforeDbRestore(
                commonModule()->resourcePool()->getAdministrator(),
                m_mediaServer,
                nx::mserver_aux::createServerSettingsProxy(serverModule()).get()
            ).saveToSettings(serverModule()->roSettings());
    NX_INFO(this, "Server restart is scheduled after dump database");
    restartServer(500);
}

void MediaServerProcess::at_serverPropertyChanged(const QnResourcePtr& /*resource*/, const QString& key)
{
    if (key == QnMediaServerResource::kMetadataStorageIdKey)
        initializeAnalyticsEvents();
}

void MediaServerProcess::at_systemIdentityTimeChanged(qint64 value, const QnUuid& sender)
{
    if (isStopping())
        return;

    SettingsHelper(serverModule()).setSysIdTime(value);
    if (sender != commonModule()->moduleGUID())
    {
        serverModule()->mutableSettings()->removeDbOnStartup.set(true);
        // If system Id has been changed, reset 'database restore time' variable
        nx::mserver_aux::savePersistentDataBeforeDbRestore(
                    commonModule()->resourcePool()->getAdministrator(),
                    m_mediaServer,
                    nx::mserver_aux::createServerSettingsProxy(serverModule()).get()
                ).saveToSettings(serverModule()->roSettings());
        NX_INFO(this, "Server restart is scheduled because sysId time is changed");
        restartServer(0);
    }
}

void MediaServerProcess::stopSync()
{
    {
        QnMutexLocker lock(&m_stopMutex);
        if (m_stopping)
            return;

        m_stopping = true;
    }

    qWarning() << "Stopping server";
    pleaseStop();
    quit();

    const std::chrono::seconds kStopTimeout(ini().stopTimeoutS);
    if (!wait(kStopTimeout))
        NX_CRITICAL(false, lm("Server was unable to stop within %1").arg(kStopTimeout));

    qApp->quit();
}

void MediaServerProcess::stopAsync()
{
    // ATTENTION: This method is also called from a signal handler, thus, IO operations of any kind
    // are prohibited because of potential deadlocks.

    static std::atomic_flag wasCalled = /*false*/ ATOMIC_FLAG_INIT;
    if (wasCalled.test_and_set())
        return;

    if (serviceMainInstance)
        QTimer::singleShot(0, this, SLOT(stopSync()));
}

int MediaServerProcess::getTcpPort() const
{
    return m_universalTcpListener ? m_universalTcpListener->getPort() : 0;
}

void MediaServerProcess::updateDisabledVendorsIfNeeded()
{
    // migration from old version. move setting from registry to the DB
    static const QString DV_PROPERTY = QLatin1String("disabledVendors");
    QString disabledVendors = serverModule()->roSettings()->value(DV_PROPERTY).toString();
    if (!disabledVendors.isNull())
    {
        qnGlobalSettings->setDisabledVendors(disabledVendors);
        serverModule()->roSettings()->remove(DV_PROPERTY);
    }
}

void MediaServerProcess::updateAllowCameraChangesIfNeeded()
{
    static const QString DV_PROPERTY = QLatin1String("cameraSettingsOptimization");

    QString allowCameraChanges = serverModule()->roSettings()->value(DV_PROPERTY).toString();
    if (!allowCameraChanges.isEmpty())
    {
        qnGlobalSettings->setCameraSettingsOptimizationEnabled(
            allowCameraChanges.toLower() == "yes"
            || allowCameraChanges.toLower() == "true"
            || allowCameraChanges == "1");
        serverModule()->roSettings()->setValue(DV_PROPERTY, "");
    }
}

template<typename Container>
QString containerToQString(const Container& container)
{
    QStringList list;
    for (const auto& it : container)
        list << it.toString();

    return list.join(", ");
}

void MediaServerProcess::updateAddressesList()
{
    if (isStopping())
        return;

    nx::vms::api::MediaServerData prevValue;
    ec2::fromResourceToApi(m_mediaServer, prevValue);

    nx::network::AddressFilters addressMask =
        nx::network::AddressFilter::ipV4
        | nx::network::AddressFilter::ipV6
        | nx::network::AddressFilter::noLocal
        | nx::network::AddressFilter::noLoopback;

    QList<nx::network::SocketAddress> serverAddresses;
    const auto port = m_universalTcpListener->getPort();

    for (const auto& host: allLocalAddresses(addressMask))
        serverAddresses << nx::network::SocketAddress(host, port);

    for (const auto& host : m_forwardedAddresses)
        serverAddresses << nx::network::SocketAddress(host.first, host.second);

    if (!m_ipDiscovery->publicIP().isNull())
        serverAddresses << nx::network::SocketAddress(m_ipDiscovery->publicIP().toString(), port);

    m_mediaServer->setNetAddrList(serverAddresses);
    NX_DEBUG(this, "Update mediaserver addresses: %1", containerToQString(serverAddresses));

    const nx::utils::Url defaultUrl(m_mediaServer->getApiUrl());
    const nx::network::SocketAddress defaultAddress(defaultUrl.host(), defaultUrl.port());
    if (std::find(serverAddresses.begin(), serverAddresses.end(),
        defaultAddress) == serverAddresses.end())
    {
        nx::network::SocketAddress newAddress;
        if (!serverAddresses.isEmpty())
            newAddress = serverAddresses.front();

        m_mediaServer->setPrimaryAddress(newAddress);
    }

    nx::vms::api::MediaServerData server;
    ec2::fromResourceToApi(m_mediaServer, server);
    if (server != prevValue)
    {
        auto mediaServerManager =
            commonModule()->ec2Connection()->getMediaServerManager(Qn::kSystemAccess);
        mediaServerManager->save(server, this, &MediaServerProcess::at_serverSaved);
    }

    nx::network::SocketGlobals::cloud().addressPublisher().updateAddresses(
        std::list<nx::network::SocketAddress>(
            serverAddresses.begin(),
            serverAddresses.end()));
}

void MediaServerProcess::saveServerInfo(const QnMediaServerResourcePtr& server)
{
    using namespace nx::utils;

    namespace Server = ResourcePropertyKey::Server;
    const auto hwInfo = HardwareInformation::instance();
    server->setProperty(Server::kCpuArchitecture, hwInfo.cpuArchitecture);
    server->setProperty(Server::kCpuModelName, hwInfo.cpuModelName);
    server->setProperty(Server::kPhysicalMemory, QString::number(hwInfo.physicalMemory));
    server->setProperty(Server::kBrand, AppInfo::brand());
    server->setProperty(Server::kFullVersion, AppInfo::applicationFullVersion());
    server->setProperty(Server::kBeta, QString::number(AppInfo::beta() ? 1 : 0));
    server->setProperty(Server::kPublicIp, m_ipDiscovery->publicIP().toString());
    server->setProperty(Server::kSystemRuntime,
        nx::vms::api::SystemInformation::currentSystemRuntime());

    if (m_mediaServer->getPanicMode() == Qn::PM_BusinessEvents)
        server->setPanicMode(Qn::PM_None);

    static const QString kHddListFilename("/tmp/hddlist");

    QFile hddList(kHddListFilename);
    if (hddList.open(QFile::ReadOnly))
    {
        const auto content = QString::fromUtf8(hddList.readAll());
        if (content.size())
        {
            auto hhds = content.split("\n", QString::SkipEmptyParts);
            for (auto& hdd : hhds)
                hdd = hdd.trimmed();
            server->setProperty(Server::kHddList, hhds.join(", "),
                                QnResource::NO_ALLOW_EMPTY);
        }
    }

    server->saveProperties();
    m_mediaServer->setStatus(Qn::Online);

    #ifdef ENABLE_EXTENDED_STATISTICS
        qnServerDb->setBookmarkCountController(
            [server](size_t count)
            {
                server->setProperty(Server::kBookmarkCount, QString::number(count));
                server->saveProperties();
            });
    #endif
}

void MediaServerProcess::at_updatePublicAddress(const QHostAddress& publicIp)
{
    if (isStopping())
        return;

    NX_DEBUG(this, "Server %1 has changed publicIp to value %2", commonModule()->moduleGUID(), publicIp);

    QnPeerRuntimeInfo localInfo = commonModule()->runtimeInfoManager()->localInfo();
    localInfo.data.publicIP = publicIp.toString();
    commonModule()->runtimeInfoManager()->updateLocalItem(localInfo);

    const auto& resPool = commonModule()->resourcePool();
    QnMediaServerResourcePtr server = resPool->getResourceById<QnMediaServerResource>(commonModule()->moduleGUID());
    if (server)
    {
        auto serverFlags = server->getServerFlags();
        if (publicIp.isNull())
            serverFlags &= ~nx::vms::api::SF_HasPublicIP;
        else
            serverFlags |= nx::vms::api::SF_HasPublicIP;

        if (serverFlags != server->getServerFlags())
        {
            server->setServerFlags(serverFlags);
            ec2::AbstractECConnectionPtr ec2Connection = commonModule()->ec2Connection();

            nx::vms::api::MediaServerData apiServer;
            ec2::fromResourceToApi(server, apiServer);
            ec2Connection->getMediaServerManager(Qn::kSystemAccess)->save(apiServer, this, [] {});
        }

        if (server->setProperty(ResourcePropertyKey::Server::kPublicIp,
            publicIp.toString(), QnResource::NO_ALLOW_EMPTY))
            server->saveProperties();

        updateAddressesList(); //< update interface list to add/remove publicIP
    }
}

void MediaServerProcess::at_portMappingChanged(QString address)
{
    if (isStopping())
        return;

    nx::network::SocketAddress mappedAddress(address);
    if (mappedAddress.port)
    {
        auto it = m_forwardedAddresses.emplace(mappedAddress.address, 0).first;
        if (it->second != mappedAddress.port)
        {
            NX_INFO(this, "New external address %1 has been mapped", address);

            it->second = mappedAddress.port;
            updateAddressesList();
        }
    }
    else
    {
        const auto oldIp = m_forwardedAddresses.find(mappedAddress.address);
        if (oldIp != m_forwardedAddresses.end())
        {
            NX_INFO(this, "External address %1:%2 has been unmapped",
                oldIp->first.toString(), oldIp->second);

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

void MediaServerProcess::writeServerStartedEvent()
{
    if (isStopping())
        return;

    const auto& resPool = commonModule()->resourcePool();
    const QnUuid serverGuid(serverModule()->settings().serverGuid());
    qint64 lastRunningTime = serverModule()->lastRunningTimeBeforeRestart().count();
    if (lastRunningTime)
    {
        serverModule()->eventConnector()->at_serverFailure(
            resPool->getResourceById<QnMediaServerResource>(serverGuid),
            lastRunningTime * 1000,
            nx::vms::api::EventReason::serverStarted,
            QString());
    }

    serverModule()->eventConnector()->at_serverStarted(
        resPool->getResourceById<QnMediaServerResource>(serverGuid),
        qnSyncTime->currentUSecsSinceEpoch());
}

void MediaServerProcess::at_serverModuleConflict(nx::vms::discovery::ModuleEndpoint module)
{
    const auto& resPool = commonModule()->resourcePool();
    serverModule()->eventConnector()->at_serverConflict(
        resPool->getResourceById<QnMediaServerResource>(commonModule()->moduleGUID()),
        qnSyncTime->currentUSecsSinceEpoch(),
        module,
        QUrl(lm("http://%1").arg(module.endpoint.toString())));
}

void MediaServerProcess::at_checkAnalyticsUsed()
{
    if (!m_storageInitializationDone)
        return; //< Initially analytics initialized from initStoragesAsync.

    if (!m_oldAnalyticsStoragePath.isEmpty())
        return; //< Analytics already initialized.

    if (!nx::analytics::serverHasActiveObjectEngines(commonModule(), m_mediaServer->getId()))
        return;

    if (m_mediaServer->metadataStorageId().isNull() && !QFile::exists(getMetadataDatabaseName()))
    {
        m_mediaServer->setMetadataStorageId(selectDefaultStorageForAnalyticsEvents(m_mediaServer));
        m_mediaServer->saveProperties();
    }

    initializeAnalyticsEvents();
}

void MediaServerProcess::at_timer()
{
    if (isStopping())
        return;

    // TODO: #2.4 #GDM This timer make two totally different functions. Split it.
    serverModule()->setLastRunningTime(
        std::chrono::milliseconds(qnSyncTime->currentMSecsSinceEpoch()));

    const auto& resPool = commonModule()->resourcePool();
    QnResourcePtr mServer = resPool->getResourceById(commonModule()->moduleGUID());
    if (!mServer)
        return;

    for(const auto& camera: resPool->getAllCameras(mServer, true))
        camera->cleanCameraIssues();
}

void MediaServerProcess::setRuntimeFlag(nx::vms::api::RuntimeFlag flag, bool isSet)
{
    QnPeerRuntimeInfo localInfo = commonModule()->runtimeInfoManager()->localInfo();
    localInfo.data.flags.setFlag(flag, isSet);
    commonModule()->runtimeInfoManager()->updateLocalItem(localInfo);
}

void MediaServerProcess::at_storageManager_noStoragesAvailable()
{
    if (isStopping())
        return;
    serverModule()->eventConnector()->at_noStorages(m_mediaServer);
    setRuntimeFlag(nx::vms::api::RuntimeFlag::noStorages, true);
}

void MediaServerProcess::at_storageManager_storagesAvailable()
{
    if (isStopping())
        return;
    setRuntimeFlag(nx::vms::api::RuntimeFlag::noStorages, false);
}

void MediaServerProcess::at_storageManager_storageFailure(const QnResourcePtr& storage,
    nx::vms::api::EventReason reason)
{
    if (isStopping())
        return;
    serverModule()->eventConnector()->at_storageFailure(
        m_mediaServer, qnSyncTime->currentUSecsSinceEpoch(), reason, storage);
}

void MediaServerProcess::at_storageManager_raidStorageFailure(const QString& description,
    nx::vms::event::EventReason reason)
{
    if (isStopping())
        return;
    serverModule()->eventConnector()->at_raidStorageFailure(
        m_mediaServer, qnSyncTime->currentUSecsSinceEpoch(), reason, description);
}

void MediaServerProcess::at_storageManager_rebuildFinished(QnSystemHealth::MessageType msgType)
{
    if (isStopping())
        return;
    serverModule()->eventConnector()->at_archiveRebuildFinished(m_mediaServer, msgType);
}

void MediaServerProcess::at_archiveBackupFinished(
    qint64                      backedUpToMs,
    nx::vms::api::EventReason code
)
{
    if (isStopping())
        return;

    serverModule()->eventConnector()->at_archiveBackupFinished(
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
    serverModule()->eventConnector()->at_cameraIPConflict(
        m_mediaServer,
        host,
        macAddrList,
        qnSyncTime->currentUSecsSinceEpoch());
}

void MediaServerProcess::registerRestHandlers(
    nx::vms::cloud_integration::CloudManagerGroup* cloudManagerGroup,
    QnUniversalTcpListener* tcpListener,
    ec2::TransactionMessageBusAdapter* messageBus)
{
    auto processorPool = tcpListener->processorPool();
    const auto welcomePage = "/static/index.html";
    processorPool->registerRedirectRule("", welcomePage);
    processorPool->registerRedirectRule("/", welcomePage);
    processorPool->registerRedirectRule("/static", welcomePage);
    processorPool->registerRedirectRule("/static/", welcomePage);

    // TODO: When supported by apidoctool, the comment to these constants should be parsed.
    const auto kAdmin = GlobalPermission::admin;
    const auto kViewLogs = GlobalPermission::viewLogs;

    const auto reg = [this](auto&&... args) { registerRestHandler(std::move(args)...); };

    /**%apidoc GET /api/synchronizedTime
     * This method is used for internal purpose to synchronize time between mediaservers and clients.
     * %return:object Information about server synchronized time.
     *     %param:integer utcTimeMs Server synchronized time.
     *     %param:boolean isTakenFromInternet Whether the server has got the time from the internet.
     */
    reg(nx::vms::time_sync::TimeSyncManager::kTimeSyncUrlPath.mid(1),
        new ::rest::handlers::SyncTimeRestHandler());

    /**%apidoc GET /ec2/mergeStatus
     * Return information if the last merge request still is in progress.
     * If merge is not in progress it means all data that belongs to servers on the moment when merge was requested
     * are synchronized. This functions is a system wide and can be called from any server in the system to check merge status.
     * %return:object JSON object with an error code, error string, and the reply on success.
     *     %struct MergeStatusReply
     */
    reg(nx::vms::server::rest::GetMergeStatusHandler::kUrlPath,
        new nx::vms::server::rest::GetMergeStatusHandler(serverModule()));

    /**%apidoc POST /ec2/forcePrimaryTimeServer
     * Set primary time server. Requires a JSON object with optional "id" field in the message
     * body. If "id" field is missing, the primary time server is turned off.
     * %permissions Owner.
     * %return:object JSON object with error message and error code (0 means OK).
     */
    reg("ec2/forcePrimaryTimeServer",
        new ::rest::handlers::SetPrimaryTimeServerRestHandler(), kAdmin);

    /**%apidoc GET /api/storageStatus
     * Check whether the specified location could be used as a server storage path and report the location details.
     * If the location is already used as the server storage report, the details, storage id and status are expected in
     * response.
     * %param:string path Folder to check.
     * %return:object JSON object with an error code, error string, and the reply on success.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *     %param:object reply JSON object with details about the requested folder.
     */
    reg("api/storageStatus", new QnStorageStatusRestHandler(serverModule()));

    /**%apidoc GET /api/storageSpace
     * Get the list of all server storages.
     * %return:object JSON data with server storages.
     */
    reg("api/storageSpace", new QnStorageSpaceRestHandler(serverModule()));

    /**%apidoc GET /api/statistics
     * Get the Server info: CPU usage, HDD usage e.t.c.
     * %return:object JSON data with statistics.
     */
    reg("api/statistics", new QnStatisticsRestHandler(serverModule()));

    /**%apidoc GET /api/getCameraParam
     * Read camera parameters. For instance: brightness, contrast e.t.c. Parameters to read should
     * be specified.
     * %param:string cameraId Camera id (can be obtained from "id" field via /ec2/getCamerasEx),
     *     or MAC address (not supported for certain cameras), or "Logical Id".
     * %param[opt]:string <any_name> Parameter name to read. Request can contain one or more
     *     parameters.
     * %return:object JSON object with an error code, error string, and the reply on success.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *     %param:array reply List of objects representing the camera parameters.
     *         %param:string reply[].id Parameter id.
     *         %param:string reply[].value Parameter value.
     */
    reg("api/getCameraParam", new QnCameraSettingsRestHandler(serverModule()->resourceCommandProcessor()));

    /**%apidoc POST /api/setCameraParam
     * Sets values of several camera parameters. These parameters are used on the Advanced tab in
     * camera settings. For instance: brightness, contrast, etc.
     * %param:string cameraId Camera id (can be obtained from "id" field via /ec2/getCamerasEx),
     *     or MAC address (not supported for certain cameras), or "Logical Id".
     * %param:object paramValues Name-to-value map of camera parameters to set.
     * %return:object JSON object with an error code, error string, and the reply on success.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *     %param:array reply List of objects representing the camera parameters that have
     *         changed their values, including the ones that were changed by the device in
     *         response to changing some other parameters.
     *         %param:string reply[].id Parameter id.
     *         %param:string reply[].value Parameter value.
     */
    reg("api/setCameraParam", new QnCameraSettingsRestHandler(serverModule()->resourceCommandProcessor()));

    /**%apidoc GET /api/manualCamera/search
     * Start searching for the cameras in manual mode. There are two ways to call this method:
     * IP range search and single host search. To scan an IP range, "start_ip" and "end_ip" must be
     * specified. To run a single host search, "url" must be specified.
     * %param[opt]:string url A valid URL, hostname or hostname:port are accepted.
     * %param[opt]:string start_ip First IP address in the range to scan. Conflicts with "url".
     * %param[opt]:string end_ip Last IP address in the range to scan. Conflicts with "url".
     * %param[opt]:integer port Cameras IP port to check. Port is auto-detected if this parameter
     *     is omitted and "url" does not contain one. Overwrites the port in "url" if specified.
     * %param[opt]:string user Camera(s) username. Overwrites the user in "url" if specified.
     * %param[opt]:string password Camera(s) password. Overwrites the password in "url" if specified.
     * %return:object JSON object with the initial status of camera search process, including
     *     processUuid used for other /api/manualCamera calls, and the list of objects describing
     *     cameras found to the moment.
     *
     **%apidoc GET /api/manualCamera/status
     * Get the current status of the process of searching for the cameras.
     * %param:uuid uuid Process unique id, can be obtained from "processUuid" field in the result
     *     of /api/manualCamera/search.
     * %return:object JSON object with the initial status of camera search process, including
     *     processUuid used for other /api/manualCamera calls, and the list of objects describing
     *     cameras found to the moment.
     *
     **%apidoc POST /api/manualCamera/stop
     * Stop manual adding progress.
     * %param:uuid uuid Process unique id, can be obtained from "processUuid" field in the result
     *     of /api/manualCamera/search.
     * %return:object JSON object with error message and error code (0 means OK).
     *
     **%apidoc[proprietary] GET /api/manualCamera/add
     * Manually add camera(s). If several cameras are added, parameters "url" and "manufacturer"
     * must be defined several times with incrementing suffix "0", "1", etc.
     * %param:string url0 Camera url, can be obtained from "reply.cameras[].url" field in the
     *     result of /api/manualCamera/status.
     * %param:string uniqueId0 Camera physical id, can be obtained from "reply.cameras[].uniqueId"
     *     field in the result of /api/manualCamera/status.
     * %param:string manufacturer0 Camera manufacturer, can be obtained from
     *     "reply.cameras[].manufacturer" field in the result of /api/manualCamera/status.
     * %param[opt]:string user Username for the cameras.
     * %param[opt]:string password Password for the cameras.
     * %return:object JSON object with error message and error code (0 means OK).
     *
     **%apidoc POST /api/manualCamera/add
     * Manually add camera(s).
     * <p>
     * Parameters should be passed as a JSON object in POST message body with
     * content type "application/json". Example of such object:
     * <pre><code>
     * {
     *     "user": "some_user",
     *     "password": "some_password",
     *     "cameras":
     *     [
     *         {
     *             "uniqueId": "00-1A-07-00-FF-FF",
     *             "url": "192.168.0.100",
     *             "manufacturer": "3100"
     *         }
     *     ]
     * }
     * </code></pre></p>
     * %param[opt]:string user Username for the cameras.
     * %param[opt]:string password Password for the cameras.
     * %param:array cameras List of objects representing the cameras.
     *     %param:string cameras[].url Camera url, can be obtained from "reply.cameras[].url"
     *         field in the result of /api/manualCamera/status.
     *     %param:string cameras[].uniqueId Camera physical id, can be obtained from
     *         "reply.cameras[].uniqueId" field in the result of /api/manualCamera/status.
     *     %param:string cameras[].manufacturer Camera manufacturer, can be obtained from
     *         "reply.cameras[].manufacturer" field in the result of /api/manualCamera/status.
     * %return:object JSON object with error message and error code (0 means OK).
     */
    reg("api/manualCamera", new QnManualCameraAdditionRestHandler(serverModule()));

    /**%apidoc[proprietary] TODO /api/wearableCamera
     * %// TODO: Write apidoc comment.
     */
    reg("api/wearableCamera", new QnWearableCameraRestHandler(serverModule()));

    /**%apidoc GET /api/ptz
     * Perform reading or writing PTZ operation
     * %param:string cameraId Camera id (can be obtained from "id" field via /ec2/getCamerasEx)
     *     or MAC address (not supported for certain cameras).
     * %param:enum command PTZ operation
     *     %value ContinuousMovePtzCommand Start PTZ continues move. Parameters xSpeed, ySpeed and
     *         zSpeed are used in range [-1.0..+1.0]. To stop moving use value 0 for all
     *         parameters.
     *     %value ContinuousFocusPtzCommand Start PTZ focus in or out. Parameter speed defines
     *         speed and focus direction in range [-1.0..+1.0].
     *     %value AbsoluteDeviceMovePtzCommand Move camera to absolute position. Parameters xPos,
     *         yPos and zPos are used in range defined by camera. Parameter speed is used in range
     *         [0..1.0].
     *     %value AbsoluteLogicalMovePtzCommand Move camera to absolute position. Parameters xPos,
     *         yPos range are: [-180..+180]. Parameter zPos range is: [0..180] (field of view in
     *         degree). Parameters speed range is: [0..1.0].
     *     %value GetDevicePositionPtzCommand Read camera current position. Return parameters xPos,
     *         yPos and zPos in range defined by camera.
     *     %value GetLogicalPositionPtzCommand Read camera current position. Return parameters
     *         xPos, yPos in range [-180..+180]. Return parameter zPos in range [0..180] (field of
     *         view in degree).
     *     %value CreatePresetPtzCommand Create PTZ preset. Parameter presetId defines internal
     *         preset name. Parameter presetName defines display preset name.
     *     %value UpdatePresetPtzCommand Update PTZ preset display name. Parameter presetId defines
     *         internal preset name. Parameter presetName defines display preset name.
     *     %value RemovePresetPtzCommand Update PTZ preset display name. Parameter presetId defines
     *         internal preset name
     *     %value ActivatePresetPtzCommand Go to PTZ preset. Parameter presetId defines internal
     *         preset name. Parameter speed defines move speed in range [0..1.0.]
     *     %value GetPresetsPtzCommand Read PTZ presets list.
     *     %value GetToursPtzCommand Read PTZ tours list.
     *     %value ActivateTourPtzCommand Activate PTZ tour. Parameter tourId defines defines internal
     *         tour name. Parameter tourName defines display preset name.
     * %return:object JSON object with error message and error code (0 means OK).
     */
    reg("api/ptz", new QnPtzRestHandler(serverModule()));

    /**%apidoc GET /api/createEvent
     * Using this method it is possible to trigger a generic event in the system from a 3rd party
     * system. Such event will be handled and logged according to current event rules.
     * Parameters of the generated event, such as "source", "caption" and "description", are
     * intended to be analyzed by these rules.
     * <tt>
     *     <br/>Example:
     *     <pre><![CDATA[
     * http://127.0.0.1:7001/api/createEvent?timestamp=2016-09-16T16:02:41Z&caption=CreditCardUsed&metadata={"cameraRefs":["3A4AD4EA-9269-4B1F-A7AA-2CEC537D0248","3A4AD4EA-9269-4B1F-A7AA-2CEC537D0240"]}
     *     ]]></pre>
     *     This example triggers a generic event informing the system that a
     *     credit card has been used on September 16, 2016 at 16:03:41 UTC in a POS
     *     terminal being watched by the two specified cameras.
     * </tt>
     * %param[opt]:string timestamp Event date and time (as a string containing time in
     *     milliseconds since epoch, or a local time formatted like
     *     <code>"<i>YYYY</i>-<i>MM</i>-<i>DD</i>T<i>HH</i>:<i>mm</i>:<i>ss</i>.<i>zzz</i>"</code>
     *     - the format is auto-detected). If "timestamp" is absent, the current server date and
     *     time is used.
     * %param[opt]:string source Name of the device which has triggered the event. It can be used
     *     in a filter in event rules to assign different actions to different devices. Also, the
     *     user could see this name in the notifications panel. Example: "POS terminal 5".
     * %param[opt]:string caption Short event description. It can be used in a filter in event
     *     rules to assign actions depending on this text.
     * %param[opt]:string description Long event description. It can be used as a filter in event
     *     rules to assign actions depending on this text.
     * %param[opt]:objectJson metadata Additional information associated with the event, in the
     *     form of a JSON object. Currently this object can specify the only field "cameraRefs",
     *     but other fields could be added in the future.
     *     <ul>
     *         <li>"cameraRefs" specifies the list of cameras which are linked to the event (e.g.
     *         the event will appear on their timelines), in the form of a list of camera ids (can
     *         be obtained from "id" field via /ec2/getCamerasEx).</li>
     *     </ul>
     * %param[opt]:enum state Generic events can be used either with "long" actions like
     *     "do recording", or instant actions like "send email". This parameter should be specified
     *     in case "long" actions are going to be used with generic events.
     *     %value Active Generic event is considered a "long" event. It transits to the "active"
     *         state. "Long" actions will continue executing until the generic event transits to
     *         the "inactive" state.
     *     %value Inactive A "long" action associated with this generic event in event rules will
     *         stop.
     * %return:object JSON object with error message and error code (0 means OK).
     */
    reg("api/createEvent", new QnExternalEventRestHandler(serverModule()));

    static const QString kGetTimePath("api/getTime");
    /**%apidoc GET /api/getTime
     * Get the Server time, time zone and authentication realm (realm is added for convenience).
     * %return:object JSON object with an error code, error string, and the reply on success.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *     %param:object reply Time-related data.
     *         %param:string reply.realm Authentication realm.
     *         %param:string reply.timeZoneOffset Time zone offset, in milliseconds.
     *         %param:string reply.timeZoneId Identification of the time zone in the text form.
     *         %param:string reply.osTime Local OS time on the Server, in milliseconds since epoch.
     *         %param:string reply.vmsTime Synchronized time of the VMS System, in milliseconds
     *             since epoch.
     */
    reg(kGetTimePath, new nx::vms::server::rest::GetTimeHandler());

    /**%apidoc[proprietary] TODO /ec2/getTimeOfServers
     * %// TODO: Write apidoc comment.
     */
    reg("ec2/getTimeOfServers", new nx::vms::server::rest::ServerTimeHandler("/" + kGetTimePath));

    /**%apidoc GET /api/gettime
     * [DEPRECATED in favor of /api/getTime] Get the Server time, time zone and authentication
     * realm (the realm is added for convenience).
     * %// NOTE: The name of this method and its timezoneId parameter use wrong case.
     * %param[proprietary]:boolean local If specified, the returned utcTime will contain the local
     *     OS time on the Server, otherwise, it will contain the synchronized time of the VMS
     *     System. All values are in milliseconds since epoch.
     * %return:object JSON object with an error code, error string, and the reply on success.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *     %param:object reply Time-related data.
     *         %param:string reply.realm Authentication realm.
     *         %param:string reply.timeZoneOffset Time zone offset, in seconds.
     *         %param:string reply.timezoneId Identification of the time zone in the text form.
     *         %param:string reply.utcTime Server time, in milliseconds since epoch.
     */
    reg("api/gettime", new QnTimeRestHandler());

    /**%apidoc GET /api/getTimeZones
     * Return the complete list of time zones supported by the server machine.
     * %return:object JSON object with an error code, error string, and the reply on success.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *     %param:array reply List of objects representing the time zones.
     *         %param:string reply[].comment Time zone description in English.
     *         %param:string reply[].displayName Time zone verbose name in English.
     *         %param:boolean reply[].hasDaylightTime Whether the time zone has the DST feature.
     *             %value false
     *             %value true
     *         %param:string reply[].id Time zone identifier, to be used for e.g. /api/setTime.
     *         %param:boolean reply[].isDaylightTime Whether the time zone is on DST right now. To
     *             be reported properly, the server machine should have the correct current time
     *             set.
     *             %value false
     *             %value true
     *         %param:integer reply[].offsetFromUtc Time zone offset from UTC (in seconds).
     */
    reg("api/getTimeZones", new QnGetTimeZonesRestHandler());

    /**%apidoc GET /api/getNonce
     * Return authentication parameters: "nonce" and "realm".
     * %return:object JSON object with an error code, error string, and the reply on success.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *     %param:object reply Object with the nonce parameters.
     *         %param:string reply.realm A string token used in authentication methods as "realm".
     *         %param:string reply.nonce A session key for the current user. The current server
     *             time is used as a nonce value, and the nonce is valid for about 5 minutes.
     */
    reg("api/getNonce", new QnGetNonceRestHandler());

    /**%apidoc[proprietary] TODO /api/getRemoteNonce
     * %// TODO: Write apidoc comment.
     */
    reg("api/getRemoteNonce", new QnGetNonceRestHandler("/api/getNonce"));

    /**%apidoc POST /api/cookieLogin
     * Log in to the Server using cookie-based authentication. For usage details, refer to the
     * <b>"Cookie-based authentication"</b> section of this documentation.
     * %param:string auth The authentication hash, calculated using a special value "GET" for the
     *     <i>method</i> parameter of the algorithm.
     */
    reg("api/cookieLogin", new QnCookieLoginRestHandler());

    /**%apidoc[proprietary] TODO /api/cookieLogout
     * %// TODO: Write apidoc comment.
     */
    reg("api/cookieLogout", new QnCookieLogoutRestHandler());

    /**%apidoc[proprietary] TODO /api/getCurrentUser
     * %// TODO: Write apidoc comment.
     */
    reg("api/getCurrentUser", new QnCurrentUserRestHandler());

    /**%apidoc POST /api/activateLicense
     * Activate the new License and return the signed License Block on success. The activation is
     * performed by sending the License serial number and the current Server Hardware Id to the
     * License Server over the internet (thus, an internet connection is required), and getting
     * back the LicenseBlock signed by the License Server. After activation, the License is
     * registered in the System the same way as POST /ec2/addLicenses does.
     * %param:string licenseKey License serial number.
     * %return:object JSON object containing the structured representation of the License Block.
     *     See the description of License Block properties in the documentation for the result of
     *     <code>GET /ec2/getLicenses</code>.
     *     %struct DetailedLicenseData
     */
    reg("api/activateLicense", new QnActivateLicenseRestHandler());

    /**%apidoc[proprietary] TODO /api/testEmailSettings
     * %// TODO: Write apidoc comment.
     */
    reg("api/testEmailSettings", new QnTestEmailSettingsHandler());

    /**%apidoc[proprietary] GET /api/getHardwareInfo
     * Get hardware information
     * %return:object JSON with hardware information.
     */
    reg("api/getHardwareInfo", new QnGetHardwareInfoHandler());

    /**%apidoc[proprietary] TODO /api/testLdapSettings
     * %// TODO: Write apidoc comment.
     */
    reg("api/testLdapSettings", new QnTestLdapSettingsHandler());

    /**%apidoc GET /api/ping
     * Ping the Server.
     * %return:object JSON object with an error code, error string, and the reply on success.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *     %param:object reply Object with data.
     *         %param:uuid reply.moduleGuid Id of the Server Module.
     *         %param:uuid reply.localSystemId Id of the System.
     *         %param[proprietary]:integer sysIdTime
     *         %param[proprietary] tranLogTime
     */
    reg("api/ping", new QnPingRestHandler());

    /**%apidoc GET /api/metrics
     * %param[opt]:string brief Suppress parameters description and other details in result JSON file.
     *     Keep parameter name and its value only.
     * %return:object JSON with various counters that display server load, amount of DB transactions, etc.
     *     These counters may be used for server health monitoring, etc.
     */
    reg("api/metrics", new QnMetricsRestHandler());

    reg(::rest::helper::P2pStatistics::kUrlPath, new QnP2pStatsRestHandler());

    /**%apidoc[proprietary] TODO /api/recStats
     * %// TODO: Write apidoc comment.
     */
    reg("api/recStats", new QnRecordingStatsRestHandler(serverModule()));

    /**%apidoc GET /api/auditLog
     * Return audit log information.
     * %permissions Owner.
     * %param:string from Start time of a time interval (as a string containing time in
     *     milliseconds since epoch, or a local time formatted like
     *     <code>"<i>YYYY</i>-<i>MM</i>-<i>DD</i>T<i>HH</i>:<i>mm</i>:<i>ss</i>.<i>zzz</i>"</code>
     *     - the format is auto-detected).
     * %param[opt]:string to End time of a time interval(as a string containing time in
     *     milliseconds since epoch, or a local time formatted like
     *     <code>"<i>YYYY</i>-<i>MM</i>-<i>DD</i>T<i>HH</i>:<i>mm</i>:<i>ss</i>.<i>zzz</i>"</code>
     *     - the format is auto-detected).
     * %return:text Tail of the server log file in plain text format.
     */
    reg("api/auditLog", new QnAuditLogRestHandler(serverModule()), kAdmin);

    /**%apidoc[proprietary] TODO /api/checkDiscovery
     * %// TODO: Write apidoc comment.
     */
    reg("api/checkDiscovery", new QnCanAcceptCameraRestHandler());

    /**%apidoc GET /api/pingSystem
     * Ping the system.
     * %param:string url System URL to ping.
     * %param:string getKey Authorization key ("auth" param) for the System to ping.
     * %return:object JSON object with an error code, error string, and the reply on success.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error token, or an empty string on success.
     *         %value "FAIL" The specified System is unreachable or there is no System.
     *         %value "UNAUTHORIZED" The authentication credentials are invalid.
     *         %value "INCOMPATIBLE" The found system has an incompatible version or a different
     *             customization.
     *     %param:object reply Module information.
     */
    reg("api/pingSystem", new QnPingSystemRestHandler());

    /**%apidoc POST /api/changeCameraPassword
     * Change password for already existing user on a camera.
     * This method is allowed for cameras with 'SetUserPasswordCapability' capability only.
     * Otherwise it returns an error in the JSON result.
     * %permissions Owner.
     * %param:string cameraId Camera id (can be obtained from "id" field via /ec2/getCamerasEx)
     * or MAC address (not supported for certain cameras).
     * %param:string user User name.
     * %param:string password New password to set.
     * %return:object JSON object with error message and error code (0 means OK).
     */
    reg("api/changeCameraPassword", new QnChangeCameraPasswordRestHandler(), kAdmin);

    /**%apidoc GET /api/rebuildArchive
     * Start or stop the server archive rebuilding, also can report this process status.
     * %param[opt]:enum action What to do and what to report about the server archive rebuild.
     *     %value start Start server archive rebuild.
     *     %value stop Stop rebuild.
     *     %value <any_other_value_or_no_parameter> Report server archive rebuild status
     * %param:integer mainPool 1 (for the main storage) or 0 (for the backup storage)
     * %return:object Rebuild progress status or an error code.
     */
    reg("api/rebuildArchive", new QnRebuildArchiveRestHandler(serverModule()));

    /**%apidoc GET /api/backupControl
     * Start or stop the recorded data backup process, also can report this process status.
     * %param[opt]:enum action What to do and what to report about the backup process.
     *     %value start Start backup just now.
     *     %value stop Stop backup.
     *     %value <any_other_value_or_no_parameter> Report the backup process status.
     * %return:object Backup process progress status or an error code.
     */
    reg("api/backupControl", new QnBackupControlRestHandler(serverModule()));

    /**%apidoc[proprietary] GET /api/events
     * Return event log in the proprietary binary format.
     * %param:string from Start of time period (in milliseconds since epoch).
     * %param[opt]:string to End of time period (in milliseconds since epoch).
     * %param[opt]:enum event Event type.
     * %param[opt]:enum action Action type.
     * %param[opt]:uuid brule_id Event rule id.
     * %return:binary Server event log in the proprietary binary format.
     */
    reg("api/events", new QnEventLogRestHandler(serverModule()), kViewLogs); //< deprecated, still used in the client

    /**%apidoc GET /api/getEvents
     * Get server event log information.
     * %permissions At least Advanced Viewer.
     * %param:string from Start time of a time interval (as a string containing time in
     *     milliseconds since epoch, or a local time formatted like
     *     <code>"<i>YYYY</i>-<i>MM</i>-<i>DD</i>T<i>HH</i>:<i>mm</i>:<i>ss</i>.<i>zzz</i>"</code>
     *     - the format is auto-detected).
     * %param:string to End time of a time interval (as a string containing time in
     *     milliseconds since epoch, or a local time formatted like
     *     <code>"<i>YYYY</i>-<i>MM</i>-<i>DD</i>T<i>HH</i>:<i>mm</i>:<i>ss</i>.<i>zzz</i>"</code>
     *     - the format is auto-detected).
     * %param[opt]:string cameraId Camera id (can be obtained from "id" field via
     *     /ec2/getCamerasEx) or MAC address (not supported for
     *     certain cameras). Used to filter events log by a single camera.
     * %param[opt]:enum event_type Filter events log by specified event type.
     * %param[opt]:enum action_type Filter events log by specified action type.
     * %param[opt]:uuid brule_id Filter events log by specified event rule (keep only records
     *     generated via that event rule). This id could be obtained via /ec2/getEventRules.
     * %return:object JSON object with an error code, error string, and the reply on success.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *     %param:array reply List of objects describing the events.
     *         %param:enum reply[].actionType Type of the action.
     *             %value UndefinedAction
     *             %value CameraOutputAction Change camera output state.
     *             %value BookmarkAction
     *             %value CameraRecordingAction Start camera recording.
     *             %value PanicRecordingAction Activate panic recording mode.
     *             %value SendMailAction Send an email.
     *             %value DiagnosticsAction Write a record to the server's log.
     *             %value ShowPopupAction
     *             %value PlaySoundAction
     *             %value PlaySoundOnceAction
     *             %value SayTextAction
     *             %value ExecutePtzPresetAction Execute given PTZ preset.
     *             %value ShowTextOverlayAction Show text overlay over the given camera(s).
     *             %value ShowOnAlarmLayoutAction Put the given camera(s) to the Alarm Layout.
     *             %value ExecHttpRequestAction Send HTTP request as an action.
     *             %value BuzzerAction Enable an NVR buzzer
     *         %param:object reply[].actionParams JSON object with action parameters. Only fields
     *             that are applicable to the particular action are used.
     *             %param:uuid reply[].actionParams.actionResourceId Additional parameter for event
     *                 log convenience.
     *             %param:string reply[].actionParams.url Play Sound / exec HTTP action.
     *             %param:string reply[].actionParams.emailAddress Email.
     *             %param:enum reply[].actionParams.userGroup Popups and System Health.
     *                 %value EveryOne
     *                 %value AdminOnly
     *             %param:integer reply[].actionParams.fps Frames per second for recording.
     *             %param:enum reply[].actionParams.streamQuality Stream quality for recording.
     *                 %value undefined
     *                 %value lowest
     *                 %value low
     *                 %value normal
     *                 %value high
     *                 %value highest
     *                 %value preset
     *                 %//value[proprietary] rapidReview
     *             %param:integer reply[].actionParams.recordingDuration Duration of the recording,
     *                 in seconds.
     *             %param:integer reply[].actionParams.recordAfter For Bookmark, extension to the
     *                 recording time, in seconds.
     *             %param:string reply[].actionParams.relayOutputId Camera Output.
     *             %param:string reply[].actionParams.sayText
     *             %param:string reply[].actionParams.tags Bookmark.
     *             %param:string reply[].actionParams.text Text for Show Text Overlay, or message
     *                 body for Exec HTTP Action.
     *             %param:integer reply[].actionParams.durationMs Duration in milliseconds for
     *                 Bookmark and Show Text Overlay.
     *             %param:arrayJson reply[].actionParams.additionalResources JSON list of ids of
     *                 additional resources; user ids for Show On Alarm Layout.
     *             %param:boolean reply[].actionParams.forced Alarm Layout - if it must be opened
     *                 immediately.
     *                 %value true
     *                 %value false
     *             %param:string reply[].actionParams.presetId Execute PTZ preset action.
     *             %param:boolean reply[].actionParams.useSource Alarm Layout - if the source
     *                 resource should also be used.
     *             %param:integer reply[].actionParams.recordBeforeMs Bookmark start time is
     *                 adjusted to the left by this value in milliseconds.
     *             %param:boolean reply[].actionParams.playToClient Text to be pronounced.
     *             %param:string reply[].actionParams.contentType HTTP action.
     *         %param:object reply[].eventParams JSON object with event parameters.
     *             %param:enum reply[].eventParams.eventType Type of the event.
     *                 %value UndefinedEvent Event type is not defined. Used in rules.
     *                 %value CameraMotionEvent Motion has occurred on a camera.
     *                 %value CameraInputEvent Camera input signal is received.
     *                 %value CameraDisconnectEvent Camera was disconnected.
     *                 %value StorageFailureEvent Storage read error has occurred.
     *                 %value NetworkIssueEvent Network issue: packet lost, RTP timeout, etc.
     *                 %value CameraIpConflictEvent Found some cameras with same IP address.
     *                 %value ServerFailureEvent Connection to server lost.
     *                 %value ServerConflictEvent Two or more servers are running.
     *                 %value ServerStartEvent Server started.
     *                 %value LicenseIssueEvent Not enough licenses.
     *                 %value BackupFinishedEvent Archive backup done.
     *                 %value SystemHealthEvent System health message.
     *                 %value MaxSystemHealthEvent System health message.
     *                 %value AnyCameraEvent Event group.
     *                 %value AnyServerEvent Event group.
     *                 %value AnyBusinessEvent Event group.
     *                 %value UserDefinedEvent Base index for the user-defined events.
     *             %param:integer reply[].eventParams.eventTimestampUsec When did the event occur,
     *                 in microseconds.
     *             %param:uuid reply[].eventParams.eventResourceId Event source - camera or server
     *                 id.
     *             %param:string reply[].eventParams.resourceName Name of the resource which caused
     *                 the event. Used if no resource is actually registered in the system. Generic
     *                 event can provide some resource name which doesn't match any resourceId in
     *                 the system. In this case resourceName is filled and resourceId remains
     *                 empty.
     *             %param:uuid reply[].eventParams.sourceServerId Id of a server that generated the
     *                 event.
     *             %param:enum reply[].eventParams.reasonCode Used in Reasoned Events as a reason
     *                 code.
     *                 %value NoReason
     *                 %value NetworkNoFrameReason
     *                 %value NetworkConnectionClosedReason
     *                 %value NetworkRtpPacketLossReason
     *                 %value ServerTerminatedReason
     *                 %value ServerStartedReason
     *                 %value StorageIoErrorReason
     *                 %value StorageTooSlowReason
     *                 %value StorageFullReason
     *                 %value LicenseRemoved
     *                 %value BackupFailedNoBackupStorageError
     *                 %value BackupFailedSourceStorageError
     *                 %value BackupFailedSourceFileError
     *                 %value BackupFailedTargetFileError
     *                 %value BackupFailedChunkError
     *                 %value BackupEndOfPeriod
     *                 %value BackupDone
     *                 %value BackupCancelled
     *                 %value NetworkNoResponseFromDevice
     *             %param:string reply[].eventParams.inputPortId Used for Input events only.
     *             %param:string reply[].eventParams.caption Short event description. Used for
     *                 camera/server conflict as resource name which cause error. Used in generic
     *                 events as a short description.
     *             %param:string reply[].eventParams.description Long event description. Used for
     *                 camera/server conflict as a long description (conflict list). Used in
     *                 Reasoned Events as reason description. Used in generic events as a long
     *                 description.
     *             %param:arrayJson reply[].eventParams.metadata Camera list which is associated
     *                 with the event. EventResourceId may be a POS terminal, but this is a camera
     *                 list which should be shown with this event.
     *         %param:uuid reply[].businessRuleId Id of the event rule.
     *         %param:integer reply[].aggregationCount Number of identical events grouped into one.
     *         %param[proprietary]:flags reply[].flags Combination (via "|") of the flags.
     *             %value VideoLinkExists
     */
    reg("api/getEvents", new QnEventLog2RestHandler(serverModule()), kViewLogs); //< new version

    /**%apidoc[proprietary] TODO /ec2/getEvents
     * %// TODO: Write apidoc comment.
     */
    reg("ec2/getEvents", new QnMultiserverEventsRestHandler(serverModule(), "ec2/getEvents"), kViewLogs);

    /**%apidoc GET /api/showLog
     * Return the tail of the Server log file.
     * %param[opt]:integer lines Display the last N log lines.
     * %param[opt]:integer id Id of the log file. By default, the main log is returned.
     *     %value 0 Main server log.
     *     %value 2 Http log.
     *     %value 3 Transaction log.
     * %return:text Tail of the Server log file in the text format.
     */
    reg("api/showLog", new QnLogRestHandler(), kAdmin);

    /**%apidoc[proprietary] TODO /api/getSystemId
     * %// TODO: Write apidoc comment.
     */
    reg("api/getSystemId", new QnGetSystemIdRestHandler());

    /**%apidoc GET /api/doCameraDiagnosticsStep
     * Performs camera diagnostics.
     * %param:string cameraId Camera id (can be obtained from "id" field via /ec2/getCamerasEx)
     *     or MAC address (not supported for certain cameras).
     * %param:enum type Diagnostics to perform.
     *     %value mediaServerAvailability Checks server availability
     *     %value cameraAvailability Checks if camera is accessible from the server
     *     %value mediaStreamAvailability Checks if camera media stream can be opened
     *     %value mediaStreamIntegrity Checks additional media stream parameters
     * %return:object JSON object with an error code, error string, and the reply on success.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *     %param:object reply Diagnostics result.
     */
    reg("api/doCameraDiagnosticsStep", new QnCameraDiagnosticsRestHandler(serverModule()));

    /**%apidoc POST /api/restart
     * Restarts the server.
     * %permissions Owner.
     * %return:object JSON object with error message and error code (0 means OK).
     */
    reg("api/restart", new QnRestartRestHandler(), kAdmin);

    /**%apidoc[proprietary] TODO /api/connect
     * %// TODO: Write apidoc comment.
     */
    reg("api/connect", new QnOldClientConnectRestHandler());

    /**%apidoc GET /api/moduleInformation
     * Get information about the server.
     * %param[opt]:boolean allModules Set it to true to get all modules from the system.
     * %param[opt]:boolean showAddresses Set it to true to show server addresses.
     * %return:object JSON object with module information.
     */
    reg("api/moduleInformation", new QnModuleInformationRestHandler(commonModule()));

    /**%apidoc GET /api/iflist
     * Get network settings (list of interfaces) for the server. Can be called only if server flags
     * include "SF_IfListCtrl" (server flags can be obtained via /ec2/getMediaServersEx in
     * "flags" field).
     * %return:object List of objects with interface parameters.
     *     %param:string  name Interface name.
     *     %param:string ipAddr IP address with dot-separated decimal components.
     *     %param:string netMask Network mask with dot-separated decimal components.
     *     %param:string mac MAC address with colon-separated upper-case hex components.
     *     %param:string gateway IP address of the gateway with dot-separated decimal components.
     *         Can be empty.
     *     %param:boolean dhcp
     *         %value false DHCP is not used, IP address and other parameters should be specified
     *             in the respective JSON fields.
     *         %value true IP address and other parameters assigned via DHCP, the respective JSON
     *             fields can be empty.
     *     %param:object extraParams JSON object with data in the internal format.
     *     %param:string dns_servers Space-separated list of IP addresses with dot-separated
     *         decimal components.
     */
    reg("api/iflist", new QnIfListRestHandler());

    /**%apidoc GET /api/aggregator
     * This function allows to execute several requests with json content type and returns result
     * as a single JSON object.
     * %param[opt]:string exec_cmd HTTP url path to execute. This parameter could be repeated
     *     several times to execute several nested methods. All additions parameters after current
     *     "exec_cmd" and before next "exec_cmd" are passed as parameters to the nested method.
     * %return:object Merged JSON data from nested methods.
     */
    reg("api/aggregator", new QnJsonAggregatorRestHandler());

    /**%apidoc POST /api/ifconfig
     * Set new network settings (list of interfaces) for the server. Can be called only if server
     * flags include "SF_IfListCtrl" (server flags can be obtained via /ec2/getMediaServersEx
     * in "flags" field). <p> Parameters should be passed as a JSON array of objects in POST
     * message body with content type "application/json". Example of such object can be seen in
     * the result of GET /api/iflist function. </p>
     * %permissions Owner.
     * %param:array interfaces List of network interfaces settings
     *     %param:string interfaces[].name  Interface name.
     *     %param:string interfaces[].ipAddr IP address with dot-separated decimal components.
     *     %param:string interfaces[].netMask Network mask with dot-separated decimal components.
     *     %param:string interfaces[].mac MAC address with colon-separated upper-case hex components.
     *     %param:string interfaces[].gateway IP address of the gateway with dot-separated decimal components. Can
     *         be empty.
     *     %param:boolean interfaces[].dhcp
     *         %value false DHCP is not used, IP address and other parameters should be specified in
     *             the respective JSON fields.
     *         %value true IP address and other parameters assigned via DHCP, the respective JSON
     *             fields can be empty.
     *     %param:object interfaces[].extraParams JSON object with data in the internal format.
     *     %param:string interfaces[].dns_servers Space-separated list of IP addresses with dot-separated decimal
     *         components.
     */
    reg("api/ifconfig", new QnIfConfigRestHandler(), kAdmin);

    /**%apidoc DELETE /api/downloads/{fileName}
     * Deletes the record about the specified file previously registered in the Server's Downloader
     * via POST /api/downloads/{fileName}.
     * %param[opt]:boolean deleteData If set to false, the actual file will not be deleted from
     *     the Server's filesystem. Default: true.
     *
     **%apidoc GET /api/downloads/{fileName}/chunks/{chunkIndex}
     * Retrieves the binary content of the specified chunk ({chunkIndex} is zero-based) of the file
     * previously registered in the Server's Downloader via POST /api/downloads/{fileName}.
     * %param[proprietary]:boolean fromInternet Whether the chunk should be downloaded from the
     *     internet and stored in the Server's Downloader. Default: false.
     * %param[proprietary]:string url Must be specified if and only if fromInternet is true.
     * %param[proprietary]:integer chunkSize Size of the chunk, in bytes, from 1 to 10485760 (10
     *     MB). Must be specified if and only if fromInternet is true.
     * %return:binary Content of the specified file chunk, as raw bytes.
     *
     **%apidoc GET /api/downloads/{fileName}/checksums
     * Retrieves chunk checksums for the specified file previously registered in the Server's
     * Downloader via POST /api/downloads/{fileName}.
     * %param[proprietary]:option extraFormatting If present and the requested result format is
     *     non-binary, indentation and spacing will be used to improve readability.
     * %return:stringArray List of base64-encoded checksums for all chunks.
     *
     **%apidoc GET /api/downloads/status
     * Retrieves the detailed status information for all files previously registered in the
     * Server's Downloader via POST /api/downloads/{fileName}.
     * %return:object JSON object with an error code, error message, and the reply data on success.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *     %param[opt]:array reply On success, contains the list of file information objects for
     *         each file registered in the Server's Downloader.
     *         %struct FileInformation
     *
     **%apidoc GET /api/downloads/{fileName}/status
     * Retrieves the detailed status information for the specified file previously registered in
     * the Server's Downloader via POST /api/downloads/{fileName}.
     * %return:object JSON object with an error code, error message, and the reply data on success.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *     %param[opt]:object reply On success, contains the file information object.
     *         %struct FileInformation
     *
     **%apidoc[proprietary] PUT /api/downloads/{fileName}
     * Equivalent of POST /api/downloads/{fileName}.
     *
     **%apidoc POST /api/downloads/{fileName}
     * Registers the file in the Server's Downloader. Here {fileName} is an arbitrary identifier
     * for the file; it must be unique throughout the entire VMS System, and is intended to be used
     * in further calls related to this file record.
     * <br/>
     * After registering, starts the download session for the file - the content for the file will
     * be downloaded by the Server from the internet and/or other Servers in the System which have
     * already downloaded certain chunks of the file.
     * <br/>
     * The file, when its content is obtained by the Server, can then be used for various purposes,
     * e.g. to create a Virtual Camera having the file content as its video archive.
     * %// ATTENTION: This API function has an option-typed param `upload`, which, if present,
     *     changes the set of other params and the semantics of this function, thus, such case is
     *     documented as a separate API function with `?upload` appended to the function name.
     * %param[opt]:integer size File size, in bytes. Can be omitted if unknown.
     * %param[opt]:string md5 MD5 checksum of the whole file content. Can be omitted if unknown.
     * %param:string url Internet URL from which the Server will download the file content.
     * %param[proprietary]:string peerPolicy Default: none. Defines whether the Server will attempt
     *     to download the file from other Servers in the System, and which Servers.
     *     %value none Do not attempt to download the file from other Servers of the System.
     *     %value all Attempt to download the file from other Servers of the System.
     *     %value byPlatform Attempt to download the file from other Servers of the System, but
     *         only those Servers which have the same hardware platform.
     * %return:object JSON object with an error code and error message.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *
     **%apidoc[proprietary] PUT /api/downloads/{fileName}?upload
     * Equivalent of POST /api/downloads/{fileName}?upload.
     *
     **%apidoc POST /api/downloads/{fileName}?upload
     * Registers the file in the Server's Downloader. Here {fileName} is an arbitrary identifier
     * for the file; it must be unique throughout the entire VMS System, and is intended to be used
     * in further calls related to this file record.
     * <br/>
     * After registering, starts the upload session for the file - the content for the file should
     * then be uploaded chunk-by-chunk via PUT /api/downloads/{fileName}/chunks/{chunkIndex}.
     * <br/>
     * The file, when its content is obtained by the Server, can then be used for various purposes,
     * e.g. to create a Virtual Camera having the file content as its video archive.
     * %// ATTENTION: This function has `?upload` appended to its name. The corresponding API call
     *     without `upload` param is documented as a separate function.
     * %param:integer size File size, in bytes.
     * %param:string md5 MD5 checksum of the entire file.
     * %param:integer chunkSize Size of the chunk, in bytes, from 1 to 10485760 (10 MB).
     * %param[opt]:integer ttl If specified, defines the time-to-live for the file, in
     *     milliseconds: if the file was not written to during the specified amount of time, its
     *     record and content will be deleted from the Server.
     * %param[opt]:boolean recreate Allows to create the new file record if a record for the same
     *     {fileName} already exists. Default: false.
     * %return:object JSON object with an error code and error message.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *
     **%apidoc[proprietary] POST /api/downloads/{fileName}/chunks/{chunkIndex}
     * Equivalent of PUT /api/downloads/{fileName}/chunks/{chunkIndex}.
     *
     **%apidoc PUT /api/downloads/{fileName}/chunks/{chunkIndex}
     * Sends to the Server the binary content of the specified chunk of the file previously
     * registered in the Server's Downloader via POST /api/downloads/{fileName}. Here {chunkIndex}
     * is zero-based, and the request body must contain the chunk bytes and must have
     * "Content-Type: application/octet-stream". To validate the upload, call
     * GET /api/downloads/{fileName}/status.
     * %return:object JSON object with an error code and error message.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     */
    reg("api/downloads/", new nx::vms::server::rest::handlers::Downloads(serverModule()));

    /**%apidoc[proprietary] GET /api/settime
     * Set current time on the server machine. Can be called only if server flags include
     * "SF_timeCtrl" (server flags can be obtained via /ec2/getMediaServersEx in "flags"
     * field).
     * %permissions Owner.
     * %param[opt]:string timezone Time zone identifier, can be obtained via /api/getTimeZones.
     * %param:string datetime System date and time (as a string containing time in milliseconds
     *     since epoch, or a local time formatted like
     *     <code>"<i>YYYY</i>-<i>MM</i>-<i>DD</i>T<i>HH</i>:<i>mm</i>:<i>ss</i>.<i>zzz</i>"</code>
     *     - the format is auto-detected).
     */
    reg("api/settime", new QnSetTimeRestHandler(), kAdmin); //< deprecated

    /**%apidoc POST /api/setTime
     * Set current time on the server machine.
     * Can be called only if server flags include "SF_timeCtrl"
     * (server flags can be obtained via /ec2/getMediaServersEx in "flags" field).
     * <p>
     *     Parameters should be passed as a JSON object in POST message body with
     *     content type "application/json". Example of such object:
     * <pre><code>
     * {
     *     "dateTime": "2015-02-28T16:37:00",
     *     "timeZoneId": "Europe/Moscow"
     * }
     * </code></pre>
     * </p>
     * %permissions Owner.
     * %param[opt]:string timeZoneId Time zone identifier, can be obtained via /api/getTimeZones.
     * %param:string dateTime Date and time (as string containing time in milliseconds since
     *     epoch, or a local time formatted like
     *     <code>"<i>YYYY</i>-<i>MM</i>-<i>DD</i>T<i>HH</i>:<i>mm</i>:<i>ss</i>.<i>zzz</i>"</code>
     *     - the format is auto-detected).
     */
    reg("api/setTime", new QnSetTimeRestHandler(), kAdmin); //< new version

    /**%apidoc GET /api/moduleInformationAuthenticated
     * The same as moduleInformation but requires authentication. Useful to test connection.
     * %return:object JSON object with module information.
     */
    reg("api/moduleInformationAuthenticated", new QnModuleInformationRestHandler(commonModule()));

    /**%apidoc POST /api/configure
     * Configure various server parameters.
     * %permissions Owner.
     * %param[opt]:string systemName System display name. It affects all servers in the system.
     * %param[opt]:integer port Server API port. It affects the current server only.
     * %param[opt]:string password Set new admin password.
     * %param[opt]:string currentPassword Required if new admin password is provided.
     * %return JSON with error code, error string, and flag "restartNeeded" that shows whether the
     *     server must be restarted to apply settings. Error string contains a hint to identify the
     *     problem: "SYSTEM_NAME" or "PORT".
     */
    reg("api/configure", new QnConfigureRestHandler(serverModule()), kAdmin);

    /**%apidoc POST /api/detachFromCloud
     * Detaches the Server from the Cloud. Local admin user is enabled, admin password is changed to
     * new value (if specified), all cloud users are disabled. Cloud link is removed. Function can
     * be called either via GET or POST method. POST data should be a json object.
     * %permissions Owner.
     * %param[opt]:string password Set new admin password after detach.
     * %param:string currentPassword Current user password.
     * %return JSON result with error code.
     */
    reg("api/detachFromCloud", new QnDetachFromCloudRestHandler(serverModule(), cloudManagerGroup), kAdmin);

    /**%apidoc POST /api/detachFromSystem
     * Detaches the Server from the System and resets its state to the initial one.
     * %permissions Owner.
     * %param:string currentPassword Current user password.
     * %return JSON result with error code.
     */
    reg("api/detachFromSystem", new QnDetachFromSystemRestHandler(
        serverModule(), &cloudManagerGroup->connectionManager, messageBus), kAdmin);

    /**%apidoc[proprietary] POST /api/restoreState
     * Restore initial server state, i.e. <b>delete server's database</b>.
     * <br/>Server will restart after executing this command.
     * %permissions Owner.
     * %param:string currentPassword Current admin password
     * %return:object JSON object with error message and error code (0 means OK).
     */
    reg("api/restoreState", new QnRestoreStateRestHandler(serverModule()), kAdmin);

    /**%apidoc POST /api/setupLocalSystem
     * Configure Server's VMS System name and password. Can be called only for a Server with the
     *     default System name, otherwise, returns an error.
     * %permissions Owner.
     * %param:string password New password for "admin" user.
     * %param:string systemName New System name.
     * %param[opt]:object systemSettings JSON object containing a map of the VMS System settings
     *     (can be obtained via GET /api/systemSettings) that will be applied to the created System
     *     in addition to (on top of) the default ones.
     * %return:object JSON object with error message and error code (0 means OK).
     */
    reg("api/setupLocalSystem", new QnSetupLocalSystemRestHandler(serverModule()), kAdmin);

    /**%apidoc POST /api/setupCloudSystem
     * Configure Server's VMS System name, and attach the System to the Cloud. Can be called only
     *     for a Server with the default System name, otherwise, returns an error.
     * %permissions Owner.
     * %param:string systemName New System name.
     * %param:string cloudAuthKey Could authentication key.
     * %param:string cloudSystemID Could System id.
     * %param[opt]:object systemSettings JSON object containing a map of the VMS System settings
     *     (can be obtained via GET /api/systemSettings) that will be applied to the created System
     *     in addition to (on top of) the default ones.
     * %return:object JSON object with error message and error code (0 means OK).
     */
    reg("api/setupCloudSystem", new QnSetupCloudSystemRestHandler(serverModule(), cloudManagerGroup), kAdmin);

    /**%apidoc POST /api/mergeSystems
     * Merge two Systems. <br/> The System that joins another System is called the current System,
     * the joinable System is called the target System. The <b>URL</b> parameter sets the
     * target Server which should be joined with the current System. Other servers, that are
     * merged with the target Server will be joined if parameter <b>mergeOneServer</b> is set
     * to false. <br/> The method uses digest authentication. Two hashes should be previously
     * calculated: <b>getKey</b> and <b>postKey</b>. Both are mandatory. The calculation
     * algorithm is described in the <b>"Calculating authentication hash"</b> section of this
     * documentation. While calculating hashes, username and password of the target Server are
     * needed. Digest authentication needs realm and nonce, both can be obtained with <code>GET
     * /api/getNonce</code> call. The lifetime of a nonce is about a few minutes.
     * %permissions Owner.
     * %param:string currentPassword Current user password.
     * %param:string url URL of one Server in the target System to join. The URL may contain
     *     credentials for the target System, in that case getKey and postKey are not required.
     *     The target System user should have Owner permissions as well.
     * %param[opt]:string getKey Authorization key ("auth" param) for GET requests to the target
     *     System. Required if the URL does not contain credentials.
     * %param[opt]:string postKey Authorization key ("auth" param) for POST requests to the target
     *     System. Required if the URL does not contain credentials.
     * %param[opt]:boolean takeRemoteSettings Direction of the merge. Default value is false. If
     *     <b>mergeOneServer</b> is true, <b>takeRemoteSettings</b> parameter is ignored and
     *     treated as false.
     *     %value true The current system will get system name and administrator password of the
     *         target system.
     *     %value false The target system will get system name and administrator password of the
     *         current system.
     * %param[opt]:boolean mergeOneServer Whether to merge with servers merged with the target
     *     server. Default value is false. If <b>mergeOneServer</b> is set to true,
     *     <b>takeRemoteSettings</b> is ignored and treated as false.
     *     %value true The current system will merge with target server only. The target server
     *         will be disjoined from another system (if it was joined).
     *     %value false The current system will merge with target server and all servers which are
     *         merged with the target server.
     * %param[opt]:boolean ignoreIncompatible Whether to ignore different version of merged server
     *     protocols. Default value is false.
     *     %value true Merge will start anyway.
     *     %value false If the target server protocol version differs from the current server
     *         protocol version merge aborts.
     * %param[opt]:boolean dryRun Only check for merge possibility, do not perform actual merge.
     *     Known issues: dry run may produce false positive results in some rare cases.
     * %return:object JSON object with an error code and error string.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error token, or an empty string on success.
     *         %value "FAIL" The specified System is unreachable or there is no System.
     *         %value "UNAUTHORIZED" The authentication credentials are invalid.
     *         %value "INCOMPATIBLE" The found system has an incompatible version or a different
     *             customization.
     *         %value "FORBIDDEN" The user does not have permission for merge.
     *         %value "BACKUP_ERROR" The database backup could not be created.
     *         %value "STARTER_LICENSE_ERROR" You are about to merge Systems with Starter licenses.
     *         %value "CONFIGURATION_ERROR" Could not configure the remote System.
     *         %value "DEPENDENT_SYSTEM_BOUND_TO_CLOUD" System can only be merged with non-cloud.
     *         %value "CLOUD_SYSTEMS_HAVE_DIFFERENT_OWNERS" Cannot merge two cloud systems with
     *             different owners.
     *         %value "UNCONFIGURED_SYSTEM" Cannot merge to the unconfigured system.
     *         %value "UNKNOWN_ERROR" something unexpected has happend.
     *         %value "DUPLICATE_MEDIASERVER_FOUND" Cannot merge Systems because they have at least
     *             one Server with the same id.
     */
    reg("api/mergeSystems", new QnMergeSystemsRestHandler(serverModule()), kAdmin);

    /**%apidoc GET /api/backupDatabase
     * Back up the Server database.
     * %return:object JSON object with error message and error code (0 means OK).
     */
    reg("api/backupDatabase", new QnBackupDbRestHandler(serverModule()));

    /**%apidoc GET /api/discoveredPeers
     * Return a list of the discovered peers.
     * %return:object JSON with a list of the discovered peers.
     */
    reg("api/discoveredPeers", new QnDiscoveredPeersRestHandler());

    /**%apidoc GET /api/logLevel
     * Get or set server log level.
     * %param[opt]:integer name Log name.
     *     %value MAIN Main server log.
     *     %value HTTP HTTP log.
     *     %value EC2_TRAN Transaction log.
     *     %value HWID Service log.
     *     %value PERMISSIONS Permissions log.
     * %param[opt]:integer id Log id. Deprecated, use name instead.
     *     %value 0 Main server log.
     *     %value 1 HTTP log.
     *     %value 2 Service log.
     *     %value 3 Transaction log.
     *     %value 4 Permissions log.
     * %param[opt]:enum value Target value for log level. More detailed level includes all less
     *     detailed levels. This parameter is ignored if neither name nor id is specified.
     *     %value None Disable log.
     *     %value Always Log only the most important messages.
     *     %value Error Log errors.
     *     %value Warning Log warnings.
     *     %value Info Log information messages.
     *     %value Debug Log debug messages.
     *     %value Debug2 Log additional debug messages.
     */
    reg("api/logLevel", new QnLogLevelRestHandler());

    /**%apidoc[proprietary] GET /api/execute
     * Execute any script from subfolder "scripts" of the Server. Script name provides directly
     * in a URL path like "/api/execute/script1.sh". All URL parameters are passed directly to
     * a script as an parameters.
     * %permissions Owner.
     * %return:object JSON object with error message and error code (0 means OK).
     */
    reg("api/execute", new QnExecScript(serverModule()->settings().dataDir()), kAdmin);

    /**%apidoc[proprietary] GET /api/scriptList
     * Return list of scripts to execute.
     * %permissions Owner.
     * %return:object JSON object with string list.
     */
    reg("api/scriptList", new QnScriptListRestHandler(serverModule()->settings().dataDir()), kAdmin);

    /**%apidoc GET /api/systemSettings
     * Get or set global system settings. If called with no arguments, just returns the list of all
     * system settings with their values.
     * To modify a settings, it is needed to specify the setting name as a query parameter. Thus,
     * this method doesn't have fixed parameter names. To obtain the full list of possible names,
     * call this method without parameters.
     * Example: /api/systemSettings?smtpTimeout=30&amp;smtpUser=test
     */
    reg("api/systemSettings", new QnSystemSettingsHandler());

    /**%apidoc[proprietary] TODO /api/transmitAudio
     * %// TODO: Write apidoc comment.
     */
    reg("api/transmitAudio", new QnAudioTransmissionRestHandler(serverModule()));

    // TODO: Introduce constants for API methods registered here, also use them in
    // media_server_connection.cpp. Get rid of static/global urlPath passed to some handler ctors,
    // except when it is the path of some other api method.

    /**%apidoc[proprietary] TODO /api/RecordedTimePeriods
     * %// TODO: Write apidoc comment.
     */
    reg("api/RecordedTimePeriods", new QnRecordedChunksRestHandler(serverModule())); //< deprecated

    /**%apidoc GET /ec2/recordedTimePeriods
     * Return the recorded chunks info for the specified cameras.
     * %param:string cameraId Camera id (can be obtained from "id" field via /ec2/getCamerasEx)
     *     or MAC address (not supported for certain cameras).
     *     This parameter can be used several times to define a list of cameras.
     * %param[opt]:string startTime Start time of the interval (as a string containing time in
     *     milliseconds since epoch, or a local time formatted like
     *     <code>"<i>YYYY</i>-<i>MM</i>-<i>DD</i>T<i>HH</i>:<i>mm</i>:<i>ss</i>.<i>zzz</i>"</code>
     *     - the format is auto-detected).
     * %param[opt]:string endTime End time of the interval (as a string containing time in
     *     milliseconds since epoch, or a local time formatted like
     *     <code>"<i>YYYY</i>-<i>MM</i>-<i>DD</i>T<i>HH</i>:<i>mm</i>:<i>ss</i>.<i>zzz</i>"</code>
     *     - the format is auto-detected).
     * %param[opt]:arrayJson filter This parameter is used for Motion and Analytics Search
     *     (if "periodsType" is set to 1 or 2). Search motion or analytics event on a video
     *     according to the specified attribute values.
     *     <br/>Motion Search Format: string with a JSON list of <i>sensors</i>,
     *     each <i>sensor</i> is a JSON list of <i>rectangles</i>, each <i>rectangle</i> is:
     *     <br/>
     *     <code>
     *         {"x": <i>x</i>, "y": <i>y</i>, "width": <i>width</i>,"height": <i>height</i>}
     *     </code>
     *     <br/>All values are measured in relative portions of a video frame,
     *     <i>x</i> and <i>width</i> in range [0..43], <i>y</i> and <i>height</i> in range [0..31],
     *     zero is the left-top corner.
     *     <br/>Example of a full-frame rectangle for a single-sensor camera:
     *     <code>[[{"x":0,"y":0,"width":43,"height":31}]]</code>
     *     <br/>Example of two rectangles for a single-sensor camera:
     *     <code>[[{"x":0,"y":0,"width":5,"height":7},{"x":12,"y":10,"width":8,"height":6}]]</code>
     *     <br/>Analytics Search Format: string with a JSON object that might contain the following
     *     fields (keys):
     *     <br/>
     *     <ul>
     *     <li>"boundingBox" key represents a <i>rectangle</i>. The value is a JSON object with the
     *     same format as the Motion Search rectangle;</li>
     *     <li>"freeText" key for full-text search over analytics data attributes. The value is
     *     expected to be a string with search input;
     *     </li>
     *     </ul>
     *     <br/>Example of the JSON object:
     *     <code>{"boundingBox":{"height":0,"width":0.1,"x":0.0,"y":1.0},"freeText":"Test"}</code>
     * %param[proprietary]:enum format Data format. Default value is "json".
     *     %value ubjson Universal Binary JSON data format.
     *     %value json JSON data format.
     *     %value periods Internal compressed binary format.
     * %param[opt]:integer detail Chunk detail level, in milliseconds. Time periods that are
     *     shorter than the detail level are discarded. You can treat the detail level as the
     *     amount of microseconds per screen pixel.
     * %param[opt]:integer periodsType Chunk type.
     *     %value 0 All records.
     *     %value 1 Only chunks with motion (parameter "filter" might be applied).
     *     %value 2 Only chunks with analytics event (parameter "filter" might be applied).
     * %param[opt]:option keepSmallChunks If specified, standalone chunks smaller than the detail
     *     level are not removed from the result.
     * %param[opt]:integer limit Maximum number of chunks to return.
     * %param[opt]:option flat [DEPRECATED in favor of "groupBy"] If specified, do not group chunk
     *     lists by Server.
     * %param[opt]:enum groupBy group type. Default value is "serverId".
     *     %value serverId group data by serverId. Result field 'guid' has server Guid value.
     *     %value cameraId group data by cameraId. Result field 'guid' has camera Guid value.
     *     %value none do not group data. Result is the flat list of data.
     * %param[opt]:option desc Sort data in descending order if provided.
     * %return:object JSON object with an error code, error string, and the reply on success.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *     %param:array reply If no "flat" parameter is specified, it is the list which contains
     *         for each server its GUID (as "guid" field) and the list of chunks (as "periods"
     *         field). If "flat" parameter is specified, it is just the list of chunks.
     *         <br/>
     *         Each chunk is a pair of <code>(durationMs, startTimeMs)</code>. Chunks are merged
     *         for all requested cameras. Start time and duration are in milliseconds since epoch.
     *         Duration -1 means the last chunk is being recorded now.
     */
    reg(QnMultiserverChunksRestHandler::kUrlPath, new QnMultiserverChunksRestHandler(serverModule())); //< new version

    /**%apidoc[proprietary] TODO /ec2/cameraHistory
     * %// TODO: Write apidoc comment.
     */
    reg("ec2/cameraHistory", new QnCameraHistoryRestHandler(serverModule()));

    /**%apidoc GET /ec2/bookmarks
     * Read bookmarks using the specified parameters.
     * %param:string cameraId Camera id (can be obtained from "id" field via /ec2/getCamerasEx)
     *     or MAC address (not supported for certain cameras).
     * %param[opt]:string startTime Start time of the interval with bookmarks (in milliseconds
     *     since epoch). Default value is 0. Should be less than endTime.
     * %param[opt]:string endTime End time of the interval with bookmarks (in milliseconds since
     *     epoch). Default value is the current time. Should be greater than startTime.
     * %param[opt]:enum sortBy Field to sort the results by. Default value is "startTime".
     *     %value name Sort bookmarks by name.
     *     %value startTime Sort bookmarks by start time.
     *     %value duration Sort bookmarks by duration.
     *     %value cameraName Sort bookmarks by camera name.
     * %param[opt]:enum sortOrder Sort order. Default order is ascending.
     *     %value asc Ascending sort order.
     *     %value desc Descending sort order.
     * %param[opt]:integer limit Maximum number of bookmarks to return. Unlimited by default.
     * %param[opt]:string filter Text-search filter string.
     * %param[proprietary]:option local If present, the request should not be redirected to another
     *     server.
     * %param[proprietary]:option extraFormatting If present and the requested result format is
     *     non-binary, indentation and spacing will be used to improve readability.
     * %param[default]:enum format
     *
     **%apidoc GET /ec2/bookmarks/add
     * Add a bookmark to the target server.
     * %param:uuid guid Identifier of the bookmark.
     * %param:string cameraId Camera id (can be obtained from "id" field via /ec2/getCamerasEx)
     *     or MAC address (not supported for certain cameras).
     * %param:string name Caption of the bookmark.
     * %param[opt]:string description Details of the bookmark.
     * %param[proprietary]:integer timeout Time during which the recorded period should be preserved
     *     (in milliseconds).
     * %param:integer startTime Start time of the bookmark (in milliseconds since epoch).
     * %param:integer duration Length of the bookmark (in milliseconds).
     * %param[opt]:string tag Applied tag. Several "tag" parameters can be specified to set
     *     multiple tags.
     * %param[proprietary]:option local If present, the request should not be redirected to another
     *     server.
     * %param[proprietary]:option extraFormatting If present and the requested result format is
     *     non-binary, indentation and spacing will be used to improve readability.
     * %param[default]:enum format
     *
     **%apidoc GET /ec2/bookmarks/delete
     * Remove a bookmark with the specified identifier.
     * %param:uuid guid Identifier of the bookmark.
     * %param[proprietary]:option local If present, the request should not be redirected to another
     *     server.
     * %param[proprietary]:option extraFormatting If present and the requested result format is
     *     non-binary, indentation and spacing will be used to improve readability.
     * %param[default]:enum format
     *
     **%apidoc GET /ec2/bookmarks/tags
     * Return currently used tags.
     * %param[opt]:integer limit Maximum number of tags to return.
     * %param[proprietary]:option local If present, the request should not be redirected to another
     *     server.
     * %param[proprietary]:option extraFormatting If present and the requested result format is
     *     non-binary, indentation and spacing will be used to improve readability.
     * %param[default]:enum format
     *
     **%apidoc GET /ec2/bookmarks/update
     * Update information for a bookmark.
     * %param:uuid guid Identifier of the bookmark.
     * %param:string cameraId Camera id (can be obtained from "id" field via /ec2/getCamerasEx)
     *     or MAC address (not supported for certain cameras).
     * %param:string name Caption of the bookmark.
     * %param[opt]:string  description Details of the bookmark.
     * %param[proprietary]:integer timeout Time during which the recorded period should be preserved
     *     (in milliseconds).
     * %param:integer startTime Start time of the bookmark (in milliseconds since epoch).
     * %param:integer duration Length of the bookmark (in milliseconds).
     * %param[opt]:string tag Applied tag. Several "tag" parameters can be specified to set
     *     multiple tags.
     * %param[proprietary]:option local If present, the request should not be redirected to another
     *     server.
     * %param[proprietary]:option extraFormatting If present and the requested result format is
     *     non-binary, indentation and spacing will be used to improve readability.
     * %param[default]:enum format
     */
    reg("ec2/bookmarks", new QnMultiserverBookmarksRestHandler(serverModule(), "ec2/bookmarks"));

    /**%apidoc[proprietary] TODO /api/mergeLdapUsers
     * %// TODO: Write apidoc comment.
     */
    reg("api/mergeLdapUsers", new QnMergeLdapUsersRestHandler());

    /**%apidoc GET /ec2/updateInformation
     * Retrieves a currently present or specified via a parameter update information manifest.
     * %param[opt]:string version If present, The Server makes an attempt to retrieve an update
     *      manifest for the specified version id from the dedicated updates server and return it
     *      as a result.
     * %return:object JSON with the update manifest.
     */
    reg("ec2/updateInformation", new QnUpdateInformationRestHandler(serverModule()));

    /**%apidoc POST /ec2/startUpdate
     * Starts an update process.
     * %param:object JSON with the update manifest. This JSON might be requested with the
     *     /ec2/updateInformation?version=versionId request.
     */
    reg("ec2/startUpdate", new QnStartUpdateRestHandler(serverModule()), kAdmin);

    /**%apidoc POST /ec2/finishUpdate
     * Puts a system in the 'Update Finished' state.
     * %param[opt]:option ignorePendingPeers Force an update process completion regardless actual
     *     peers state.
     */
    reg("ec2/finishUpdate", new QnFinishUpdateRestHandler(serverModule()), kAdmin);

    /**%apidoc GET /ec2/updateStatus
     * Retrieves a current update processing system-wide state.
     * %return:object A JSON array with the current update per-server state. Possible values for a
     *     specific server are: idle, downloading, preparing, readyToInstall,
     *     latestUpdateInstalled, offline, error.
     */
    reg("ec2/updateStatus", new QnUpdateStatusRestHandler(serverModule()));

    /**%apidoc POST /api/installUpdate
     * Initiates update package installation.
     */
    reg("api/installUpdate", new QnInstallUpdateRestHandler(serverModule(),
        [this]() { m_installUpdateRequestReceived = true; }), kAdmin);

    /**%apidoc POST /ec2/cancelUpdate
     * Puts a system in the 'Idle' state update-wise. This means that the current update
     * manifest is cleared and all downloads are cancelled.
     */
    reg("ec2/cancelUpdate", new QnCancelUpdateRestHandler(serverModule()), kAdmin);

    /**%apidoc POST /ec2/retryUpdate
     * Retries the latest failed update action. E.g. if one of servers has failed update because
     * there was not enough free space, it will repty to reserve space and start downloading.
     * %return:object Update status after retry. See ec2/updateStatus.
     */
    reg("ec2/retryUpdate", new nx::vms::server::rest::handlers::RetryUpdate(serverModule()));

    /**%apidoc GET /ec2/cameraThumbnail
     * Get the static image from the camera.
     * %param:string cameraId Camera id (can be obtained from "id" field via /ec2/getCamerasEx)
     *     or MAC address (not supported for certain cameras).
     * %param[opt]:string time Timestamp of the requested image (as a string containing time in
     *     milliseconds since epoch, or a local time formatted like
     *     <code>"<i>YYYY</i>-<i>MM</i>-<i>DD</i>T<i>HH</i>:<i>mm</i>:<i>ss</i>.<i>zzz</i>"</code>
     *     - the format is auto-detected).<br/>
     *     The special value "now" requires to retrieve the thumbnail only from the live stream.
     *     <br/>The special value "latest", which is the default value, requires to retrieve
     *     thumbnail from the live stream if possible, otherwise the latest one from the archive.
     *     <br/>Note: Extraction from the archive can be quite slow depending on the place where the
     *     frame is stored.
     * %param[opt]:integer rotate Image orientation. Can be 0, 90, 180 or 270 degrees. If the
     *     parameter is absent or equals -1, the image will be rotated as defined in the camera
     *     settings.
     * %param[opt]:integer height Desired image height. Should be not less than 128, or equal to
     *     -1 (the default value) which implies the original frame size, and in this case the width
     *     should also be omitted or set to -1.
     * %param[opt]:integer width Desired image width. Should be not less than 128, or equal to -1
     *     (the default value) which implies auto-sizing: if the height is specified, the width
     *     will be calculated based on the aspect ratio, otherwise, the original frame size will be
     *     used.
     * %param[opt]:enum imageFormat Format of the requested image. Default value is "JpgFormat".
     *     %value png PNG
     *     %value jpg JPEG
     *     %value tif TIFF
     *     %value raw Raw video frame. Makes the request much more lightweight for Edge servers.
     * %param[opt]:enum method Getting a thumbnail at the exact timestamp is costly, so, it
     *     can be rounded to the nearest keyframe, thus, the default value is "after".
     *     %value before Get the thumbnail from the nearest keyframe before the given time.
     *     %value precise Get the thumbnail as near to given time as possible.
     *     %value after Get the thumbnail from the nearest keyframe after the given time.
     *     %// TODO: Rename this param to "rounding", and values to "iFrameBefore", "precise" and
     *         "iFrameAfter", deprecating the old method name and param names.
     * %param[opt]:enum streamSelectionMode Policy for stream selection. Default value is "auto".
     *     %value auto Chooses the most suitable stream automatically.
     *     %value forcedPrimary Primary stream is forced. Secondary stream will be used if the
     *         primary one is not available.
     *     %value forcedSecondary Secondary stream is forced. Primary stream will be used if the
     *         secondary one is not available.
     *     %value sameAsMotion Use the same stream as the one used by software motion detection.
     * %param[opt]:enum aspectRatio Allows to avoid scaling the image to the aspect ratio from
     *     camera settings.
     *     %value auto Default value. Use aspect ratio from camera settings (if any).
     *     %value source Use the source frame aspect ratio, despite the value in camera settings.
     * %param[opt]:option ignoreExternalArchive If present and "time" parameter has value
     *     "latest", the image will not be downloaded from archive of the dts-based devices.
     * %param[opt]:string crop Apply cropping to the source image. Parameter defines rect in range [0..1].
     *     Format: 'left,top,widthxheight'. Example: '0.5,0.4,0.25x0.3'.
     * %param[proprietary]:option local If present, the request should not be redirected to another
     *     server.
     * %param[proprietary]:option extraFormatting If present and the requested result format is
     *     non-binary, indentation and spacing will be used to improve readability.
     * %param[default]:enum format
     */
    reg("ec2/cameraThumbnail", new QnMultiserverThumbnailRestHandler(serverModule(), "ec2/cameraThumbnail"));

    /**%apidoc[proprietary] TODO /ec2/statistics
     * %// TODO: Write apidoc comment.
     */
    reg("ec2/statistics", new QnMultiserverStatisticsRestHandler("ec2/statistics"));

    /**%apidoc[proprietary] GET /ec2/getStatisticsReport
     * Get anonymous statistic about the System.
     * %return:object System statistics:
     *     %struct SystemStatistics
     */
    reg("ec2/getStatisticsReport",
        new nx::vms::server::rest::GetStatisticsReportHandler(serverModule()));

    /**%apidoc[proprietary] POST /ec2/triggerStatisticsReport
     * Initiate delivery of the System statistics to the statistics server.
     * %return:object Status of the triggered delivery operation:
     *     %struct StatisticsServerInfo
     */
    reg("ec2/triggerStatisticsReport",
        new nx::vms::server::rest::TriggerStatisticsReportHandler(serverModule()));


    /**%apidoc GET /ec2/analyticsLookupObjectTracks
     * Search analytics DB for objects that match filter specified.
     * %param[opt]:string deviceId device id (can be obtained from "id" field via
     *     /ec2/getCamerasEx), or MAC address (not supported for
     *     certain cameras), or "Logical Id".
     * %param[opt]:string objectTypeId Analytics Object Type id.
     * %param[opt]:string objectTrackId Analytics Object Track id.
     * %param[opt]:integer startTime Milliseconds since epoch (1970-01-01 00:00, UTC).
     * %param[opt]:integer endTime Milliseconds since epoch (1970-01-01 00:00, UTC).
     * %param[opt]:float x1 Top left "x" coordinate of picture bounding box to search within. In
     *     range [0.0; 1.0].
     * %param[opt]:float y1 Top left "y" coordinate of picture bounding box to search within. In
     *     range [0.0; 1.0].
     * %param[opt]:float x2 Bottom right "x" coordinate of picture bounding box to search within.
     *     In range [0.0; 1.0].
     * %param[opt]:float y2 Bottom right "y" coordinate of picture bounding box to search within.
     *     In range [0.0; 1.0].
     * %param[opt]:string freeText Text to match within Object Track properties.
     * %param[opt]:integer limit Maximum number of Object Tracks to return.
     * %param[opt]:boolean needFullTrack Whether to select track details data. This is a heavy
     *     operation and makes the request much slower. For performance reason this parameter
     *     is ignored in case the parameter 'objectTrackId' is not present in the request.
     * %param[opt]::enum sortOrder Sort order of Object Tracks by a Track start timestamp.
     *     %value asc Ascending order.
     *     %value desc Descending order.
     * %param[opt] isLocal If "false" then request is forwarded to every other online Server and
     *     results are merged. Otherwise, request is processed on receiving Server only.
     * %return JSON data.
     */
    reg("ec2/analyticsLookupObjectTracks", new QnMultiserverAnalyticsLookupObjectTracks(
        commonModule(), serverModule()->analyticsEventsStorage()));

    /**%apidoc GET /api/getAnalyticsActions
     * Get analytics actions from all analytics plugins on the current server which are applicable
     *     to the specified metadata object type.
     * %param objectTypeId Id of an object type to which an action should be applicable.
     * %return:object JSON object with an error code, error string, and the reply on success.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *     %param:array reply List of objects corresponding to Analytics Actions.
     *         %param reply[].actions List of JSON objects, each describing a set of actions from a
     *             particular analytics plugin.
     *         %param:stringArray reply[].actions[].actionIds List of action ids.
     *         %param:string reply[].actions[].pluginId Id of an analytics plugin which offers the
     *             actions.
     */
    reg("api/getAnalyticsActions", new QnGetAnalyticsActionsRestHandler());

    /**%apidoc POST /api/executeAnalyticsAction
     * Execute analytics Action from the particular analytics Plugin on this Server. The action is
     * applied to the specified Object Track.
     * %param engineId Id of an Analytics Engine which offers the Action.
     * %param actionId Id of an Action to execute.
     * %param objectTrackId Id of an Object Track to which the Action is applied.
     * %param deviceId Id of a Device from which the Action was triggered.
     * %param timestampUs Timestamp (microseconds) of the video frame from which the Action was
     *     triggered.
     * %param params JSON object with key-value pairs containing values for the Action params
     *     described in the Engine manifest.
     * %return:object JSON object with an error code, error string, and the reply on success.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *     %param:object reply Result returned by the executed Action.
     *         %param reply.actionUrl If not empty, provides a URL composed by the Plugin, to be
     *             opened by the Client in an embedded browser.
     *         %param reply.messageToUser If not empty, provides a message composed by the Plugin,
     *             to be shown to the user who triggered the Action.
     */
    reg("api/executeAnalyticsAction", new QnExecuteAnalyticsActionRestHandler(serverModule()));

    /**%apidoc[proprietary] POST /api/executeEventAction
     * Execute event action.
     * %param:enum actionType Type of the action.
     *     %value UndefinedAction
     *     %value CameraOutputAction Change camera output state.
     *     %value BookmarkAction
     *     %value CameraRecordingAction Start camera recording.
     *     %value PanicRecordingAction Activate panic recording mode.
     *     %value SendMailAction Send an email.
     *     %value DiagnosticsAction Write a record to the server's log.
     *     %value ShowPopupAction
     *     %value PlaySoundAction
     *     %value PlaySoundOnceAction
     *     %value SayTextAction
     *     %value ExecutePtzPresetAction Execute given PTZ preset.
     *     %value ShowTextOverlayAction Show text overlay over the given camera(s).
     *     %value ShowOnAlarmLayoutAction Put the given camera(s) to the Alarm Layout.
     *     %value ExecHttpRequestAction Send HTTP request as an action.
     *     %value BuzzerAction Enable an NVR buzzer
     * %param[opt]:enum EventState
     *     %value inactive Event has been finished (for prolonged events).
     *     %value active Event has been started (for prolonged events).
     *     %value undefined Event state is undefined (for instant events).
     * %param[proprietary]:boolean receivedFromRemoteHost
     * %param:stringArray resourceIds List of ids for resources associated with the event.
     *     Each id can be either camera id, camera MAC address or camera External id.
     * %param[opt]:objectJson params Json object with action parameters.
     * %param[opt]:objectJson runtimeParams Json object with event parameters.
     * %param[opt]:integer ruleId Event rule id.
     * %param[opt]:integer aggregationCount How many events were aggregated together for this action.
     */
    reg("api/executeEventAction",
        new nx::vms::server::rest::QnExecuteEventActionRestHandler(serverModule()));

    /**%apidoc GET /api/pluginInfo
     * Deprecated in favor of /ec2/pluginInfo.
     * %return:object JSON object with an error code, error string, and a list with information
     *     about Server plugins on success.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *     %param:array reply List of JSON objects with the following structure:
     *         %struct PluginInfoEx
     */
    reg("api/pluginInfo",
        new nx::vms::server::rest::PluginInfoHandler(serverModule()));

    /**%apidoc GET /ec2/pluginInfo
     * %param:boolean isLocal If true, data is collected only from the Server that received the
     *     request. Otherwise data is collected from all online Servers in the System.
     * %return:object JSON object with an error code, error string, and a list with information
     *     about Server plugins on success.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *     %param:object reply Map containing per Server information about Plugins.
     *         %param:string key Unique id of a Server
     *         %param:object value
     *             %struct PluginInfoEx
     */
    reg(nx::vms::server::rest::MultiserverPluginInfoHandler::kPath,
        new nx::vms::server::rest::MultiserverPluginInfoHandler(serverModule()));

    // TODO: #dmishin register this handler conditionally?
    /**%apidoc GET /api/nvrNetworkBlock
     * %return:object JSON object with an error code, error string, and an object with information
     *     about NVR network block, including each port state and the total power consumption
     *     limit.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *     %param:object reply JSON object with the following structure:
     *         %struct NetworkBlockData
     *
     * %apidoc POST /api/nvrNetworkBlock
     * %param:array portPoweringModes List of port powering modes with the following structure:
     *     %struct NetworkPortWithPoweringMode
     * %return:object JSON object with an error code, error string, and an object with information
     *     about NVR network block after powering mode update, including each port state and the
     *     total power consumption limit.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *     %param:array reply List of port powering modes after update with the following
     *         structure:
     *         %struct NetworkPortWithPoweringMode
     */
    reg("api/nvrNetworkBlock",
        new nx::vms::server::rest::NvrNetworkBlockHandler(serverModule()), kAdmin);

    /**%apidoc[proprietary] POST /api/saveCloudSystemCredentials
     * Sets or resets cloud credentials (systemId and authorization key) to be used by system
     * %param[opt]:string cloudSystemId
     * %param[opt]:string cloudAuthenticationKey
     * %param[opt]:string reset
     *     %value true If specified, removes cloud credentials from DB. System will not connect to
     *         cloud anymore
     */
    reg("api/saveCloudSystemCredentials", new QnSaveCloudSystemCredentialsHandler(serverModule(), cloudManagerGroup));

    reg("favicon.ico", new QnFavIconRestHandler());

    /**%apidoc[proprietary] GET /api/debug
     * Intended for debugging and experimenting.
     * <br/>ATTENTION: Works only if enabled by nx_vms_server.ini.
     * %param[opt]:option crash Intentionally crashes the Server.
     * %param[opt]:option fullDump If specified together with "crash", creates full crash dump.
     * %param[opt]:option exit Terminates the Server normally, via "exit(64)".
     * %param[opt]:option throwException Terminates the Server via throwing an exception.
     * %param[opt]:option abort Terminates the Server via "abort()".
     * %param[opt] delayS Sleep for the specified number of seconds, and then reply.
     * %permissions Owner.
     */
    reg("api/debug", new QnDebugHandler(), kAdmin);

    /**%apidoc[proprietary] GET /api/iniConfig
     * Intended for debugging and experimenting. Retrieves the current state of ini-config
     * mechanism, including the directory used for .ini files. Note that this directory is also
     * used for other mechanisms like Output Redirector and log configuration snippets.
     */
    reg("api/iniConfig", new QnIniConfigHandler());

    /**%apidoc[proprietary] TODO /api/startLiteClient
     * %// TODO: Write apidoc comment.
     */
    reg("api/startLiteClient", new QnStartLiteClientRestHandler(serverModule()));

    #if defined(_DEBUG)

        /**%apidoc[proprietary] TODO /api/debugEvent
         * %// TODO: Write apidoc comment.
         */
        reg("api/debugEvent", new QnDebugEventsRestHandler(serverModule()));
    #endif

    /**%apidoc[proprietary] TODO /ec2/runtimeInfo
     * %// TODO: Write apidoc comment.
     */
    reg("ec2/runtimeInfo", new QnRuntimeInfoRestHandler());

    static const char kGetHardwareIdsPath[] = "api/getHardwareIds";
    /**%apidoc GET /api/getHardwareIds
     * Get the list of Hardware Ids of the server.
     * %return:object JSON object with an error code, error string, and the reply on success.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *     %param:stringArray reply List of ids.
     */
    reg(kGetHardwareIdsPath, new QnGetHardwareIdsRestHandler());

    /**%apidoc GET /ec2/getHardwareIdsOfServers
     * Return the list of Hardware Ids for each Server in the System which is online at the moment
     * of executing this function.
     * %return:object JSON object with an error code, error string, and the reply on success.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *     %param:array reply List of objects representing Servers in the System.
     *         %param:uuid reply[].serverId Id of a server.
     *         %param:stringArray reply[].hardwareIds All Hardware Ids of the server, as a list of
     *             strings.
     */
    reg("ec2/getHardwareIdsOfServers", new QnMultiserverGetHardwareIdsRestHandler(QLatin1String("/") + kGetHardwareIdsPath));

    /**%apidoc GET /api/settingsDocumentation
     * Return settings documentation.
     * %return:array List of setting descriptions.
     *     %param:string name Setting name.
     *     %param:string defaultValue Setting default value.
     *     %param:string description Setting description.
     */
    reg("api/settingsDocumentation", new QnSettingsDocumentationHandler(&serverModule()->settings()));

    /**%apidoc GET /ec2/analyticsEngineSettings
     * Return values of settings of the specified Engine.
     * %param:string analyticsEngineId Id of an Analytics Engine.
     * %return:object JSON object with an error code, error string, and the reply on success.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *     %param:object reply Object with Engine settings model and values.
     *         %param:object reply.model Settings model, as in Engine manifest.
     *         %param:object reply.values Name-value map with setting values, using JSON types
     *             corresponding to each setting type.
     *
     **%apidoc POST /ec2/analyticsEngineSettings
     * Applies passed settings values to correspondent Analytics Engine.
     * %param:string engineId Id of an Analytics Engine.
     * %param:object settings Name-value map with setting values, using JSON types corresponding to
     *     each setting type.
     * %return:object JSON object with an error code, error string, and the reply on success.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *     %param:object reply Object with Engine settings model and values that the Engine returns
     *         after the values have been supplied.
     *         %param:object reply.model Settings model, as in Engine manifest.
     *         %param:object reply.values Name-value map with setting values, using JSON types
     *             corresponding to each setting type.
     */
    reg("ec2/analyticsEngineSettings",
        new nx::vms::server::rest::AnalyticsEngineSettingsHandler(serverModule()));

    /**%apidoc GET /ec2/deviceAnalyticsSettings
     * Return settings values of the specified DeviceAgent (which is a device-engine pair).
     * %param:string analyticsEngineId Id of an Analytics Engine.
     * %param:string deviceId Id of a device.
     * %return:object JSON object with an error code, error string, and the reply on success.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *     %param:object reply Object with DeviceAgent settings model and values.
     *         %struct DeviceAnalyticsSettingsResponse
     *
     **%apidoc POST /ec2/deviceAnalyticsSettings
     * Applies passed settings values to the corresponding DeviceAgent (which is a device-engine
     * pair).
     * %struct DeviceAnalyticsSettingsRequest
     *
     * %return:object JSON object with an error code, error string, and the reply on success.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *     %param:object reply Object with DeviceAgent settings model and values that the
     *         DeviceAgent returns after the values have been applied.
     *         %struct DeviceAnalyticsSettingsResponse
     */
    reg("ec2/deviceAnalyticsSettings",
        new nx::vms::server::rest::DeviceAnalyticsSettingsHandler(serverModule()));

    reg(nx::network::http::Method::options,
        QnRestProcessorPool::kAnyPath,
        new OptionsRequestHandler());

    if (const auto controller = m_metricsController.get())
    {
        /**%apidoc[proprietary] GET /api/metrics/
         * Returns the same values as GET /ec2/metrics/ but without multiserver aggregation.
         * See metrics.md for details.
         */
        reg("api/metrics/", new nx::vms::server::metrics::LocalRestHandler(controller), kAdmin);

        /**%apidoc GET /ec2/metrics/manifest
         * Returns the manifest for GET /ec2/metrics/alarms and GET /ec2/metrics/values visualization.
         * %return:object JSON object with an error code, error string, and the reply on success.
         *      %param:string error Error code, "0" means no error.
         *      %param:string errorString Error message in English, or an empty string.
         *      %param:array reply
         *          %param:string reply[].id Resource group id.
         *          %param:string reply[].name Resource group name.
         *          %param:string reply[].resource Resource label.
         *          %param:array reply[].values Resource group manifest.
         *              %param:string reply[].values[].id Parameter group id.
         *              %param:string reply[].values[].name Parameter group name.
         *              %param:array reply[].values[].values Parameter group manifest.
         *                  %param:string reply[].values[].values[].id Parameter id.
         *                  %param:string reply[].values[].values[].name Parameter name.
         *                  %param:string reply[].values[].values[].description Parameter description.
         *                  %param:string reply[].values[].values[].format Parameter format or units.
         *                  %param:string reply[].values[].values[].display Display type.
         *
         * %apidoc GET /ec2/metrics/values
         * Returns the current state of the values.
         * %param:string formatted Apply format like in /ec2/metrics/alarms.
         * %return:object JSON object with an error code, error string, and the reply on success.
         *      %param:string error Error code, "0" means no error.
         *      %param:string errorString Error message in English, or an empty string.
         *      %param:object reply
         *          %param:object reply.{resourceGroupId}
         *              Resource group values. Possible {resourceGroupId} values match reply[].id
         *              from manifest.
         *              %param:object reply.{resourceGroupId}.{resourceId}
         *                  Resource group values. Possible {resourceId} values indicate VMS resources.
         *                  %param:object reply.{resourceGroupId}.{resourceId}.{groupId}
         *                      Parameter group values. Possible {groupId} values match
         *                      reply[].values[].id from manifest.
         *                      %param reply.{resourceGroupId}.{resourceId}.{groupId}.{parameterId}
         *                          Parameter value. Possible {parameterId} values match
         *                          reply[].values[].values[].id from manifest.
         *
         * %apidoc GET /ec2/metrics/alarms
         * Returns the currently active alarms.
         * %return:object JSON object with an error code, error string, and the reply on success.
         *      %param:string error Error code, "0" means no error.
         *      %param:string errorString Error message in English, or an empty string.
         *      %param:object reply
         *          %param:object reply.{resourceGroupId}
         *              Resource group alarms. Possible {resourceGroupId} values match reply[].id
         *              from manifest.
         *              %param:object reply.{resourceGroupId}.{resourceId}
         *                  Resource group alarms. Possible {resourceId} values indicate VMS resources.
         *                  %param:object reply.{resourceGroupId}.{resourceId}.{groupId}
         *                      Parameter group alarms. Possible {groupId} values match
         *                      reply[].values[].id from manifest.
         *                      %param reply.{resourceGroupId}.{resourceId}.{groupId}.{parameterId}
         *                          Parameter alarm. Possible {parameterId} values match
         *                          reply[].values[].values[].id from manifest.
         *                      %param:string reply.{resourceGroupId}.{resourceId}.{groupId}.{parameterId}.level
         *                          Alarm level.
         *                      %param:string reply.{resourceGroupId}.{resourceId}.{groupId}.{parameterId}.text
         *                          Alarm text.
         *
         * %apidoc[proprietary] GET /ec2/metrics/rules
         * The rules to calculate the final manifest and raise alarms. See metrics.md for details.
         */
        reg("ec2/metrics/", new nx::vms::server::metrics::SystemRestHandler(
            controller, serverModule(), m_ec2ConnectionFactory->serverConnector()), kAdmin);
    }
}

void MediaServerProcess::registerRestHandler(
    const QString& path,
    QnRestRequestHandler* handler,
    GlobalPermission permission)
{
    registerRestHandler(QnRestProcessorPool::kAnyHttpMethod, path, handler, permission);
}

void MediaServerProcess::registerRestHandler(
    const nx::network::http::Method::ValueType& method,
    const QString& path,
    QnRestRequestHandler* handler,
    GlobalPermission permission)
{
    m_universalTcpListener->processorPool()->registerHandler(
        method, path, handler, permission);

    const auto& cameraIdUrlParams = handler->cameraIdUrlParams();
    if (!cameraIdUrlParams.isEmpty())
        m_autoRequestForwarder->addCameraIdUrlParams(path, cameraIdUrlParams);

    if (DeviceIdRetriever deviceIdRetriever = handler->createCustomDeviceIdRetriever())
    {
        QString realPath = path;
        if (!realPath.startsWith('/'))
            realPath.prepend('/');

        m_autoRequestForwarder->addCustomDeviceIdRetriever(realPath, std::move(deviceIdRetriever));
    }
}

template<class TcpConnectionProcessor, typename... ExtraParams>
void MediaServerProcess::registerTcpHandler(
    const QByteArray& protocol, const QString& path, ExtraParams... extraParams)
{
    m_universalTcpListener->addHandler<TcpConnectionProcessor>(
        protocol, path, extraParams...);

    if (TcpConnectionProcessor::doesPathEndWithCameraId())
        m_autoRequestForwarder->addAllowedProtocolAndPathPart(protocol, path);
}

bool MediaServerProcess::initTcpListener(
    TimeBasedNonceProvider* timeBasedNonceProvider,
    nx::vms::cloud_integration::CloudManagerGroup* const cloudManagerGroup,
    ec2::LocalConnectionFactory* ec2ConnectionFactory)
{
    const auto messageBus = ec2ConnectionFactory->messageBus();
    m_universalTcpListener->setupAuthorizer(timeBasedNonceProvider, *cloudManagerGroup);
    m_universalTcpListener->setCloudConnectionManager(cloudManagerGroup->connectionManager);

    m_autoRequestForwarder = std::make_unique<QnAutoRequestForwarder>(commonModule());
    m_autoRequestForwarder->addPathToIgnore("/ec2/*");

    configureApiRestrictions(m_universalTcpListener->authenticator()->restrictionList());
    connect(
        m_universalTcpListener->authenticator(), &nx::vms::server::Authenticator::emptyDigestDetected,
        this, &MediaServerProcess::at_emptyDigestDetected);

    m_universalTcpListener->httpModManager()->addCustomRequestMod(std::bind(
        &QnAutoRequestForwarder::processRequest,
        m_autoRequestForwarder.get(),
        std::placeholders::_1));

    #if defined(ENABLE_ACTI)
        QnActiResource::setEventPort(serverModule()->settings().port());
        // Used to receive event from an acti camera.
        // TODO: Remove this from api.
        m_universalTcpListener->processorPool()->registerHandler(
            "api/camera_event", new QnActiEventRestHandler());
    #endif

    registerRestHandlers(cloudManagerGroup, m_universalTcpListener.get(), messageBus);

    if (!m_universalTcpListener->bindToLocalAddress())
    {
        NX_ERROR(this) << "Failed to bind to local port; terminating";
        return false;
    }

    m_universalTcpListener->setDefaultPage("/static/index.html");

    // Server returns code 403 (forbidden) instead of 401 if the user isn't authorized for requests
    // starting with "web" path.
    m_universalTcpListener->setPathIgnorePrefix("web/");
    m_universalTcpListener->authenticator()->restrictionList()->deny(
        "/web/.+", nx::network::http::AuthMethod::http);

    nx::network::http::AuthMethod::Values methods = (nx::network::http::AuthMethod::Values) (
        nx::network::http::AuthMethod::cookie |
        nx::network::http::AuthMethod::urlQueryDigest |
        nx::network::http::AuthMethod::temporaryUrlQueryKey);
    QnUniversalRequestProcessor::setUnauthorizedPageBody(
        QnFileConnectionProcessor::readStaticFile("static/login.html"), methods);
    registerTcpHandler<QnRtspConnectionProcessor>("RTSP", "*", serverModule());
    registerTcpHandler<http_audio::AudioRequestProcessor>("HTTP", "api/http_audio", serverModule());
    registerTcpHandler<QnRestConnectionProcessor>("HTTP", "api");
    registerTcpHandler<QnRestConnectionProcessor>("HTTP", "ec2");
    registerTcpHandler<QnRestConnectionProcessor>("HTTP", "favicon.ico");
    registerTcpHandler<QnFileConnectionProcessor>("HTTP", "static");
    registerTcpHandler<QnCrossdomainConnectionProcessor>("HTTP", "crossdomain.xml");
    registerTcpHandler<ProgressiveDownloadingServer>("HTTP", "media", serverModule());
    registerTcpHandler<QnIOMonitorConnectionProcessor>("HTTP", "api/iomonitor");

    hls::HttpLiveStreamingProcessor::setMinPlayListSizeToStartStreaming(
        serverModule()->settings().hlsPlaylistPreFillChunks());
    registerTcpHandler<hls::HttpLiveStreamingProcessor>("HTTP", "hls", serverModule());

    // Our HLS uses implementation uses authKey (generated by target server) to skip authorization,
    // to keep this warning we should not ask for authorization along the way.
    m_universalTcpListener->enableUnauthorizedForwarding("hls");

    //regTcp<QnDefaultTcpConnectionProcessor>("HTTP", "*");

    registerTcpHandler<nx::vms::network::ProxyConnectionProcessor>(
        "*", "proxy", ec2ConnectionFactory->serverConnector(),
        serverModule()->settings().allowThirdPartyProxy());

    registerTcpHandler<QnAudioProxyReceiver>("HTTP", "proxy-2wayaudio", serverModule());

    if( !serverModule()->settings().authenticationEnabled())
        m_universalTcpListener->disableAuth();

    #if defined(ENABLE_DESKTOP_CAMERA)
        registerTcpHandler<QnDesktopCameraRegistrator>("HTTP", "desktop_camera", serverModule());
    #endif

    return true;
}

void MediaServerProcess::initializeCloudConnect()
{
    nx::network::SocketGlobals::cloud().outgoingTunnelPool()
        .assignOwnPeerId("ms", commonModule()->moduleGUID());

    nx::network::SocketGlobals::cloud().addressPublisher().setRetryInterval(
        nx::utils::parseTimerDuration(
            serverModule()->settings().mediatorAddressUpdate(),
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

static void setFdLimit(RootFileSystem* rootFs, int value)
{
    #if defined (Q_OS_LINUX)
        int newValue = rootFs->setFdLimit(getpid(), value);
        if (newValue == -1)
        {
            NX_WARNING(
                typeid(MediaServerProcess),
                "Failed to set file descriptor limit and get previous value");
            return;
        }

        if (newValue != value)
        {
            NX_WARNING(
                typeid(MediaServerProcess),
                "Failed to set file descriptor limit. Current value: %1", newValue);
            return;
        }

        NX_INFO(
            typeid(MediaServerProcess),
            "Successfully set file descriptor limit: %1", newValue);
    #else
        (void) rootFs;
        (void) value;
    #endif // Q_OS_LINUX
}

void MediaServerProcess::prepareOsResources()
{
    auto rootToolPtr = serverModule()->rootFileSystem();
    if (!rootToolPtr->changeOwner(nx::kit::IniConfig::iniFilesDir()))
        qWarning().noquote() << "Unable to chown" << nx::kit::IniConfig::iniFilesDir();

    setFdLimit(rootToolPtr, 32000);

    // Change owner of all data files, so mediaserver can use them as different user.
    const std::vector<QString> chmodPaths =
    {
        MSSettings::defaultConfigDirectory(),
        serverModule()->roSettings()->fileName(),
        serverModule()->runTimeSettings()->fileName(),
        QnFileConnectionProcessor::externalPackagePath()
    };

    for (const auto& path: chmodPaths)
    {
        if (!rootToolPtr->changeOwner(path)) //< Let the errors reach stdout and stderr.
            qWarning().noquote() << "WARNING: Unable to chown" << path;
    }

    // We don't want to chown recursively directory with archive since it may take a while.
    if (!rootToolPtr->changeOwner(serverModule()->settings().dataDir(), /*isRecursive*/ false))
    {
        qWarning().noquote() << "WARNING: Unable to chown" << serverModule()->settings().dataDir();
        return;
    }

    for (const auto& entry: QDir(serverModule()->settings().dataDir()).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot))
    {
        if (entry.isDir() && entry.absoluteFilePath().endsWith("data"))
            continue;

        if (!rootToolPtr->changeOwner(entry.absoluteFilePath()))
            qWarning().noquote() << "WARNING: Unable to chown" << entry.absoluteFilePath();
    }
}

void MediaServerProcess::initializeUpnpPortMapper()
{
    m_upnpPortMapper = std::make_unique<nx::network::upnp::PortMapper>(
        serverModule()->upnpDeviceSearcher(),
        /*isEnabled*/ false,
        nx::network::upnp::PortMapper::kDefaultCheckMappingsInterval,
        QnAppInfo::organizationName());
    auto updateEnabled =
        [this]()
        {
            const auto& settings = commonModule()->globalSettings();
            const auto isCloudSystem = !settings->cloudSystemId().isEmpty();
            m_upnpPortMapper->setIsEnabled(isCloudSystem && settings->isUpnpPortMappingEnabled());
        };

    const auto& settings = commonModule()->globalSettings();
    connect(settings, &QnGlobalSettings::upnpPortMappingEnabledChanged, updateEnabled);
    connect(settings, &QnGlobalSettings::cloudSettingsChanged, updateEnabled);
    updateEnabled();

    m_upnpPortMapper->enableMapping(
        m_mediaServer->getPort(), nx::network::upnp::PortMapper::Protocol::TCP,
        [this](nx::network::SocketAddress address)
        {
            const auto result = QMetaObject::invokeMethod(
                this, "at_portMappingChanged", Qt::AutoConnection,
                Q_ARG(QString, address.toString()));

            NX_ASSERT(result, "Could not call at_portMappingChanged(...)");
        });

}

nx::vms::api::ServerFlags MediaServerProcess::calcServerFlags()
{
    nx::vms::api::ServerFlags serverFlags = nx::vms::api::SF_None; // TODO: #Elric #EC2 type safety has just walked out of the window.

    if (nx::utils::AppInfo::isEdgeServer())
        serverFlags |= nx::vms::api::SF_Edge;

    if (QnAppInfo::isNx1())
    {
        serverFlags |= nx::vms::api::SF_IfListCtrl | nx::vms::api::SF_timeCtrl;
        QnStartLiteClientRestHandler handler(serverModule());
        if (handler.isLiteClientPresent())
            serverFlags |= nx::vms::api::SF_HasLiteClient;
    }

    if (ini().forceLiteClient)
    {
        QnStartLiteClientRestHandler handler(serverModule());
        if (handler.isLiteClientPresent())
            serverFlags |= nx::vms::api::SF_HasLiteClient;
    }

#ifdef __arm__
    serverFlags |= nx::vms::api::SF_ArmServer;

    struct stat st;
    memset(&st, 0, sizeof(st));
    const bool hddPresent =
        ::stat("/dev/sda", &st) == 0 ||
        ::stat("/dev/sdb", &st) == 0 ||
        ::stat("/dev/sdc", &st) == 0 ||
        ::stat("/dev/sdd", &st) == 0;
    if (hddPresent)
        serverFlags |= nx::vms::api::SF_Has_HDD;
#else
    serverFlags |= nx::vms::api::SF_Has_HDD;
#endif

    if (!(serverFlags & (nx::vms::api::SF_ArmServer | nx::vms::api::SF_Edge)))
        serverFlags |= nx::vms::api::SF_SupportsTranscoding;

    const QString appserverHostString = serverModule()->settings().appserverHost();
    bool isLocal = Utils::isLocalAppServer(appserverHostString);
    if (!isLocal)
        serverFlags |= nx::vms::api::SF_RemoteEC;

    initPublicIpDiscovery();
    if (!m_ipDiscovery->publicIP().isNull())
        serverFlags |= nx::vms::api::SF_HasPublicIP;

    if (const nvr::IService* const nvrService = serverModule()->nvrService())
    {
        const nvr::IService::Capabilities capabilities = nvrService->capabilities();
        if (capabilities.testFlag(nvr::IService::Capability::buzzer))
            serverFlags |= nx::vms::api::SF_HasBuzzer;
        if (capabilities.testFlag(nvr::IService::Capability::poeManagement))
            serverFlags |= nx::vms::api::SF_HasPoeManagementCapability;
        if (capabilities.testFlag(nvr::IService::Capability::fanMonitoring))
            serverFlags |= nx::vms::api::SF_HasFanMonitoringCapability;
    }

    return serverFlags;
}

void MediaServerProcess::initPublicIpDiscovery()
{
    m_ipDiscovery = std::make_unique<nx::network::PublicIPDiscovery>(
        serverModule()->settings().publicIPServers().split(";", QString::SkipEmptyParts));

    int publicIPEnabled = serverModule()->settings().publicIPEnabled();
    if (publicIPEnabled == 0) //< Public IP disabled.
        return;

    if (publicIPEnabled > 1) //< Public IP manually set.
    {
        auto staticIp = serverModule()->settings().staticPublicIP();
        at_updatePublicAddress(QHostAddress(staticIp));
        return;
    }

    // Discover public IP.
    m_ipDiscovery->update();
    m_ipDiscovery->waitForFinished(); //< NOTE: Slows down server startup, should be avoided here.
    at_updatePublicAddress(m_ipDiscovery->publicIP());
}

void MediaServerProcess::startPublicIpDiscovery()
{
    // Should start periodic discovery only when public IP auto-discovery is enabled.
    if (serverModule()->settings().publicIPEnabled() != 1)
        return;

    m_updatePiblicIpTimer = std::make_unique<QTimer>();
    connect(m_updatePiblicIpTimer.get(), &QTimer::timeout,
        m_ipDiscovery.get(), &nx::network::PublicIPDiscovery::update);

    connect(m_ipDiscovery.get(), &nx::network::PublicIPDiscovery::found,
        this, &MediaServerProcess::at_updatePublicAddress);

    m_updatePiblicIpTimer->start(kPublicIpUpdateTimeoutMs);
}

void MediaServerProcess::resetSystemState(
    nx::vms::cloud_integration::CloudConnectionManager& cloudConnectionManager)
{
    while (!needToStop())
    {
        if (!cloudConnectionManager.detachSystemFromCloud())
        {
            qWarning() << "Error while clearing cloud information. Trying again...";
            QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
            continue;
        }

        if (!nx::vms::utils::resetSystemToStateNew(commonModule()))
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

    QString fileName = serverModule()->settings().dataDir() + "/scripts/" + kOnExitScriptName;
    if (!QFile::exists(fileName))
    {
        NX_VERBOSE(this, "Script '%1' is missing at the server", fileName);
        return;
    }

    // Currently, no args are needed, hence the empty list.
    QStringList args{};

    NX_VERBOSE(this, "Calling the script: %1 %2", fileName, args.join(" "));
    if (!QProcess::startDetached(fileName, args))
    {
        NX_VERBOSE(this,
            "Unable to start script \"%1\" because of a system error", kOnExitScriptName);
    }
}

void MediaServerProcess::moveHandlingCameras()
{
    QSet<QnUuid> servers;
    const auto& resPool = commonModule()->resourcePool();
    for (const auto& server: resPool->getResources<QnMediaServerResource>())
        servers << server->getId();
    nx::vms::api::CameraDataList camerasToUpdate;
    for (const auto& camera: resPool->getAllCameras(/*all*/ QnResourcePtr()))
    {
        if (!servers.contains(camera->getParentId()))
        {
            nx::vms::api::CameraData apiCameraData;
            ec2::fromResourceToApi(camera, apiCameraData);
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
    QString ifList = serverModule()->settings().if_();
    // check startup parameter
    if (ifList.isEmpty())
        ifList = m_cmdLineArguments.ifListFilter;

    QList<QHostAddress> allowedInterfaces;
    for (const QString& s : ifList.split(QLatin1Char(';'), QString::SkipEmptyParts))
        allowedInterfaces << QHostAddress(s);

    if (!allowedInterfaces.isEmpty())
        qWarning() << "Using net IF filter:" << allowedInterfaces;
    nx::network::setInterfaceListFilter(allowedInterfaces);
}

QString MediaServerProcess::hardwareIdAsGuid() const
{
    auto hwId = LLUtil::getLatestHardwareId();
    auto hwIdString = QnUuid::fromHardwareId(hwId).toString();
    qWarning() << "Got hwID" << hwIdString;
    return hwIdString;
}

void MediaServerProcess::updateGuidIfNeeded()
{
    QString guidIsHWID = serverModule()->settings().guidIsHWID();
    QString serverGuid = serverModule()->settings().serverGuid();
    QString serverGuid2 = serverModule()->settings().serverGuid2();
    QString pendingSwitchToClusterMode = serverModule()->settings().pendingSwitchToClusterMode();

    QString hwidGuid = hardwareIdAsGuid();

    if (guidIsHWID == YES) {
        serverModule()->mutableSettings()->serverGuid.set(hwidGuid);
        serverModule()->mutableSettings()->serverGuid2.remove();
    }
    else if (guidIsHWID == NO) {
        if (serverGuid.isEmpty()) {
            // serverGuid remove from settings manually?
            serverModule()->mutableSettings()->serverGuid.set(hwidGuid);
            serverModule()->mutableSettings()->guidIsHWID.set(YES);
        }

        serverModule()->mutableSettings()->serverGuid2.remove();
    }
    else if (guidIsHWID.isEmpty()) {
        if (!serverGuid2.isEmpty()) {
            serverModule()->mutableSettings()->serverGuid.set(serverGuid2);
            serverModule()->mutableSettings()->guidIsHWID.set(NO);
            serverModule()->mutableSettings()->serverGuid2.remove();
        }
        else {
            // Don't reset serverGuid if we're in pending switch to cluster mode state.
            // As it's stored in the remote database.
            if (pendingSwitchToClusterMode == YES)
                return;

            serverModule()->mutableSettings()->serverGuid.set(hwidGuid);
            serverModule()->mutableSettings()->guidIsHWID.set(YES);

            if (!serverGuid.isEmpty()) {
                serverModule()->mutableSettings()->obsoleteServerGuid.set(serverGuid);
            }
        }
    }

    connect(commonModule()->globalSettings(), &QnGlobalSettings::localSystemIdChanged,
        [this, serverGuid, hwidGuid]()
        {
            // Stop moving HwId to serverGuid as soon as first setup wizard is done.
            if (!commonModule()->globalSettings()->localSystemId().isNull())
                serverModule()->mutableSettings()->guidIsHWID.set(NO);
        });

    QnUuid obsoleteGuid = QnUuid(serverModule()->settings().obsoleteServerGuid());
    if (!obsoleteGuid.isNull())
        commonModule()->setObsoleteServerGuid(obsoleteGuid);
}

nx::utils::log::Settings MediaServerProcess::makeLogSettings(
    const nx::vms::server::Settings& settings)
{
    nx::utils::log::Settings s;
    s.loggers.resize(1);
    s.loggers.front().maxBackupCount = settings.logArchiveSize();
    s.loggers.front().directory = settings.logDir();
    s.loggers.front().maxFileSize = settings.maxLogFileSize();
    s.updateDirectoryIfEmpty(settings.dataDir() + "/log");

    for (const auto& loggerArg: cmdLineArguments().auxLoggers)
    {
        nx::utils::log::LoggerSettings loggerSettings;
        loggerSettings.parse(loggerArg);
        s.loggers.push_back(std::move(loggerSettings));
    }

    return s;
}

void MediaServerProcess::initializeLogging(MSSettings* serverSettings)
{
    using namespace nx::utils::log;

    auto& settings = serverSettings->settings();
    auto roSettings = serverSettings->roSettings();

    const auto binaryPath = QFile::decodeName(m_argv[0]);

    const QString logConfigFile(
        QDir(nx::kit::IniConfig::iniFilesDir()).absoluteFilePath("nx_vms_server_log.ini"));
    if (cmdLineArguments().logLevel.isEmpty() && QFile::exists(logConfigFile))
    {
        initializeFromConfigFile(
            logConfigFile,
            settings.logDir().isEmpty() ? settings.dataDir() + "/log" : settings.logDir(),
            qApp->applicationName(),
            binaryPath);
    }
    else
    {
        // TODO: Implement "--log-file" option like in client_startup_parameters.cpp.

        auto logSettings = makeLogSettings(settings);
        logSettings.loggers.front().level.parse(cmdLineArguments().logLevel,
            settings.logLevel(), toString(nx::utils::log::kDefaultLevel));
        logSettings.loggers.front().logBaseName = "log_file";
        setMainLogger(
            buildLogger(
                logSettings,
                qApp->applicationName(),
                binaryPath));
    }

    if (auto path = mainLogger()->filePath())
        roSettings->setValue("logFile", path->replace(".log", ""));
    else
        roSettings->remove("logFile");

    auto logSettings = makeLogSettings(settings);
    logSettings.loggers.front().level.parse(cmdLineArguments().httpLogLevel,
        settings.httpLogLevel(), toString(Level::none));
    logSettings.loggers.front().logBaseName = "http_log";
    addLogger(
        buildLogger(
            logSettings, qApp->applicationName(), binaryPath,
            {Filter(QnLog::HTTP_LOG_INDEX)}));

    logSettings = makeLogSettings(settings);
    logSettings.loggers.front().level.parse(cmdLineArguments().systemLogLevel,
        settings.systemLogLevel(), toString(nx::utils::log::Level::info));
    logSettings.loggers.front().logBaseName = "hw_log";
    addLogger(
        buildLogger(
            logSettings,
            qApp->applicationName(),
            binaryPath,
            {
                Filter(QnLog::HWID_LOG),
                Filter(Tag(typeid(nx::vms::server::LicenseWatcher)))
            }),
        /*writeLogHeader*/ false);

    logSettings = makeLogSettings(settings);
    logSettings.loggers.front().level.parse(cmdLineArguments().ec2TranLogLevel,
        settings.tranLogLevel(), toString(Level::none));
    logSettings.loggers.front().logBaseName = "ec2_tran";
    addLogger(
        buildLogger(
            logSettings,
            qApp->applicationName(),
            binaryPath,
            {Filter(QnLog::EC2_TRAN_LOG)}));

    logSettings = makeLogSettings(settings);
    logSettings.loggers.front().level.parse(cmdLineArguments().permissionsLogLevel,
        settings.permissionsLogLevel(), toString(Level::none));
    logSettings.loggers.front().logBaseName = "permissions";
    addLogger(
        buildLogger(
            logSettings,
            qApp->applicationName(),
            binaryPath,
            {Filter(QnLog::PERMISSIONS_LOG)}));

    nx::utils::enableQtMessageAsserts();
}

void MediaServerProcess::initializeHardwareId()
{
    const auto binaryPath = QFile::decodeName(m_argv[0]);

    auto logSettings = makeLogSettings(serverModule()->settings());

    using namespace nx::utils::log;
    logSettings.loggers.front().level.parse(cmdLineArguments().systemLogLevel,
        serverModule()->settings().systemLogLevel(), toString(Level::info));
    logSettings.loggers.front().logBaseName = "hw_log";
    addLogger(
        buildLogger(
            logSettings,
            qApp->applicationName(),
            binaryPath,
            {
                Filter(QnLog::HWID_LOG),
                Filter(Tag(typeid(nx::vms::server::LicenseWatcher)))
            }));

    LLUtil::initHardwareId(serverModule());
    updateGuidIfNeeded();
    m_hardwareIdHlist = LLUtil::getAllHardwareIds().toVector();

    const QnUuid guid(serverModule()->settings().serverGuid());
    if (guid.isNull())
    {
        const char* const kMessage = "Can't save guid. Run once as administrator.";
        std::cerr << kMessage;
        NX_ERROR(this, kMessage);
        qApp->quit();
        return;
    }
}

void MediaServerProcess::connectArchiveIntegrityWatcher()
{
    using namespace nx::vms::server;
    auto serverArchiveIntegrityWatcher = static_cast<ServerArchiveIntegrityWatcher*>(
        serverModule()->archiveIntegrityWatcher());

    if (serverArchiveIntegrityWatcher)
    {
        connect(
            serverArchiveIntegrityWatcher,
            &ServerArchiveIntegrityWatcher::fileIntegrityCheckFailed,
            serverModule()->eventConnector(),
            &event::EventConnector::at_fileIntegrityCheckFailed);
    }
}

class TcpLogReceiverConnection: public QnTCPConnectionProcessor
{
public:
    TcpLogReceiverConnection(
        const QString& dataDir,
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        QnTcpListener* owner)
        :
        QnTCPConnectionProcessor(std::move(socket), owner),
        m_socket(std::move(socket)),
        m_file(closeDirPath(dataDir) + "log/external_device.log")
    {
        m_file.open(QFile::WriteOnly);
        socket->setRecvTimeout(1000 * 3);
    }
    virtual ~TcpLogReceiverConnection() override { stop(); }
protected:
    virtual void run() override
    {
        while (true)
        {
            quint8 buffer[1024 * 16];
            int bytesRead = m_socket->recv(buffer, sizeof(buffer));
            if (bytesRead < 1 && SystemError::getLastOSErrorCode() != SystemError::timedOut)
                break; //< Connection closed
            m_file.write((const char*)buffer, bytesRead);
            m_file.flush();
        }
    }
private:
    std::unique_ptr<nx::network::AbstractStreamSocket> m_socket;
    QFile m_file;
};

class TcpLogReceiver : public QnTcpListener
{
public:
    TcpLogReceiver(
        const QString& dataDir,
        QnCommonModule* commonModule,
        const QHostAddress& address,
        int port)
        :
        QnTcpListener(commonModule, address, port),
        m_dataDir(dataDir)
    {
    }
    virtual ~TcpLogReceiver() override { stop(); }

protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(
        std::unique_ptr<nx::network::AbstractStreamSocket> clientSocket) override
    {
        return new TcpLogReceiverConnection(m_dataDir, std::move(clientSocket), this);
    }
private:
    QString m_dataDir;
};

void MediaServerProcess::initStaticCommonModule()
{
    m_staticCommonModule = std::make_unique<QnStaticCommonModule>(
        nx::vms::api::PeerType::server,
        nx::utils::AppInfo::brand(),
        nx::utils::AppInfo::customizationName());
}

void MediaServerProcess::setSetupModuleCallback(std::function<void(QnMediaServerModule*)> callback)
{
    m_setupModuleCallback = std::move(callback);
}

bool MediaServerProcess::setUpMediaServerResource(
    CloudIntegrationManager* /*cloudIntegrationManager*/,
    QnMediaServerModule* serverModule,
    const ec2::AbstractECConnectionPtr& ec2Connection)
{
    bool foundOwnServerInDb = false;
    const bool sslAllowed = serverModule->settings().allowSslConnections();

    while (m_mediaServer.isNull())
    {
        if (needToStop())
            return false;
        QnMediaServerResourcePtr server = findServer(ec2Connection);
        nx::vms::api::MediaServerData prevServerData;
        if (server)
        {
            ec2::fromResourceToApi(server, prevServerData);
            foundOwnServerInDb = true;
        }
        else
        {
            server = QnMediaServerResourcePtr(new QnMediaServerResource(commonModule()));
            const QnUuid serverGuid(serverModule->settings().serverGuid());
            server->setIdUnsafe(serverGuid);
            server->setMaxCameras(nx::utils::AppInfo::isEdgeServer() ? 1 : 128);

            QString serverName(getDefaultServerName());
            auto beforeRestoreDbData = commonModule()->beforeRestoreDbData();
            if (!beforeRestoreDbData.serverName.isEmpty())
                serverName = QString::fromLocal8Bit(beforeRestoreDbData.serverName);
            server->setName(serverName);
        }

        server->setServerFlags(calcServerFlags());

        QHostAddress appserverHost;
        const QString appserverHostString = serverModule->settings().appserverHost();
        bool isLocal = Utils::isLocalAppServer(appserverHostString);
        if (!isLocal) {
            do
            {
                appserverHost = resolveHost(appserverHostString);
            } while (appserverHost.toIPv4Address() == 0);
        }

        server->setPrimaryAddress(
            nx::network::SocketAddress(defaultLocalAddress(appserverHost), m_universalTcpListener->getPort()));
        server->setSslAllowed(sslAllowed);
        m_cloudIntegrationManager->cloudManagerGroup().connectionManager.setProxyVia(
            nx::network::SocketAddress(nx::network::HostAddress::localhost, m_universalTcpListener->getPort()));

        // used for statistics reported
        server->setOsInfo(nx::utils::OsInfo::current());
        server->setVersion(commonModule()->engineVersion());

        SettingsHelper settingsHelper(this->serverModule());
        QByteArray settingsAuthKey = settingsHelper.getAuthKey();
        QByteArray authKey = settingsAuthKey;
        if (authKey.isEmpty())
            authKey = server->getAuthKey().toLatin1();
        if (authKey.isEmpty())
            authKey = QnUuid::createUuid().toString().toLatin1();
        server->setAuthKey(authKey);

        // Keep server auth key in registry. Server MUST be able pass authorization after deleting database in database restore process
        if (settingsAuthKey != authKey)
            settingsHelper.setAuthKey(authKey);

        nx::vms::api::MediaServerData newServerData;
        ec2::fromResourceToApi(server, newServerData);
        if (prevServerData != newServerData)
        {
            m_mediaServer = registerServer(
                ec2Connection,
                server,
                nx::mserver_aux::isNewServerInstance(
                    commonModule()->beforeRestoreDbData(),
                    foundOwnServerInDb,
                    serverModule->settings().noSetupWizard() > 0));
        }
        else
        {
            m_mediaServer = server;
        }

        if (m_mediaServer.isNull())
            QnSleep::msleep(1000);
    }

    const auto& resPool = commonModule()->resourcePool();
    resPool->addResource(m_mediaServer);

    QString moduleName = qApp->applicationName();
    if (moduleName.startsWith(qApp->organizationName()))
        moduleName = moduleName.mid(qApp->organizationName().length()).trimmed();

    nx::vms::api::ModuleInformation selfInformation = commonModule()->moduleInformation();
    selfInformation.version = commonModule()->engineVersion();
    selfInformation.sslAllowed = serverModule->settings().allowSslConnections();
    selfInformation.serverFlags = m_mediaServer->getServerFlags();
    selfInformation.ecDbReadOnly = ec2Connection->connectionInfo().ecDbReadOnly;

    commonModule()->setModuleInformation(selfInformation);
    commonModule()->bindModuleInformation(m_mediaServer);

    commonModule()->setMediaStatisticsWindowSize(
        std::chrono::seconds(serverModule->settings().mediaStatisticsWindowSize()));
    commonModule()->setMediaStatisticsMaxDurationInFrames(
        serverModule->settings().mediaStatisticsMaxDurationInFrames());

    return foundOwnServerInDb;
}

void MediaServerProcess::stopObjects()
{
    auto safeDisconnect =
        [](QObject* src, QObject* dst)
        {
            if (src && dst)
                src->disconnect(dst);
        };

    NX_INFO(this, "Event loop has returned. Destroying objects...");

    quint64 dumpSystemResourceUsageTaskID = 0;
    {
        QnMutexLocker lk(&m_mutex);
        dumpSystemResourceUsageTaskID = m_dumpSystemResourceUsageTaskId;
        m_dumpSystemResourceUsageTaskId = 0;
    }

    if (dumpSystemResourceUsageTaskID)
    {
        NX_DEBUG(this, "Cancelling the dump system resource usage timer");
        commonModule()->timerManager()->joinAndDeleteTimer(dumpSystemResourceUsageTaskID);
    }

    commonModule()->setNeedToStop(true);

    NX_DEBUG(this, "Stopping the universal TCP listener");
    m_universalTcpListener->stop();

    NX_DEBUG(this, "Stopping the server module");
    serverModule()->stop();

    NX_DEBUG(this, "Resetting the check analytics timer");
    m_checkAnalyticsTimer.reset();

    NX_DEBUG(this, "Resetting the general task timer");
    m_generalTaskTimer.reset();

    NX_DEBUG(this, "Resetting the \"server started\" timer");
    m_serverStartedTimer.reset();

    NX_DEBUG(this, "Resetting the UDT traffic timer");
    m_udtInternetTrafficTimer.reset();

    NX_DEBUG(this, "Resetting the DB creation timer");
    m_createDbBackupTimer.reset();

    NX_DEBUG(this, "Disconnecting different media server objects");
    safeDisconnect(commonModule()->globalSettings(), this);
    safeDisconnect(m_universalTcpListener->authenticator(), this);
    safeDisconnect(commonModule()->resourceDiscoveryManager(), this);
    safeDisconnect(serverModule()->normalStorageManager(), this);
    safeDisconnect(serverModule()->backupStorageManager(), this);
    m_raidEventLogReader->unsubscribe();
    safeDisconnect(m_raidEventLogReader.get(), this);

    safeDisconnect(commonModule(), this);
    safeDisconnect(commonModule()->runtimeInfoManager(), this);
    if (m_ec2Connection)
        safeDisconnect(m_ec2Connection->timeNotificationManager().get(), this);
    safeDisconnect(m_ec2Connection.get(), this);
    safeDisconnect(m_updatePiblicIpTimer.get(), this);

    NX_DEBUG(this, "Resetting public IP update timer");
    m_updatePiblicIpTimer.reset();
    safeDisconnect(m_ipDiscovery.get(), this);
    safeDisconnect(commonModule()->moduleDiscoveryManager(), this);

    NX_DEBUG(this, "Waiting until event queue is empty");
    WaitingForQThreadToEmptyEventQueue waitingForObjectsToBeFreed(QThread::currentThread(), 3);
    waitingForObjectsToBeFreed.join();

    NX_DEBUG(this, "Resetting the discovery monitor");
    m_discoveryMonitor.reset();

    NX_DEBUG(this, "Resetting the crash reporter");
    m_crashReporter.reset();

    NX_DEBUG(this, "Resetting the public IP searcher");
    m_ipDiscovery.reset(); // stop it before IO deinitialized

    NX_DEBUG(this, "Resetting the multicast HTTP server");
    m_multicastHttp.reset();

    if (const auto manager = commonModule()->moduleDiscoveryManager())
    {
        NX_DEBUG(this, "Stopping the module discovery manager");
        manager->stop();
    }

    NX_DEBUG(this, "Stopping the resource command processor");
    serverModule()->resourceCommandProcessor()->stop();
    if (m_initStoragesAsyncPromise)
    {
        NX_DEBUG(this, "Waiting on the asynchronous storage initialization promise");
        m_initStoragesAsyncPromise->get_future().wait();
    }

    // todo: #rvasilenko some undeleted resources left in the QnMain event loop. I stopped TimerManager as temporary solution for it.
    NX_DEBUG(this, "Stopping the timer manager");
    commonModule()->timerManager()->stop();

    // Remove all stream recorders.
    NX_DEBUG(this, "Stopping the remote archive synchronizer");
    m_remoteArchiveSynchronizer.reset();

    NX_DEBUG(this, "Stopping the media server resource searcher");
    m_mserverResourceSearcher.reset();

    NX_DEBUG(this, "Stopping the resource discovery manager");
    commonModule()->resourceDiscoveryManager()->stop();

    NX_DEBUG(this, "Stopping the analytics manager");
    serverModule()->analyticsManager()->stop(); //< Stop processing analytics events.

    NX_DEBUG(this, "Stopping the plugin manager");
    serverModule()->pluginManager()->unloadPlugins();

    NX_DEBUG(this, "Stopping the event rule processor");
    serverModule()->eventRuleProcessor()->stop();

    NX_DEBUG(this, "Stopping the P2P downloader");
    serverModule()->p2pDownloader()->stopDownloads();
    if (nx::vms::server::nvr::IService* const nvrService = serverModule()->nvrService())
    {
        NX_DEBUG(this, "Stopping the NVR service");
        nvrService->stop();
    }

    //since mserverResourceDiscoveryManager instance is dead no events can be delivered to serverResourceProcessor: can delete it now
    //TODO refactoring of discoveryManager <-> resourceProcessor interaction is required
    NX_DEBUG(this, "Stopping the server resource processor");
    m_serverResourceProcessor.reset();

    NX_DEBUG(this, "Waiting for resource pool thread pool");
    serverModule()->resourcePool()->threadPool()->waitForDone();

    NX_DEBUG(this, "Shutting down the EC2 connection factory");
    m_ec2ConnectionFactory->shutdown();

    NX_DEBUG(this, "Deleting message processor");
    commonModule()->deleteMessageProcessor(); // stop receiving notifications

    if (m_universalTcpListener)
    {
        NX_DEBUG(this, "Resetting the universal TCP listener");
        m_universalTcpListener.reset();
    }

    NX_DEBUG(this, "Clearing resource pool");
    commonModule()->resourcePool()->clear();

    //disconnecting from EC2
    NX_DEBUG(this, "Disconnecting from EC2 connection");
    QnAppServerConnectionFactory::setEc2Connection(ec2::AbstractECConnectionPtr());

    NX_DEBUG(this, "Resetting the cloud integration manager");
    m_cloudIntegrationManager.reset();

    NX_DEBUG(this, "Resetting the media server status watcher");
    m_mediaServerStatusWatcher.reset();

    NX_DEBUG(this, "Resetting the time based nonce provider");
    m_timeBasedNonceProvider.reset();

    NX_DEBUG(this, "Resetting the EC2 connection");
    m_ec2Connection.reset();

    NX_DEBUG(this, "Resetting the EC2 connection factory");
    m_ec2ConnectionFactory.reset();

    // This method will set flag on message channel to threat next connection close as normal
    //appServerConnection->disconnectSync();
    serverModule()->setLastRunningTime(std::chrono::milliseconds::zero());

    if (m_mediaServer)
        m_mediaServer->beforeDestroy();

    NX_DEBUG(this, "Resetting the media server resource");
    m_mediaServer.clear();

    performActionsOnExit();

    nx::network::SocketGlobals::cloud().outgoingTunnelPool().clearOwnPeerIdIfEqual(
        "ms", commonModule()->moduleGUID());

    NX_DEBUG(this, "Resetting the auto request forwarder");
    m_autoRequestForwarder.reset();

    NX_DEBUG(this, "Resetting the audio streamer pool");
    m_audioStreamerPool.reset();

    NX_DEBUG(this, "Resetting the UPnP port mapper");
    m_upnpPortMapper.reset();

    NX_DEBUG(this, "Resetting the metrics controller");
    m_metricsController.reset();

    stopAsync();
}

ec2::AbstractECConnectionPtr MediaServerProcess::createEc2Connection() const
{
    while (!needToStop())
    {
        ec2::AbstractECConnectionPtr ec2Connection;
        const ec2::ErrorCode errorCode = m_ec2ConnectionFactory->connectSync(
            appServerConnectionUrl(), nx::vms::api::ClientInfoData(), &ec2Connection);
        if (ec2Connection)
        {
            const auto connectInfo = ec2Connection->connectionInfo();
            auto connectionResult = QnConnectionValidator::validateConnection(connectInfo, errorCode);
            if (connectionResult == Qn::SuccessConnectionResult)
            {
                NX_INFO(this, lm("Successfully connected to a local database"));
                return ec2Connection;
            }

            switch (connectionResult)
            {
                case Qn::IncompatibleInternalConnectionResult:
                case Qn::IncompatibleCloudHostConnectionResult:
                case Qn::IncompatibleVersionConnectionResult:
                case Qn::IncompatibleProtocolConnectionResult:
                    NX_ERROR(this, "Incompatible Server version detected! Giving up.");
                    return ec2::AbstractECConnectionPtr();
                default:
                    break;
            }
        }

        NX_ERROR(this, lm("Can't connect to local EC2. %1").arg(ec2::toString(errorCode)));
        QnSleep::msleep(3000);
    }
    return ec2::AbstractECConnectionPtr();
}

bool MediaServerProcess::connectToDatabase()
{
    m_ec2Connection = createEc2Connection();
    QnAppServerConnectionFactory::setEc2Connection(m_ec2Connection);
    if (m_ec2Connection)
    {
        connect(m_ec2Connection.get(), &ec2::AbstractECConnection::databaseDumped,
            this, &MediaServerProcess::at_databaseDumped);
        commonModule()->setRemoteGUID(m_ec2Connection->connectionInfo().serverId());
        serverModule()->syncRoSettings();
        commonModule()->setCloudMode(true);
    }
    return m_ec2Connection != nullptr;
}

void MediaServerProcess::migrateDataFromOldDir()
{
#ifdef Q_OS_WIN32
    nx::misc::ServerDataMigrateHandler migrateHandler(serverModule()->settings().dataDir());
    switch (nx::misc::migrateFilesFromWindowsOldDir(&migrateHandler))
    {
    case nx::misc::MigrateDataResult::WinDirNotFound:
        NX_WARNING(this, "Moving data from the old windows dir. Windows dir not found.");
        break;
    case nx::misc::MigrateDataResult::NoNeedToMigrate:
        NX_VERBOSE(this, "Moving data from the old windows dir. Nothing to move");
        break;
    case nx::misc::MigrateDataResult::MoveDataFailed:
        NX_WARNING(this, "Moving data from the old windows dir. Old data found but move failed.");
        break;
    case nx::misc::MigrateDataResult::Ok:
        NX_INFO(this,
            "Moving data from the old windows dir. Old data found and successfully moved.");
        break;
    }
#endif
}

void MediaServerProcess::initCrashDump()
{
#ifdef _WIN32
    win32_exception::setCreateFullCrashDump(serverModule()->settings().createFullCrashDump());
#endif

#ifdef __linux__
    linux_exception::setSignalHandlingDisabled(serverModule()->settings().createFullCrashDump());
    // This is needed because setting capability (CAP_NET_BIND_SERVICE in our case) on the
    // executable automatically sets PR_SET_DUMPABLE to false which in turn stops core dumps from
    // being created.
    prctl(PR_SET_DUMPABLE, 1, 0, 0, 0, 0);
#endif
    m_crashReporter = std::make_unique<ec2::CrashReporter>(commonModule());
}

void MediaServerProcess::setUpServerRuntimeData()
{
    nx::vms::api::RuntimeData runtimeData;
    runtimeData.peer.id = commonModule()->moduleGUID();
    runtimeData.peer.instanceId = commonModule()->runningInstanceGUID();
    runtimeData.peer.persistentId = commonModule()->dbId();
    runtimeData.peer.peerType = nx::vms::api::PeerType::server;
    runtimeData.box = nx::utils::AppInfo::armBox();
    runtimeData.brand = nx::utils::AppInfo::brand();
    runtimeData.customization = nx::utils::AppInfo::customizationName();
    runtimeData.platform = nx::utils::AppInfo::applicationPlatform();

    if (nx::utils::AppInfo::isNx1())
    {
        runtimeData.nx1mac = Nx1::getMac();
        runtimeData.nx1serial = Nx1::getSerial();
    }

    runtimeData.hardwareIds = m_hardwareIdHlist;
    commonModule()->runtimeInfoManager()->updateLocalItem(runtimeData);    // initializing localInfo
}

void MediaServerProcess::initSsl()
{
    static QnMutex initSslMutex;
    static bool sslInitialized = false;
    QnMutexLocker lock(&initSslMutex);
    if (sslInitialized)
        return;

    const auto allowedSslVersions = serverModule()->settings().allowedSslVersions();
    if (!allowedSslVersions.isEmpty())
        nx::network::ssl::Engine::setAllowedServerVersions(allowedSslVersions.toUtf8());

    const auto allowedSslCiphers = serverModule()->settings().allowedSslCiphers();
    if (!allowedSslCiphers.isEmpty())
        nx::network::ssl::Engine::setAllowedServerCiphers(allowedSslCiphers.toUtf8());

    nx::network::ssl::Engine::useOrCreateCertificate(
        serverModule()->settings().sslCertificatePath(),
        nx::utils::AppInfo::vmsName().toUtf8(),
        "US",
        nx::utils::AppInfo::organizationName().toUtf8());

    sslInitialized = true;
}

void MediaServerProcess::doMigrationFrom_2_4()
{
    const auto& settings = serverModule()->settings();
    if (settings.pendingSwitchToClusterMode() == "yes")
    {
        NX_WARNING(this, QString::fromLatin1("Switching to cluster mode and restarting..."));
        SystemName systemName(serverModule(), m_ec2Connection->connectionInfo().systemName);
        systemName.saveToConfig(); //< migrate system name from foreign database via config
        SettingsHelper(serverModule()).setSysIdTime(0);
        serverModule()->mutableSettings()->appserverHost.remove();
        serverModule()->mutableSettings()->appserverLogin.remove();
        serverModule()->mutableSettings()->appserverPassword.set("");
        serverModule()->mutableSettings()->pendingSwitchToClusterMode.remove();
        serverModule()->syncRoSettings();

        QFile::remove(closeDirPath(settings.dataDir()) + "/ecs.sqlite");

        // kill itself to restart
#ifdef Q_OS_WIN
        HANDLE hProcess = GetCurrentProcess();
        TerminateProcess(hProcess, ERROR_SERVICE_SPECIFIC_ERROR);
        WaitForSingleObject(hProcess, 10 * 1000);
#endif
        abort();
        return;
    }
}

void MediaServerProcess::loadPlugins()
{
    auto storagePlugins = serverModule()->storagePluginFactory();
    auto pluginManager = serverModule()->pluginManager();
    for (nx_spl::StorageFactory* const storagePlugin:
    pluginManager->findNxPlugins<nx_spl::StorageFactory>(nx_spl::IID_StorageFactory))
    {
        storagePlugins->registerStoragePlugin(
            storagePlugin->storageType(),
            [this, storagePlugin](QnCommonModule*, const QString& path)
            {
                auto settings = &serverModule()->settings();
                return QnThirdPartyStorageResource::instance(
                    serverModule(), path, storagePlugin, settings);
            }, /*isDefaultProtocol*/ false);
    }

    storagePlugins->registerStoragePlugin(
        "file",
        [this](QnCommonModule*, const QString& path)
        {
            return QnFileStorageResource::instance(this->serverModule(), path);
        }, /*isDefaultProtocol*/ true);

    storagePlugins->registerStoragePlugin(
        "dbfile",
        QnDbStorageResource::instance, /*isDefaultProtocol*/ false);

    storagePlugins->registerStoragePlugin(
        "smb",
        [this](QnCommonModule*, const QString& path)
        {
            return QnFileStorageResource::instance(this->serverModule(), path);
        }, /*isDefaultProtocol*/ false);
}

void MediaServerProcess::connectStorageSignals(QnStorageManager* storage)
{
    connect(storage, &QnStorageManager::noStoragesAvailable, this,
        &MediaServerProcess::at_storageManager_noStoragesAvailable);
    connect(storage, &QnStorageManager::storagesAvailable, this,
        &MediaServerProcess::at_storageManager_storagesAvailable);
    connect(storage, &QnStorageManager::storageFailure, this,
        &MediaServerProcess::at_storageManager_storageFailure);
    connect(storage, &QnStorageManager::rebuildFinished, this,
        &MediaServerProcess::at_storageManager_rebuildFinished);
    connect(storage, &QnStorageManager::backupFinished, this,
        &MediaServerProcess::at_archiveBackupFinished);
}

void MediaServerProcess::connectSignals()
{
    connect(
        this, &MediaServerProcess::started,
        [this]() { this->serverModule()->updateManager()->connectToSignals(); });

    using namespace nx::vms::common::p2p::downloader;
    connect(this, &MediaServerProcess::started,
        [this]()
        {
            Downloader* downloader = this->serverModule()->findInstance<Downloader>();
            downloader->findExistingDownloads();
            downloader->startDownloads();
        });

    connect(commonModule()->resourceDiscoveryManager(),
        &QnResourceDiscoveryManager::CameraIPConflict, this,
        &MediaServerProcess::at_cameraIPConflict);

    connectStorageSignals(serverModule()->normalStorageManager());
    connectStorageSignals(serverModule()->backupStorageManager());

    connectArchiveIntegrityWatcher();

    connect(commonModule(), &QnCommonModule::systemIdentityTimeChanged, this,
        &MediaServerProcess::at_systemIdentityTimeChanged, Qt::QueuedConnection);

    const auto& runtimeManager = commonModule()->runtimeInfoManager();
    connect(runtimeManager, &QnRuntimeInfoManager::runtimeInfoAdded, this,
        &MediaServerProcess::at_runtimeInfoChanged, Qt::QueuedConnection);
    connect(runtimeManager, &QnRuntimeInfoManager::runtimeInfoChanged, this,
        &MediaServerProcess::at_runtimeInfoChanged, Qt::QueuedConnection);
    connect(commonModule()->moduleDiscoveryManager(), &nx::vms::discovery::Manager::conflict, this,
        &MediaServerProcess::at_serverModuleConflict);

    connect(commonModule()->resourceDiscoveryManager(),
        &QnResourceDiscoveryManager::localInterfacesChanged, this,
        &MediaServerProcess::updateAddressesList);

    m_checkAnalyticsTimer = std::make_unique<QTimer>();
    connect(m_checkAnalyticsTimer.get(), SIGNAL(timeout()), this, SLOT(at_checkAnalyticsUsed()), Qt::DirectConnection);
    m_generalTaskTimer = std::make_unique<QTimer>();
    connect(m_generalTaskTimer.get(), SIGNAL(timeout()), this, SLOT(at_timer()), Qt::DirectConnection);
    m_serverStartedTimer = std::make_unique<QTimer>();
    connect(m_serverStartedTimer.get(), SIGNAL(timeout()), this, SLOT(writeServerStartedEvent()), Qt::DirectConnection);

    m_udtInternetTrafficTimer = std::make_unique<QTimer>();
    connect(m_udtInternetTrafficTimer.get(), &QTimer::timeout,
        [common = commonModule()]()
        {
            namespace Server = ResourcePropertyKey::Server;
            QnResourcePtr server = common->resourcePool()->getResourceById(common->moduleGUID());
            const auto old = server->getProperty(Server::kUdtInternetTraffic_bytes).toULongLong();
            const auto current = nx::network::UdtStatistics::global.internetBytesTransfered.load();
            const auto update = old + (qulonglong)current;
            if (server->setProperty(Server::kUdtInternetTraffic_bytes, QString::number(update))
                && server->saveProperties())
            {
                NX_DEBUG(kLogTag, lm("%1 is updated to %2").args(
                    Server::kUdtInternetTraffic_bytes, update));
                nx::network::UdtStatistics::global.internetBytesTransfered -= current;
            }
        });

    m_createDbBackupTimer.reset(new nx::utils::EventLoopTimer);
    connect(
        m_universalTcpListener.get(),
        &QnTcpListener::portChanged,
        this,
        [this]()
    {
        updateAddressesList();
        m_cloudIntegrationManager->cloudManagerGroup().connectionManager.setProxyVia(
            nx::network::SocketAddress(nx::network::HostAddress::localhost, m_universalTcpListener->getPort()));
    });

    connect(m_raidEventLogReader.get(), &RaidEventLogReader::eventOccurred, this,
        &MediaServerProcess::at_storageManager_raidStorageFailure);
    m_raidEventLogReader->subscribe();

    connect(
        this, &MediaServerProcess::started,
        [this]() { emit MediaServerProcess::startedWithSignalsProcessed(); });
}

void MediaServerProcess::setUpDataFromSettings()
{
    QnMulticodecRtpReader::setDefaultTransport(QnLexical::deserialized(
        serverModule()->settings().rtspTransport(),
        nx::vms::api::RtpTransportType::automatic));

    // If adminPassword is set by installer save it and create admin user with it if not exists yet
    commonModule()->setDefaultAdminPassword(serverModule()->settings().appserverPassword());
    commonModule()->setUseLowPriorityAdminPasswordHack(
        serverModule()->settings().lowPriorityPassword());

    BeforeRestoreDbData beforeRestoreDbData;
    beforeRestoreDbData.loadFromSettings(serverModule()->roSettings());
    commonModule()->setBeforeRestoreData(beforeRestoreDbData);

    commonModule()->setModuleGUID(QnUuid(serverModule()->settings().serverGuid()));

    const QString appserverHostString = serverModule()->settings().appserverHost();

    SettingsHelper settingsHelper(this->serverModule());
    commonModule()->setSystemIdentityTime(settingsHelper.getSysIdTime(), commonModule()->moduleGUID());
    if (serverModule()->settings().disableTranscoding())
        commonModule()->setTranscodeDisabled(true);

    if (!m_cmdLineArguments.engineVersion.isNull())
    {
        qWarning() << "Starting with overridden version: " << m_cmdLineArguments.engineVersion;
        commonModule()->setEngineVersion(nx::utils::SoftwareVersion(m_cmdLineArguments.engineVersion));
    }

    if (!m_cmdLineArguments.allowedDiscoveryPeers.isEmpty())
    {
        QSet<QnUuid> allowedPeers;
        for (const QString &peer : m_cmdLineArguments.allowedDiscoveryPeers.split(";")) {
            QnUuid peerId(peer);
            if (!peerId.isNull())
                allowedPeers << peerId;
        }
        commonModule()->setAllowedPeers(allowedPeers);
    }

    if (!m_cmdLineArguments.enforceSocketType.isEmpty())
        nx::network::SocketFactory::enforceStreamSocketType(m_cmdLineArguments.enforceSocketType);
    auto ipVersion = m_cmdLineArguments.ipVersion;
    if (ipVersion.isEmpty())
        ipVersion = serverModule()->settings().ipVersion();

    nx::network::SocketFactory::setIpVersion(ipVersion);
}

QnUuid MediaServerProcess::selectDefaultStorageForAnalyticsEvents(QnMediaServerResourcePtr server)
{
    QnUuid result;
    qint64 maxTotalSpace = 0;

    for (const auto& storage: server->getStorages())
    {
        if (auto fileStorage = storage.dynamicCast<QnFileStorageResource>())
        {
            if (fileStorage->isLocal()
                && !fileStorage->isSystem()
                && fileStorage->isUsedForWriting()
                && storage->initOrUpdate() == Qn::StorageInit_Ok
                && fileStorage->isWritable()
                && fileStorage->getTotalSpace() > maxTotalSpace)
            {
                maxTotalSpace = fileStorage->getTotalSpace();
                result = fileStorage->getId();
            }
        }
    }

    return result;
}

QString MediaServerProcess::getMetadataDatabaseName() const
{
    return serverModule()->metadataDatabaseDir() + "object_detection.sqlite";
}

bool MediaServerProcess::initializeAnalyticsEvents()
{
    const auto dbFilePath = getMetadataDatabaseName();
    auto settings = this->serverModule()->analyticEventsStorageSettings();
    settings.path = QFileInfo(dbFilePath).absoluteDir().path();
    settings.dbConnectionOptions.dbName = QFileInfo(dbFilePath).fileName();

    if (!this->serverModule()->analyticsEventsStorage()->initialize(settings))
    {
        NX_ERROR(this, "Failed to change analytics events storage, initialization error");
        serverModule()->serverRuntimeEventManager()->triggerAnalyticsStorageParametersChanged(
            commonModule()->moduleGUID());
        return false;
    }

    if (!m_oldAnalyticsStoragePath.isEmpty())
    {
        auto policy = commonModule()->globalSettings()->metadataStorageChangePolicy();
        if (policy == nx::vms::api::MetadataStorageChangePolicy::remove)
        {
            if (!QFile::remove(m_oldAnalyticsStoragePath))
                NX_WARNING(this, "Can't remove analytics database [%1]", m_oldAnalyticsStoragePath);
            else
                NX_INFO(this, "Analytics database [%1] removed", m_oldAnalyticsStoragePath);
            QString dirName = QFileInfo(m_oldAnalyticsStoragePath).absolutePath() + "/archive/metadata";
            QDir dir(dirName);
            if (!dir.removeRecursively())
                NX_WARNING(this, "Can't remove analytics binary database [%1]", dirName);
            else
                NX_INFO(this, "Analytics binary database [%1] removed", dirName);
        }
    }

    m_oldAnalyticsStoragePath = closeDirPath(settings.path) + settings.dbConnectionOptions.dbName;
    serverModule()->serverRuntimeEventManager()->triggerAnalyticsStorageParametersChanged(
        commonModule()->moduleGUID());
    return true;
}

void MediaServerProcess::setUpTcpLogReceiver()
{
    // Start plain TCP listener and write data to a separate log file.
    const int tcpLogPort = serverModule()->settings().tcpLogPort();
    if (tcpLogPort)
    {
        m_logReceiver = std::make_shared<TcpLogReceiver>(
            serverModule()->settings().dataDir(),
            commonModule(), QHostAddress::Any, tcpLogPort);
        m_logReceiver->start();
    }
}

void MediaServerProcess::initNewSystemStateIfNeeded(
    bool foundOwnServerInDb,
    const nx::mserver_aux::SettingsProxyPtr& settingsProxy)
{
    const auto& globalSettings = commonModule()->globalSettings();
    if (!QnPermissionsHelper::isSafeMode(serverModule()))
    {
        if (nx::mserver_aux::needToResetSystem(
            nx::mserver_aux::isNewServerInstance(
            commonModule()->beforeRestoreDbData(),
            foundOwnServerInDb,
            serverModule()->settings().noSetupWizard() > 0),
            settingsProxy.get()))
        {
            if (settingsProxy->isCloudInstanceChanged())
                qWarning() << "Cloud instance changed from" << globalSettings->cloudHost() <<
                    "to" << nx::network::SocketGlobals::cloud().cloudHost() << ". Server goes to the new state";

            resetSystemState(m_cloudIntegrationManager->cloudManagerGroup().connectionManager);
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
        globalSettings->setCloudHost(nx::network::SocketGlobals::cloud().cloudHost());
        globalSettings->synchronizeNow();
    }

    if (m_cmdLineArguments.cleanupDb)
    {
        const bool kCleanupDbObjects = true;
        const bool kCleanupTransactionLog = true;
        auto miscManager = m_ec2Connection->getMiscManager(Qn::kSystemAccess);
        miscManager->cleanupDatabaseSync(kCleanupDbObjects, kCleanupTransactionLog);
    }
}

void MediaServerProcess::onBackupDbTimer()
{
    Utils(serverModule()).backupDatabase("timer");
    m_createDbBackupTimer->start(calculateDbBackupTimeout(), [this]() { onBackupDbTimer(); });
}

std::chrono::milliseconds MediaServerProcess::calculateDbBackupTimeout() const
{
    using namespace std::chrono;
    using namespace std::chrono_literals;

    const auto lastBackupTimestamp = Utils(serverModule()).lastDbBackupTimestamp();
    if (!lastBackupTimestamp)
        return 0ms;

    const auto periodFromSettings = serverModule()->settings().dbBackupPeriodMS().count();
    const auto nowMs = qnSyncTime->currentMSecsSinceEpoch();
    if (*lastBackupTimestamp >= nowMs) //< Last backup was performed in the future.
        return milliseconds(*lastBackupTimestamp - nowMs + periodFromSettings);

    return milliseconds(std::max<int64_t>(*lastBackupTimestamp + periodFromSettings - nowMs, 0LL));
}

void MediaServerProcess::startObjects()
{
    QTimer::singleShot(0, this, SLOT(at_appStarted()));

    at_timer();
    m_generalTaskTimer->start(QnVirtualCameraResource::issuesTimeoutMs());
    m_udtInternetTrafficTimer->start(UDT_INTERNET_TRAFIC_TIMER);
    m_createDbBackupTimer->start(calculateDbBackupTimeout(), [this]() { onBackupDbTimer(); });

    const bool isDiscoveryDisabled = serverModule()->settings().noResourceDiscovery();

    serverModule()->resourceCommandProcessor()->start();
    if (m_ec2Connection->connectionInfo().ecUrl.scheme() == "file" && !isDiscoveryDisabled)
        commonModule()->moduleDiscoveryManager()->start();

    if (!m_ec2Connection->connectionInfo().ecDbReadOnly && !isDiscoveryDisabled)
        commonModule()->resourceDiscoveryManager()->start();

    serverModule()->recordingManager()->start();
    if (!isDiscoveryDisabled)
        m_mserverResourceSearcher->start();
    serverModule()->serverConnector()->start();
    serverModule()->backupStorageManager()->scheduleSync()->start();
    serverModule()->unusedWallpapersWatcher()->start();
    if (m_serviceMode)
        serverModule()->licenseWatcher()->start();
    serverModule()->videoWallLicenseWatcher()->start();

    commonModule()->messageProcessor()->init(commonModule()->ec2Connection()); // start receiving notifications
    m_universalTcpListener->start();

    // Write server started event with delay. In case of client has time to reconnect, it could display it on the right panel.
    m_serverStartedTimer->setSingleShot(true);
    m_serverStartedTimer->start(serverModule()->settings().serverStartedEventTimeoutMs());
    m_checkAnalyticsTimer->start(kCheckAnalyticsUsedTimeout);

    if (nx::vms::server::nvr::IService* const nvrService = serverModule()->nvrService())
        nvrService->start();
}

std::map<QString, QVariant> MediaServerProcess::confParamsFromSettings() const
{
    //passing settings
    std::map<QString, QVariant> confParams;
    for (const auto& paramName: serverModule()->roSettings()->allKeys())
    {
        if (paramName.startsWith("ec"))
            confParams.emplace(paramName, serverModule()->roSettings()->value(paramName));
    }
    return confParams;
}

void MediaServerProcess::writeMutableSettingsData()
{
    serverModule()->mutableSettings()->removeDbOnStartup.set(false);
    serverModule()->mutableSettings()->lowPriorityPassword.set(false);

    /* This key means that password should be forcibly changed in the database. */
    serverModule()->mutableSettings()->obsoleteServerGuid.remove();
    serverModule()->mutableSettings()->appserverPassword.set("");
#ifdef _DEBUG
    NX_ASSERT(serverModule()->settings().appserverPassword().isEmpty(),
        "appserverPassword is not empty in registry. Restart the server as Administrator");
#endif

    // show our cloud host value in registry in case of installer will check it
    serverModule()->roSettings()->setValue(QnServer::kIsConnectedToCloudKey,
        commonModule()->globalSettings()->cloudSystemId().isEmpty() ? "no" : "yes");
    serverModule()->roSettings()->setValue("cloudHost", nx::network::SocketGlobals::cloud().cloudHost());
    serverModule()->runTimeSettings()->remove("rebuild");

    serverModule()->syncRoSettings();
}

void MediaServerProcess::createTcpListener()
{
    const int maxConnections = serverModule()->settings().maxConnections();
    const bool useTwoSockets = serverModule()->settings().useTwoSockets();
    NX_INFO(this, lm("Max TCP connections from server= %1").arg(maxConnections));

    // Accept SSL connections in all cases as it is always in use by cloud modules and old clients,
    // config value only affects server preference listed in moduleInformation.
    const bool acceptSslConnections = true;

    m_universalTcpListener = std::make_unique<QnUniversalTcpListener>(
        commonModule(),
        QHostAddress::Any,
        serverModule()->settings().port(),
        maxConnections,
        acceptSslConnections,
        useTwoSockets);
}

void MediaServerProcess::loadResourcesFromDatabase()
{
    auto commonModule = serverModule()->commonModule();

    nx::vms::utils::loadResourcesFromEcs(
        commonModule,
        m_ec2Connection,
        commonModule->messageProcessor(),
        m_mediaServer,
        [this]() { return needToStop(); });

    if (m_cmdLineArguments.moveHandlingCameras)
        moveHandlingCameras();
}

static std::pair<QString, QByteArray> loadResourceParamsSettingsFromDb(
    ec2::AbstractResourceManagerPtr manager)
{
    nx::vms::api::ResourceParamWithRefDataList data;
    manager->getKvPairsSync(QnUuid(), &data);
    std::pair<QString, QByteArray> result;
    int found = 0;
    for (const auto& param: data)
    {
        if (param.name == "resourceFileUri")
        {
            result.first = param.value;
            found |= 1;
            if (found == 3)
                return result;
        }
        else if (param.name == Qn::kResourceDataParamName)
        {
            result.second = param.value.toUtf8();
            found |= 2;
            if (found == 3)
                return result;
        }
    }
    return result;
}

void MediaServerProcess::loadResourceParamsData()
{
    using Data = nx::vms::utils::ResourceParamsData;
    auto manager = m_ec2Connection->getResourceManager(Qn::kSystemAccess);
    const auto [resourceFileUriFromDb, dataFromDb] = loadResourceParamsSettingsFromDb(manager);
    std::vector<Data> datas;
    QString local = QCoreApplication::applicationDirPath() + "/resource_data.json";
    if (QFile::exists(local))
        datas.push_back(Data::load(QFile(local)));
    if (serverModule()->settings().onlineResourceDataEnabled())
    {
        datas.push_back(Data::load(resourceFileUriFromDb.isEmpty()
            ? commonModule()->globalSettings()->resourceFileUri()
            : nx::utils::Url(resourceFileUriFromDb)));
        datas.push_back(Data::load(
            nx::utils::Url("http://beta.vmsproxy.com/beta-builds/daily/resource_data.json")));
    }
    datas.push_back({"server DB", dataFromDb});
    datas.push_back(Data::load(QFile(":/resource_data.json")));

    if (auto data = Data::getWithGreaterVersion(datas); data.value != dataFromDb)
    {
        NX_INFO(this, "Update system wide resource_data.json from %1", data.location);
        manager->saveSync({{QnUuid(), Qn::kResourceDataParamName, std::move(data.value)}});
    }
}

void MediaServerProcess::initMetricsController()
{
    if (!ini().enableVmsMetrics)
    {
        NX_WARNING(this, "VMS Metrics are disabled by ini config");
        return;
    }

    m_metricsController = std::make_unique<nx::vms::utils::metrics::SystemController>();

    using namespace nx::vms;
    m_metricsController->add(std::make_unique<server::metrics::SystemResourceController>(serverModule()));
    m_metricsController->add(std::make_unique<server::metrics::ServerController>(serverModule()));
    m_metricsController->add(std::make_unique<server::metrics::CameraController>(serverModule()));
    m_metricsController->add(std::make_unique<server::metrics::StorageController>(serverModule()));
    m_metricsController->add(std::make_unique<server::metrics::NetworkController>(serverModule()));

    QFile rulesFile(":/metrics_rules.json");
    const auto rulesJson = rulesFile.open(QIODevice::ReadOnly) ? rulesFile.readAll() : QByteArray();
    api::metrics::SystemRules rules;
    NX_CRITICAL(QJson::deserialize(rulesJson, &rules), rulesJson);

    m_metricsController->setRules(std::move(rules));
    m_metricsController->manifest(); //< Cause manifest to cache for faster future use.
    m_metricsController->start();
}

void MediaServerProcess::updateRootPassword()
{
    // TODO: Root password for Nx1 should be updated in case of cloud owner.
    if (QnUserResourcePtr adminUser = commonModule()->resourcePool()->getAdministrator())
    {
        serverModule()->hostSystemPasswordSynchronizer()->
            syncLocalHostRootPasswordWithAdminIfNeeded(adminUser);
    }
    serverModule()->syncRoSettings();
}

void MediaServerProcess::createResourceProcessor()
{
    m_serverResourceProcessor = std::make_unique<QnAppserverResourceProcessor>(
        serverModule(), m_ec2ConnectionFactory->distributedMutex(), m_mediaServer->getId());
    m_serverResourceProcessor->moveToThread(commonModule()->resourceDiscoveryManager());
    commonModule()->resourceDiscoveryManager()->setResourceProcessor(m_serverResourceProcessor.get());
}

void MediaServerProcess::run()
{
    // All managers use QnConcurent with blocking tasks, this hack is required to avoid delays.
    if (QThreadPool::globalInstance()->maxThreadCount() < kMinimalGlobalThreadPoolSize)
        QThreadPool::globalInstance()->setMaxThreadCount(kMinimalGlobalThreadPoolSize);

    auto serverSettings = std::make_unique<MSSettings>(
        cmdLineArguments().configFilePath,
        cmdLineArguments().rwConfigFilePath);

    if (m_serviceMode)
        initializeLogging(serverSettings.get());

    // This must be done before QnCommonModule instantiation.
    nx::utils::OsInfo::currentVariantOverride = ini().currentOsVariantOverride;
    nx::utils::OsInfo::currentVariantVersionOverride = ini().currentOsVariantVersionOverride;

    if (m_cmdLineArguments.vmsProtocolVersion > 0)
    {
        nx::vms::api::protocolVersionOverride = m_cmdLineArguments.vmsProtocolVersion;
        NX_WARNING(this, "Starting with overridden protocol version: %1",
            m_cmdLineArguments.vmsProtocolVersion);
    }

    std::shared_ptr<QnMediaServerModule> serverModule(new QnMediaServerModule(
        &m_cmdLineArguments,
        std::move(serverSettings)));

    m_serverModule = serverModule;

    if (m_serviceMode)
        initializeHardwareId();

    prepareOsResources();
    serverModule->initializeP2PDownloader();

    updateAllowedInterfaces();

    setUpTcpLogReceiver();
    migrateDataFromOldDir();
    QnFileStorageResource::removeOldDirs(serverModule.get()); //< Cleanup temp folders.
    initCrashDump();
    initSsl();

    m_serverMessageProcessor =
        commonModule()->createMessageProcessor<QnServerMessageProcessor>(this->serverModule());

    m_remoteArchiveSynchronizer = std::make_unique<
        nx::vms::server::recorder::RemoteArchiveSynchronizer>(serverModule.get());

    setUpDataFromSettings();
    initializeCloudConnect();
    setUpServerRuntimeData();

    createTcpListener();
    connectSignals();

    m_ec2ConnectionFactory = std::make_unique<ec2::LocalConnectionFactory>(
        commonModule(),
        nx::vms::api::PeerType::server,
        serverModule->settings().p2pMode(),
        serverModule->settings().ecDbReadOnly(),
        m_universalTcpListener.get());

    m_timeBasedNonceProvider = std::make_unique<TimeBasedNonceProvider>();
    m_cloudIntegrationManager = std::make_unique<CloudIntegrationManager>(
        this->serverModule(),
        m_ec2ConnectionFactory->messageBus(),
        m_timeBasedNonceProvider.get());

    m_mediaServerStatusWatcher = std::make_unique<MediaServerStatusWatcher>(serverModule.get());

    // If an exception is thrown by Qt event handler from within exec(), we want to do some cleanup
    // anyway.
    auto stopObjectsGuard = nx::utils::makeScopeGuard([this]() { stopObjects(); });

    if (!serverModule->serverDb()->open())
    {
        NX_ERROR(this, "Stopping the Server because can't open the database");
        return;
    }

    const auto nxVersionFromDb =
        ec2::detail::QnDbManager::currentSoftwareVersion(appServerConnectionUrl().toLocalFile());
    const auto nxVersion = nx::utils::SoftwareVersion(nx::utils::AppInfo::applicationVersion());
    NX_ASSERT(!nxVersion.isNull());

    if (!nxVersionFromDb.isNull() && nxVersion != nxVersionFromDb)
    {
        nx::vms::server::Utils utils(serverModule.get());
        utils.backupDatabaseViaCopy(nxVersionFromDb.build(), "timer");
    }

    if (!connectToDatabase())
        return;

    m_discoveryMonitor = std::make_unique<nx::vms::server::discovery::DiscoveryMonitor>(
        m_ec2ConnectionFactory->messageBus());

    if (needToStop())
        return;

    doMigrationFrom_2_4();

    m_mserverResourceSearcher = std::make_unique<QnMServerResourceSearcher>(this->serverModule());

    loadPlugins();

    initResourceTypes();

    initMetricsController();

    if (needToStop())
        return;

    if (!initTcpListener(
        m_timeBasedNonceProvider.get(),
        &m_cloudIntegrationManager->cloudManagerGroup(),
        m_ec2ConnectionFactory.get()))
    {
        return;
    }

    m_ec2ConnectionFactory->registerRestHandlers(m_universalTcpListener->processorPool());

    m_multicastHttp = std::make_unique<QnMulticast::HttpServer>(
        commonModule()->moduleGUID().toQUuid(), m_universalTcpListener.get());

    m_universalTcpListener->setProxyHandler<nx::vms::network::ProxyConnectionProcessor>(
        &nx::vms::network::ProxyConnectionProcessor::isProxyNeeded,
        m_ec2ConnectionFactory->serverConnector(),
        serverModule->settings().allowThirdPartyProxy());

    m_ec2ConnectionFactory->registerTransactionListener(m_universalTcpListener.get());

    const bool foundOwnServerInDb = setUpMediaServerResource(
        m_cloudIntegrationManager.get(), serverModule.get(), m_ec2Connection);

    writeMutableSettingsData();

    if (needToStop())
        return;

    serverModule->resourcePool()->threadPool()->setMaxThreadCount(
        serverModule->settings().resourceInitThreadsCount());

    createResourceProcessor();

    // Searchers must be initialized before the resources are loaded as resources instances
    // are created by searchers.
    serverModule->resourceSearchers()->initialize();

    m_audioStreamerPool = std::make_unique<QnAudioStreamerPool>(serverModule.get());

    initializeUpnpPortMapper();

    loadResourceParamsData();
    loadResourcesFromDatabase();

    m_serverMessageProcessor->startReceivingLocalNotifications(m_ec2Connection);

    serverModule->sdkObjectFactory()->init();
    serverModule->analyticsManager()->init();
    m_cloudIntegrationManager->init();

    at_runtimeInfoChanged(commonModule()->runtimeInfoManager()->localInfo());

    startPublicIpDiscovery();

    saveServerInfo(m_mediaServer);

    commonModule()->globalSettings()->initialize();

    updateAddressesList();

    auto settingsProxy = nx::mserver_aux::createServerSettingsProxy(this->serverModule());
    auto systemNameProxy = nx::mserver_aux::createServerSystemNameProxy(this->serverModule());

    nx::mserver_aux::setUpSystemIdentity(commonModule()->beforeRestoreDbData(),
        settingsProxy.get(), std::move(systemNameProxy));

    BeforeRestoreDbData::clearSettings(serverModule->roSettings());

    addFakeVideowallUser(commonModule());

    if (!serverModule->settings().noInitStoragesOnStartup())
        initStoragesAsync(commonModule()->messageProcessor());

    initNewSystemStateIfNeeded(foundOwnServerInDb, settingsProxy);

    commonModule()->globalSettings()->takeFromSettings(serverModule->roSettings(), m_mediaServer);

    updateRootPassword();

    if (!nx::utils::AppInfo::isEdgeServer())
    {
        // TODO: #sivanov Rewrite this consistently with other settings.
        updateDisabledVendorsIfNeeded();
        updateAllowCameraChangesIfNeeded();
        commonModule()->globalSettings()->synchronizeNowSync();
    }
    if (m_setupModuleCallback)
        m_setupModuleCallback(serverModule.get());

    commonModule()->resourceDiscoveryManager()->setReady(true);

    m_dumpSystemResourceUsageTaskId = commonModule()->timerManager()->addTimer(
        std::bind(&MediaServerProcess::dumpSystemUsageStats, this),
        std::chrono::seconds(ini().systemUsageDumpTimeoutS));

    nx::mserver_aux::makeFakeData(
        cmdLineArguments().createFakeData, m_ec2Connection, commonModule()->moduleGUID());

    startObjects();

    emit started();
    exec(); //< Start Qt event loop.
}

void MediaServerProcess::at_appStarted()
{
    if (isStopping())
        return;

    m_crashReporter->scanAndReportByTimer(serverModule()->runTimeSettings());
    startDeletor();
    updateSpecificFeatures();
};

void MediaServerProcess::at_runtimeInfoChanged(const QnPeerRuntimeInfo& runtimeInfo)
{
    if (isStopping())
        return;
    if (runtimeInfo.uuid != commonModule()->moduleGUID())
        return;
    auto connection = commonModule()->ec2Connection();
    if (connection)
    {
        ec2::QnTransaction<nx::vms::api::RuntimeData> tran(
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

        nx::vms::api::UserData userData;
        ec2::fromResourceToApi(user, userData);

        QnUuid userId = user->getId();
        m_updateUserRequests << userId;
        appServerConnection->getUserManager(Qn::kSystemAccess)->save(userData, password, this,
            [this, userId]( int /*reqID*/, ec2::ErrorCode /*errorCode*/)
            {
                m_updateUserRequests.remove(userId);
            });
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
        qnStaticCommon->instance<QnLongRunnablePool>()->stopAll();

        m_main.reset();

#ifdef Q_OS_WIN
        // stop the service unexpectedly to let windows service management system restart it
        if (gRestartFlag)
        {
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

        m_main->initStaticCommonModule();

        if (application->isRunning() &&
            m_main->enableMultipleInstances() == 0)
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
            serviceMainInstance.load()->stopSync();
    }

private:
    int m_argc;
    char **m_argv;
    QScopedPointer<MediaServerProcess> m_main;
};

void stopServer(int /*signal*/)
{
    gRestartFlag = false;
    if (serviceMainInstance)
    {
        // ATTENTION: This method is called from a signal handler, thus, IO operations of any kind
        // are prohibited because of potential deadlocks.

        // TODO: Potential deadlock - the signal may come when the event queue mutex is locked.
        serviceMainInstance.load()->stopAsync();
    }
}

void restartServer(int restartTimeout)
{
    gRestartFlag = true;
    if (serviceMainInstance) {
        qWarning() << "restart requested!";
        QTimer::singleShot(restartTimeout, serviceMainInstance.load(), SLOT(stopAsync()));
    }
}

/*
bool changePort(quint16 port)
{
    if (serviceMainInstance)
        return serviceMainInstance.load()->changePort(port);
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
    // Set locale to default "C" locale to have no issues with locale dependent standard conversion
    // functions. Using of QnTranslationManager::installTranslation is not enough as QLocale
    // affects only Qt locale dependent functions.
    setlocale(LC_ALL, "C");
    std::locale::global(std::locale("C"));
    nx::kit::OutputRedirector::ensureOutputRedirection();

    #if defined(_WIN32)
        win32_exception::installGlobalUnhandledExceptionHandler();
        _tzset();
    #endif

    #if defined(__linux__)
        signal(SIGUSR1, SIGUSR1_handler);

    #endif

    // Festival should be initialized before QnVideoService has started because of a Festival bug.
    auto speechSynthesisDataProviderBackend = QnSpeechSynthesisDataProvider::backendInstance(
        nx::utils::file_system::applicationDirPath(argc, argv));

    QnVideoService service(argc, argv);

    const int res = service.exec();
    return (gRestartFlag && res == 0) ? 1 : 0;
}

const nx::vms::server::CmdLineArguments MediaServerProcess::cmdLineArguments() const
{
    return m_cmdLineArguments;
}

void MediaServerProcess::configureApiRestrictions(nx::network::http::AuthMethodRestrictionList* restrictions)
{
    // For "OPTIONS * RTSP/1.0"
    restrictions->allow("\\*", nx::network::http::AuthMethod::noAuth);

    const auto webPrefix = std::string("(/web)?(/proxy/[^/]*(/[^/]*)?)?");
    restrictions->allow(webPrefix + "/api/ping", nx::network::http::AuthMethod::noAuth);
    restrictions->allow(webPrefix + "/api/camera_event.*", nx::network::http::AuthMethod::noAuth);
    restrictions->allow(webPrefix + "/api/moduleInformation", nx::network::http::AuthMethod::noAuth);
    restrictions->allow(webPrefix + "/api/getTime", nx::network::http::AuthMethod::noAuth);
    restrictions->allow(webPrefix + "/api/gettime", nx::network::http::AuthMethod::noAuth);
    restrictions->allow(
        webPrefix + nx::vms::time_sync::TimeSyncManager::kTimeSyncUrlPath.toStdString(),
        nx::network::http::AuthMethod::noAuth);
    restrictions->allow(webPrefix + "/api/getTimeZones", nx::network::http::AuthMethod::noAuth);
    restrictions->allow(webPrefix + "/api/getNonce", nx::network::http::AuthMethod::noAuth);
    restrictions->allow(webPrefix + "/api/cookieLogin", nx::network::http::AuthMethod::noAuth);
    restrictions->allow(webPrefix + "/api/cookieLogout", nx::network::http::AuthMethod::noAuth);
    restrictions->allow(webPrefix + "/api/getCurrentUser", nx::network::http::AuthMethod::noAuth);
    restrictions->allow(webPrefix + "/static/.*", nx::network::http::AuthMethod::noAuth);
    restrictions->allow("/crossdomain.xml", nx::network::http::AuthMethod::noAuth);
    restrictions->allow("/favicon.ico", nx::network::http::AuthMethod::noAuth);
    restrictions->allow(webPrefix + "/api/startLiteClient", nx::network::http::AuthMethod::noAuth);

    // For open in new browser window.
    restrictions->allow(webPrefix + "/api/showLog.*",
        nx::network::http::AuthMethod::urlQueryDigest | nx::network::http::AuthMethod::allowWithourCsrf);

    // For inserting in HTML <img src="...">.
    restrictions->allow(webPrefix + "/ec2/cameraThumbnail",
        nx::network::http::AuthMethod::allowWithourCsrf);

    nx::network::http::AuthMethodRestrictionList::Filter filter;
    filter.protocol = nx::network::http::http_1_0.protocol.toStdString();
    filter.method = nx::network::http::Method::options.toStdString();
    restrictions->allow(
        filter,
        nx::network::http::AuthMethod::noAuth);
}

void MediaServerProcess::updateSpecificFeatures() const
{
    static const QString kSpecificFeaturesFileName("specific_features.txt");
    QDir directory(QCoreApplication::applicationDirPath());
    for (int level = 0; !directory.exists(kSpecificFeaturesFileName); ++level)
    {
        if (level > 3 || !directory.cdUp())
        {
            NX_WARNING(this, "Unable to find %1", kSpecificFeaturesFileName);
            return;
        }
    }

    QFile file(directory.filePath(kSpecificFeaturesFileName));
    if (!file.open(QIODevice::ReadOnly))
    {
        NX_WARNING(this, "Unable to open %1", file.fileName());
        return;
    }

    std::map<QString, int> values;
    for (const auto& line: file.readAll().split('\n'))
    {
        const auto parts = line.split('=');
        if (parts.size() == 2)
            values[parts[0].trimmed()] = parts[1].trimmed().toInt();
        else
            NX_WARNING(this, "Syntax error in %1 on line: %2", file.fileName(), line);
    }

    if (values.empty())
    {
        NX_WARNING(this, "File %1 does not contain any valid values", file.fileName());
        return;
    }

    NX_INFO(this, "Update %1 to %2", file.fileName(), QJson::serialized(values));
    serverModule()->globalSettings()->setSpecificFeatures(values);
    serverModule()->globalSettings()->synchronizeNow();
}
