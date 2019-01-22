#include "local_connection_factory.h"

#include <functional>

#include <api/global_settings.h>

#include <nx/utils/thread/mutex.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/address_resolver.h>
#include <nx/network/deprecated/simple_http_client.h>

#include <utils/common/app_info.h>
#include <nx/utils/concurrent.h>
#include <network/http_connection_listener.h>
#include <nx_ec/ec_proto_version.h>
#include <nx/vms/api/data/access_rights_data.h>
#include <nx/vms/api/data/discovery_data.h>
#include <nx/vms/api/data/user_role_data.h>
#include <nx/vms/api/data/camera_history_data.h>
#include <nx/vms/api/data/media_server_data.h>

#include <rest/handlers/active_connections_rest_handler.h>
#include "compatibility/old_ec_connection.h"
#include "ec2_connection.h"
#include "ec2_thread_pool.h"
#include "remote_ec_connection.h"
#include <rest/handlers/ec2_base_query_http_handler.h>
#include <rest/handlers/ec2_update_http_handler.h>
#include "rest/server/rest_connection_processor.h"
#include "rest/request_type_wrappers.h"
#include "client_registrar.h"
#include "transaction/transaction.h"
#include "transaction/transaction_message_bus.h"
#include "http/ec2_transaction_tcp_listener.h"
#include "http/http_transaction_receiver.h"
#include "mutex/distributed_mutex_manager.h"
#include <http/p2p_connection_listener.h>
#include <transaction/message_bus_adapter.h>
#include "server_query_processor.h"
#include <nx/p2p/p2p_message_bus.h>
#include <transaction/json_transaction_serializer.h>
#include <transaction/ubjson_transaction_serializer.h>

#include <nx/p2p/p2p_server_message_bus.h>
#include <transaction/server_transaction_message_bus.h>
#include <nx/vms/time_sync/server_time_sync_manager.h>

using namespace nx::vms::api;

namespace ec2 {

static const char* const kIncomingTransactionsPath = "ec2/forward_events";

LocalConnectionFactory::LocalConnectionFactory(
    QnCommonModule* commonModule,
    PeerType peerType,
    bool isP2pMode,
    bool ecDbReadOnly,
    QnHttpConnectionListener* tcpListener)
    :
    AbstractECConnectionFactory(commonModule),
    // dbmanager is initialized by direct connection.

    m_jsonTranSerializer(new QnJsonTransactionSerializer()),
    m_ubjsonTranSerializer(new QnUbjsonTransactionSerializer()),
    m_serverConnector(new nx::vms::network::ReverseConnectionManager(tcpListener)),
    m_timeSynchronizationManager(new nx::vms::time_sync::ServerTimeSyncManager(
        commonModule, m_serverConnector.get())),
    m_terminated(false),
    m_runningRequests(0),
    m_sslEnabled(false),
    m_p2pMode(isP2pMode),
    m_ecDbReadOnly(ecDbReadOnly)
{
    if (peerType == PeerType::server)
    {
        m_dbManager.reset(new detail::QnDbManager(commonModule));
        m_transactionLog.reset(new QnTransactionLog(m_dbManager.get(), m_ubjsonTranSerializer.get()));
    }

    m_bus.reset(new TransactionMessageBusAdapter(
        commonModule,
        m_jsonTranSerializer.get(),
        m_ubjsonTranSerializer.get()));

    if (m_p2pMode)
    {
        auto messageBus = m_bus->init<nx::p2p::ServerMessageBus>(peerType);
        messageBus->setDatabase(m_dbManager.get());
    }
    else
    {
        auto messageBus = m_bus->init<ec2::ServerTransactionMessageBus>(peerType);
        messageBus->setDatabase(m_dbManager.get());
        m_distributedMutexManager.reset(new QnDistributedMutexManager(messageBus));
    }

    m_serverQueryProcessor.reset(new ServerQueryProcessorAccess(m_dbManager.get(), m_bus.get()));

    m_clientRegistrar = std::make_unique<ClientRegistrar>(
        m_bus.get(),
        commonModule->runtimeInfoManager());

    m_dbManager->setTransactionLog(m_transactionLog.get());

    // Cannot be done in TimeSynchronizationManager constructor to keep valid object destruction
    // order.
    // TODO: #Elric #EC2 register in a proper place!
    // Registering ec2 types with Qt meta-type system.
    qRegisterMetaType<QnTransactionTransportHeader>("QnTransactionTransportHeader");
}

void LocalConnectionFactory::shutdown()
{
    m_serverQueryProcessor->stop();

    pleaseStop();
    join();

    if (m_directConnection)
        messageBus()->removeHandler(m_directConnection->notificationManager());
}

LocalConnectionFactory::~LocalConnectionFactory()
{
}

void LocalConnectionFactory::pleaseStop()
{
    QnMutexLocker lk(&m_mutex);
    m_terminated = true;
}

void LocalConnectionFactory::join()
{
    // Cancelling all ongoing requests.
}

// Implementation of AbstractECConnectionFactory::testConnectionAsync
int LocalConnectionFactory::testConnectionAsync(
    const nx::utils::Url &addr,
    impl::TestConnectionHandlerPtr handler)
{
    nx::utils::Url url = addr;
    url.setUserName(url.userName().toLower());

    QUrlQuery query(url.toQUrl());
    query.removeQueryItem(lit("format"));
    query.addQueryItem(lit("format"), QnLexical::serialized(Qn::JsonFormat));
    url.setQuery(query);

