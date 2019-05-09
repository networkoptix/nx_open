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
#include <nx/vms/server/analytics/db/analytics_db.h>

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
#include <network/connection_validator.h>
#include <network/default_tcp_connection_processor.h>
#include <network/system_helpers.h>

#include <nx_ec/ec_api.h>
#include <nx_ec/ec_proto_version.h>
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
#include <rest/handlers/update_rest_handler.h>
#include <rest/handlers/update_information_rest_handler.h>
#include <rest/handlers/start_update_rest_handler.h>
#include <rest/handlers/finish_update_rest_handler.h>
#include <rest/handlers/update_status_rest_handler.h>
#include <rest/handlers/install_update_rest_handler.h>
#include <rest/handlers/cancel_update_rest_handler.h>
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
#include <rest/handlers/multiserver_analytics_lookup_detected_objects.h>
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

#include <rtsp/rtsp_connection.h>

#include <nx/vms/discovery/manager.h>
#include <nx/vms/utils/initial_data_loader.h>
#include <network/multicodec_rtp_reader.h>
#include <network/router.h>

#include <utils/common/command_line_parser.h>
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
#include "ec2_statictics_reporter.h"

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
#include <nx/vms/server/fs/media_paths/media_paths.h>
#include <nx/vms/server/fs/media_paths/media_paths_filter_config.h>
#include <nx/vms/common/p2p/downloader/downloader.h>
#include <nx/vms/server/root_fs.h>
#include <nx/vms/server/server_update_manager.h>
#include <nx_vms_server_ini.h>
#include <proxy/2wayaudio/proxy_audio_receiver.h>
#include <local_connection_factory.h>
#include <core/resource/resource_command_processor.h>
#include <rest/handlers/sync_time_rest_handler.h>
#include <rest/handlers/metrics_rest_handler.h>
#include <nx/vms/server/event/event_connector.h>
#include <nx/network/http/http_client.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource/storage_plugin_factory.h>

#include <providers/speech_synthesis_data_provider.h>
#include <nx/utils/file_system.h>
#include <nx/network/url/url_builder.h>

#include "nx/vms/server/system/nx1/info.h"
#include <atomic>

using namespace nx::vms::server;

// This constant is used while checking for compatibility.
// Do not change it until you know what you're doing.
static const char COMPONENT_NAME[] = "MediaServer";

static QString SERVICE_NAME = QnServerAppInfo::serviceName();
static const int UDT_INTERNET_TRAFIC_TIMER = 24 * 60 * 60 * 1000; //< Once a day;
//static const quint64 DEFAULT_MSG_LOG_ARCHIVE_SIZE = 5;
static const unsigned int APP_SERVER_REQUEST_ERROR_TIMEOUT_MS = 5500;

class MediaServerProcess;
static MediaServerProcess* serviceMainInstance = nullptr;
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

void addFakeVideowallUser(QnCommonModule* commonModule)
{
    nx::vms::api::UserData fakeUserData;
    fakeUserData.realm = nx::network::AppInfo::realm();
    fakeUserData.permissions = GlobalPermission::videowallModePermissions;

    auto fakeUser = ec2::fromApiToResource(fakeUserData);
    fakeUser->setId(Qn::kVideowallUserAccess.userId);
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

    {
        // if nothing else works use first enabled hostaddr
        QList<nx::network::QnInterfaceAndAddr> interfaces = nx::network::getAllIPv4Interfaces();

        for (int i = 0; i < interfaces.size();++i)
        {
            QUdpSocket socket;
            if (!socket.bind(interfaces.at(i).address, 0))
                continue;

            QString result = socket.localAddress().toString();

            NX_ASSERT(result.length() > 0);

            return result;
        }
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
                storage->getUrl(), static_cast<bool>(fileStorage), totalSpace,
                storage->getSpaceLimit()));
    }
    return result;
}

QnStorageResourcePtr MediaServerProcess::createStorage(const QnUuid& serverId, const QString& path)
{
    NX_VERBOSE(kLogTag, lm("Attempting to create storage %1").arg(path));
    QnStorageResourcePtr storage(serverModule()->storagePluginFactory()->createStorage(commonModule(), "ufile"));
    storage->setName("Initial");
    storage->setParentId(serverId);
    storage->setUrl(path);

    const QString storagePath = QnStorageResource::toNativeDirPath(storage->getPath());
    const auto partitions = m_platform->monitor()->totalPartitionSpaceInfo();
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
        calculateSpaceLimitOrLoadFromConfig(commonModule(), fileStorage);

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

    storage->setParentId(QnUuid(serverModule()->settings().serverGuid()));
    return storage;
}

QStringList MediaServerProcess::listRecordFolders(bool includeNonHdd) const
{
    using namespace nx::vms::server::fs::media_paths;

    auto mediaPathList = get(FilterConfig::createDefault(
        m_platform.get(), includeNonHdd, &serverModule()->settings()));
    NX_VERBOSE(this, lm("Record folders: %1").container(mediaPathList));
    return mediaPathList;
}

QnStorageResourceList MediaServerProcess::createStorages(const QnMediaServerResourcePtr& mServer)
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
        QnStorageResourcePtr storage = createStorage(mServer->getId(), folderPath);
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

QnStorageResourceList MediaServerProcess::updateStorages(QnMediaServerResourcePtr mServer)
{
    const auto partitions = m_platform->monitor()->totalPartitionSpaceInfo();

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
            if (storage->getUrl().contains("://"))
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

void MediaServerProcess::initStoragesAsync(QnCommonMessageProcessor* messageProcessor)
{
    m_initStoragesAsyncPromise.reset(new nx::utils::promise<void>());
    QtConcurrent::run([messageProcessor, this]
    {
        NX_VERBOSE(this, "Init storages begin");
        const auto setPromiseGuardFunc = nx::utils::makeScopeGuard(
            [&]()
            {
                NX_VERBOSE(this, "Init storages end");
                m_initStoragesAsyncPromise->set_value();
            });

        //read server's storages
        ec2::AbstractECConnectionPtr ec2Connection = messageProcessor->commonModule()->ec2Connection();
        ec2::ErrorCode rez;
        nx::vms::api::StorageDataList storages;

        while ((rez = ec2Connection->getMediaServerManager(Qn::kSystemAccess)->getStoragesSync(
            QnUuid(), &storages)) != ec2::ErrorCode::ok)
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

        const auto unmountedStorages =
            nx::mserver_aux::getUnmountedStorages(
                m_platform.get(),
                m_mediaServer->getStorages(),
                &serverModule()->settings());
        for (const auto& storageResource: unmountedStorages)
        {
            auto fileStorageResource = storageResource.dynamicCast<QnFileStorageResource>();
            if (fileStorageResource)
                fileStorageResource->setMounted(false);
        }

        QnStorageResourceList smallStorages = getSmallStorages(m_mediaServer->getStorages());
        QnStorageResourceList storagesToRemove;
        // We won't remove automatically storages which might have been unmounted because of their
        // small size. This small size might be the result of the unmounting itself (just the size
        // of the local drive where mount folder is located). User will be able to remove such
        // storages by themselves.
        for (const auto& smallStorage: smallStorages)
        {
            bool isSmallStorageAmongstUnmounted = false;
            for (const auto& unmountedStorage: unmountedStorages)
            {
                if (unmountedStorage == smallStorage)
                {
                    isSmallStorageAmongstUnmounted = true;
                    break;
                }
            }

            if (!isSmallStorageAmongstUnmounted)
                storagesToRemove.append(smallStorage);
        }

        NX_DEBUG(this, lm("Found %1 storages to remove").arg(storagesToRemove.size()));
        for (const auto& storage: storagesToRemove)
        {
            NX_DEBUG(this, lm("Storage to remove: %2, id: %3").args(
                storage->getUrl(), storage->getId()));
        }

        if (!storagesToRemove.isEmpty())
        {
            nx::vms::api::IdDataList idList;
            for (const auto& value: storagesToRemove)
                idList.push_back(value->getId());
            if (ec2Connection->getMediaServerManager(Qn::kSystemAccess)->removeStoragesSync(idList) != ec2::ErrorCode::ok)
                qWarning() << "Failed to remove deprecated storage on startup. Postpone removing to the next start...";
            commonModule()->resourcePool()->removeResources(storagesToRemove);
        }

        QnStorageResourceList modifiedStorages = createStorages(m_mediaServer);
        modifiedStorages.append(updateStorages(m_mediaServer));
        saveStorages(ec2Connection, modifiedStorages);
        for(const QnStorageResourcePtr &storage: modifiedStorages)
            messageProcessor->updateResource(storage, ec2::NotificationSource::Local);

        serverModule()->normalStorageManager()->initDone();
        serverModule()->backupStorageManager()->initDone();

        if (m_mediaServer->metadataStorageId().isNull() && !QFile::exists(getMetadataDatabaseName()))
        {
            m_mediaServer->setMetadataStorageId(selectDefaultStorageForAnalyticsEvents(m_mediaServer));
            nx::vms::api::MediaServerUserAttributesData userAttrsData;
            ec2::fromResourceToApi(m_mediaServer->userAttributes(), userAttrsData);
            saveMediaServerUserAttributes(ec2Connection, userAttrsData);
        }
        initializeAnalyticsEvents();
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
        id = nx::network::getMacFromPrimaryIF();
    return lm("Server %1").arg(id);
}

QnMediaServerResourcePtr MediaServerProcess::findServer(ec2::AbstractECConnectionPtr ec2Connection)
{
    nx::vms::api::MediaServerDataList servers;

    while (servers.empty() && !needToStop())
    {
        ec2::ErrorCode rez = ec2Connection->getMediaServerManager(Qn::kSystemAccess)->getServersSync(&servers);
        if (rez == ec2::ErrorCode::ok)
            break;

        qDebug() << "findServer(): Call to getServers failed. Reason: " << ec2::toString(rez);
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

void MediaServerProcess::saveStorages(
    ec2::AbstractECConnectionPtr ec2Connection,
    const QnStorageResourceList& storages)
{
    nx::vms::api::StorageDataList apiStorages;
    ec2::fromResourceListToApi(storages, apiStorages);

    ec2::ErrorCode rez;
    while((rez = ec2Connection->getMediaServerManager(Qn::kSystemAccess)->saveStoragesSync(apiStorages))
        != ec2::ErrorCode::ok && !needToStop())
    {
        NX_WARNING(this) << "Call to change server's storages failed. Reason: " << rez;
        QnSleep::msleep(APP_SERVER_REQUEST_ERROR_TIMEOUT_MS);
    }
}

static const int SYSTEM_USAGE_DUMP_TIMEOUT = 7*60*1000;

void MediaServerProcess::dumpSystemUsageStats()
{
    if (!m_platform->monitor())
        return;

    m_platform->monitor()->totalCpuUsage();
    m_platform->monitor()->totalRamUsage();
    m_platform->monitor()->totalHddLoad();

    // TODO: #muskov
    //  - Add some more fields that might be interesting.
    //  - Make and use JSON serializable struct rather than just a string.
    QStringList networkIfList;
    for (const auto& iface: m_platform->monitor()->totalNetworkLoad())
    {
        if (iface.type != QnPlatformMonitor::LoopbackInterface)
        {
            networkIfList.push_back(
                lm("%1: %2 bps").args(iface.interfaceName, iface.bytesPerSecMax));
        }
    }
    const auto networkIfInfo = networkIfList.join(", ");
    if (m_mediaServer->setProperty(
        ResourcePropertyKey::Server::kNetworkInterfaces, networkIfInfo))
    {
        m_mediaServer->saveProperties();
    }

    QnMutexLocker lk(&m_mutex);
    if(m_dumpSystemResourceUsageTaskId == 0)  //monitoring cancelled
        return;
    m_dumpSystemResourceUsageTaskId = nx::utils::TimerManager::instance()->addTimer(
        std::bind(&MediaServerProcess::dumpSystemUsageStats, this),
        std::chrono::milliseconds(SYSTEM_USAGE_DUMP_TIMEOUT));
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

    const bool isStatisticsDisabled =
        settings->settings().noMonitorStatistics();

    m_platform.reset(new QnPlatformAbstraction());
    m_platform->process(NULL)->setPriority(QnPlatformProcess::HighPriority);
    m_platform->setUpdatePeriodMs(
        isStatisticsDisabled ? 0 : QnGlobalMonitor::kDefaultUpdatePeridMs);
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

void MediaServerProcess::at_metadataStorageIdChanged(const QnResourcePtr& /*resource*/)
{
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
    qWarning() << "Stopping server";

    {
        QnMutexLocker lock( &m_stopMutex );
        if (m_stopping)
            return;

        m_stopping = true;
    }

    pleaseStop();
    quit();

    const std::chrono::seconds kStopTimeout(100);
    if (!wait(kStopTimeout))
        NX_CRITICAL(false, lm("Server was unable to stop within %1").arg(kStopTimeout));

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
    namespace Server = ResourcePropertyKey::Server;
    const auto hwInfo = HardwareInformation::instance();
    server->setProperty(Server::kCpuArchitecture, hwInfo.cpuArchitecture);
    server->setProperty(Server::kCpuModelName, hwInfo.cpuModelName);
    server->setProperty(Server::kPhysicalMemory, QString::number(hwInfo.physicalMemory));
    server->setProperty(Server::kProductNameShort, QnAppInfo::productNameShort());
    server->setProperty(Server::kFullVersion, nx::utils::AppInfo::applicationFullVersion());
    server->setProperty(Server::kBeta, QString::number(QnAppInfo::beta() ? 1 : 0));
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
            NX_ALWAYS(this, "New external address %1 has been mapped", address);

            it->second = mappedAddress.port;
            updateAddressesList();
        }
    }
    else
    {
        const auto oldIp = m_forwardedAddresses.find(mappedAddress.address);
        if (oldIp != m_forwardedAddresses.end())
        {
            NX_ALWAYS(this, "External address %1:%2 has been unmapped",
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

void MediaServerProcess::at_connectionOpened()
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
    serverModule()->eventConnector()->at_storageFailure(m_mediaServer, qnSyncTime->currentUSecsSinceEpoch(), reason, storage);
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

    /**%apidoc GET /api/synchronizedTime
     * This method is used for internal purpose to synchronize time between mediaservers and clients.
     * %return:object Information about server synchronized time.
     *     %param:integer utcTimeMs Server synchronized time.
     *     %param:boolean isTakenFromInternet Whether the server has got the time from the internet.
     */
    reg(nx::vms::time_sync::TimeSyncManager::kTimeSyncUrlPath.mid(1),
        new ::rest::handlers::SyncTimeRestHandler());

    /**%apidoc POST /ec2/forcePrimaryTimeServer
     * Set primary time server. Requires a JSON object with optional "id" field in the message
     * body. If "id" field is missing, the primary time server is turned off.
     * %permissions Owner.
     * %return:object JSON object with error message and error code (0 means OK).
     */
    reg("ec2/forcePrimaryTimeServer",
        new ::rest::handlers::SetPrimaryTimeServerRestHandler(), kAdmin);

    /**%apidoc GET /api/storageStatus
     * Check whether specified folder is used as a Server storage, and if yes, report details.
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
     * %param:string cameraId Camera id (can be obtained from "id" field via /ec2/getCamerasEx or
     *     /ec2/getCameras?extraFormatting) or MAC address (not supported for certain cameras).
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
     * %param:string cameraId Camera id (can be obtained from "id" field via /ec2/getCamerasEx or
     *     /ec2/getCameras?extraFormatting) or MAC address (not supported for certain cameras).
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

    reg("api/wearableCamera", new QnWearableCameraRestHandler(serverModule()));

    /**%apidoc GET /api/ptz
     * Perform reading or writing PTZ operation
     * %param:string cameraId Camera id (can be obtained from "id" field via /ec2/getCamerasEx or
     *     /ec2/getCameras?extraFormatting) or MAC address (not supported for certain cameras).
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
     *     %value GetPresetsPtzCommand Read PTZ presets list.
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
     *         be obtained from "id" field via /ec2/getCamerasEx or
     *         /ec2/getCameras?extraFormatting).</li>
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
     *         %param:string reply.vmsTime Synchronized time, in milliseconds since epoch.
     */
    reg(kGetTimePath, new nx::vms::server::rest::GetTimeHandler());

    reg("ec2/getTimeOfServers", new nx::vms::server::rest::ServerTimeHandler("/" + kGetTimePath));

    /**%apidoc GET /api/gettime
     * Get the Server time, time zone and authentication realm (realm is added for convenience).
     * %// TODO: The name of this method and its timezoneId parameter use wrong case.
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

    reg("api/getRemoteNonce", new QnGetNonceRestHandler("/api/getNonce"));
    reg("api/cookieLogin", new QnCookieLoginRestHandler());
    reg("api/cookieLogout", new QnCookieLogoutRestHandler());
    reg("api/getCurrentUser", new QnCurrentUserRestHandler());

    /**%apidoc POST /api/activateLicense
     * Activate new license and return license JSON data if success. It requires internet to
     * connect to the license server.
     * %param:string licenseKey License serial number.
     * %return:object License JSON data.
     */
    reg("api/activateLicense", new QnActivateLicenseRestHandler());

    reg("api/testEmailSettings", new QnTestEmailSettingsHandler());

    /**%apidoc[proprietary] GET /api/getHardwareInfo
     * Get hardware information
     * %return:object JSON with hardware information.
     */
    reg("api/getHardwareInfo", new QnGetHardwareInfoHandler());

    reg("api/testLdapSettings", new QnTestLdapSettingsHandler());

    /**%apidoc GET /api/ping
     * Ping the server.
     * %return:object JSON object with an error code, error string, and the reply on success.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *     %param:object reply Object with data.
     *         %param:string reply.moduleGuid Module unique id.
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
    reg("api/recStats", new QnRecordingStatsRestHandler(serverModule()));

    /**%apidoc GET /api/auditLog
     * %permissions Owner.
     * Return audit log information in the requested format.
     * %param:string from Start time of a time interval (as a string containing time in
     *     milliseconds since epoch, or a local time formatted like
     *     <code>"<i>YYYY</i>-<i>MM</i>-<i>DD</i>T<i>HH</i>:<i>mm</i>:<i>ss</i>.<i>zzz</i>"</code>
     *     - the format is auto-detected).
     * %param[opt]:string to End time of a time interval(as a string containing time in
     *     milliseconds since epoch, or a local time formatted like
     *     <code>"<i>YYYY</i>-<i>MM</i>-<i>DD</i>T<i>HH</i>:<i>mm</i>:<i>ss</i>.<i>zzz</i>"</code>
     *     - the format is auto-detected).
     * %return:text Tail of the server log file in text format
     */
    reg("api/auditLog", new QnAuditLogRestHandler(serverModule()), kAdmin);

    reg("api/checkDiscovery", new QnCanAcceptCameraRestHandler());

    /**%apidoc GET /api/pingSystem
     * Ping the system.
     * %param:string url System URL to ping.
     * %param:string getKey Authorization key ("auth" param) for the system to ping.
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
     * %param:string cameraId Camera id (can be obtained from "id" field via /ec2/getCamerasEx or
     *     /ec2/getCameras?extraFormatting) or MAC address (not supported for certain cameras).
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
     * %return:object Bakcup process progress status or an error code.
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
     *     /ec2/getCamerasEx or /ec2/getCameras?extraFormatting) or MAC address (not supported for
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
     *                 %value QualityLowest
     *                 %value QualityLow
     *                 %value QualityNormal
     *                 %value QualityHigh
     *                 %value QualityHighest
     *                 %value QualityPreSet
     *                 %value QualityNotDefined
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

    // TODO: Add apidoc comment.
    reg("ec2/getEvents", new QnMultiserverEventsRestHandler(serverModule(), "ec2/getEvents"), kViewLogs);

    /**%apidoc GET /api/showLog
     * Return tail of the server log file
     * %param[opt]:integer lines Display last N log lines.
     * %param[opt]:integer id Id of log file. By default main log is returned
     *     %value 0 Main server log
     *     %value 2 Http log
     *     %value 3 Transaction log
     * %return:text Tail of the server log file in text format
     */
    reg("api/showLog", new QnLogRestHandler());

    reg("api/getSystemId", new QnGetSystemIdRestHandler());

    /**%apidoc GET /api/doCameraDiagnosticsStep
     * Performs camera diagnostics.
     * %param:string cameraId Camera id (can be obtained from "id" field via /ec2/getCamerasEx or
     *     /ec2/getCameras?extraFormatting) or MAC address (not supported for certain cameras).
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
     * %param:string name Interface name.
     * %param:string ipAddr IP address with dot-separated decimal components.
     * %param:string netMask Network mask with dot-separated decimal components.
     * %param:string  mac MAC address with colon-separated upper-case hex components.
     * %param:string gateway IP address of the gateway with dot-separated decimal components. Can
     *     be empty.
     * %param:boolean dhcp
     *     %value false DHCP is not used, IP address and other parameters should be specified in
     *         the respective JSON fields.
     *     %value true IP address and other parameters assigned via DHCP, the respective JSON
     *         fields can be empty.
     * %param:object extraParams JSON object with data in the internal format.
     * %param:string dns_servers Space-separated list of IP addresses with dot-separated decimal
     *     components.
     */
    reg("api/ifconfig", new QnIfConfigRestHandler(), kAdmin);

    reg("api/downloads/", new QnDownloadsRestHandler(serverModule()));

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
     * </code>
     * </pre>
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
     * Configure server system name and password. This function can be called for server with
     * default system name. Otherwise function returns error. This method requires owner
     * permissions.
     * %permissions Owner.
     * %param:string password New password for admin user
     * %param:string systemName New system name
     * %return:object JSON object with error message and error code (0 means OK).
     */
    reg("api/setupLocalSystem", new QnSetupLocalSystemRestHandler(serverModule()), kAdmin);

    /**%apidoc POST /api/setupCloudSystem
     * Configure server system name and attach it to cloud. This function can be called for server
     * with default system name. Otherwise function returns error. This method requires owner
     * permissions.
     * %permissions Owner.
     * %param:string systemName New system name
     * %param:string cloudAuthKey could authentication key
     * %param:string cloudSystemID could system id
     * %return:object JSON object with error message and error code (0 means OK).
     */
    reg("api/setupCloudSystem", new QnSetupCloudSystemRestHandler(serverModule(), cloudManagerGroup), kAdmin);

    /**%apidoc POST /api/mergeSystems
     * Merge two Systems. <br/> The System that joins another System is called the current System,
     * the joinable System is called the target System. The <b>URL</b> parameter sets the
     * target Server which should be joined with the current System. Other servers, that are
     * merged with the target Server will be joined if parameter <b>mergeOneServer</b> is set
     * to false. <br/> The method uses digest authentication. Two hashes should be previouly
     * calculated: <b>getKey</b> and <b>postKey</b>. Both are mandatory. The calculation
     * algorithm is described in <b>Calculating authentication hash</b> section (in the bootom
     * of the page). While calculating hashes, username and password of the target Server are
     * needed. Digest authentication needs realm and nonce, both can be obtained with <code>GET
     * /api/getNonce call</code> call. The lifetime of a nonce is about a few minutes.
     * %permissions Owner.
     * %param:string url URL of one Server in the System to join.
     * %param:string getKey Authentication hash of the target Server for GET requests.
     * %param:string postKey Authentication hash of the target Server for POST requests.
     * %param:string currentPassword Current user password.
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
     * %return:object JSON object with an error code and error string.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error token, or an empty string on success.
     *         %value "FAIL" The specified System is unreachable or there is no System.
     *         %value "UNAUTHORIZED" The authentication credentials are invalid.
     *         %value "INCOMPATIBLE" The found system has an incompatible version or a different
     *             customization.
     *         %value "BACKUP_ERROR" The database backup could not be created.
     */
    reg("api/mergeSystems", new QnMergeSystemsRestHandler(serverModule()), kAdmin);

    /**%apidoc GET /api/backupDatabase
     * Back up server database.
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
     * %param[opt]:integer id Log id
     *     %value 0 Main server log
     *     %value 2 Http log
     *     %value 3 Transaction log
     * %param[opt]:enum value Target value for log level. More detailed level includes all less
     *     detailed levels.
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
     * Execute any script from subfolder "scripts" of media server. Script name provides directly
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

    reg("api/transmitAudio", new QnAudioTransmissionRestHandler(serverModule()));

    // TODO: Introduce constants for API methods registered here, also use them in
    // media_server_connection.cpp. Get rid of static/global urlPath passed to some handler ctors,
    // except when it is the path of some other api method.

    reg("api/RecordedTimePeriods", new QnRecordedChunksRestHandler(serverModule())); //< deprecated

    /**%apidoc GET /ec2/recordedTimePeriods
     * Return the recorded chunks info for the specified cameras.
     * %param:string cameraId Camera id (can be obtained from "id" field via /ec2/getCamerasEx or
     *     /ec2/getCameras?extraFormatting) or MAC address (not supported for certain cameras).
     *     This parameter can be used several times to define a list of cameras.
     * %param[opt]:string startTime Start time of the interval (as a string containing time in
     *     milliseconds since epoch, or a local time formatted like
     *     <code>"<i>YYYY</i>-<i>MM</i>-<i>DD</i>T<i>HH</i>:<i>mm</i>:<i>ss</i>.<i>zzz</i>"</code>
     *     - the format is auto-detected).
     * %param[opt]:string endTime End time of the interval (as a string containing time in
     *     milliseconds since epoch, or a local time formatted like
     *     <code>"<i>YYYY</i>-<i>MM</i>-<i>DD</i>T<i>HH</i>:<i>mm</i>:<i>ss</i>.<i>zzz</i>"</code>
     *     - the format is auto-detected).
     * %param[opt]:arrayJson filter This parameter is used for motion search ("periodsType" must
     *     be 1). Match motion on a video by specified rectangle.
     *     <br/>Format: string with a JSON list of <i>sensors</i>,
     *     each <i>sensor</i> is a JSON list of <i>rects</i>, each <i>rect</i> is:
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
     * %param[proprietary]:enum format Data format. Default value is "json".
     *     %value ubjson Universal Binary JSON data format.
     *     %value json JSON data format.
     *     %value periods Internal comperssed binary format.
     * %param[opt]:integer detail Chunk detail level, in microseconds. Time periods that are
     *     shorter than the detail level are discarded. You can treat the detail level as the
     *     amount of microseconds per screen pixel.
     * %param[opt]:integer periodsType Chunk type.
     *     %value 0 All records.
     *     %value 1 Only chunks with motion (parameter "filter" is required).
     * %param[opt]:option keepSmallChunks If specified, standalone chunks smaller than the detail
     *     level are not removed from the result.
     * %param[opt]:integer limit Maximum number of chunks to return.
     * %param[opt]:option flat If specified, do not group chunk lists by server. This parameter is
     *     deprecated. Please use parameter "groupBy" instead.
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

    reg("ec2/cameraHistory", new QnCameraHistoryRestHandler(serverModule()));

    /**%apidoc GET /ec2/bookmarks
     * Read bookmarks using the specified parameters.
     * %param:string cameraId Camera id (can be obtained from "id" field via /ec2/getCamerasEx or
     *     /ec2/getCameras?extraFormatting) or MAC address (not supported for certain cameras).
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
     * %param:string cameraId Camera id (can be obtained from "id" field via /ec2/getCamerasEx or
     *     /ec2/getCameras?extraFormatting) or MAC address (not supported for certain cameras).
     * %param:string name Caption of the bookmark.
     * %param[opt]:string description Details of the bookmark.
     * %param[opt]:integer timeout Time during which the recorded period should be preserved (in
     *     milliseconds).
     * %param:integer startTime Start time of the bookmark (in milliseconds since epoch).
     * %param:integer duration Length of the bookmark (in milliseconds).
     * %param[opt] tag Applied tag. Several tag parameters could be used to specify multiple tags.
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
     * %param:string cameraId Camera id (can be obtained from "id" field via /ec2/getCamerasEx or
     *     /ec2/getCameras?extraFormatting) or MAC address (not supported for certain cameras).
     * %param:string name Caption of the bookmark.
     * %param[opt]:string  description Details of the bookmark.
     * %param[opt]:integer timeout Time during which the recorded period should be preserved (in
     *     milliseconds).
     * %param:integer startTime Start time of the bookmark (in milliseconds since epoch).
     * %param:integer duration Length of the bookmark (in milliseconds).
     * %param[opt]:string tag Applied tag. Serveral tag parameters could be used to specify
     *     multiple tags.
     * %param[proprietary]:option local If present, the request should not be redirected to another
     *     server.
     * %param[proprietary]:option extraFormatting If present and the requested result format is
     *     non-binary, indentation and spacing will be used to improve readability.
     * %param[default]:enum format
     */
    reg("ec2/bookmarks", new QnMultiserverBookmarksRestHandler(serverModule(), "ec2/bookmarks"));

    reg("api/mergeLdapUsers", new QnMergeLdapUsersRestHandler());

    /**%apidoc GET /ec2/updateInformation
     * Retrieves a currently present or specified via a parameter update information manifest.
     * %param[opt]:string version If present, Media Server makes an attempt to retrieve an update
     *      manifest for the specified version id from the dedicated updates server and return it
     *      as a result.
     * %return:object JSON with the update manifest.
     */
    reg("ec2/updateInformation", new QnUpdateInformationRestHandler(&serverModule()->settings(),
        commonModule()->engineVersion()));

    /**%apidoc POST /ec2/startUpdate
     * Starts an update process.
     * %param:object JSON with the update manifest. This JSON might be requested with the
     *     /ec2/updateInformation?version=versionId request.
     */
    reg("ec2/startUpdate", new QnStartUpdateRestHandler(serverModule()));

    /**%apidoc POST /ec2/finishUpdate
     * Puts a system in the 'Update Finished' state.
     * %param[opt]:option ignorePendingPeers Force an update process completion regardless actual
     *     peers state.
     */
    reg("ec2/finishUpdate", new QnFinishUpdateRestHandler(serverModule()));

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
        [this]() { m_installUpdateRequestReceived = true; }));

    /**%apidoc POST /ec2/cancelUpdate
     * Puts a system in the 'Idle' state update-wise. This means that the current update
     * manifest is cleared and all downloads are cancelled.
     */
    reg("ec2/cancelUpdate", new QnCancelUpdateRestHandler(serverModule()));

    /**%apidoc GET /ec2/cameraThumbnail
     * Get the static image from the camera.
     * %param:string cameraId Camera id (can be obtained from "id" field via /ec2/getCamerasEx or
     *     /ec2/getCameras?extraFormatting) or MAC address (not supported for certain cameras).
     * %param[opt]:string time Timestamp of the requested image (in milliseconds since epoch).<br/>
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
     *     (the default value) which implies autosizing: if the height is specified, the width will
     *     be calculated based on the aspect ratio, otherwise, the original frame size will be
     *     used.
     * %param[opt]:enum imageFormat Format of the requested image. Default value is "JpgFormat".
     *     %value png PNG
     *     %value jpg JPEG
     *     %value tif TIFF
     *     %value raw Raw video frame. Makes the request much more lightweight for Edge servers.
     * %param[opt]:enum roundMethod Getting a thumbnail at the exact timestamp is costly, so, it
     *     can be rounded to the nearest keyframe, thus, the default value is
     *     "KeyFrameAfterMethod".
     *     %value before Get the thumbnail from the nearest keyframe before the given time.
     *     %value precise Get the thumbnail as near to given time as possible.
     *     %value after Get the thumbnail from the nearest keyframe after the given time.
     * %param[opt]:enum streamSelectionMode Policy for stream selection. Default value is "auto".
     *     %value auto Chooses the most suitable stream automatically.
     *     %value forcedPrimary Primary stream is forced. Secondary stream will be used if the
     *         primary one is not available.
     *     %value forcedSecondary Secondary stream is forced. Primary stream will be used if the
     *         secondary one is not available.
     *     %value sameAsAnalytics Use the same stream as the one used by analytics engine.
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

    reg("ec2/statistics", new QnMultiserverStatisticsRestHandler("ec2/statistics"));

    /**%apidoc GET /ec2/analyticsLookupDetectedObjects
     * Search analytics DB for objects that match filter specified.
     * %param[opt] deviceId Id of camera.
     * %param[opt] objectTypeId Analytics object type id.
     * %param[opt] objectId Analytics object id.
     * %param[opt] startTime Milliseconds since epoch (1970-01-01 00:00, UTC).
     * %param[opt] endTime Milliseconds since epoch (1970-01-01 00:00, UTC).
     * %param[opt] x1 Top left "x" coordinate of picture bounding box to search within. In range
     *     [0.0; 1.0].
     * %param[opt] y1 Top left "y" coordinate of picture bounding box to search within. In range
     *     [0.0; 1.0].
     * %param[opt] x2 Bottom right "x" coordinate of picture bounding box to search within. In
     *     range [0.0; 1.0].
     * %param[opt] y2 Bottom right "y" coordinate of picture bounding box to search within. In
     *     range [0.0; 1.0].
     * %param[opt] freeText Text to match within object's properties.
     * %param[opt] limit Maximum number of objects to return.
     * %param[opt] maxTrackSize Maximum length of elements of object's track.
     * %param[opt] sortOrder Sort order of objects by track start timestamp.
     *     %value asc Ascending order.
     *     %value desc Descending order.
     * %param[opt] isLocal If "false" then request is forwarded to every other online server and
     *     results are merged. Otherwise, request is processed on receiving server only.
     * %return JSON data.
     */
    reg("ec2/analyticsLookupDetectedObjects", new QnMultiserverAnalyticsLookupDetectedObjects(
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
     *         %param reply[].actions[].actionIds List of action ids (strings).
     *         %param reply[].actions[].pluginId Id of a analytics plugin which offers the actions.
     */
    reg("api/getAnalyticsActions", new QnGetAnalyticsActionsRestHandler());


    /**%apidoc POST /api/executeAnalyticsAction
     * Execute analytics action from the particular analytics plugin on this server. The action is
     * applied to the specified metadata object.
     * %param engineId Id of an Analytics Engine which offers the Action.
     * %param actionId Id of an Action to execute.
     * %param objectId Id of an Analytics Object to which the Action is applied.
     * %param deviceId Id of a Device from which the Action was triggered.
     * %param timestampUs Timestamp (microseconds) of the video frame from which the action was
     *     triggered.
     * %param params JSON object with key-value pairs containing values for the Action params
     *     described in the Engine manifest.
     * %return:object JSON object with an error code, error string, and the reply on success.
     *     %param:string error Error code, "0" means no error.
     *     %param:string errorString Error message in English, or an empty string.
     *     %param:object reply Result returned by the executed action.
     *         %param reply.actionUrl If not empty, provides a URL composed by the plugin, to be
     *             opened by the Client in an embedded browser.
     *         %param reply.messageToUser If not empty, provides a message composed by the plugin,
     *             to be shown to the user who triggered the action.
     */
    reg("api/executeAnalyticsAction", new QnExecuteAnalyticsActionRestHandler(serverModule()));

    /**%apidoc POST /api/executeEventAction
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
     * %permissions Owner.
     */
    reg("api/debug", new QnDebugHandler(), kAdmin);

    reg("api/startLiteClient", new QnStartLiteClientRestHandler(serverModule()));

    #if defined(_DEBUG)
        reg("api/debugEvent", new QnDebugEventsRestHandler(serverModule()));
    #endif

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
     *     %param:string description Setiing description.
     */
    reg("api/settingsDocumentation", new QnSettingsDocumentationHandler(&serverModule()->settings()));

    /**%apidoc GET /ec2/analyticsEngineSettings
     * Return settings values of the specified Engine.
     * %return:object TODO:CHECK JSON object consisting of name-value settings pairs.
     *      %param:string engineId Id of analytics engine.
     *
     * %apidoc POST /ec2/analyticsEngineSettings
     * Applies passed settings values to correspondent Analytics Engine.
     * %param:string engineId Unique id of Analytics Engine.
     * %param settings JSON object consisting of name-value settings pairs.
     */
    reg("ec2/analyticsEngineSettings",
        new nx::vms::server::rest::AnalyticsEngineSettingsHandler(serverModule()));

    /**%apidoc GET /ec2/deviceAnalyticsSettings
     * Return settings values of the specified device-engine pair.
     * %return:object TODO:CHECK JSON object consisting of name-value settings pairs.
     *      %param:string engineId Unique id of an Analytics Engine.
     *      %param:string deviceId Id of a device.
     *
     * %apidoc POST /ec2/deviceAnalyticsSettings
     * Applies passed settings values to the correspondent device-engine pair.
     * %param:string engineId Unique id of an Analytics Engine.
     * %param:string deviceId Id of a device.
     * %param settings JSON object consisting of name-value settings pairs.
     */
    reg("ec2/deviceAnalyticsSettings",
        new nx::vms::server::rest::DeviceAnalyticsSettingsHandler(serverModule()));

    reg(
        nx::network::http::Method::options,
        QnRestProcessorPool::kAnyPath,
        new OptionsRequestHandler());
}

void MediaServerProcess::reg(
    const QString& path,
    QnRestRequestHandler* handler,
    GlobalPermission permission)
{
    reg(QnRestProcessorPool::kAnyHttpMethod, path, handler, permission);
}

void MediaServerProcess::reg(
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
    TimeBasedNonceProvider* timeBasedNonceProvider,
    nx::vms::cloud_integration::CloudManagerGroup* const cloudManagerGroup,
    ec2::LocalConnectionFactory* ec2ConnectionFactory)
{
    auto messageBus = ec2ConnectionFactory->messageBus();
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
    regTcp<QnRtspConnectionProcessor>("RTSP", "*", serverModule());
    regTcp<QnRestConnectionProcessor>("HTTP", "api");
    regTcp<QnRestConnectionProcessor>("HTTP", "ec2");
    regTcp<QnRestConnectionProcessor>("HTTP", "favicon.ico");
    regTcp<QnFileConnectionProcessor>("HTTP", "static");
    regTcp<QnCrossdomainConnectionProcessor>("HTTP", "crossdomain.xml");
    regTcp<QnProgressiveDownloadingConsumer>("HTTP", "media", serverModule());
    regTcp<QnIOMonitorConnectionProcessor>("HTTP", "api/iomonitor");

    nx::vms::server::hls::HttpLiveStreamingProcessor::setMinPlayListSizeToStartStreaming(
        serverModule()->settings().hlsPlaylistPreFillChunks());
    regTcp<nx::vms::server::hls::HttpLiveStreamingProcessor>("HTTP", "hls", serverModule());

    // Our HLS uses implementation uses authKey (generated by target server) to skip authorization,
    // to keep this warning we should not ask for authorization along the way.
    m_universalTcpListener->enableUnauthorizedForwarding("hls");

    //regTcp<QnDefaultTcpConnectionProcessor>("HTTP", "*");

    regTcp<nx::vms::network::ProxyConnectionProcessor>("*", "proxy", ec2ConnectionFactory->serverConnector());
    regTcp<QnAudioProxyReceiver>("HTTP", "proxy-2wayaudio", serverModule());

    if( !serverModule()->settings().authenticationEnabled())
        m_universalTcpListener->disableAuth();

    #if defined(ENABLE_DESKTOP_CAMERA)
        regTcp<QnDesktopCameraRegistrator>("HTTP", "desktop_camera", serverModule());
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

void MediaServerProcess::prepareOsResources()
{
    auto rootToolPtr = serverModule()->rootFileSystem();
    if (!rootToolPtr->changeOwner(nx::kit::IniConfig::iniFilesDir()))
        qWarning().noquote() << "Unable to chown" << nx::kit::IniConfig::iniFilesDir();

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
        nx::network::upnp::PortMapper::DEFAULT_CHECK_MAPPINGS_INTERVAL,
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
    for (;;)
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
    std::cout << "Got hwID \"" << hwIdString.toStdString() << "\"" << std::endl;
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
    s.updateDirectoryIfEmpty(settings.dataDir());

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

    if (auto path = mainLogger()->filePath())
        roSettings->setValue("logFile", path->replace(".log", ""));
    else
        roSettings->remove("logFile");

    logSettings = makeLogSettings(settings);
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
        qDebug() << "Can't save guid. Run once as administrator.";
        NX_ERROR(this, "Can't save guid. Run once as administrator.");
        qApp->quit();
        return;
    }
}

void MediaServerProcess::connectArchiveIntegrityWatcher()
{
    using namespace nx::vms::server;
    auto serverArchiveIntegrityWatcher = static_cast<ServerArchiveIntegrityWatcher*>(
        serverModule()->archiveIntegrityWatcher());

    connect(
        serverArchiveIntegrityWatcher,
        &ServerArchiveIntegrityWatcher::fileIntegrityCheckFailed,
        serverModule()->eventConnector(),
        &event::EventConnector::at_fileIntegrityCheckFailed);
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
        QnAppInfo::productNameShort(),
        QnAppInfo::customizationName());
}

void MediaServerProcess::setSetupModuleCallback(std::function<void(QnMediaServerModule*)> callback)
{
    m_setupModuleCallback = std::move(callback);
}

bool MediaServerProcess::setUpMediaServerResource(
    CloudIntegrationManager* cloudIntegrationManager,
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
            server->setId(serverGuid);
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
        server->setSystemInfo(QnAppInfo::currentSystemInformation());
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


    connect(m_mediaServer.get(), &QnMediaServerResource::metadataStorageIdChanged, this,
        &MediaServerProcess::at_metadataStorageIdChanged);

    return foundOwnServerInDb;
}

void MediaServerProcess::stopObjects()
{
    auto safeDisconnect =
        [this](QObject* src, QObject* dst)
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
        nx::utils::TimerManager::instance()->joinAndDeleteTimer(dumpSystemResourceUsageTaskID);

    commonModule()->setNeedToStop(true);
    m_universalTcpListener->stop();
    serverModule()->stop();

    m_generalTaskTimer.reset();
    m_udtInternetTrafficTimer.reset();
    m_createDbBackupTimer.reset();

    safeDisconnect(commonModule()->globalSettings(), this);
    safeDisconnect(m_universalTcpListener->authenticator(), this);
    safeDisconnect(commonModule()->resourceDiscoveryManager(), this);
    safeDisconnect(serverModule()->normalStorageManager(), this);
    safeDisconnect(serverModule()->backupStorageManager(), this);
    safeDisconnect(commonModule(), this);
    safeDisconnect(commonModule()->runtimeInfoManager(), this);
    if (m_ec2Connection)
        safeDisconnect(m_ec2Connection->timeNotificationManager().get(), this);
    safeDisconnect(m_ec2Connection.get(), this);
    safeDisconnect(m_updatePiblicIpTimer.get(), this);
    m_updatePiblicIpTimer.reset();
    safeDisconnect(m_ipDiscovery.get(), this);
    safeDisconnect(commonModule()->moduleDiscoveryManager(), this);

    WaitingForQThreadToEmptyEventQueue waitingForObjectsToBeFreed(QThread::currentThread(), 3);
    waitingForObjectsToBeFreed.join();

    m_discoveryMonitor.reset();
    m_crashReporter.reset();
    m_ipDiscovery.reset(); // stop it before IO deinitialized
    m_multicastHttp.reset();

    if (const auto manager = commonModule()->moduleDiscoveryManager())
        manager->stop();

    if (m_universalTcpListener)
        m_universalTcpListener.reset();

    serverModule()->resourceCommandProcessor()->stop();
    if (m_initStoragesAsyncPromise)
        m_initStoragesAsyncPromise->get_future().wait();
    // todo: #rvasilenko some undeleted resources left in the QnMain event loop. I stopped TimerManager as temporary solution for it.
    nx::utils::TimerManager::instance()->stop();

    // Remove all stream recorders.
    m_remoteArchiveSynchronizer.reset();

    m_mserverResourceSearcher.reset();

    commonModule()->resourceDiscoveryManager()->stop();
    serverModule()->analyticsManager()->stop(); //< Stop processing analytics events.
    serverModule()->pluginManager()->unloadPlugins();

    //since mserverResourceDiscoveryManager instance is dead no events can be delivered to serverResourceProcessor: can delete it now
    //TODO refactoring of discoveryManager <-> resourceProcessor interaction is required
    m_serverResourceProcessor.reset();

    serverModule()->resourcePool()->threadPool()->waitForDone();

    m_ec2ConnectionFactory->shutdown();
    commonModule()->deleteMessageProcessor(); // stop receiving notifications

    commonModule()->resourcePool()->clear();


    //disconnecting from EC2
    QnAppServerConnectionFactory::setEc2Connection(ec2::AbstractECConnectionPtr());

    m_cloudIntegrationManager.reset();
    m_mediaServerStatusWatcher.reset();
    m_timeBasedNonceProvider.reset();
    m_ec2Connection.reset();
    m_ec2ConnectionFactory.reset();

    // This method will set flag on message channel to threat next connection close as normal
    //appServerConnection->disconnectSync();
    serverModule()->setLastRunningTime(std::chrono::milliseconds::zero());

    if (m_mediaServer)
        m_mediaServer->beforeDestroy();
    m_mediaServer.clear();

    performActionsOnExit();

    nx::network::SocketGlobals::cloud().outgoingTunnelPool().clearOwnPeerIdIfEqual(
        "ms", commonModule()->moduleGUID());

    m_autoRequestForwarder.reset();
    m_audioStreamerPool.reset();
    m_upnpPortMapper.reset();

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
    runtimeData.box = QnAppInfo::armBox();
    runtimeData.brand = QnAppInfo::productNameShort();
    runtimeData.customization = QnAppInfo::customizationName();
    runtimeData.platform = QnAppInfo::applicationPlatform();

    if (QnAppInfo::isNx1())
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
        nx::utils::AppInfo::productName().toUtf8(), "US",
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
        auto settings = &serverModule()->settings();
        storagePlugins->registerStoragePlugin(
            storagePlugin->storageType(),
            std::bind(
                &QnThirdPartyStorageResource::instance,
                std::placeholders::_1,
                std::placeholders::_2,
                storagePlugin,
                settings
            ),
            false);
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

    m_generalTaskTimer = std::make_unique<QTimer>();
    connect(m_generalTaskTimer.get(), SIGNAL(timeout()), this, SLOT(at_timer()), Qt::DirectConnection);

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

    m_createDbBackupTimer = std::make_unique<QTimer>();
    connect(
        m_createDbBackupTimer.get(),
        &QTimer::timeout,
        [firstRun = true, this]() mutable
        {
            auto utils = nx::vms::server::Utils(serverModule());
            utils.backupDatabase();
            if (firstRun)
            {
                m_createDbBackupTimer->start(serverModule()->settings().dbBackupPeriodMS().count());
                firstRun = false;
            }
        });

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
        return false;
    }

    if (!m_oldAnalyticsStoragePath.isEmpty())
    {
        auto policy = commonModule()->globalSettings()->metadataStorageChangePolicy();
        if (policy == nx::vms::api::MetadataStorageChangePolicy::remove)
        {
            if (!QFile::remove(m_oldAnalyticsStoragePath))
                NX_WARNING(this, "Can not remove database %1", m_oldAnalyticsStoragePath);
        }
    }

    m_oldAnalyticsStoragePath = settings.dbConnectionOptions.dbName;
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

void MediaServerProcess::startObjects()
{
    QTimer::singleShot(3000, this, SLOT(at_connectionOpened()));
    QTimer::singleShot(0, this, SLOT(at_appStarted()));

    at_timer();
    m_generalTaskTimer->start(QnVirtualCameraResource::issuesTimeoutMs());
    m_udtInternetTrafficTimer->start(UDT_INTERNET_TRAFIC_TIMER);

    int64_t initialBackupDbPeriodMs;
    const auto lastDbBackupTimestamp =
        nx::vms::server::Utils(serverModule()).lastDbBackupTimestamp();

    if (!lastDbBackupTimestamp)
    {
        initialBackupDbPeriodMs = 0; //< If no backup files so far, let's create one now.
    }
    else
    {
        const int64_t dbBackupPeriodMS = serverModule()->settings().dbBackupPeriodMS().count();
        initialBackupDbPeriodMs = std::max<int64_t>(
            *lastDbBackupTimestamp + dbBackupPeriodMS - qnSyncTime->currentMSecsSinceEpoch(),
            0LL);
    }
    m_createDbBackupTimer->start(initialBackupDbPeriodMs);

    const bool isDiscoveryDisabled = serverModule()->settings().noResourceDiscovery();

    serverModule()->resourceCommandProcessor()->start();
    if (m_ec2Connection->connectionInfo().ecUrl.scheme() == "file" && !isDiscoveryDisabled)
        commonModule()->moduleDiscoveryManager()->start();

    if (!m_ec2Connection->connectionInfo().ecDbReadOnly && !isDiscoveryDisabled)
        commonModule()->resourceDiscoveryManager()->start();

    serverModule()->recordingManager()->start();
    if (!isDiscoveryDisabled)
        m_mserverResourceSearcher->start();
    m_universalTcpListener->start();
    serverModule()->serverConnector()->start();
    serverModule()->backupStorageManager()->scheduleSync()->start();
    serverModule()->unusedWallpapersWatcher()->start();
    if (m_serviceMode)
        serverModule()->licenseWatcher()->start();

    commonModule()->messageProcessor()->init(commonModule()->ec2Connection()); // start receiving notifications
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
        "appserverPassword is not emptyu in registry. Restart the server as Administrator");
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
    NX_INFO(this, lm("Max TCP connections from server= %1").arg(maxConnections));

    // Accept SSL connections in all cases as it is always in use by cloud modules and old clients,
    // config value only affects server preference listed in moduleInformation.
    const bool acceptSslConnections = true;

    m_universalTcpListener = std::make_unique<QnUniversalTcpListener>(
        commonModule(),
        QHostAddress::Any,
        serverModule()->settings().port(),
        maxConnections,
        acceptSslConnections);
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

static QByteArray loadDataFromFile(const QString& fileName)
{
    QFile file(fileName);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        return file.readAll();
    return QByteArray();
}

static QByteArray loadDataFromUrl(nx::utils::Url url)
{
    auto httpClient = std::make_unique<nx::network::http::HttpClient>();
    httpClient->setResponseReadTimeout(kResourceDataReadingTimeout);
    if (httpClient->doGet(url)
        && httpClient->response()->statusLine.statusCode == nx::network::http::StatusCode::ok)
    {
        const auto value = httpClient->fetchEntireMessageBody();
        if (value)
            return *value;
    }
    return QByteArray();
}

void MediaServerProcess::loadResourceParamsData()
{
    const std::array<const char*,2> kUrlsToLoadResourceData =
    {
        "http://resources.vmsproxy.com/resource_data.json",
        "http://beta.vmsproxy.com/beta-builds/daily/resource_data.json"
    };

    auto manager = m_ec2Connection->getResourceManager(Qn::kSystemAccess);

    using namespace nx::vms::api;
    QString source;
    ResourceParamWithRefData param;
    param.name = Qn::kResourceDataParamName;

    nx::utils::SoftwareVersion dataVersion;
    QString oldValue;
    nx::vms::api::ResourceParamWithRefDataList data;
    manager->getKvPairsSync(QnUuid(), &data);
    for (const auto& param: data)
    {
        if (param.name == Qn::kResourceDataParamName)
        {
            oldValue = param.value;
            dataVersion = QnResourceDataPool::getVersion(param.value.toUtf8());
            break;
        }
    }

    static const QString kBuiltinFileName(":/resource_data.json");
    const auto builtinVersion = QnResourceDataPool::getVersion(loadDataFromFile(kBuiltinFileName));
    if (builtinVersion > dataVersion)
    {
        dataVersion = builtinVersion;
        source = kBuiltinFileName;
        param.value = loadDataFromFile(source); //< Default value.
    }

    for (const auto& url: kUrlsToLoadResourceData)
    {
        const auto internetValue = loadDataFromUrl(url);
        QString internetVersion;
        if (!internetValue.isEmpty())
        {
            const auto internetVersion = QnResourceDataPool::getVersion(internetValue);
            if (internetVersion > dataVersion)
            {
                if (serverModule()->commonModule()->resourceDataPool()->validateData(internetValue))
                {
                    param.value = internetValue;
                    source = url;
                    dataVersion = internetVersion;
                    break;
                }
                else
                {
                    NX_WARNING(this, "Skip invalid resource_data.json from %1", internetValue);
                }
            }
            else
            {
                NX_DEBUG(this, "Skip internet resource_data.json. Current version %1, internet %2", dataVersion, internetVersion);
            }
        }
    }

    if (!param.value.isEmpty() && oldValue != param.value)
    {
        NX_INFO(this, "Update system wide resource_data.json from %1", source);

        // Update data in the database if there is no value or get update from the HTTP request.
        ResourceParamWithRefDataList params;
        params.push_back(param);
        manager->saveSync(params);
    }

    const auto externalResourceFileName =
        QCoreApplication::applicationDirPath() + "/resource_data.json";
    auto externalFile = loadDataFromFile(externalResourceFileName);
    if (!externalFile.isEmpty())
    {
        // Update local data only without saving to DB if external static file is defined.
        NX_INFO(this, "Update local resource_data.json from %1", externalResourceFileName);
        param.value = externalFile;
        ResourceParamWithRefDataList params;
        params.push_back(param);
        manager->saveSync(params);
        m_serverMessageProcessor->resetPropertyList(params);
    }
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

    std::shared_ptr<QnMediaServerModule> serverModule(new QnMediaServerModule(
        &m_cmdLineArguments,
        std::move(serverSettings)));

    m_serverModule = serverModule;

    m_platform->setServerModule(serverModule.get());
    serverModule->setPlatform(m_platform.get());
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
        NX_ERROR(this, "Stopping media server because can't open database");
        return;
    }

    auto utils = nx::vms::server::Utils(serverModule.get());
    if (utils.timeToMakeDbBackup())
    {
        utils.backupDatabase(ec2::detail::QnDbManager::ecsDbFileName(
            serverModule->settings().dataDir()),
            ec2::detail::QnDbManager::currentBuildNumber(appServerConnectionUrl().toLocalFile()));
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
        m_ec2ConnectionFactory->serverConnector());

    m_ec2ConnectionFactory->registerTransactionListener(m_universalTcpListener.get());

    const bool foundOwnServerInDb = setUpMediaServerResource(
        m_cloudIntegrationManager.get(), serverModule.get(), m_ec2Connection);

    writeMutableSettingsData();

    if (needToStop())
        return;

    serverModule->serverUpdateTool()->removeUpdateFiles(m_mediaServer->getVersion().toString());

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

    m_dumpSystemResourceUsageTaskId = nx::utils::TimerManager::instance()->addTimer(
        std::bind(&MediaServerProcess::dumpSystemUsageStats, this),
        std::chrono::milliseconds(SYSTEM_USAGE_DUMP_TIMEOUT));

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

    QString dataLocation = serverModule()->settings().dataDir();
    QDir stateDirectory;
    stateDirectory.mkpath(dataLocation + QLatin1String("/state"));
    serverModule()->fileDeletor()->init(dataLocation + QLatin1String("/state")); // constructor got root folder for temp files
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
            serviceMainInstance->stopSync();
    }

private:
    int m_argc;
    char **m_argv;
    QScopedPointer<MediaServerProcess> m_main;
};

void stopServer(int /*signal*/)
{
    gRestartFlag = false;
    if (serviceMainInstance) {
        //output to console from signal handler can cause deadlock
        //qWarning() << "got signal" << signal << "stop server!";
        serviceMainInstance->stopAsync();
    }
}

void restartServer(int restartTimeout)
{
    gRestartFlag = true;
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
    nx::kit::OutputRedirector::ensureOutputRedirection();

    nx::utils::rlimit::setMaxFileDescriptors(32000);

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