    return testDirectConnection(url, handler);
}

// Implementation of AbstractECConnectionFactory::connectAsync
int LocalConnectionFactory::connectAsync(
    const nx::utils::Url& addr,
    const nx::vms::api::ClientInfoData& clientInfo,
    impl::ConnectHandlerPtr handler)
{
    return establishDirectConnection(addr, handler);
}

void LocalConnectionFactory::registerTransactionListener(
    QnHttpConnectionListener* httpConnectionListener)
{
    if (auto bus = m_bus->dynamicCast<ServerTransactionMessageBus*>())
    {
        httpConnectionListener->addHandler<QnTransactionTcpProcessor, ServerTransactionMessageBus*>(
            "HTTP", "ec2/events", bus);
        httpConnectionListener->addHandler<QnHttpTransactionReceiver, ServerTransactionMessageBus*>(
            "HTTP", kIncomingTransactionsPath, bus);
    }
    else if (auto bus = m_bus->dynamicCast<nx::p2p::MessageBus*>())
    {
        httpConnectionListener->addHandler<nx::p2p::ConnectionProcessor>(
            "HTTP", QnTcpListener::normalizedPath(nx::p2p::ConnectionBase::kDeprecatedUrlPath));
        httpConnectionListener->addHandler<nx::p2p::ConnectionProcessor>(
            "HTTP", QnTcpListener::normalizedPath(nx::p2p::ConnectionBase::kWebsocketUrlPath));
        httpConnectionListener->addHandler<nx::p2p::ConnectionProcessor>(
            "HTTP", QnTcpListener::normalizedPath(nx::p2p::ConnectionBase::kHttpUrlPath));
    }

    m_sslEnabled = httpConnectionListener->isSslEnabled();
}

void LocalConnectionFactory::registerRestHandlers(QnRestProcessorPool* const p)
{
    using namespace std::placeholders;
    using namespace nx::vms::api;

    /**%apidoc GET /ec2/getResourceTypes
     * Read all resource types. Resource type contains object type such as
     * "Server", "Camera", etc. Also, resource type contains additional information
     * for cameras such as maximum FPS, resolution, etc.
     * %param[default] format
     * %return Object in the requested format.
     * %// AbstractResourceManager::getResourceTypes
     */
    regGet<std::nullptr_t, ResourceTypeDataList>(p, ApiCommand::getResourceTypes);

    /**%apidoc[proprietary] GET /ec2/getSystemMergeHistory
     * Return information about previous system merges.
     */
    regGet<std::nullptr_t, SystemMergeHistoryRecordList>(p, ApiCommand::getSystemMergeHistory);

    /**%apidoc[proprietary] POST /ec2/setResourceStatus
     * Change a resource status.
     * <p>
     * Parameters should be passed as a JSON object in POST message body with
     * content type "application/json". Example of such object can be seen in
     * the result of the corresponding GET function.
     * </p>
     * %permissions Administrator, or a custom user with "Edit camera settings" permission,
     *     or a user who owns the resource in case the resource is a layout.
     * %param id Unique id of the resource.
     * %param status new resource status.
     */
    regUpdate<ResourceStatusData>(p, ApiCommand::setResourceStatus);

    /**%apidoc GET /ec2/getResourceParams
     * Read resource (camera, user or server) additional parameters (camera firmware version, etc).
     * The list of parameters depends on the resource type.
     * %param[default] format
     * %param id Resource unique id.
     * %return Object in the requested format.
     * %// AbstractResourceManager::getKvPairs
     */
    regGet<QnUuid, ResourceParamWithRefDataList>(p, ApiCommand::getResourceParams);

    // AbstractResourceManager::save
    regUpdate<ResourceParamWithRefDataList>(p, ApiCommand::setResourceParams);

    /**%apidoc POST /ec2/removeResource
     * Delete the resource.
     * <p>
     * Parameters should be passed as a JSON object in POST message body with
     * content type "application/json". Example of such object can be seen in
     * the result of the corresponding GET function.
     * </p>
     * %permissions Administrator, or a custom user with "Edit camera settings" permission,
     *     or a user who owns the resource in case the resource is a layout.
     * %param id Unique id of the resource.
     * %// AbstractResourceManager::remove
     */
    regUpdate<IdData>(p, ApiCommand::removeResource);

    /**%apidoc GET /ec2/getStatusList
     * Read current status of the resources: cameras, servers and storages.
     * %param[default] format
     * %param[opt] id Resource unique id. If omitted, return data for all resources.
     * %return List of status objects in the requested format.
     *     %// TODO: Describe ResourceStatus fields.
     */
    regGet<QnUuid, ResourceStatusDataList>(p, ApiCommand::getStatusList);

    // AbstractMediaServerManager::getServers
    regGet<QnUuid, MediaServerDataList>(p, ApiCommand::getMediaServers);

    // AbstractMediaServerManager::save
    regUpdate<MediaServerData>(p, ApiCommand::saveMediaServer);

    /**%apidoc POST /ec2/saveMediaServerUserAttributes
     * Save additional attributes of a server.
     * <p>
     * Parameters should be passed as a JSON object in POST message body with
     * content type "application/json". Example of such object can be seen in
     * the result of the corresponding GET function.
     * </p>
     * %permissions Administrator.
     * %param serverId Server unique id. If such object exists, omitted fields will not be changed.
     * %param serverName Server name.
     * %param maxCameras Maximum number of cameras on the server.
     * %param allowAutoRedundancy Whether the server can take cameras from
     *     an offline server automatically.
     *     %value false
     *     %value true
     * %param backupType Settings for storage redundancy.
     *     %value Backup_Manual Backup is performed only at a user's request.
     *     %value Backup_RealTime Backup is performed during recording.
     *     %value Backup_Schedule Backup is performed on schedule.
     * %param backupDaysOfTheWeek Combination (via "|") of the days of week on which the backup is
     *     active.
     *     %value Monday
     *     %value Tuesday
     *     %value Wednesday
     *     %value Thursday
     *     %value Friday
     *     %value Saturday
     *     %value Sunday
     * %param backupStart Time of day when the backup starts (in seconds passed from 00:00:00).
     * %param backupDuration Duration of the synchronization period (in seconds). -1 if not set.
     * %param backupBitrate Maximum backup bitrate (in bytes per second). Negative value if not
     *     limited.
     * %// AbstractCameraManager::saveUserAttributes
     */
    regUpdate<MediaServerUserAttributesData>(p, ApiCommand::saveMediaServerUserAttributes);

    /**%apidoc:arrayParams POST /ec2/saveMediaServerUserAttributesList
     * Save additional attributes of a number of servers.
     * <p>
     * Parameters should be passed as a JSON array of objects in POST message body with
     * content type "application/json". Example of such object can be seen in
     * the result of the corresponding GET function.
     * </p>
     * %param serverId Server unique id. If such object exists, omitted fields will not be changed.
     * %param serverName Server name.
     * %param maxCameras Maximum number of cameras on the server.
     * %param allowAutoRedundancy Whether the server can take cameras from an offline server
     *     automatically.
     *     %value false
     *     %value true
     * %param backupType Settings for storage redundancy.
     *     %value Backup_Manual Backup is performed only at a user's request.
     *     %value Backup_RealTime Backup is performed during recording.
     *     %value Backup_Schedule Backup is performed on schedule.
     * %param backupDaysOfTheWeek Combination (via "|") of weekdays the backup is active on.
     *     %value Monday
     *     %value Tuesday
     *     %value Wednesday
     *     %value Thursday
     *     %value Friday
     *     %value Saturday
     *     %value Sunday
     * %param backupStart Time of day when the backup starts (in seconds passed from 00:00:00).
     * %param backupDuration Duration of the synchronization period (in seconds). -1 if not set.
     * %param backupBitrate Maximum backup bitrate (in bytes per second). Negative
     *     value if not limited.
     * %// AbstractMediaServerManager::saveUserAttributes
     */
    regUpdate<MediaServerUserAttributesDataList>(p, ApiCommand::saveMediaServerUserAttributesList);

    /**%apidoc GET /ec2/getMediaServerUserAttributesList
    * Read additional media server attributes.
    * %param[default] format
    * %param[opt] id Server unique id. If omitted, return data for all servers.
    * %return List of objects with additional server attributes for all servers, in the requested
    *     format.
    *     %param serverId Server unique id.
    *     %param serverName Server name.
    *     %param maxCameras Maximum number of cameras on the server.
    *     %param allowAutoRedundancy Whether the server can take cameras from
    *         an offline server automatically.
    *         %value false
    *         %value true
    *     %param backupType Settings for storage redundancy.
    *         %value Backup_Manual Backup is performed only at a user's request.
    *         %value Backup_RealTime Backup is performed during recording.
    *         %value Backup_Schedule Backup is performed on schedule.
    *     %param backupDaysOfTheWeek Combination (via "|") of the days of week on which the backup
    *         is active on.
    *         %value Monday
    *         %value Tuesday
    *         %value Wednesday
    *         %value Thursday
    *         %value Friday
    *         %value Saturday
    *         %value Sunday
    *     %param backupStart Time of day when the backup starts (in seconds passed from 00:00:00).
    *     %param backupDuration Duration of the synchronization period in seconds. -1 if not set.
    *     %param backupBitrate Maximum backup bitrate in bytes per second. Negative
    *         value if not limited.
    * %// AbstractMediaServerManager::getUserAttributes
    */
    regGet<QnUuid, MediaServerUserAttributesDataList>(p, ApiCommand::getMediaServerUserAttributesList);

    // AbstractMediaServerManager::remove
    regUpdate<IdData>(p, ApiCommand::removeMediaServer);

    /**%apidoc GET /ec2/getMediaServersEx
     * Return server list.
     * %param[default] format
     * %return Server object in the requested format.
     * %// AbstractMediaServerManager::getServersEx
     */
    regGet<QnUuid, MediaServerDataExList>(p, ApiCommand::getMediaServersEx);

    regUpdate<StorageDataList>(p, ApiCommand::saveStorages);

    /**%apidoc[proprietary] POST /ec2/saveStorage
     * Save the storage.
     * <p>
     * Parameters should be passed as a JSON object in POST message body with
     * content type "application/json". Example of such object can be seen in
     * the result of the corresponding GET function.
     * </p>
     * %permissions Administrator.
     * %param[opt] id Storage unique id. Can be omitted when creating a new object.
     * %param parentId Parent server unique id.
     * %param name Arbitrary resource name (optional)
     * %param url Full storage url (path to the local folder).
     * %param[proprietary] typeId Should have fixed value.
     *     %value {f8544a40-880e-9442-b78a-9da6db6862b4}
     * %param spaceLimit Free space to maintain on the storage,
     *     in bytes. Recommended value is 10 gigabytes for local storages and
     *     100 gigabytes for NAS.
     * %param usedForWriting Whether writing to the storage is allowed.
     *     %value false
     *     %value true
     * %param storageType Type of the method to access the storage.
     *     %value local
     *     %value network (manually mounted NAS)
     *     %value smb (auto mounted NAS)
     * %param addParams List of storage additional parameters. Intended for
     *     internal use; leave empty when creating a new storage.
     * %param isBackup Whether the storage is used for backup.
     *     %value false
     *     %value true
     */
    regUpdate<StorageData>(p, ApiCommand::saveStorage);

    regUpdate<IdDataList>(p, ApiCommand::removeStorages);
    regUpdate<IdData>(p, ApiCommand::removeStorage);
    regUpdate<IdDataList>(p, ApiCommand::removeResources);

    // AbstractCameraManager::addCamera
    regUpdate<CameraData>(p, ApiCommand::saveCamera);

    // AbstractCameraManager::save
    regUpdate<CameraDataList>(p, ApiCommand::saveCameras);

    // AbstractCameraManager::getCameras
    regGet<QnCameraUuid, CameraDataList>(p, ApiCommand::getCameras);

    /**%apidoc:arrayParams POST /ec2/saveCameraUserAttributesList
     * Save additional camera attributes for a number of cameras.
     * <p>
     * Parameters should be passed as a JSON array of objects in POST message body with
     * content type "application/json". Example of such object can be seen in
     * the result of the corresponding GET function.
     * </p>
     * %param cameraId Camera unique id. If such object exists, omitted fields will not be changed.
     * %param cameraName Camera name.
     * %param userDefinedGroupName Name of the user-defined camera group.
     * %param scheduleEnabled Whether recording to the archive is enabled for the camera.
     *     %value false
     *     %value true
     * %param licenseUsed Whether the license is used for the camera.
     *     %value false
     *     %value true
     * %param motionType Type of motion detection method.
     *     %value MT_Default Use default method.
     *     %value MT_HardwareGrid Use motion detection grid implemented by the camera.
     *     %value MT_SoftwareGrid Use motion detection grid implemented by the server.
     *     %value MT_MotionWindow Use motion detection window implemented by the camera.
     *     %value MT_NoMotion Do not perform motion detection.
     * %param motionMask List of motion detection areas and their
     *     sensitivity. The format is proprietary and is likely to change in
     *     future API versions. Currently, this string defines several rectangles separated
     *     with ":", each rectangle is described by 5 comma-separated numbers: sensitivity,
     *     x and y (for left top corner), width, height.
     * %param scheduleTasks List of scheduleTask objects which define the camera recording
     *     schedule.
     *     %param scheduleTasks[].startTime Time of day when the backup starts (in seconds passed
     *         from 00:00:00).
     *     %param scheduleTasks[].endTime Time of day when the backup ends (in seconds passed
     *         from 00:00:00).
     *     %param scheduleTasks[].recordingType
     *         %value RT_Always Record always.
     *         %value RT_MotionOnly Record only when the motion is detected.
     *         %value RT_Never Never record.
     *         %value RT_MotionAndLowQuality Always record low quality
     *             stream, and record high quality stream on motion.
     *     %param scheduleTasks[].dayOfWeek Day of week for the recording task.
     *         %value 1 Monday
     *         %value 2 Tuesday
     *         %value 3 Wednesday
     *         %value 4 Thursday
     *         %value 5 Friday
     *         %value 6 Saturday
     *         %value 7 Sunday
     *     %param scheduleTasks[].streamQuality Quality of the recording.
     *         %value QualityLowest
     *         %value QualityLow
     *         %value QualityNormal
     *         %value QualityHigh
     *         %value QualityHighest
     *         %value QualityPreSet
     *         %value QualityNotDefined
     *     %param scheduleTasks[].fps Frames per second (integer).
     * %param audioEnabled Whether audio is enabled on the camera.
     *     %value false
     *     %value true
     * %param disableDualStreaming
     *     %value false turn of dual streaming.
     *     %value true enable dual streaming if it supported by camera.
     * %param controlEnabled Whether server manages the camera (changes resolution, FPS, create
     *     profiles, etc).
     *     %value false
     *     %value true
     * %param dewarpingParams Image dewarping parameters.
     *     The format is proprietary and is likely to change in future API
     *     versions.
     * %param minArchiveDays Minimum number of days to keep the archive for.
     *     If the value is less than or equal to zero, it is not used.
     * %param maxArchiveDays Maximum number of days to keep the archive for.
     *     If the value is less than or equal zero, it is not used.
     * %param recordBeforeMotionSec The number of seconds before a motion event to record the video
     *     for.
     * %param recordAfterMotionSec The number of seconds after a motion event to record the video
     *     for.
     * %param preferredServerId Unique id of a server which has the highest priority of hosting
     *     the camera for failover (if the current server fails).
     * %param failoverPriority Priority for the camera to be moved
     *     to another server for failover (if the current server fails).
     *     %value FP_Never Will never be moved to another server.
     *     %value FP_Low Low priority against other cameras.
     *     %value FP_Medium Medium priority against other cameras.
     *     %value FP_High High priority against other cameras.
     * %param backupType Combination (via "|") of the flags defining backup options.
     *     %value CameraBackup_Disabled Backup is disabled.
     *     %value CameraBackup_HighQuality Backup is in high quality.
     *     %value CameraBackup_LowQuality Backup is in low quality.
     *     %value CameraBackup_Both
     *         Equivalent of "CameraBackup_HighQuality|CameraBackup_LowQuality".
     *     %value CameraBackup_Default A default value is used for backup options.
     * %// AbstractCameraManager::saveUserAttributes
     */
    regUpdate<CameraAttributesDataList>(p, ApiCommand::saveCameraUserAttributesList);

    /**%apidoc POST /ec2/saveCameraUserAttributes
     * Save additional camera attributes for a single camera.
     * <p>
     * Parameters should be passed as a JSON object in POST message body with
     * content type "application/json". Example of such object can be seen in
     * the result of the corresponding GET function.
     * </p>
     * %permissions Administrator, or a custom user with "Edit camera settings" permission.
     * %param cameraId Camera unique id. If such object exists, omitted fields will not be changed.
     * %param cameraName Camera name.
     * %param userDefinedGroupName Name of the user-defined camera group.
     * %param scheduleEnabled Whether recording to the archive is enabled for the camera.
     *     %value false
     *     %value true
     * %param licenseUsed Whether the license is used for the camera.
     *     %value false
     *     %value true
     * %param motionType Type of motion detection method.
     *     %value MT_Default Use default method.
     *     %value MT_HardwareGrid Use motion detection grid implemented by the camera.
     *     %value MT_SoftwareGrid Use motion detection grid implemented by the server.
     *     %value MT_MotionWindow Use motion detection window implemented by the camera.
     *     %value MT_NoMotion Do not perform motion detection.
     * %param motionMask List of motion detection areas and their
     *     sensitivity. The format is proprietary and is likely to change in
     *     future API versions. Currently, this string defines several rectangles separated
     *     with ":", each rectangle is described by 5 comma-separated numbers: sensitivity,
     *     x and y (for left top corner), width, height.
     * %param scheduleTasks List of scheduleTask objects which define the camera recording
     *     schedule.
     *     %param scheduleTasks[].startTime Time of day when the backup starts (in seconds passed
     *         from 00:00:00).
     *     %param scheduleTasks[].endTime Time of day when the backup ends (in seconds passed
     *         from 00:00:00).
     *     %param scheduleTasks[].recordingType
     *         %value RT_Always Record always.
     *         %value RT_MotionOnly Record only when the motion is detected.
     *         %value RT_Never Never record.
     *         %value RT_MotionAndLowQuality Always record low quality
     *             stream, and record high quality stream on motion.
     *     %param scheduleTasks[].dayOfWeek Weekday for the recording task.
     *         %value 1 Monday
     *         %value 2 Tuesday
     *         %value 3 Wednesday
     *         %value 4 Thursday
     *         %value 5 Friday
     *         %value 6 Saturday
     *         %value 7 Sunday
     *     %param scheduleTasks[].streamQuality Quality of the recording.
     *         %value QualityLowest
     *         %value QualityLow
     *         %value QualityNormal
     *         %value QualityHigh
     *         %value QualityHighest
     *         %value QualityPreSet
     *         %value QualityNotDefined
     *     %param scheduleTasks[].fps Frames per second (integer).
     * %param audioEnabled Whether audio is enabled on the camera.
     *     %value false
     *     %value true
     * %param disableDualStreaming
     *     %value false turn of dual streaming.
     *     %value true enable dual streaming if it supported by camera.
     * %param controlEnabled Whether server manages the camera (changes resolution, FPS, create
     *     profiles, etc).
     *     %value false
     *     %value true
     * %param dewarpingParams Image dewarping parameters.
     *     The format is proprietary and is likely to change in future API
     *     versions.
     * %param minArchiveDays Minimum number of days to keep the archive for.
     *     If the value is less than or equal zero, it is not used.
     * %param maxArchiveDays Maximum number of days to keep the archive for.
     *     If the value is less than or equal zero, it is not used.
     * %param recordBeforeMotionSec The number of seconds before a motion event to record the video
     *     for.
     * %param recordAfterMotionSec The number of seconds after a motion event to record the video
     *     for.
     * %param preferredServerId Unique id of a server which has the highest priority of hosting
     *     the camera for failover (if the current server fails).
     * %param failoverPriority Priority for the camera to be moved
     *     to another server for failover (if the current server fails).
     *     %value FP_Never Will never be moved to another server.
     *     %value FP_Low Low priority against other cameras.
     *     %value FP_Medium Medium priority against other cameras.
     *     %value FP_High High priority against other cameras.
     * %param backupType Combination (via "|") of the flags defining backup options.
     *     %value CameraBackup_Disabled Backup is disabled.
     *     %value CameraBackup_HighQuality Backup is in high quality.
     *     %value CameraBackup_LowQuality Backup is in low quality.
     *     %value CameraBackup_Both
     *         Equivalent of "CameraBackup_HighQuality|CameraBackup_LowQuality".
     *     %value CameraBackup_Default A default value is used for backup options.
     * %// AbstractCameraManager::saveUserAttributes
     */
    regUpdate<CameraAttributesData>(p, ApiCommand::saveCameraUserAttributes);

    /**%apidoc GET /ec2/getCameraUserAttributesList
     * Read additional camera attributes.
     * %param[default] format
     * %param[opt]:string id Camera id (can be obtained from "id", "physicalId" or "logicalId"
     *     field via /ec2/getCamerasEx or /ec2/getCameras?extraFormatting) or MAC address (not
     *     supported for certain cameras). If omitted, return data for all cameras.
     * %return List of objects with additional camera attributes for all cameras, in the requested
     *     format.
     *     %param cameraId Camera unique id.
     *     %param cameraName Camera name.
     *     %param userDefinedGroupName Name of the user-defined camera group.
     *     %param scheduleEnabled Whether recording to the archive is enabled for the camera.
     *         %value false
     *         %value true
     *     %param licenseUsed Whether the license is used for the camera.
     *         %value false
     *         %value true
     *     %param motionType Type of motion detection method.
     *         %value MT_Default Use default method.
     *         %value MT_HardwareGrid Use motion detection grid implemented by the camera.
     *         %value MT_SoftwareGrid Use motion detection grid implemented by the server.
     *         %value MT_MotionWindow Use motion detection window implemented by the camera.
     *         %value MT_NoMotion Do not perform motion detection.
     *     %param motionMask List of motion detection areas and their
     *         sensitivity. The format is proprietary and is likely to change in
     *         future API versions. Currently, this string defines several rectangles separated
     *         with ":", each rectangle is described by 5 comma-separated numbers: sensitivity,
     *         x and y (for left top corner), width, height.
     *     %param scheduleTasks List of scheduleTask objects which define the camera recording
     *         schedule.
     *         %param scheduleTasks[].startTime Time of day when the backup starts (in seconds
     *             passed from 00:00:00).
     *         %param scheduleTasks[].endTime Time of day when the backup ends (in seconds passed
     *             from 00:00:00).
     *         %param scheduleTasks[].recordingType
     *             %value RT_Always Record always.
     *             %value RT_MotionOnly Record only when the motion is detected.
     *             %value RT_Never Never record.
     *             %value RT_MotionAndLowQuality Always record low quality
     *                 stream, and record high quality stream on motion.
     *         %param scheduleTasks[].dayOfWeek Weekday for the recording task.
     *             %value 1 Monday
     *             %value 2 Tuesday
     *             %value 3 Wednesday
     *             %value 4 Thursday
     *             %value 5 Friday
     *             %value 6 Saturday
     *             %value 7 Sunday
     *         %param scheduleTasks[].streamQuality Quality of the recording.
     *             %value QualityLowest
     *             %value QualityLow
     *             %value QualityNormal
     *             %value QualityHigh
     *             %value QualityHighest
     *             %value QualityPreSet
     *             %value QualityNotDefined
     *         %param scheduleTasks[].fps Frames per second (integer).
     *     %param audioEnabled Whether audio is enabled on the camera.
     *         %value false
     *         %value true
     *     %param disableDualStreaming
     *         %value false turn of dual streaming.
     *         %value true enable dual streaming if it supported by camera.
     *     %param controlEnabled Whether server manages the camera (changes resolution, FPS, create
     *         profiles, etc).
     *         %value false
     *         %value true
     *     %param dewarpingParams Image dewarping parameters.
     *         The format is proprietary and is likely to change in future API
     *         versions.
     *     %param minArchiveDays Minimum number of days to keep the archive for.
     *         If the value is less than or equal zero, it is not used.
     *     %param maxArchiveDays Maximum number of days to keep the archive for.
     *         If the value is less than or equal zero, it is not used.
     *     %param recordBeforeMotionSec The number of seconds before a motion event to record the
     *         video for.
     *     %param recordAfterMotionSec The number of seconds after a motion event to record the
     *         video for.
     *     %param preferredServerId Unique id of a server which has the highest priority of hosting
     *         the camera for failover (if the current server fails).
     *     %param failoverPriority Priority for the camera to be moved
     *         to another server for failover (if the current server fails).
     *         %value FP_Never Will never be moved to another server.
     *         %value FP_Low Low priority against other cameras.
     *         %value FP_Medium Medium priority against other cameras.
     *         %value FP_High High priority against other cameras.
     *     %param backupType Combination (via "|") of the flags defining backup options.
     *         %value CameraBackup_Disabled Backup is disabled.
     *         %value CameraBackup_HighQuality Backup is in high quality.
     *         %value CameraBackup_LowQuality Backup is in low quality.
     *         %value CameraBackup_Both
     *             Equivalent of "CameraBackup_HighQuality|CameraBackup_LowQuality".
     *         %value CameraBackup_Default A default value is used for backup options.
     * %// AbstractCameraManager::getUserAttributes
     */
    regGet<QnCameraUuid, CameraAttributesDataList>(p, ApiCommand::getCameraUserAttributesList);

    // AbstractCameraManager::addCameraHistoryItem
    regUpdate<ServerFootageData>(p, ApiCommand::addCameraHistoryItem);

    /**%apidoc GET /ec2/getCameraHistoryItems
     * Read information about which servers have archive for which camera.
     * This information is used for archive playback if camera has been moved from
     * one server to another.
     * There is no detail information about archive time periods.
     * To obtain detail information please use method ec2/recordedTimePeriods.
     * %param[default] format
     * %return List of camera history items in the requested format.
     * %// AbstractCameraManager::getCameraHistoryItems
     */
    regGet<std::nullptr_t, ServerFootageDataList>(p, ApiCommand::getCameraHistoryItems);

    /**%apidoc GET /ec2/getCamerasEx
     * Read camera list.
     * %param[default] format
     * %param[opt]:string id Camera id (can be obtained from "id", "physicalId" or "logicalId"
     *     field via /ec2/getCamerasEx or /ec2/getCameras?extraFormatting) or MAC address (not
     *     supported for certain cameras). If omitted, return data for all cameras.
     * %param[opt]:boolean showDesktopCameras Whether desktop cameras should be listed. False by
     *     default.
     * %return List of camera information objects in the requested format.
     *     %// From struct ApiResourceData:
     *     %param id Camera unique id.
     *     %param parentId Unique id of the server hosting the camera.
     *     %param name Camera name.
     *     %param url Camera IP address, or a complete HTTP URL if the camera was added manually.
     *         Also, for multichannel encoders a complete URL is always used.
     *     %param typeId Unique id of the camera type. A camera type can describe predefined
     *         information such as camera maximum resolution, FPS, etc. Detailed type information
     *         can be obtained via GET /ec2/getResourceTypes request.
     *
     *     %// From struct CameraData (inherited from ResourceData):
     *     %param mac Camera MAC address.
     *     %param physicalId Camera unique identifier. This identifier can used in some requests
     *         related to a camera.
     *     %param manuallyAdded Whether the user added the camera manually.
     *         %value false
     *         %value true
     *     %param model Camera model.
     *     %param groupId Internal group identifier. It is used for grouping channels of
     *         multi-channel cameras together.
     *     %param groupName Group name. This name can be changed by users.
     *     %param statusFlags Usually this field is zero. Non-zero value indicates that the camera
     *         is causing a lot of network issues.
     *     %param vendor Camera manufacturer.
     *
     *     %// From struct ApiCameraAttributesData:
     *     %param cameraId Camera unique id. If such object exists, omitted fields will not be
     *           changed.
     *     %param cameraName Camera name.
     *     %param userDefinedGroupName Name of the user-defined camera group.
     *     %param scheduleEnabled Whether recording to the archive is enabled for the camera.
     *         %value false
     *         %value true
     *     %param licenseUsed Whether the license is used for the camera.
     *         %value false
     *         %value true
     *     %param motionType Type of motion detection method.
     *         %value MT_Default Use default method.
     *         %value MT_HardwareGrid Use motion detection grid implemented by the camera.
     *         %value MT_SoftwareGrid Use motion detection grid implemented by the server.
     *         %value MT_MotionWindow Use motion detection window implemented by the camera.
     *         %value MT_NoMotion Do not perform motion detection.
     *     %param motionMask List of motion detection areas and their
     *         sensitivity. The format is proprietary and is likely to change in
     *         future API versions. Currently, this string defines several rectangles separated
     *         with ":", each rectangle is described by 5 comma-separated numbers: sensitivity,
     *         x and y (for the left top corner), width, height.
     *     %param scheduleTasks List of scheduleTask objects which define the camera recording
     *         schedule.
     *         %param scheduleTasks[].startTime Time of day when the backup starts (in seconds
     *             passed from 00:00:00).
     *         %param scheduleTasks[].endTime Time of day when the backup ends (in seconds passed
     *             from 00:00:00).
     *         %param scheduleTasks[].recordingType
     *             %value RT_Always Record always.
     *             %value RT_MotionOnly Record only when the motion is detected.
     *             %value RT_Never Never record.
     *             %value RT_MotionAndLowQuality Always record low quality
     *                 stream, and record high quality stream on motion.
     *         %param scheduleTasks[].dayOfWeek Day of week for the recording task.
     *             %value 1 Monday
     *             %value 2 Tuesday
     *             %value 3 Wednesday
     *             %value 4 Thursday
     *             %value 5 Friday
     *             %value 6 Saturday
     *             %value 7 Sunday
     *         %param scheduleTasks[].streamQuality Quality of the recording.
     *             %value QualityLowest
     *             %value QualityLow
     *             %value QualityNormal
     *             %value QualityHigh
     *             %value QualityHighest
     *             %value QualityPreSet
     *             %value QualityNotDefined
     *         %param scheduleTasks[].fps Frames per second (integer).
     *     %param audioEnabled Whether audio is enabled on the camera.
     *         %value false
     *         %value true
     *     %param disableDualStreaming
     *         %value false turn of dual streaming.
     *         %value true enable dual streaming if it supported by camera.
     *     %param controlEnabled Whether server manages the camera (changes resolution, FPS, create
     *         profiles, etc).
     *         %value false
     *         %value true
     *     %param dewarpingParams Image dewarping parameters.
     *         The format is proprietary and is likely to change in future API
     *         versions.
     *     %param minArchiveDays Minimum number of days to keep the archive for.
     *         If the value is less than or equal to zero, it is not used.
     *     %param maxArchiveDays Maximum number of days to keep the archive for.
     *         If the value is less than or equal to zero, it is not used.
     *     %param recordBeforeMotionSec The number of seconds before a motion event to record the
     *         video for.
     *     %param recordAfterMotionSec The number of seconds after a motion event to record the
     *         video for.
     *     %param preferredServerId Unique id of a server which has the highest priority of hosting
     *         the camera for failover (if the current server fails).
     *     %param failoverPriority Priority for the camera to be moved
     *         to another server for failover (if the current server fails).
     *         %value FP_Never Will never be moved to another server.
     *         %value FP_Low Low priority against other cameras.
     *         %value FP_Medium Medium priority against other cameras.
     *         %value FP_High High priority against other cameras.
     *     %param backupType Combination (via "|") of the flags defining backup options.
     *         %value CameraBackup_Disabled Backup is disabled.
     *         %value CameraBackup_HighQuality Backup is in high quality.
     *         %value CameraBackup_LowQuality Backup is in low quality.
     *         %value CameraBackup_Both
     *             Equivalent of "CameraBackup_HighQuality|CameraBackup_LowQuality".
     *         %value CameraBackup_Default A default value is used for backup options.
     *
     *     %// From CameraDataEx:
     *     %param status Camera status.
     *         %value Offline
     *         %value Online
     *         %value Recording
     *     %param addParams List of additional parameters for the camera. This list can contain
     *         such information as full ONVIF URL, camera maximum FPS, etc.
     * %// AbstractCameraManager::getCamerasEx
     */
    regGet<QnCameraDataExQuery, CameraDataExList>(p, ApiCommand::getCamerasEx);

    /**%apidoc GET /ec2/getStorages
     * Read the list of current storages.
     * %param[default] format
     * %param[opt]:uuid id Parent server unique id. If omitted, return storages for all servers.
     * %return List of storages.
     *     %param id Storage unique id.
     *     %param parentId Id of a server to which the storage belongs.
     *     %param name Storage name.
     *     %param url Storage URL.
     *     %param spaceLimit Storage space to leave free on the storage,
     *         in bytes.
     *     %param usedForWriting Whether writing to the storage is allowed.
     *         %value false
     *         %value true
     *     %param storageType Type of the method to access the storage.
     *         %value local
     *         %value smb
     *     %param addParams List of storage additional parameters.
     *         Intended for internal use.
     *     %param isBackup Whether the storage is used for backup.
     *         %value false
     *         %value true
     */
    regGet<StorageParentId, StorageDataList>(p, ApiCommand::getStorages);

    // AbstractLicenseManager::addLicenses
    regUpdate<LicenseDataList>(p, ApiCommand::addLicenses);
    // AbstractLicenseManager::removeLicense
    regUpdate<LicenseData>(p, ApiCommand::removeLicense);

    /**%apidoc GET /ec2/getEventRules
     * Return all event rules.
     * %// ATTENTION: Many text duplications with /api/getEvents and /api/createEvent, and with
     *     comments in structs EventMetaData, EventParameters, and enums in EventRuleData.
     * %param[default] format
     * %param[opt] id Event rule unique id. If omitted, return data for all event rules.
     * %return List of event rule objects in the requested format.
     *     %param id Event rule unique id.
     *     %param eventType One of the fixed values.
     *         %value undefinedEvent Event type is not defined.
     *         %value cameraMotionEvent Motion has occurred on a camera.
     *         %value cameraInputEvent Camera input signal is received.
     *         %value cameraDisconnectEvent Camera was disconnected.
     *         %value storageFailureEvent Storage read error has occurred.
     *         %value networkIssueEvent Network issue: packet lost, RTP timeout, etc.
     *         %value cameraIpConflictEvent Found some cameras with same IP address.
     *         %value serverFailureEvent Connection to server lost.
     *         %value serverConflictEvent Two or more servers are running.
     *         %value serverStartEvent Server started.
     *         %value licenseIssueEvent Not enough licenses.
     *         %value backupFinishedEvent Archive backup done.
     *         %value softwareTriggerEvent Software triggers.
     *         %value[proprietary] systemHealthEvent Equals 500. First system health message.
     *         %value[proprietary] maxSystemHealthEvent Equals 599. Last possible system health
     *             message.
     *         %value[proprietary] anyCameraEvent Event group.
     *         %value[proprietary] anyServerEvent Event group.
     *         %value[proprietary] anyEvent Event group.
     *         %value userDefinedEvent Custom event defined by the user.
     *     %param eventResourceIds List of event resource ids.
     *     %param:objectJson eventCondition String containing a JSON object, some fields of which
     *         depend on eventType. Defines the filter for an event to make the rule applicable.
     *         NOTE: Other fields than the described below can be stored in this object, but they
     *         are not used for event matching.
     *         %// ATTENTION: Commented-out params are present in the struct but are not used in
     *             eventCondition. Also, params which are not commented-out may have descriptions
     *             applicable only to the current usage of the struct.
     *         %//param eventCondition.eventType (string) Event type. Default: "undefinedEvent".
     *         %//param eventCondition.eventTimestampUsec (integer) Event timestamp (in
     *             milliseconds). Default: 0.
     *         %//param eventCondition.eventResourceId (string) Event source - id of a camera or a
     *             server. Default: "{00000000-0000-0000-0000-000000000000}".
     *         %param eventCondition.resourceName Substring to be found in the name of the resource
     *             (e.g. a camera) which caused the event. Empty string matches any value.
     *         %//param eventCondition.sourceServerId (string) Id of a server that generated the
     *             event. Default: "{00000000-0000-0000-0000-000000000000}".
     *         %//param eventCondition.reasonCode (string, fixed values) Used in certain event
     *             types. Default: "none".
     *             %value none
     *             %value networkNoFrame
     *             %value networkConnectionClosed
     *             %value networkRtpPacketLoss
     *             %value serverTerminated
     *             %value serverStarted
     *             %value storageIoError
     *             %value storageTooSlow
     *             %value storageFull
     *             %value systemStorageFull
     *             %value licenseRemoved
     *             %value backupFailedNoBackupStorageError
     *             %value backupFailedSourceStorageError
     *             %value backupFailedSourceFileError
     *             %value backupFailedTargetFileError
     *             %value backupFailedChunkError
     *             %value backupEndOfPeriod
     *             %value backupDone
     *             %value backupCancelled
     *             %value networkNoResponseFromDevice
     *         %param eventCondition.inputPortId (string) Used for input events only. Empty string
     *             matches any value.
     *         %param eventCondition.caption Substring to be found in the short event description.
     *             Empty string matches any value.
     *         %param eventCondition.description Substring to be found in the long event
     *             description. Empty string matches any value.
     *         %//param eventCondition.metadata (object) Imposes filtering based on the event
     *             metadata fields.
     *             %//param eventCondition.metadata.cameraRefs cameraRefs (list of strings) Camera
     *                 id list. Empty means any. Camera id can be obtained from "id", "physicalId"
     *                 or "logicalId" field via request '/ec2/getCamerasEx'.
     *
     *     %param eventState One of the fixed values.
     *         %value inactive
     *         %value active
     *         %value undefined Also used in event rule to associate non-toggle action with event
     *             with any toggle state.
     *     %param actionType Type of the action. The object fields in actionParams field depend on
     *         this value, as shown in particular values description.
     *         %value undefinedAction
     *         %value cameraOutputAction Change camera output state.
     *             actionParams:
     *             - relayOutputID (string, required) - Id of output to trigger.
     *             - durationMs (uint, optional) - Timeout (in milliseconds) to reset the camera
     *             state back.
     *         %value bookmarkAction
     *         %value cameraRecordingAction Start camera recording.
     *         %value panicRecordingAction Activate panic recording mode.
     *         %value sendMailAction Send an email. This action can be executed from any endpoint.
     *             actionParams:
     *             - emailAddress (string, required)
     *         %value diagnosticsAction Write a record to the server log.
     *         %value showPopupAction
     *         %value playSoundAction
     *             actionParams:
     *             - url (string, required) - Url of the sound, contains path to the sound on the
     *             server.
     *         %value playSoundOnceAction
     *             actionParams:
     *             - url (string, required) - Url of the sound, contains path to the sound on the
     *             server.
     *         %value sayTextAction
     *             actionParams:
     *             - sayText (string, required) - Text that will be provided to TTS engine.
     *         %value executePtzPresetAction Execute given PTZ preset.
     *             actionParams:
     *             - resourceId
     *             - presetId
     *         %value showTextOverlayAction Show text overlay over the given camera(s).
     *             actionParams:
     *             - text (string, required) - Text that will be displayed.
     *         %value showOnAlarmLayoutAction Put the given camera(s) to the Alarm Layout.
     *             actionParams:
     *             - users - List of users which will receive this alarm notification.
     *         %value execHttpRequestAction Send HTTP request as an action.
     *             actionParams:
     *             - url - Full HTTP url to execute. Username/password are stored as part of the
     *             URL.
     *             - text - HTTP message body for POST method.
     *         %value acknowledgeAction
     *     %param actionResourceIds List of action resource ids.
     *     %param:objectJson actionParams String containing a JSON object which fields depend on actionType
     *         and thus are described next to the respective actionType values.
     *     %param aggregationPeriod Period (in seconds) during which the consecutive similar events
     *         are aggregated into a single event.
     *     %param disabled Whether the rule is disabled.
     *         %value false
     *         %value true
     *     %param comment An arbitrary comment to the rule.
     *     %param schedule String containing a lower-case hex array of 21 bytes, which is a bit
     *         mask for a week (starting on Monday), where each consecutive bit corresponds to an
     *         hour and equals 1 for "enabled" and 0 for "disabled".
     *         Thus, the first byte describes the first 8 hours of Monday after midnight, and the
     *         last byte describes the last 8 hours of Sunday before midnight; in a byte, the
     *         highest significant bit corresponds to the earliest hour.
     *         Example: Enable all hours in a week except for the Monday interval from 7am to 9am:
     *         <code>fe7fffffffffffffffffffffffffffffffffffffff</code>
     *     %param system Whether the rule is a built-in one, which cannot be deleted.
     *         %value false
     *         %value true
     * %// AbstractEventRulesManager::getEventRules
     */
    regGet<QnUuid, EventRuleDataList>(p, ApiCommand::getEventRules);

    regGet<ApiTranLogFilter, ApiTransactionDataList>(p, ApiCommand::getTransactionLog);

    /**%apidoc POST /ec2/saveEventRule
     * Create or update event rule in event/actions rule list.
     * <p>
     * Parameters should be passed as a JSON object in POST message body with
     * content type "application/json". Example of such object can be seen in
     * the result of the corresponding GET function.
     * </p>
     * %param eventType Event type to match the rule. Example of possible values can be seen in
     *     the result of the corresponding GET function.
     * %param[opt] eventResourceIds List of resources to match. Any resource if the list is empty.
     * %param[opt] eventCondition Additional text filter for event rule. Used for some event types.
     * %param[opt] EventState Event state to match the rule.
     *     %value inactive Prolonged event has finished.
     *     %value active Prolonged event has started.
     *     %value undefined Any state.
     * %param actionType Action to execute if the rule matches. Example of possible values can be
     *     seen in the result of the corresponding GET function.
     * %param[opt] actionResourceIds Resource list associated with the action. The action is executed
     *     for each resource in the list.
     * %param[opt] actionParams Additional parameters used in the action. It depends on the action type.
     * %param[opt] aggregationPeriod Aggregation period in seconds. The action is not going to trigger
     *     more often than the aggregation period.
     * %param disabled Enable or disable the rule.
     * %param[opt] comment Human-readable text. Not used on the server side.
     * %param[opt] schedule Hex representation of the binary data. Each bit defines whether
     *     the action should or should not be executed at some hour of week. Hour numbers
     *     start with 0. There are 24 * 7 = 168 bits.
     * %param[opt] system Whether the rule can't be deleted by user. System rules can't be deleted.
    */
    regUpdate<EventRuleData>(p, ApiCommand::saveEventRule);

    // AbstractEventRulesManager::deleteRule
    regUpdate<IdData>(p, ApiCommand::removeEventRule);

    regUpdate<ResetEventRulesData>(p, ApiCommand::resetEventRules);
    regUpdate<EventActionData>(p, ApiCommand::broadcastAction);

    /**%apidoc[proprietary] POST /ec2/execAction
     * Execute an action. Action data should be in the internal binary format. This method is going
     * to be refactored in next versions.
     * %return XML with the "OK" message or an error code.
     */
    regUpdate<EventActionData>(p, ApiCommand::execAction);

    /**%apidoc GET /ec2/getUsers
     * Return users registered in the System. User's password contains MD5 hash data with the salt.
     * %param[default] format
     * %param[opt] id User unique id. If omitted, return data for all users.
     * %return List of user data objects in the requested format.
     * %// AbstractUserManager::getUsers
     */
    regGet<QnUuid, UserDataList>(p, ApiCommand::getUsers);

    /**%apidoc GET /ec2/getUserRoles
     * Return User roles registered in the System.
     * %param[default] format
     * %param[opt] id User role unique id. If omitted, return data for all user roles.
     * %return List of user role objects in the requested format.
     *     %// TODO: Describe ApiUserRole fields.
     * %// AbstractUserManager::getUserRoles
     */
    regGet<QnUuid, UserRoleDataList>(p, ApiCommand::getUserRoles);

    /**%apidoc GET /ec2/getAccessRights
     * Return list of ids of accessible resources for each user in the System.
     * %param[default] format
     * %return List of access rights data objects in the requested format.
     * %// AbstractUserManager::getAccessRights
     */
    regGet<std::nullptr_t, AccessRightsDataList>(p, ApiCommand::getAccessRights);

    /**%apidoc POST /ec2/setAccessRights
     * <p>
     * Parameters should be passed as a JSON object in POST message body with
     * content type "application/json". Example of such object can be seen in
     * the result of the corresponding GET function.
     * </p>
     * %permissions Administrator.
     * %param userId User unique id.
     * %param resourceIds List of accessible resources ids.
     * %// AbstractUserManager::setAccessRights
     */
    regUpdate<AccessRightsData>(p, ApiCommand::setAccessRights);

    /**%apidoc POST /ec2/saveUser
     * <p>
     * Parameters should be passed as a JSON object in POST message body with
     * content type "application/json". Example of such object can be seen in
     * the result of the corresponding GET function.
     * </p>
     * %permissions Administrator.
     * %param[opt] id User unique id. Can be omitted when creating a new object. If such object
     *     exists, omitted fields will not be changed.
     * %param[proprietary] parentId Should be empty.
     * %param name User name.
     * %param[proprietary] url Should be empty.
     * %param[proprietary] typeId Should have fixed value.
     *     %value {774e6ecd-ffc6-ae88-0165-8f4a6d0eafa7}
     * %param[proprietary] isAdmin Indended for internal use; keep the value when saving
     *     a previously received object, use false when creating a new one.
     *     %value false
     *     %value true
     * %param permissions Combination (via "|") of the flags defining permissions.
     *     %value GlobalAdminPermission Admin, can edit other non-admins.
     *     %value GlobalEditCamerasPermission Can edit camera settings.
     *     %value GlobalControlVideoWallPermission Can control video walls.
     *     %value GlobalViewArchivePermission Can view archives of available cameras.
     *     %value GlobalExportPermission Can export archives of available cameras.
     *     %value GlobalViewBookmarksPermission Can view bookmarks of available cameras.
     *     %value GlobalManageBookmarksPermission Can modify bookmarks of available cameras.
     *     %value GlobalUserInputPermission Can change PTZ state of a camera, use 2-way audio, I/O
     *         buttons.
     *     %value GlobalAccessAllMediaPermission Has access to all media (cameras and web pages).
     *     %value GlobalCustomUserPermission Flag: this user has custom permissions
     * %param[opt] userRoleId User role unique id.
     * %param email User's email.
     * %param[proprietary] digest HA1 digest hash from user password, as per RFC 2069. When modifying an
     *     existing user, supply empty string. When creating a new user, calculate the value
     *     based on UTF-8 password as follows:
     *     <code>digest = md5_hex(user_name + ":" + realm + ":" + password);</code>
     * %param[proprietary] hash User's password hash. When modifying an existing user, supply empty string.
     *     When creating a new user, calculate the value based on UTF-8 password as follows:
     *     <code>salt = rand_hex();
     *     hash = "md5$" + salt + "$" + md5_hex(salt + password);</code>
     * %param[proprietary] cryptSha512Hash Cryptography key hash. Supply empty string
     *     when creating, keep the value when modifying.
     * %param[opt] password User's password
     * %param[proprietary] realm HTTP authorization realm as defined in RFC 2617, can be obtained via
     *     /api/gettime.
     * %param[opt] isLdap Whether the user was imported from LDAP.
     *     %value false
     *     %value true
     * %param[opt] isEnabled Whether the user is enabled.
     *     %value false
     *     %value true Default value.
     * %param[opt] isCloud Whether the user is a cloud user, as opposed to a local one.
     *     %value false Default value.
     *     %value true
     * %param fullName Full name of the user.
     * %// AbstractUserManager::save
     */
    regUpdate<UserDataEx, UserData>(p, ApiCommand::saveUser);

    /**%apidoc:arrayParams POST /ec2/saveUsers
    * Saves the list of users. Only local and LDAP users are supported. Cloud users won't be saved.
    * <p>
    * Parameters should be passed as a JSON array of objects in POST message body with
    * content type "application/json". Example of such object can be seen in
    * the result of the corresponding GET function.
    * </p>
    * %permissions Administrator.
    * %param[opt] id User unique id. Can be omitted when creating a new object. If such object
    *     exists, omitted fields will not be changed.
    * %param[opt] parentId Should be empty.
    * %param name User name.
    * %param[opt] url Should be empty.
    * %param[proprietary] typeId Should have fixed value.
    *     %value {774e6ecd-ffc6-ae88-0165-8f4a6d0eafa7}
    * %param[proprietary] isAdmin Indended for internal use; keep the value when saving
    *     a previously received object, use false when creating a new one.
    *     %value false
    *     %value true
    * %param permissions Combination (via "|") of the flags defining permissions.
    *     %value GlobalAdminPermission Admin, can edit other non-admins.
    *     %value GlobalEditCamerasPermission Can edit camera settings.
    *     %value GlobalControlVideoWallPermission Can control video walls.
    *     %value GlobalViewArchivePermission Can view archives of available cameras.
    *     %value GlobalExportPermission Can export archives of available cameras.
    *     %value GlobalViewBookmarksPermission Can view bookmarks of available cameras.
    *     %value GlobalManageBookmarksPermission Can modify bookmarks of available cameras.
    *     %value GlobalUserInputPermission Can change PTZ state of a camera, use 2-way audio, I/O
    *         buttons.
    *     %value GlobalAccessAllMediaPermission Has access to all media (cameras and web pages).
    *     %value GlobalCustomUserPermission Flag: this user has custom permissions
    * %param[opt] userRoleId User role unique id.
    * %param email User's email.
    * %param[opt] digest HA1 digest hash from user password, as per RFC 2069. When modifying an
    *     existing user, supply empty string. When creating a new user, calculate the value
    *     based on UTF-8 password as follows:
    *     <code>digest = md5_hex(user_name + ":" + realm + ":" + password);</code>
    * %param[opt] hash User's password hash. When modifying an existing user, supply empty string.
    *     When creating a new user, calculate the value based on UTF-8 password as follows:
    *     <code>salt = rand_hex();
    *     hash = "md5$" + salt + "$" + md5_hex(salt + password);</code>
    * %param[opt] cryptSha512Hash Cryptography key hash. Supply empty string
    *     when creating, keep the value when modifying.
    * %param[opt] realm HTTP authorization realm as defined in RFC 2617, can be obtained via
    *     /api/gettime.
    * %param[opt] isLdap Whether the user was imported from LDAP.
    *     %value false
    *     %value true
    * %param[opt] isEnabled Whether the user is enabled.
    *     %value false
    *     %value true Default value.
    * %param[opt] isCloud Whether the user is a cloud user, as opposed to a local one.
    *     %value false Default value.
    *     %value true
    * %param fullName Full name of the user.
    * %// AbstractUserManager::save
    */
    regUpdate<UserDataList>(p, ApiCommand::saveUsers);

    /**%apidoc POST /ec2/removeUser
     * Delete the specified user.
     * <p>
     * Parameters should be passed as a JSON object in POST message body with
     * content type "application/json". Example of such object can be seen in
     * the result of the corresponding GET function.
     * </p>
     * %permissions Administrator.
     * %param id User unique id.
     * %// AbstractUserManager::remove
     */
    regUpdate<IdData>(p, ApiCommand::removeUser);

    /**%apidoc POST /ec2/removeResourceParam
     * Delete the specified property.
     * <p>
     * Parameters should be passed as a JSON object in POST message body with
     * content type "application/json". Example of such object can be seen in
     * the result of the corresponding GET function.
     * </p>
     * %param resourceId resource id.
     * %param name property name to remove.
     */
    regUpdate<ResourceParamWithRefData>(p, ApiCommand::removeResourceParam);

    /**%apidoc POST /ec2/saveUserRole
     * <p>
     * Parameters should be passed as a JSON object in POST message body with
     * content type "application/json". Example of such object can be seen in
     * the result of the corresponding GET function.
     * </p>
     * %permissions Administrator.
     * %param[opt] id User role unique id. Can be omitted when creating a new object. If such
     *     object exists, omitted fields will not be changed.
     * %param name User role name.
     * %param permissions Combination (via "|") of the flags defining permissions.
     *     %value GlobalEditCamerasPermission Can edit camera settings.
     *     %value GlobalControlVideoWallPermission Can control video walls.
     *     %value GlobalViewArchivePermission Can view archives of available cameras.
     *     %value GlobalExportPermission Can export archives of available cameras.
     *     %value GlobalViewBookmarksPermission Can view bookmarks of available cameras.
     *     %value GlobalManageBookmarksPermission Can modify bookmarks of available cameras.
     *     %value GlobalUserInputPermission Can change PTZ state of a camera, use 2-way audio, I/O
     *         buttons.
     *     %value GlobalAccessAllMediaPermission Has access to all media (cameras and web pages).
     * %// AbstractUserManager::saveUserRole
     */
    regUpdate<UserRoleData>(p, ApiCommand::saveUserRole);

    /**%apidoc POST /ec2/removeUserRole
     * Delete the specified user role.
     * <p>
     * Parameters should be passed as a JSON object in POST message body with
     * content type "application/json". Example of such object can be seen in
     * the result of the corresponding GET function.
     * </p>
     * %permissions Administrator.
     * %param id User role unique id.
     * %// AbstractUserManager::removeUserRole
     */
    regUpdate<IdData>(p, ApiCommand::removeUserRole);

    /**%apidoc GET /ec2/getPredefinedRoles
    * Return list of predefined user roles.
    * %param[default] format
    * %return List of predefined user role objects in the requested format.
    */
    regGet<std::nullptr_t, PredefinedRoleDataList>(p, ApiCommand::getPredefinedRoles);

    /**%apidoc GET /ec2/getVideowalls
     * Return list of video walls
     * %param[default] format
     * %return List of video wall objects in the requested format.
     * %// AbstractVideowallManager::getVideowalls
     */
    regGet<QnUuid, VideowallDataList>(p, ApiCommand::getVideowalls);
    // AbstractVideowallManager::save
    regUpdate<VideowallData>(p, ApiCommand::saveVideowall);
    // AbstractVideowallManager::remove
    regUpdate<IdData>(p, ApiCommand::removeVideowall);
    regUpdate<VideowallControlMessageData>(p, ApiCommand::videowallControl);

    /**%apidoc GET /ec2/getWebPages
     * Return list of web pages
     * %param[default] format
     * %return List of web page objects in the requested format.
     * %// AbstractWebPageManager::getWebPages
     */
    regGet<QnUuid, WebPageDataList>(p, ApiCommand::getWebPages);

    /**%apidoc POST /ec2/saveWebPage
     * <p>
     * Parameters should be passed as a JSON object in POST message body with
     * content type "application/json". Example of such object can be seen in
     * the result of the corresponding GET function.
     * </p>
     * %permissions Administrator.
     * %param[opt] id Web page unique id. Can be omitted when creating a new object. If such object
     *     exists, omitted fields will not be changed.
     * %param[proprietary] parentId
     * %param name Web page name.
     * %param url Web page url.
     * %param[proprietary] typeId Should have fixed value.
     *     %value {57d7112a-b9c3-247b-c045-1660250adae5}
     * %// AbstractWebPageManager::save
     */
    regUpdate<WebPageData>(p, ApiCommand::saveWebPage);


    /**%apidoc POST /ec2/removeWebPage
     * Delete the specified web page.
     * <p>
     * Parameters should be passed as a JSON object in POST message body with
     * content type "application/json". Example of such object can be seen in
     * the result of the corresponding GET function.
     * </p>
     * %permissions Administrator.
     * %param id Web page unique id.
     * %// AbstractWebPageManager::remove
     */
    regUpdate<IdData>(p, ApiCommand::removeWebPage);

    // For analytics plugins and engines we provide only getters, since such resources
    // can't be created or removed via REST API for now.

    /**%apidoc GET /ec2/getAnalyticsPlugins
     * %param[default] format
     * %return List of analytics plugins in the requested format.
     * %// AbstractAnalyticsManager::getAnalyticsPlugins
     */
    regGet<QnUuid, AnalyticsPluginDataList>(p, ApiCommand::getAnalyticsPlugins);

    /**%apidoc GET /ec2/getAnalyticsEngines
     * %param[default] format
     * %return List of analytics engines in the requested format.
     * %// AbstractAnalyticsManager::getAnalyticsEngines
     */
    regGet<QnUuid, AnalyticsEngineDataList>(p, ApiCommand::getAnalyticsEngines);

    /**%apidoc GET /ec2/getLayouts
     * Return list of user layout
     * %param[default] format
     * %param[opt]:string id Layout unique id or logical id. If omitted, return data for all
     *     layouts.
     * %return List of layout objects in the requested format.
     * %// AbstractLayoutManager::getLayouts
     */
    regGet<QnLayoutUuid, LayoutDataList>(p, ApiCommand::getLayouts);

    /**%apidoc POST /ec2/saveLayout
     * Save layout.
     * <p>
     * Parameters should be passed as a JSON object in POST message body with
     * content type "application/json". Example of such object can be seen in
     * the result of the corresponding GET function.
     * </p>
     * %permissions Administrator, or a user who owns the layout.
     * %param[opt] id Layout unique id. Can be omitted when creating a new object. If such object
     *     exists, omitted fields will not be changed.
     * %param parentId Unique id of the user owning the layout.
     * %param name Layout name.
     * %param url Should be empty string.
     * %param[proprietary] typeId Should have fixed value.
     *     %value {e02fdf56-e399-2d8f-731d-7a457333af7f}
     * %param cellAspectRatio Aspect ratio of a cell for layout items
     *     (floating-point).
     * %param cellSpacing Cell spacing between layout items as percent of an item's size
     *     (floating-point, 0..1).
     * %param items List of the layout items.
     *     %param items[].id Item unique id. Can be omitted when creating a new object.
     *     %param items[].flags Should have fixed value.
     *         %value 0
     *     %param items[].left Left coordinate of the layout item (floating-point).
     *     %param items[].top Top coordinate of the layout item (floating-point).
     *     %param items[].right Right coordinate of the layout item (floating-point).
     *     %param items[].bottom Bottom coordinate of the layout item (floating-point).
     *     %param items[].rotation Degree of image tilt; a positive value rotates
     *         counter-clockwise (floating-point, 0..360).
     *     %param items[].resourceId Camera unique id.
     *     %param items[].resourcePath If the item represents a local file - URL of the file,
     *         otherwise is empty. Can be filled with the camera logical id when saving layout.
     *     %param items[].zoomLeft Left coordinate of the displayed window inside
     *         the camera image, as a fraction of the image width
     *         (floating-point, 0..1).
     *     %param items[].zoomTop Top coordinate of the displayed window inside
     *         the camera image, as a fraction of the image height
     *         (floating-point, 0..1).
     *     %param items[].zoomRight Right coordinate of the displayed window inside
     *         the camera image, as a fraction of the image width
     *         (floating-point, 0..1).
     *     %param items[].zoomBottom Bottom coordinate of the displayed window inside
     *         the camera image, as a fraction of the image width
     *         (floating-point, 0..1).
     *     %param items[].zoomTargetId Unique id of the original layout item for
     *         which the zoom window was created.
     *     %param items[].contrastParams Image enhancement parameters.
     *         %param contrastParams.enabled Whether the image enhancement is enabled.
     *             %value false
     *             %value true
     *         %param contrastParams.blackLevel Level of the black color
     *             (floating-point, 0..99).
     *         %param contrastParams.whiteLevel Level of the white color
     *             (floating-point, 0.1..100).
     *         %param contrastParams.gamma Gamma enhancement value
     *             (floating-point, 0.1..10).
     *     %param items[].dewarpingParams Image dewarping parameters.
     *         %param dewarpingParams.enabled Whether the image dewarping is enabled.
     *             %value false
     *             %value true
     *         %param dewarpingParams.xAngle Pan in radians.
     *         %param dewarpingParams.yAngle Tilt in radians.
     *         %param dewarpingParams.fov FOV in radians.
     *         %param dewarpingParams.panoFactor Aspect ratio correction value (integer).
     *     %param items[].displayInfo Whether to display info for the layout item.
     *         %value false
     *         %value true
     * %param locked Whether the layout is locked.
     *     %value false
     *     %value true
     * %param fixedWidth Fixed width of the layout in cells (integer).
     * %param fixedHeight Fixed height of the layout in cells (integer).
     * %param logicalId Logical id of the layout, set by user (integer).
     * %param backgroundImageFilename
     * %param backgroundWidth Width of the background image in cells (integer).
     * %param backgroundHeight Height of the background image in cells (integer).
     * %param backgroundOpacity Level of opacity of the background image (floating-point, 0..1).
     * %// AbstractLayoutManager::save
     */
    regUpdate<LayoutData>(p, ApiCommand::saveLayout);

    /**%apidoc:arrayParams POST /ec2/saveLayouts
     * Save the list of layouts.
     * <p>
     * Parameters should be passed as a JSON array of objects in POST message body with
     * content type "application/json". Example of such object can be seen in
     * the result of the corresponding GET function.
     * </p>
     * %param id Layout unique id. Can be omitted when creating a new object. If such object
     *     exists, omitted fields will not be changed.
     * %param parentId Unique id of the user owning the layout.
     * %param name Layout name.
     * %param url Should be empty string.
     * %param[proprietary] typeId Should have fixed value.
     *     %value {e02fdf56-e399-2d8f-731d-7a457333af7f}
     * %param cellAspectRatio Aspect ratio of a cell for layout items
     *     (floating-point).
     * %param cellSpacing Cell spacing between layout items as percent of an item's size
     *     (floating-point, 0..1).
     * %param items List of the layout items.
     *     %param items[].id Item unique id. Can be omitted when creating a new object.
     *     %param items[].flags Should have fixed value.
     *         %value 0
     *     %param items[].left Left coordinate of the layout item (floating-point).
     *     %param items[].top Top coordinate of the layout item (floating-point).
     *     %param items[].right Right coordinate of the layout item (floating-point).
     *     %param items[].bottom Bottom coordinate of the layout item (floating-point).
     *     %param items[].rotation Degree of image tilt; a positive value rotates
     *         counter-clockwise (floating-point, 0..360).
     *     %param items[].resourceId Camera unique id.
     *     %param items[].resourcePath If the item represents a local file - URL of the file,
     *         otherwise is empty. Can be filled with the camera logical id when saving layout.
     *     %param items[].zoomLeft Left coordinate of the displayed window inside
     *         the camera image, as a fraction of the image width
     *         (floating-point, 0..1).
     *     %param items[].zoomTop Top coordinate of the displayed window inside
     *         the camera image, as a fraction of the image height
     *         (floating-point, 0..1).
     *     %param items[].zoomRight Right coordinate of the displayed window inside
     *         the camera image, as a fraction of the image width
     *         (floating-point, 0..1).
     *     %param items[].zoomBottom Bottom coordinate of the displayed window inside
     *         the camera image, as a fraction of the image width
     *         (floating-point, 0..1).
     *     %param items[].zoomTargetId Unique id of the original layout item for
     *         which the zoom window was created.
     *     %param items[].contrastParams Image enhancement parameters.
     *         %param contrastParams.enabled Whether the image enhancement is enabled.
     *             %value false
     *             %value true
     *         %param contrastParams.blackLevel Level of the black color
     *             (floating-point, 0..99).
     *         %param contrastParams.whiteLevel Level of the white color
     *             (floating-point, 0.1..100).
     *         %param contrastParams.gamma Gamma enhancement value
     *             (floating-point, 0.1..10).
     *     %param items[].dewarpingParams Image dewarping parameters.
     *         %param dewarpingParams.enabled Whether the image dewarping is enabled.
     *             %value false
     *             %value true
     *         %param dewarpingParams.xAngle Pan in radians.
     *         %param dewarpingParams.yAngle Tilt in radians.
     *         %param dewarpingParams.fov FOV in radians.
     *         %param dewarpingParams.panoFactor Aspect ratio correction value (integer).
     *     %param items[].displayInfo Whether to display info for the layout item.
     *         %value false
     *         %value true
     * %param locked Whether the layout is locked.
     *     %value false
     *     %value true
     * %param fixedWidth Fixed width of the layout in cells (integer).
     * %param fixedHeight Fixed height of the layout in cells (integer).
     * %param logicalId Logical id of the layout, set by user (integer).
     * %param backgroundImageFilename
     * %param backgroundWidth Width of the background image in pixels (integer).
     * %param backgroundHeight Height of the background image in pixels (integer).
     * %param backgroundOpacity Level of opacity of the background image in pixels (floating-point
     *     0..1).
     * %// AbstractLayoutManager::save
     */
    regUpdate<LayoutDataList>(p, ApiCommand::saveLayouts);

    /**%apidoc POST /ec2/removeLayout
     * Delete the specified layout.
     * <p>
     * Parameters should be passed as a JSON object in POST message body with
     * content type "application/json". Example of such object can be seen in
     * the result of the corresponding GET function.
     * </p>
     * %permissions Administrator, or a user who owns the layout.
     * %param id Unique id of the layout to be deleted.
     * %// AbstractLayoutManager::remove
     */
    regUpdate<IdData>(p, ApiCommand::removeLayout);

    /**%apidoc GET /ec2/getLayoutTours
    * Return list of layout tours
    * %param[default] format
    * %param[opt] id Layout tour unique id. If omitted, return data for all tours.
    * %return List of layout tour objects in the requested format.
    * %// AbstractLayoutManager::getLayoutTours
    */
    regGet<QnUuid, LayoutTourDataList>(p, ApiCommand::getLayoutTours);

    /**%apidoc POST /ec2/saveLayoutTour
    * Save layout tour.
    * <p>
    * Parameters should be passed as a JSON object in POST message body with
    * content type "application/json". Example of such object can be seen in
    * the result of the corresponding GET function.
    * </p>
    * %permissions Administrator
    * %param[opt] id Layout tour unique id. Can be omitted when creating a new object. If such
    *     object exists, omitted fields will not be changed.
    * %param parentId
    * %param name Tour name.
    * %param items List of the layout tour items.
    * %param items[].resourceId Resource unique id. Can be a layout or a camera or something else.
    * %param items[].delayMs Delay between layouts switching in milliseconds.
    * %param[opt] settings
    * %param[opt] settings.manual
    * %// AbstractLayoutTourManager::save
    */
    regUpdate<LayoutTourData>(p, ApiCommand::saveLayoutTour);

    /**%apidoc POST /ec2/removeLayoutTour
    * Delete the specified layout tour.
    * <p>
    * Parameters should be passed as a JSON object in POST message body with
    * content type "application/json". Example of such object can be seen in
    * the result of the corresponding GET function.
    * </p>
    * %permissions Administrator
    * %param id Unique id of the layout tour to be deleted.
    * %// AbstractLayoutTourManager::remove
    */
    regUpdate<IdData>(p, ApiCommand::removeLayoutTour);

    /**%apidoc GET /ec2/listDirectory
     * Return list of folders and files in a virtual FS stored inside
     * database. This function is used to add files (such audio for notifications)
     * to database.
     * %param[default] format
     * %param[unused] path
     *     %// NOTE: ApiStoredFilePath.path is serialized as "folder".
     * %param[opt] folder Folder name in a virtual FS
     * %return List of objects in the requested format.
     *    %// TODO: Describe params.
     * %// AbstractStoredFileManager::listDirectory
     */
    regGet<StoredFilePath, StoredFilePathList>(p, ApiCommand::listDirectory);

    /**%apidoc GET /ec2/getStoredFile
     * Read file data from a virtual FS
     * %param[default] format
     * %param[unused] path
     *     %// NOTE: ApiStoredFilePath.path is serialized as "folder".
     * %param[opt] folder File name
     * %return Object in the requested format.
     * %// AbstractStoredFileManager::getStoredFile
     */
    regGet<StoredFilePath, StoredFileData>(p, ApiCommand::getStoredFile);

    // AbstractStoredFileManager::addStoredFile
    regUpdate<StoredFileData>(p, ApiCommand::addStoredFile);
    // AbstractStoredFileManager::updateStoredFile
    regUpdate<StoredFileData>(p, ApiCommand::updateStoredFile);
    // AbstractStoredFileManager::deleteStoredFile
    regUpdate<StoredFilePath>(p, ApiCommand::removeStoredFile);

    // AbstractUpdatesManager::uploadUpdate
    regUpdate<UpdateUploadData>(p, ApiCommand::uploadUpdate);
    // AbstractUpdatesManager::uploadUpdateResponce
    regUpdate<UpdateUploadResponseData>(p, ApiCommand::uploadUpdateResponce);
    // AbstractUpdatesManager::installUpdate
    regUpdate<UpdateInstallData>(p, ApiCommand::installUpdate);

    // AbstractDiscoveryManager::discoveredServerChanged
    regUpdate<DiscoveredServerData>(p, ApiCommand::discoveredServerChanged);
    // AbstractDiscoveryManager::discoveredServersList
    regUpdate<DiscoveredServerDataList>(p, ApiCommand::discoveredServersList);

    // AbstractDiscoveryManager::discoverPeer
    regUpdate<DiscoverPeerData>(p, ApiCommand::discoverPeer);
    // AbstractDiscoveryManager::addDiscoveryInformation
    regUpdate<DiscoveryData>(p, ApiCommand::addDiscoveryInformation);
    // AbstractDiscoveryManager::removeDiscoveryInformation
    regUpdate<DiscoveryData>(p, ApiCommand::removeDiscoveryInformation);
    // AbstractDiscoveryManager::getDiscoveryData
    regGet<QnUuid, DiscoveryDataList>(p, ApiCommand::getDiscoveryData);
    // AbstractMiscManager::changeSystemId
    regUpdate<SystemIdData>(p, ApiCommand::changeSystemId);

    // AbstractECConnection
    regUpdate<DatabaseDumpData>(p, ApiCommand::restoreDatabase);

    /**%apidoc GET /ec2/getFullInfo
     * Read all data such as all servers, cameras, users, etc.
     * %param[default] format
     * %return Object in the requested format.
     */
    regGet<std::nullptr_t, FullInfoData>(p, ApiCommand::getFullInfo);

    /**%apidoc GET /ec2/getLicenses
     * Read license list
     * %param[default] format
     * %return List of license objects in the requested format.
     */
    regGet<std::nullptr_t, LicenseDataList>(p, ApiCommand::getLicenses);

    regGet<std::nullptr_t, DatabaseDumpData>(p, ApiCommand::dumpDatabase);
    regGet<StoredFilePath, DatabaseDumpToFileData>(p, ApiCommand::dumpDatabaseToFile);

    // AbstractECConnectionFactory
    regFunctorWithResponse<ConnectionData, QnConnectionInfo>(p, ApiCommand::connect,
        std::bind(&LocalConnectionFactory::fillConnectionInfo, this, _1, _2, _3));
    regFunctorWithResponse<ConnectionData, QnConnectionInfo>(p, ApiCommand::testConnection,
        std::bind(&LocalConnectionFactory::fillConnectionInfo, this, _1, _2, _3));

    /**%apidoc GET /ec2/getSettings
     * Read general System settings such as email address, etc.
     * %param[default] format
     * %return List of objects in the requested format.
     */
    regFunctor<std::nullptr_t, ResourceParamDataList>(p, ApiCommand::getSettings,
        std::bind(&LocalConnectionFactory::getSettings, this, _1, _2, _3));

    // Ec2StaticticsReporter
    regFunctor<std::nullptr_t, ApiSystemStatistics>(p, ApiCommand::getStatisticsReport,
        [this](std::nullptr_t, ApiSystemStatistics* const out, const Qn::UserAccessData&)
        {
            if (!m_directConnection)
                return ErrorCode::failure;
            return m_directConnection->getStaticticsReporter()->collectReportData(nullptr, out);
        });
    regFunctor<ApiStatisticsServerArguments, ApiStatisticsServerInfo>(
        p, ApiCommand::triggerStatisticsReport,
        [this](const ApiStatisticsServerArguments& in,
            ApiStatisticsServerInfo* const out, const Qn::UserAccessData&)
        {
            if (!m_directConnection)
                return ErrorCode::failure;
            return m_directConnection->getStaticticsReporter()->triggerStatisticsReport(in, out);
        });

    p->registerHandler("ec2/activeConnections", new QnActiveConnectionsRestHandler(m_bus.get()));

#if 0 // Using HTTP processor since HTTP REST does not support HTTP interleaving.
    p->registerHandler(
        QLatin1String(kIncomingTransactionsPath),
        new QnRestTransactionReceiver());
#endif // 0
}

int LocalConnectionFactory::establishDirectConnection(
    const nx::utils::Url& url, impl::ConnectHandlerPtr handler)
{
    const int reqId = generateRequestID();

    ConnectionData loginInfo;
    QnConnectionInfo connectionInfo;
    fillConnectionInfo(loginInfo, &connectionInfo); // < TODO: #ak not appropriate here
    connectionInfo.ecUrl = url;
    ErrorCode connectionInitializationResult = ErrorCode::ok;
    {
        QnMutexLocker lk(&m_mutex);
        if (!m_directConnection)
        {
            m_directConnection.reset(
                new Ec2DirectConnection(
                    this,
                    m_serverQueryProcessor.get(),
                    connectionInfo,
                    url));
            m_timeSynchronizationManager->init(m_directConnection);

            messageBus()->setHandler(m_directConnection->notificationManager());

            if (m_directConnection->initialized())
            {
                m_serverConnector->startReceivingNotifications(m_directConnection.get());
            }
            else
            {
                connectionInitializationResult = ErrorCode::dbError;
                messageBus()->removeHandler(m_directConnection->notificationManager());
                m_directConnection.reset();
            }

        }
    }
    nx::utils::concurrent::run(
        Ec2ThreadPool::instance(),
        std::bind(
            &impl::ConnectHandler::done,
            handler,
            reqId,
            connectionInitializationResult,
            m_directConnection));
    return reqId;
}

void LocalConnectionFactory::tryConnectToOldEC(const nx::utils::Url& ecUrl,
    impl::ConnectHandlerPtr handler, int reqId)
{
    // Checking for old EC.
    nx::utils::concurrent::run(
        Ec2ThreadPool::instance(),
        [this, ecUrl, handler, reqId]()
    {
        using namespace std::placeholders;
        return connectToOldEC(
            ecUrl,
            [reqId, handler](
                ErrorCode errorCode, const QnConnectionInfo& oldECConnectionInfo)
        {
            if (errorCode == ErrorCode::ok
                && oldECConnectionInfo.version >= nx::utils::SoftwareVersion(2, 3, 0))
            {
                // Somehow connected to 2.3 server with old ec connection. Returning
                // error, since could not connect to ec 2.3 during normal connect.
                handler->done(
                    reqId,
                    ErrorCode::ioError,
                    AbstractECConnectionPtr());
            }
            else
            {
                handler->done(
                    reqId,
                    errorCode,
                    errorCode == ErrorCode::ok
                    ? std::make_shared<OldEcConnection>(oldECConnectionInfo)
                    : AbstractECConnectionPtr());
            }
        });
    });
}

const char oldEcConnectPath[] = "/api/connect/?format=pb&guid&ping=1";

static bool parseOldECConnectionInfo(
    const QByteArray& oldECConnectResponse, QnConnectionInfo* const connectionInfo)
{
    static const char PROTOBUF_FIELD_TYPE_STRING = 0x0a;

    if (oldECConnectResponse.isEmpty())
        return false;

    const char* data = oldECConnectResponse.data();
    const char* dataEnd = oldECConnectResponse.data() + oldECConnectResponse.size();
    if (data + 2 >= dataEnd)
        return false;
    if (*data != PROTOBUF_FIELD_TYPE_STRING)
        return false;
    ++data;
    const int fieldLen = *data;
    ++data;
    if (data + fieldLen >= dataEnd)
        return false;
    connectionInfo->version = nx::vms::api::SoftwareVersion(QByteArray::fromRawData(data, fieldLen));
    return true;
}

template<class Handler>
void LocalConnectionFactory::connectToOldEC(const nx::utils::Url& ecUrl, Handler completionFunc)
{
    nx::utils::Url httpsEcUrl = ecUrl; // < Old EC supports only https.
    httpsEcUrl.setScheme(lit("https"));

    QAuthenticator auth;
    auth.setUser(httpsEcUrl.userName());
    auth.setPassword(httpsEcUrl.password());
    CLSimpleHTTPClient simpleHttpClient(httpsEcUrl, 3000, auth);
    const CLHttpStatus statusCode = simpleHttpClient.doGET(
        QByteArray::fromRawData(oldEcConnectPath, sizeof(oldEcConnectPath)));
    switch (statusCode)
    {
    case CL_HTTP_SUCCESS:
    {
        // Reading mesasge body.
        QByteArray oldECResponse;
        simpleHttpClient.readAll(oldECResponse);
        QnConnectionInfo oldECConnectionInfo;
        oldECConnectionInfo.ecUrl = httpsEcUrl;
        if (parseOldECConnectionInfo(oldECResponse, &oldECConnectionInfo))
        {
            if (oldECConnectionInfo.version >= nx::utils::SoftwareVersion(2, 3))
            {
                // Ignoring response from 2.3+ server received using compatibility response.
                completionFunc(ErrorCode::ioError, QnConnectionInfo());
            }
            else
            {
                completionFunc(ErrorCode::ok, oldECConnectionInfo);
            }
        }
        else
        {
            completionFunc(ErrorCode::badResponse, oldECConnectionInfo);
        }
        break;
    }

    case CL_HTTP_AUTH_REQUIRED:
        completionFunc(ErrorCode::unauthorized, QnConnectionInfo());
        break;

    case CL_HTTP_FORBIDDEN:
        completionFunc(ErrorCode::forbidden, QnConnectionInfo());
        break;

    default:
        completionFunc(ErrorCode::ioError, QnConnectionInfo());
        break;
    }

    QnMutexLocker lk(&m_mutex);
    --m_runningRequests;
}

ErrorCode LocalConnectionFactory::fillConnectionInfo(
    const ConnectionData& loginInfo,
    QnConnectionInfo* const connectionInfo,
    nx::network::http::Response* response)
{
    connectionInfo->version = qnStaticCommon->engineVersion();
    connectionInfo->brand = qnStaticCommon->brand();
    connectionInfo->customization = qnStaticCommon->customization();
    connectionInfo->systemName = commonModule()->globalSettings()->systemName();
    connectionInfo->ecsGuid = commonModule()->moduleGUID().toString();
    connectionInfo->cloudSystemId = commonModule()->globalSettings()->cloudSystemId();
    connectionInfo->localSystemId = commonModule()->globalSettings()->localSystemId();
#if defined(__arm__) || defined(__aarch64__)
    connectionInfo->box = QnAppInfo::armBox();
#endif
    connectionInfo->allowSslConnections = m_sslEnabled;
    connectionInfo->nxClusterProtoVersion = nx_ec::EC2_PROTO_VERSION;
    connectionInfo->ecDbReadOnly = m_ecDbReadOnly;
    connectionInfo->newSystem = commonModule()->globalSettings()->isNewSystem();
    connectionInfo->p2pMode = m_p2pMode;
    if (response)
    {
        connectionInfo->effectiveUserName =
            nx::network::http::getHeaderValue(response->headers, Qn::EFFECTIVE_USER_NAME_HEADER_NAME);
    }

#ifdef ENABLE_EXTENDED_STATISTICS
    if (!loginInfo.clientInfo.id.isNull())
    {
        auto clientInfo = loginInfo.clientInfo;
        clientInfo.parentId = commonModule()->moduleGUID();

        nx::vms::api::ClientInfoDataList infoList;
        auto result = dbManager(Qn::kSystemAccess).doQuery(clientInfo.id, infoList);
        if (result != ErrorCode::ok)
            return result;

        if (infoList.size() > 0
            && QJson::serialized(clientInfo) == QJson::serialized(infoList.front()))
        {
            NX_VERBOSE(this, lit("New client had already been registered with the same params"));
            return ErrorCode::ok;
        }

        m_serverQueryProcessor.getAccess(Qn::kSystemAccess).processUpdateAsync(
            ApiCommand::saveClientInfo, clientInfo,
            [&](ErrorCode result)
        {
            if (result == ErrorCode::ok)
            {
                NX_INFO(this, lit("New client has been registered"));
            }
            else
            {
                NX_ERROR(this, lit("New client transaction has failed %1")
                    .arg(toString(result)));
            }
        });
    }
#else
    nx::utils::unused(loginInfo);
#endif

    return ErrorCode::ok;
}

int LocalConnectionFactory::testDirectConnection(
    const nx::utils::Url& /*addr*/,
    impl::TestConnectionHandlerPtr handler)
{
    const int reqId = generateRequestID();
    QnConnectionInfo connectionInfo;
    fillConnectionInfo(ConnectionData(), &connectionInfo);
    nx::utils::concurrent::run(
        Ec2ThreadPool::instance(),
        std::bind(
            &impl::TestConnectionHandler::done,
            handler,
            reqId,
            ErrorCode::ok,
            connectionInfo));
    return reqId;
}

ErrorCode LocalConnectionFactory::getSettings(
    std::nullptr_t,
    nx::vms::api::ResourceParamDataList* const outData,
    const Qn::UserAccessData& accessData)
{
    if (!m_dbManager)
        return ErrorCode::ioError;
    return QnDbManagerAccess(m_dbManager.get(), accessData).doQuery(ApiCommand::getSettings, nullptr, *outData);
}

template<class InputDataType, class ProcessedDataType>
void LocalConnectionFactory::regUpdate(
    QnRestProcessorPool* const restProcessorPool,
    ApiCommand::Value cmd,
    GlobalPermission permission)
{
    restProcessorPool->registerHandler(
        lit("ec2/%1").arg(ApiCommand::toString(cmd)),
        new UpdateHttpHandler<InputDataType, ProcessedDataType>(m_directConnection),
        permission);
}

template<class InputDataType, class CustomActionType>
void LocalConnectionFactory::regUpdate(
    QnRestProcessorPool* const restProcessorPool,
    ApiCommand::Value cmd,
    CustomActionType customAction,
    GlobalPermission permission)
{
    restProcessorPool->registerHandler(
        lit("ec2/%1").arg(ApiCommand::toString(cmd)),
        new UpdateHttpHandler<InputDataType>(m_directConnection, customAction),
        permission);
}

template<class InputDataType, class OutputDataType>
void LocalConnectionFactory::regGet(
    QnRestProcessorPool* const restProcessorPool,
    ApiCommand::Value cmd,
    GlobalPermission permission)
{
    restProcessorPool->registerHandler(
        lit("ec2/%1").arg(ApiCommand::toString(cmd)),
        new QueryHttpHandler<InputDataType, OutputDataType>(cmd, m_serverQueryProcessor.get()),
        permission);
}

template<class InputType, class OutputType>
void LocalConnectionFactory::regFunctor(
    QnRestProcessorPool* const restProcessorPool,
    ApiCommand::Value cmd,
    std::function<ErrorCode(InputType, OutputType*, const Qn::UserAccessData&)> handler, GlobalPermission permission)
{
    restProcessorPool->registerHandler(
        lit("ec2/%1").arg(ApiCommand::toString(cmd)),
        new FlexibleQueryHttpHandler<InputType, OutputType>(cmd, std::move(handler)),
        permission);
}

/**
* Register handler which is able to modify HTTP response.
*/
template<class InputType, class OutputType>
void LocalConnectionFactory::regFunctorWithResponse(
    QnRestProcessorPool* const restProcessorPool,
    ApiCommand::Value cmd,
    std::function<ErrorCode(InputType, OutputType*, nx::network::http::Response*)> handler,
    GlobalPermission permission)
{
    restProcessorPool->registerHandler(
        lit("ec2/%1").arg(ApiCommand::toString(cmd)),
        new FlexibleQueryHttpHandler<InputType, OutputType>(cmd, std::move(handler)),
        permission);
}

TransactionMessageBusAdapter* LocalConnectionFactory::messageBus() const
{
    return m_bus.get();
}

QnDistributedMutexManager* LocalConnectionFactory::distributedMutex() const
{
    return m_distributedMutexManager.get();
}

nx::vms::time_sync::AbstractTimeSyncManager* LocalConnectionFactory::timeSyncManager() const
{
    return m_timeSynchronizationManager.get();
}

nx::vms::network::ReverseConnectionManager* LocalConnectionFactory::serverConnector() const
{
    return m_serverConnector.get();
}

} // namespace ec2
