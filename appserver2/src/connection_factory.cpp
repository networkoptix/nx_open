#include "connection_factory.h"

#include <functional>

#include <api/global_settings.h>

#include <nx/utils/thread/mutex.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/simple_http_client.h>

#include <utils/common/app_info.h>
#include <utils/common/concurrent.h>
#include <network/http_connection_listener.h>
#include <nx_ec/ec_proto_version.h>
#include <nx_ec/data/api_access_rights_data.h>
#include <nx_ec/data/api_user_role_data.h>
#include <nx_ec/data/api_camera_history_data.h>

#include "rest/active_connections_rest_handler.h"
#include "compatibility/old_ec_connection.h"
#include "ec2_connection.h"
#include "ec2_thread_pool.h"
#include "remote_ec_connection.h"
#include "rest/ec2_base_query_http_handler.h"
#include "rest/ec2_update_http_handler.h"
#include "rest/time_sync_rest_handler.h"
#include "rest/server/rest_connection_processor.h"
#include "transaction/transaction.h"
#include "transaction/transaction_message_bus.h"
#include "http/ec2_transaction_tcp_listener.h"
#include "http/http_transaction_receiver.h"
#include "mutex/distributed_mutex_manager.h"

namespace ec2 {

static const char* const kIncomingTransactionsPath = "ec2/forward_events";

Ec2DirectConnectionFactory::Ec2DirectConnectionFactory(
    Qn::PeerType peerType,
    nx::utils::TimerManager* const timerManager)
    :
    // dbmanager is initialized by direct connection.
    m_dbManager(peerType == Qn::PT_Server ? new detail::QnDbManager() : nullptr),
    m_timeSynchronizationManager(new TimeSynchronizationManager(peerType, timerManager)),
    m_transactionMessageBus(new QnTransactionMessageBus(peerType)),
    m_terminated(false),
    m_runningRequests(0),
    m_sslEnabled(false)
{
    // Cannot be done in TimeSynchronizationManager constructor to keep valid object destruction
    // order.
    m_timeSynchronizationManager->start();

    // TODO: #Elric #EC2 register in a proper place!
    // Registering ec2 types with Qt meta-type system.
    qRegisterMetaType<QnTransactionTransportHeader>("QnTransactionTransportHeader");

    QnDistributedMutexManager::initStaticInstance(new QnDistributedMutexManager());

    // TODO: Add comment why this code is commented out.
    //m_transactionMessageBus->start();
}

Ec2DirectConnectionFactory::~Ec2DirectConnectionFactory()
{
    pleaseStop();
    join();

    // Have to do it before m_transactionMessageBus destruction since TimeSynchronizationManager
    // uses QnTransactionMessageBus.
    m_timeSynchronizationManager->pleaseStop();

    QnDistributedMutexManager::initStaticInstance(0);
}

void Ec2DirectConnectionFactory::pleaseStop()
{
    QnMutexLocker lk(&m_mutex);
    m_terminated = true;
}

void Ec2DirectConnectionFactory::join()
{
    // Cancelling all ongoing requests.
    m_remoteQueryProcessor.pleaseStopSync();
}

// Implementation of AbstractECConnectionFactory::testConnectionAsync
int Ec2DirectConnectionFactory::testConnectionAsync(
    const QUrl& addr, impl::TestConnectionHandlerPtr handler)
{
    QUrl url = addr;
    url.setUserName(url.userName().toLower());

    QUrlQuery query(url);
    query.removeQueryItem(lit("format"));
    query.addQueryItem(lit("format"), QnLexical::serialized(Qn::JsonFormat));
    url.setQuery(query);

    if (url.isEmpty())
        return testDirectConnection(url, handler);
    else
        return testRemoteConnection(url, handler);
}

// Implementation of AbstractECConnectionFactory::connectAsync
int Ec2DirectConnectionFactory::connectAsync(
    const QUrl& addr, const ApiClientInfoData& clientInfo, impl::ConnectHandlerPtr handler)
{
    QUrl url = addr;
    url.setUserName(url.userName().toLower());

    if (m_transactionMessageBus->localPeer().isMobileClient())
    {
        QUrlQuery query(url);
        query.removeQueryItem(lit("format"));
        query.addQueryItem(lit("format"), QnLexical::serialized(Qn::JsonFormat));
        url.setQuery(query);
    }

    if (url.scheme() == "file")
        return establishDirectConnection(url, handler);
    else
        return establishConnectionToRemoteServer(url, handler, clientInfo);
}

void Ec2DirectConnectionFactory::registerTransactionListener(
    QnHttpConnectionListener* httpConnectionListener)
{
    httpConnectionListener->addHandler<QnTransactionTcpProcessor>(
        "HTTP", "ec2/events");
    httpConnectionListener->addHandler<QnHttpTransactionReceiver>(
        "HTTP", kIncomingTransactionsPath);

    m_sslEnabled = httpConnectionListener->isSslEnabled();
}

void Ec2DirectConnectionFactory::registerRestHandlers(QnRestProcessorPool* const p)
{
    using namespace std::placeholders;

    /**%apidoc GET /ec2/getResourceTypes
     * Read all resource types. Resource type contains object type such as
     * "Server", "Camera", etc. Also, resource type contains additional information
     * for cameras such as maximum FPS, resolution, etc.
     * %param[default] format
     * %return Object in the requested format.
     * %// AbstractResourceManager::getResourceTypes
     */
    regGet<nullptr_t, ApiResourceTypeDataList>(p, ApiCommand::getResourceTypes);

    // AbstractResourceManager::setResourceStatus
    regUpdate<ApiResourceStatusData>(p, ApiCommand::setResourceStatus);

    /**%apidoc GET /ec2/getResourceParams
     * Read resource (camera, user or server) additional parameters (camera firmware version, etc).
     * The list of parameters depends on the resource type.
     * %param[default] format
     * %param id Resource unique id.
     * %return Object in the requested format.
     * %// AbstractResourceManager::getKvPairs
     */
    regGet<QnUuid, ApiResourceParamWithRefDataList>(p, ApiCommand::getResourceParams);

    // AbstractResourceManager::save
    regUpdate<ApiResourceParamWithRefDataList>(p, ApiCommand::setResourceParams);

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
    regUpdate<ApiIdData>(p, ApiCommand::removeResource);

    /**%apidoc GET /ec2/getStatusList
     * Read current status of the resources: cameras, servers and storages.
     * %param[default] format
     * %param[opt] id Resource unique id. If omitted, return data for all resources.
     * %return List of status objects in the requested format.
     *     %// TODO: Describe ResourceStatus fields.
     */
    regGet<QnUuid, ApiResourceStatusDataList>(p, ApiCommand::getStatusList);

    // AbstractMediaServerManager::getServers
    regGet<QnUuid, ApiMediaServerDataList>(p, ApiCommand::getMediaServers);

    // AbstractMediaServerManager::save
    regUpdate<ApiMediaServerData>(p, ApiCommand::saveMediaServer);

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
     * %param backupBitrate Maximum backup bitrate (in bytes per second). Negative
     *     value if not limited.
     * %// AbstractCameraManager::saveUserAttributes
     */
    regUpdate<ApiMediaServerUserAttributesData>(p, ApiCommand::saveMediaServerUserAttributes);

    /**%apidoc POST /ec2/saveMediaServerUserAttributesList
     * Save additional attributes of a number of servers.
     * <p>
     * Parameters should be passed as a JSON array of objects in POST message body with
     * content type "application/json". Example of such object can be seen in
     * the result of the corresponding GET function.
     * </p>
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
     * %param backupDaysOfTheWeek Combination (via "|") of weekdays
     *     the backup is active on.
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
    regUpdate<ApiMediaServerUserAttributesDataList>(p, ApiCommand::saveMediaServerUserAttributesList);

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
    regGet<QnUuid, ApiMediaServerUserAttributesDataList>(p, ApiCommand::getMediaServerUserAttributesList);

    // AbstractMediaServerManager::remove
    regUpdate<ApiIdData>(p, ApiCommand::removeMediaServer);

    /**%apidoc GET /ec2/getMediaServersEx
     * Return server list.
     * %param[default] format
     * %return Server object in the requested format.
     * %// AbstractMediaServerManager::getServersEx
     */
    regGet<QnUuid, ApiMediaServerDataExList>(p, ApiCommand::getMediaServersEx);

    regUpdate<ApiStorageDataList>(p, ApiCommand::saveStorages);

    /**%apidoc POST /ec2/saveStorage
     * Save the storage.
     * <p>
     * Parameters should be passed as a JSON object in POST message body with
     * content type "application/json". Example of such object can be seen in
     * the result of the corresponding GET function.
     * </p>
     * %permissions Administrator.
     * %param[opt] id Storage unique id. Can be omitted when creating a new object.
     * %param parentId Server unique id.
     * %param name Storage name.
     * %param url Should be empty.
     * %param spaceLimit Free space to maintain on the storage,
     *     in bytes. Recommended free space is about 5 gigabytes.
     * %param usedForWriting Whether writing to the storage is
     *         allowed.
     *     %value false
     *     %value true
     * %param storageType Type of the method to access the storage.
     *     %value local
     *     %value smb
     * %param addParams List of storage additional parameters. Intended for
     *     internal use; leave empty when creating a new storage.
     * %param isBackup Whether the storage is used for backup.
     *     %value false
     *     %value true
     */
    regUpdate<ApiStorageData>(p, ApiCommand::saveStorage);

    regUpdate<ApiIdDataList>(p, ApiCommand::removeStorages);
    regUpdate<ApiIdData>(p, ApiCommand::removeStorage);
    regUpdate<ApiIdDataList>(p, ApiCommand::removeResources);

    // AbstractCameraManager::addCamera
    regUpdate<ApiCameraData>(p, ApiCommand::saveCamera);

    // AbstractCameraManager::save
    regUpdate<ApiCameraDataList>(p, ApiCommand::saveCameras);

    // AbstractCameraManager::getCameras
    regGet<QnUuid, ApiCameraDataList>(p, ApiCommand::getCameras);

    /**%apidoc POST /ec2/saveCameraUserAttributesList
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
     *     %param scheduleTask.startTime Time of day when the backup starts (in seconds passed
     *         from 00:00:00).
     *     %param scheduleTask.endTime Time of day when the backup ends (in seconds passed
     *         from 00:00:00).
     *     %param scheduleTask.recordAudio Whether to record sound.
     *         %value false
     *         %value true
     *     %param scheduleTask.recordingType
     *         %value RT_Always Record always.
     *         %value RT_MotionOnly Record only when the motion is detected.
     *         %value RT_Never Never record.
     *         %value RT_MotionAndLowQuality Always record low quality
     *             stream, and record high quality stream on motion.
     *     %param scheduleTask.dayOfWeek Day of week for the recording task.
     *         %value 1 Monday
     *         %value 2 Tuesday
     *         %value 3 Wednesday
     *         %value 4 Thursday
     *         %value 5 Friday
     *         %value 6 Saturday
     *         %value 7 Sunday
     *     %param scheduleTask.beforeThreshold The number of seconds before a motion event to
     *         record the video for.
     *     %param scheduleTask.afterThreshold The number of seconds after a motion event to
     *         record the video for.
     *     %param scheduleTask.streamQuality Quality of the recording.
     *         %value QualityLowest
     *         %value QualityLow
     *         %value QualityNormal
     *         %value QualityHigh
     *         %value QualityHighest
     *         %value QualityPreSet
     *         %value QualityNotDefined
     *     %param scheduleTask.fps Frames per second (integer).
     * %param audioEnabled Whether audio is enabled on the camera.
     *     %value false
     *     %value true
     * %param secondaryStreamQuality
     *     %value SSQualityLow Low quality second stream.
     *     %value SSQualityMedium Medium quality second stream.
     *     %value SSQualityHigh High quality second stream.
     *     %value SSQualityNotDefined Second stream quality is not defined.
     *     %value SSQualityDontUse Second stream is not used for the camera.
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
    regUpdate<ApiCameraAttributesDataList>(p, ApiCommand::saveCameraUserAttributesList);

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
     *     %param scheduleTask.startTime Time of day when the backup starts (in seconds passed
     *         from 00:00:00).
     *     %param scheduleTask.endTime Time of day when the backup ends (in seconds passed
     *         from 00:00:00).
     *     %param scheduleTask.recordAudio Whether to record sound.
     *         %value false
     *         %value true
     *     %param scheduleTask.recordingType
     *         %value RT_Always Record always.
     *         %value RT_MotionOnly Record only when the motion is detected.
     *         %value RT_Never Never record.
     *         %value RT_MotionAndLowQuality Always record low quality
     *             stream, and record high quality stream on motion.
     *     %param scheduleTask.dayOfWeek Weekday for the recording task.
     *         %value 1 Monday
     *         %value 2 Tuesday
     *         %value 3 Wednesday
     *         %value 4 Thursday
     *         %value 5 Friday
     *         %value 6 Saturday
     *         %value 7 Sunday
     *     %param scheduleTask.beforeThreshold The number of seconds before a motion event to
     *         record the video for.
     *     %param scheduleTask.afterThreshold The number of seconds after a motion event to
     *         record the video for.
     *     %param scheduleTask.streamQuality Quality of the recording.
     *         %value QualityLowest
     *         %value QualityLow
     *         %value QualityNormal
     *         %value QualityHigh
     *         %value QualityHighest
     *         %value QualityPreSet
     *         %value QualityNotDefined
     *     %param scheduleTask.fps Frames per second (integer).
     * %param audioEnabled Whether audio is enabled on the camera.
     *     %value false
     *     %value true
     * %param secondaryStreamQuality
     *     %value SSQualityLow Low quality second stream.
     *     %value SSQualityMedium Medium quality second stream.
     *     %value SSQualityHigh High quality second stream.
     *     %value SSQualityNotDefined Second stream quality is not defined.
     *     %value SSQualityDontUse Second stream is not used for the camera.
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
    regUpdate<ApiCameraAttributesData>(p, ApiCommand::saveCameraUserAttributes);

    /**%apidoc GET /ec2/getCameraUserAttributesList
     * Read additional camera attributes.
     * %param[default] format
     * %param[opt] id Camera unique id. If omitted, return data for all cameras.
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
     *         %param scheduleTask.startTime Time of day when the backup starts (in seconds passed
     *             from 00:00:00).
     *         %param scheduleTask.endTime Time of day when the backup ends (in seconds passed
     *             from 00:00:00).
     *         %param scheduleTask.recordAudio Whether to record sound.
     *             %value false
     *             %value true
     *         %param scheduleTask.recordingType
     *             %value RT_Always Record always.
     *             %value RT_MotionOnly Record only when the motion is detected.
     *             %value RT_Never Never record.
     *             %value RT_MotionAndLowQuality Always record low quality
     *                 stream, and record high quality stream on motion.
     *         %param scheduleTask.dayOfWeek Weekday for the recording task.
     *             %value 1 Monday
     *             %value 2 Tuesday
     *             %value 3 Wednesday
     *             %value 4 Thursday
     *             %value 5 Friday
     *             %value 6 Saturday
     *             %value 7 Sunday
     *         %param scheduleTask.beforeThreshold The number of seconds before a motion event to
     *             record the video for.
     *         %param scheduleTask.afterThreshold The number of seconds after a motion event to
     *             record the video for.
     *         %param scheduleTask.streamQuality Quality of the recording.
     *             %value QualityLowest
     *             %value QualityLow
     *             %value QualityNormal
     *             %value QualityHigh
     *             %value QualityHighest
     *             %value QualityPreSet
     *             %value QualityNotDefined
     *         %param scheduleTask.fps Frames per second (integer).
     *     %param audioEnabled Whether audio is enabled on the camera.
     *         %value false
     *         %value true
     *     %param secondaryStreamQuality
     *         %value SSQualityLow Low quality second stream.
     *         %value SSQualityMedium Medium quality second stream.
     *         %value SSQualityHigh High quality second stream.
     *         %value SSQualityNotDefined Second stream quality is not defined.
     *         %value SSQualityDontUse Second stream is not used for the camera.
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
    regGet<QnUuid, ApiCameraAttributesDataList>(p, ApiCommand::getCameraUserAttributesList);

    // AbstractCameraManager::addCameraHistoryItem
    regUpdate<ApiServerFootageData>(p, ApiCommand::addCameraHistoryItem);

    /**%apidoc GET /ec2/getCameraHistoryItems
     * Read information about which server was hosting the camera at which period.
     * This information is used for archive playback if camera has been moved from
     * one server to another.
     * %param[default] format
     * %return List of camera history items in the requested format.
     * %// AbstractCameraManager::getCameraHistoryItems
     */
    regGet<nullptr_t, ApiServerFootageDataList>(p, ApiCommand::getCameraHistoryItems);

    /**%apidoc GET /ec2/getCamerasEx
     * Read camera list.
     * %param[default] format
     * %param[opt] id Camera unique id. If omitted, return data for all cameras.
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
     *     %// From struct ApiCameraData (inherited from ApiResourceData):
     *     %param mac Camera MAC address.
     *     %param physicalId Camera unique identifier. This identifier can used in some requests
     *        related to a camera.
     *     %param manuallyAdded Whether the user added the camera manually.
     *         %value false
     *         %value true
     *     %param model Camera model.
     *     %param groupId Internal group identifier. It is used for grouping channels of
     *         multi-channel cameras together.
     *     %param groupName Group name. This name can be changed by users.
     *     %param statusFlags Usually this field is zero. Non-zero value indicates that the camera
     *          is causing a lot of network issues.
     *     %param vendor Camera manufacturer.
     *
     *     %// From struct ApiCameraAttributesData:
     *     %param cameraId Camera unique id. If such object exists, omitted fields will not be changed.
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
     *         %param scheduleTask.startTime Time of day when the backup starts (in seconds passed
     *             from 00:00:00).
     *         %param scheduleTask.endTime Time of day when the backup ends (in seconds passed
     *             from 00:00:00).
     *         %param scheduleTask.recordAudio Whether to record sound.
     *             %value false
     *             %value true
     *         %param scheduleTask.recordingType
     *             %value RT_Always Record always.
     *             %value RT_MotionOnly Record only when the motion is detected.
     *             %value RT_Never Never record.
     *             %value RT_MotionAndLowQuality Always record low quality
     *                 stream, and record high quality stream on motion.
     *         %param scheduleTask.dayOfWeek Day of week for the recording task.
     *             %value 1 Monday
     *             %value 2 Tuesday
     *             %value 3 Wednesday
     *             %value 4 Thursday
     *             %value 5 Friday
     *             %value 6 Saturday
     *             %value 7 Sunday
     *         %param scheduleTask.beforeThreshold The number of seconds before a motion event to
     *             record the video for.
     *         %param scheduleTask.afterThreshold The number of seconds after a motion event to
     *             record the video for.
     *         %param scheduleTask.streamQuality Quality of the recording.
     *             %value QualityLowest
     *             %value QualityLow
     *             %value QualityNormal
     *             %value QualityHigh
     *             %value QualityHighest
     *             %value QualityPreSet
     *             %value QualityNotDefined
     *         %param scheduleTask.fps Frames per second (integer).
     *     %param audioEnabled Whether audio is enabled on the camera.
     *         %value false
     *         %value true
     *     %param secondaryStreamQuality
     *         %value SSQualityLow Low quality second stream.
     *         %value SSQualityMedium Medium quality second stream.
     *         %value SSQualityHigh High quality second stream.
     *         %value SSQualityNotDefined Second stream quality is not defined.
     *         %value SSQualityDontUse Second stream is not used for the camera.
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
     *     %// From ApiCameraDataEx:
     *     %param status Camera status.
     *         %value Offline
     *         %value Online
     *         %value Recording
     *     %param addParams List of additional parameters for the camera. This list can contain
     *         such information as full ONVIF URL, camera maximum FPS, etc.
     * %// AbstractCameraManager::getCamerasEx
     */
    regGet<QnUuid, ApiCameraDataExList>(p, ApiCommand::getCamerasEx);

    /**%apidoc GET /ec2/getStorages
     * Read the list of current storages.
     * %param[default] format
     * %param[opt] id Parent server unique id. If omitted, return storages for all servers.
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
    regGet<ParentId, ApiStorageDataList>(p, ApiCommand::getStorages);

    // AbstractLicenseManager::addLicenses
    regUpdate<ApiLicenseDataList>(p, ApiCommand::addLicenses);
    // AbstractLicenseManager::removeLicense
    regUpdate<ApiLicenseData>(p, ApiCommand::removeLicense);

    /**%apidoc GET /ec2/getEventRules
     * Return all event rules.
     * %param[default] format
     * %param[opt] id Event rule unique id. If omitted, return data for all event rules.
     * %return List of event rule objects in the requested format.
     *     %// TODO: Describe ApiBusinessRuleData fields.
     * %// AbstractBusinessEventManager::getBusinessRules
     */
    regGet<QnUuid, ApiBusinessRuleDataList>(p, ApiCommand::getEventRules);

    regGet<ApiTranLogFilter, ApiTransactionDataList>(p, ApiCommand::getTransactionLog);

    // AbstractBusinessEventManager::save
    regUpdate<ApiBusinessRuleData>(p, ApiCommand::saveEventRule);
    // AbstractBusinessEventManager::deleteRule
    regUpdate<ApiIdData>(p, ApiCommand::removeEventRule);

    regUpdate<ApiResetBusinessRuleData>(p, ApiCommand::resetEventRules);
    regUpdate<ApiBusinessActionData>(p, ApiCommand::broadcastAction);
    regUpdate<ApiBusinessActionData>(p, ApiCommand::execAction);

    /**%apidoc GET /ec2/getUsers
     * Return users registered in the System. User's password contains MD5 hash data with the salt.
     * %param[default] format
     * %param[opt] id User unique id. If omitted, return data for all users.
     * %return List of user data objects in the requested format.
     * %// AbstractUserManager::getUsers
     */
    regGet<QnUuid, ApiUserDataList>(p, ApiCommand::getUsers);

    /**%apidoc GET /ec2/getUserRoles
     * Return User roles registered in the System.
     * %param[default] format
     * %param[opt] id User role unique id. If omitted, return data for all user roles.
     * %return List of user role objects in the requested format.
     *     %// TODO: Describe ApiUserRole fields.
     * %// AbstractUserManager::getUserRoles
     */
    regGet<QnUuid, ApiUserRoleDataList>(p, ApiCommand::getUserRoles);

    /**%apidoc GET /ec2/getAccessRights
     * Return list of ids of accessible resources for each user in the System.
     * %param[default] format
     * %return List of access rights data objects in the requested format.
     * %// AbstractUserManager::getAccessRights
     */
    regGet<nullptr_t, ApiAccessRightsDataList>(p, ApiCommand::getAccessRights);

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
    regUpdate<ApiAccessRightsData>(p, ApiCommand::setAccessRights);

    /**%apidoc POST /ec2/saveUser
     * <p>
     * Parameters should be passed as a JSON object in POST message body with
     * content type "application/json". Example of such object can be seen in
     * the result of the corresponding GET function.
     * </p>
     * %permissions Administrator.
     * %param[opt] id User unique id. Can be omitted when creating a new object. If such object
     *     exists, omitted fields will not be changed.
     * %param[opt] parentId Should be empty.
     * %param name User name.
     * %param fullName Full name of the user.
     * %param[opt] url Should be empty.
     * %param[proprietary] typeId Should have fixed value.
     *     %value {774e6ecd-ffc6-ae88-0165-8f4a6d0eafa7}
     * %param[proprietary] isAdmin Indended for internal use; keep the value when saving
     *     a previously received object, use false when creating a new one.
     *     %value false
     *     %value true
     * %param permissions Combination (via "|") of the following flags:
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
     * %param[opt] isCloud Whether the user is a cloud user, as opposed to a local one.
     *     %value false Default value.
     *     %value true
     * %param[opt] isEnabled Whether the user is enabled.
     *     %value false
     *     %value true Default value.
     * %// AbstractUserManager::save
     */
    regUpdate<ApiUserData>(p, ApiCommand::saveUser);

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
    regUpdate<ApiIdData>(p, ApiCommand::removeUser);

    /**%apidoc POST /ec2/saveUserRole
     * <p>
     * Parameters should be passed as a JSON object in POST message body with
     * content type "application/json". Example of such object can be seen in
     * the result of the corresponding GET function.
     * </p>
     * %permissions Administrator.
     * %param[opt] id User role unique id. Can be omitted when creating a new object. If such object
     * exists, omitted fields will not be changed.
     * %param name User role name.
     * %param permissions Combination (via "|") of the following flags:
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
    regUpdate<ApiUserRoleData>(p, ApiCommand::saveUserRole);

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
    regUpdate<ApiIdData>(p, ApiCommand::removeUserRole);

    /**%apidoc GET /ec2/getPredefinedRoles
    * Return list of predefined user roles.
    * %param[default] format
    * %return List of predefined user role objects in the requested format.
    */
    regGet<nullptr_t, ApiPredefinedRoleDataList>(p, ApiCommand::getPredefinedRoles);

    /**%apidoc GET /ec2/getVideowalls
     * Return list of video walls
     * %param[default] format
     * %return List of video wall objects in the requested format.
     * %// AbstractVideowallManager::getVideowalls
     */
    regGet<QnUuid, ApiVideowallDataList>(p, ApiCommand::getVideowalls);
    // AbstractVideowallManager::save
    regUpdate<ApiVideowallData>(p, ApiCommand::saveVideowall);
    // AbstractVideowallManager::remove
    regUpdate<ApiIdData>(p, ApiCommand::removeVideowall);
    regUpdate<ApiVideowallControlMessageData>(p, ApiCommand::videowallControl);

    regGet<QnUuid, ApiWebPageDataList>(p, ApiCommand::getWebPages);
    regUpdate<ApiWebPageData>(p, ApiCommand::saveWebPage);
    // AbstractWebPageManager::remove
    regUpdate<ApiIdData>(p, ApiCommand::removeWebPage);

    /**%apidoc GET /ec2/getLayouts
     * Return list of user layout
     * %param[default] format
     * %param[opt] id Layout unique id. If omitted, return data for all layouts.
     * %return List of layout objects in the requested format.
     * %// AbstractLayoutManager::getLayouts
     */
    regGet<QnUuid, ApiLayoutDataList>(p, ApiCommand::getLayouts);

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
     * %param horizontalSpacing Horizontal spacing between layout items
     *     (floating-point).
     * %param verticalSpacing Vertical spacing between layout items
     *     (floating-point).
     * %param items List of the layout items.
     * %param item.id Item unique id. Can be omitted when creating a new object.
     * %param item.flags Should have fixed value.
     *     %value 0
     * %param item.left Left coordinate of the layout item (floating-point).
     * %param item.top Top coordinate of the layout item (floating-point).
     * %param item.right Right coordinate of the layout item (floating-point).
     * %param item.bottom Bottom coordinate of the layout item (floating-point).
     * %param item.rotation Degree of image tilt; a positive value rotates
     *     counter-clockwise (floating-point, 0..360).
     * %param item.resourceId Camera unique id.
     * %param item.resourcePath If the item represents a local file - URL of
     *     the file, otherwise is empty.
     * %param item.zoomLeft Left coordinate of the displayed window inside
     *     the camera image, as a fraction of the image width
     *     (floating-point, 0..1).
     * %param item.zoomTop Top coordinate of the displayed window inside
     *     the camera image, as a fraction of the image height
     *     (floating-point, 0..1).
     * %param item.zoomRight Right coordinate of the displayed window inside
     *     the camera image, as a fraction of the image width
     *     (floating-point, 0..1).
     * %param item.zoomBottom Bottom coordinate of the displayed window inside
     *     the camera image, as a fraction of the image width
     *     (floating-point, 0..1).
     * %param item.zoomTargetId Unique id of the original layout item for
     *     which the zoom window was created.
     * %param item.contrastParams Image enhancement parameters. The format
     *     is proprietary and is likely to change in future API versions.
     * %param item.dewarpingParams Image dewarping parameters.
     *     The format is proprietary and is likely to change in future API
     *     versions.
     * %param item.displayInfo Whether to display info for the layout item.
     *     %value false
     *     %value true
     * %param locked Whether the layout is locked.
     *     %value false
     *     %value true
     * %param backgroundImageFilename
     * %param backgroundWidth Width of the background image in pixels (integer).
     * %param backgroundHeight Height of the background image in pixels (integer).
     * %param backgroundOpacity Level of opacity of the background image in pixels (floating-point
     *     0..1).
     * %// AbstractLayoutManager::save
     */
    regUpdate<ApiLayoutData>(p, ApiCommand::saveLayout);

    /**%apidoc POST /ec2/saveLayouts
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
     * %param horizontalSpacing Horizontal spacing between layout items
     *     (floating-point).
     * %param verticalSpacing Vertical spacing between layout items
     *     (floating-point).
     * %param items List of the layout items.
     * %param item.id Item unique id. Can be omitted when creating a new object.
     * %param item.flags Should have fixed value.
     *     %value 0
     * %param item.left Left coordinate of the layout item (floating-point).
     * %param item.top Top coordinate of the layout item (floating-point).
     * %param item.right Right coordinate of the layout item (floating-point).
     * %param item.bottom Bottom coordinate of the layout item (floating-point).
     * %param item.rotation Degree of image tilt; a positive value rotates
     *     counter-clockwise (floating-point, 0..360).
     * %param item.resourceId Camera unique id.
     * %param item.resourcePath If the item represents a local file - URL of
     *     the file, otherwise is empty.
     * %param item.zoomLeft Left coordinate of the displayed window inside
     *     the camera image, as a fraction of the image width
     *     (floating-point, 0..1).
     * %param item.zoomTop Top coordinate of the displayed window inside
     *     the camera image, as a fraction of the image height
     *     (floating-point, 0..1).
     * %param item.zoomRight Right coordinate of the displayed window inside
     *     the camera image, as a fraction of the image width
     *     (floating-point, 0..1).
     * %param item.zoomBottom Bottom coordinate of the displayed window inside
     *     the camera image, as a fraction of the image width
     *     (floating-point, 0..1).
     * %param item.zoomTargetId Unique id of the original layout item for
     *     which the zoom window was created.
     * %param item.contrastParams Image enhancement parameters. The format
     *     is proprietary and is likely to change in future API versions.
     * %param item.dewarpingParams Image dewarping parameters.
     *     The format is proprietary and is likely to change in future API
     *     versions.
     * %param item.displayInfo Whether to display info for the layout item.
     *     %value false
     *     %value true
     * %param locked Whether the layout is locked.
     *     %value false
     *     %value true
     * %param backgroundImageFilename
     * %param backgroundWidth Width of the background image in pixels (integer).
     * %param backgroundHeight Height of the background image in pixels (integer).
     * %param backgroundOpacity Level of opacity of the background image in pixels (floating-point
     *     0..1).
     * %// AbstractLayoutManager::save
     */
    regUpdate<ApiLayoutDataList>(p, ApiCommand::saveLayouts);

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
    regUpdate<ApiIdData>(p, ApiCommand::removeLayout);

    /**%apidoc GET /ec2/listDirectory
     * Return list of folders and files in a virtual FS stored inside
     * database. This function is used to add files (such audio for notifications)
     * to database.
     * %param[default] format
     * %param[opt] folder Folder name in a virtual FS
     * %return List of objects in the requested format.
     *    %// TODO: Describe params.
     * %// AbstractStoredFileManager::listDirectory
     */
    regGet<ApiStoredFilePath, ApiStoredDirContents>(p, ApiCommand::listDirectory);

    /**%apidoc GET /ec2/getStoredFile
     * Read file data from a virtual FS
     * %param[default] format
     * %param[opt] folder File name
     * %return Object in the requested format.
     * %// AbstractStoredFileManager::getStoredFile
     */
    regGet<ApiStoredFilePath, ApiStoredFileData>(p, ApiCommand::getStoredFile);

    // AbstractStoredFileManager::addStoredFile
    regUpdate<ApiStoredFileData>(p, ApiCommand::addStoredFile);
    // AbstractStoredFileManager::updateStoredFile
    regUpdate<ApiStoredFileData>(p, ApiCommand::updateStoredFile);
    // AbstractStoredFileManager::deleteStoredFile
    regUpdate<ApiStoredFilePath>(p, ApiCommand::removeStoredFile);

    // AbstractUpdatesManager::uploadUpdate
    regUpdate<ApiUpdateUploadData>(p, ApiCommand::uploadUpdate);
    // AbstractUpdatesManager::uploadUpdateResponce
    regUpdate<ApiUpdateUploadResponceData>(p, ApiCommand::uploadUpdateResponce);
    // AbstractUpdatesManager::installUpdate
    regUpdate<ApiUpdateInstallData>(p, ApiCommand::installUpdate);

    // AbstractDiscoveryManager::discoveredServerChanged
    regUpdate<ApiDiscoveredServerData>(p, ApiCommand::discoveredServerChanged);
    // AbstractDiscoveryManager::discoveredServersList
    regUpdate<ApiDiscoveredServerDataList>(p, ApiCommand::discoveredServersList);

    // AbstractDiscoveryManager::discoverPeer
    regUpdate<ApiDiscoverPeerData>(p, ApiCommand::discoverPeer);
    // AbstractDiscoveryManager::addDiscoveryInformation
    regUpdate<ApiDiscoveryData>(p, ApiCommand::addDiscoveryInformation);
    // AbstractDiscoveryManager::removeDiscoveryInformation
    regUpdate<ApiDiscoveryData>(p, ApiCommand::removeDiscoveryInformation);
    // AbstractDiscoveryManager::getDiscoveryData
    regGet<QnUuid, ApiDiscoveryDataList>(p, ApiCommand::getDiscoveryData);
    // AbstractMiscManager::changeSystemId
    regUpdate<ApiSystemIdData>(p, ApiCommand::changeSystemId);

    // AbstractECConnection
    regUpdate<ApiDatabaseDumpData>(p, ApiCommand::restoreDatabase);

    /**%apidoc GET /ec2/getCurrentTime
     * Read current time.
     * %permissions Administrator.
     * %param[default] format
     * %return Object in the requested format.
     * %// AbstractTimeManager::getCurrentTimeImpl
     */
    regGet<nullptr_t, ApiTimeData>(p, ApiCommand::getCurrentTime);

    // AbstractTimeManager::forcePrimaryTimeServer
    regUpdate<ApiIdData>(p, ApiCommand::forcePrimaryTimeServer,
        std::bind(&TimeSynchronizationManager::primaryTimeServerChanged,
            m_timeSynchronizationManager.get(), _1));
    // TODO: #ak register AbstractTimeManager::getPeerTimeInfoList

    // ApiClientInfoData
    regUpdate<ApiClientInfoData>(p, ApiCommand::saveClientInfo);
    regGet<QnUuid, ApiClientInfoDataList>(p, ApiCommand::getClientInfoList);

    /**%apidoc GET /ec2/getFullInfo
     * Read all data such as all servers, cameras, users, etc.
     * %param[default] format
     * %return Object in the requested format.
     */
    regGet<nullptr_t, ApiFullInfoData>(p, ApiCommand::getFullInfo);

    /**%apidoc GET /ec2/getLicenses
     * Read license list
     * %param[default] format
     * %return List of license objects in the requested format.
     */
    regGet<nullptr_t, ApiLicenseDataList>(p, ApiCommand::getLicenses);

    regGet<nullptr_t, ApiDatabaseDumpData>(p, ApiCommand::dumpDatabase);
    regGet<ApiStoredFilePath, ApiDatabaseDumpToFileData>(p, ApiCommand::dumpDatabaseToFile);

    // AbstractECConnectionFactory
    regFunctorWithResponse<ApiLoginData, QnConnectionInfo>(p, ApiCommand::connect,
        std::bind(&Ec2DirectConnectionFactory::fillConnectionInfo, this, _1, _2, _3));
    regFunctorWithResponse<ApiLoginData, QnConnectionInfo>(p, ApiCommand::testConnection,
        std::bind(&Ec2DirectConnectionFactory::fillConnectionInfo, this, _1, _2, _3));

    /**%apidoc GET /ec2/getSettings
     * Read general System settings such as email address, etc.
     * %param[default] format
     * %return List of objects in the requested format.
     */
    regFunctor<nullptr_t, ApiResourceParamDataList>(p, ApiCommand::getSettings,
        std::bind(&Ec2DirectConnectionFactory::getSettings, this, _1, _2, _3));

    // Ec2StaticticsReporter
    regFunctor<nullptr_t, ApiSystemStatistics>(p, ApiCommand::getStatisticsReport,
        [this](nullptr_t, ApiSystemStatistics* const out, const Qn::UserAccessData&)
        {
            if (!m_directConnection)
                return ErrorCode::failure;
            return m_directConnection->getStaticticsReporter()->collectReportData(
                nullptr, out);
        });
    regFunctor<nullptr_t, ApiStatisticsServerInfo>(p, ApiCommand::triggerStatisticsReport,
        [this](nullptr_t, ApiStatisticsServerInfo* const out, const Qn::UserAccessData&)
        {
            if (!m_directConnection)
                return ErrorCode::failure;
            return m_directConnection->getStaticticsReporter()->triggerStatisticsReport(
                nullptr, out);
        });

    p->registerHandler("ec2/activeConnections", new QnActiveConnectionsRestHandler());
    p->registerHandler(QnTimeSyncRestHandler::PATH, new QnTimeSyncRestHandler());

#if 0 // Using HTTP processor since HTTP REST does not support HTTP interleaving.
    p->registerHandler(
        QLatin1String(kIncomingTransactionsPath),
        new QnRestTransactionReceiver());
#endif // 0
}

void Ec2DirectConnectionFactory::setConfParams(std::map<QString, QVariant> confParams)
{
    m_settingsInstance.loadParams(std::move(confParams));
}

int Ec2DirectConnectionFactory::establishDirectConnection(
    const QUrl& url, impl::ConnectHandlerPtr handler)
{
    const int reqId = generateRequestID();

    ApiLoginData loginInfo;
    QnConnectionInfo connectionInfo;
    fillConnectionInfo(loginInfo, &connectionInfo); // < TODO: #ak not appropriate here
    connectionInfo.ecUrl = url;
    ErrorCode connectionInitializationResult = ErrorCode::ok;
    {
        QnMutexLocker lk(&m_mutex);
        if (!m_directConnection)
        {
            m_directConnection.reset(
                new Ec2DirectConnection(&m_serverQueryProcessor, connectionInfo, url));
            if (!m_directConnection->initialized())
            {
                connectionInitializationResult = ErrorCode::dbError;
                m_directConnection.reset();
            }
        }
    }
    QnConcurrent::run(
        Ec2ThreadPool::instance(),
        std::bind(
            &impl::ConnectHandler::done,
            handler,
            reqId,
            connectionInitializationResult,
            m_directConnection));
    return reqId;
}

int Ec2DirectConnectionFactory::establishConnectionToRemoteServer(
    const QUrl& addr, impl::ConnectHandlerPtr handler, const ApiClientInfoData& clientInfo)
{
    const int reqId = generateRequestID();

#if 0 // TODO: #ak Return existing connection, if any.
    {
        QnMutexLocker lk(&m_mutex);
        auto it = m_urlToConnection.find(addr);
        if (it != m_urlToConnection.end())
            AbstractECConnectionPtr connection = it->second.second;
    }
#endif // 0

    ApiLoginData loginInfo;
    loginInfo.login = addr.userName();
    loginInfo.passwordHash = nx_http::calcHa1(
        loginInfo.login.toLower(), QnAppInfo::realm(), addr.password());
    loginInfo.clientInfo = clientInfo;

    {
        QnMutexLocker lk(&m_mutex);
        if (m_terminated)
            return INVALID_REQ_ID;
        ++m_runningRequests;
    }

    const auto info = QString::fromUtf8(QJson::serialized(clientInfo) );
    NX_LOG(lit("%1 to %2 with %3").arg(Q_FUNC_INFO).arg(addr.toString(QUrl::RemovePassword)).arg(info),
            cl_logDEBUG1);

    auto func =
        [this, reqId, addr, handler](
            ErrorCode errorCode, const QnConnectionInfo& connectionInfo)
        {
            remoteConnectionFinished(reqId, errorCode, connectionInfo, addr, handler);
        };
    m_remoteQueryProcessor.processQueryAsync<ApiLoginData, QnConnectionInfo>(
        addr, ApiCommand::connect, loginInfo, func);
    return reqId;
}

void Ec2DirectConnectionFactory::tryConnectToOldEC(const QUrl& ecUrl,
    impl::ConnectHandlerPtr handler, int reqId)
{
    // Checking for old EC.
    QnConcurrent::run(
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
                && oldECConnectionInfo.version >= QnSoftwareVersion(2, 3, 0))
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
    connectionInfo->version = QnSoftwareVersion(QByteArray::fromRawData(data, fieldLen));
    return true;
}

template<class Handler>
void Ec2DirectConnectionFactory::connectToOldEC(const QUrl& ecUrl, Handler completionFunc)
{
    QUrl httpsEcUrl = ecUrl; // < Old EC supports only https.
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
                if (oldECConnectionInfo.version >= QnSoftwareVersion(2, 3))
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

void Ec2DirectConnectionFactory::remoteConnectionFinished(
    int reqId,
    ErrorCode errorCode,
    const QnConnectionInfo& connectionInfo,
    const QUrl& ecUrl,
    impl::ConnectHandlerPtr handler)
{
    NX_LOG(QnLog::EC2_TRAN_LOG, lit(
        "Ec2DirectConnectionFactory::remoteConnectionFinished. errorCode = %1, ecUrl = %2")
        .arg((int)errorCode).arg(ecUrl.toString(QUrl::RemovePassword)), cl_logDEBUG2);

    // TODO: #ak async ssl is working now, make async request to old ec here

    switch (errorCode)
    {
        case ec2::ErrorCode::ok:
        case ec2::ErrorCode::unauthorized:
        case ec2::ErrorCode::forbidden:
        case ec2::ErrorCode::ldap_temporary_unauthorized:
        case ec2::ErrorCode::cloud_temporary_unauthorized:
            break;

        default:
            tryConnectToOldEC(ecUrl, handler, reqId);
            return;
    }

    QnConnectionInfo connectionInfoCopy(connectionInfo);
    connectionInfoCopy.ecUrl = ecUrl;
    connectionInfoCopy.ecUrl.setScheme(
        connectionInfoCopy.allowSslConnections ? lit("https") : lit("http"));
    connectionInfoCopy.ecUrl.setQuery(QUrlQuery()); /*< Cleanup 'format' parameter. */

    NX_LOG(QnLog::EC2_TRAN_LOG, lit(
        "Ec2DirectConnectionFactory::remoteConnectionFinished (2). errorCode = %1, ecUrl = %2")
        .arg((int)errorCode).arg(connectionInfoCopy.ecUrl.toString(QUrl::RemovePassword)), cl_logDEBUG2);

    AbstractECConnectionPtr connection(new RemoteEC2Connection(
        std::make_shared<FixedUrlClientQueryProcessor>(
            &m_remoteQueryProcessor, connectionInfoCopy.ecUrl),
        connectionInfoCopy));
    handler->done(reqId, errorCode, connection);

    QnMutexLocker lk(&m_mutex);
    --m_runningRequests;
}

void Ec2DirectConnectionFactory::remoteTestConnectionFinished(
    int reqId,
    ErrorCode errorCode,
    const QnConnectionInfo& connectionInfo,
    const QUrl& ecUrl,
    impl::TestConnectionHandlerPtr handler)
{
    if (errorCode == ErrorCode::ok
        || errorCode == ErrorCode::unauthorized
        || errorCode == ErrorCode::forbidden
        || errorCode == ErrorCode::ldap_temporary_unauthorized
        || errorCode == ErrorCode::cloud_temporary_unauthorized)
    {
        handler->done(reqId, errorCode, connectionInfo);
        QnMutexLocker lk(&m_mutex);
        --m_runningRequests;
        return;
    }

    // Checking for old EC.
    QnConcurrent::run(
        Ec2ThreadPool::instance(),
        [this, ecUrl, handler, reqId]()
        {
            using namespace std::placeholders;
            connectToOldEC(
                ecUrl, std::bind(&impl::TestConnectionHandler::done, handler, reqId, _1, _2));
        }
    );
}

ErrorCode Ec2DirectConnectionFactory::fillConnectionInfo(
    const ApiLoginData& loginInfo,
    QnConnectionInfo* const connectionInfo,
    nx_http::Response* response)
{
    auto localInfo = qnRuntimeInfoManager->localInfo().data;
    connectionInfo->version = qnCommon->engineVersion();
    connectionInfo->brand = localInfo.brand;
    connectionInfo->customization = localInfo.customization;
    connectionInfo->systemName = qnGlobalSettings->systemName();
    connectionInfo->ecsGuid = qnCommon->moduleGUID().toString();
    connectionInfo->cloudSystemId = qnGlobalSettings->cloudSystemId();
    connectionInfo->localSystemId = qnGlobalSettings->localSystemId();
    #if defined(__arm__)
        connectionInfo->box = QnAppInfo::armBox();
    #endif
    connectionInfo->allowSslConnections = m_sslEnabled;
    connectionInfo->nxClusterProtoVersion = nx_ec::EC2_PROTO_VERSION;
    connectionInfo->ecDbReadOnly = Settings::instance()->dbReadOnly();
    connectionInfo->newSystem = qnGlobalSettings->isNewSystem();
    if (response)
    {
        connectionInfo->effectiveUserName =
            nx_http::getHeaderValue(response->headers, Qn::EFFECTIVE_USER_NAME_HEADER_NAME);
    }

    #ifdef ENABLE_EXTENDED_STATISTICS
        if (!loginInfo.clientInfo.id.isNull())
        {
            auto clientInfo = loginInfo.clientInfo;
            clientInfo.parentId = qnCommon->moduleGUID();

            ApiClientInfoDataList infoList;
            auto result = dbManager(Qn::kSystemAccess).doQuery(clientInfo.id, infoList);
            if (result != ErrorCode::ok)
                return result;

            if (infoList.size() > 0
                && QJson::serialized(clientInfo) == QJson::serialized(infoList.front()))
            {
                NX_LOG(lit("Ec2DirectConnectionFactory: New client had already been registered with the same params"),
                    cl_logDEBUG2);
                return ErrorCode::ok;
            }

            m_serverQueryProcessor.getAccess(Qn::kSystemAccess).processUpdateAsync(
                ApiCommand::saveClientInfo, clientInfo,
                [&](ErrorCode result)
                {
                    if (result == ErrorCode::ok)
                    {
                        NX_LOG(lit("Ec2DirectConnectionFactory: New client has been registered"),
                            cl_logINFO);
                    }
                    else
                    {
                        NX_LOG(lit("Ec2DirectConnectionFactory: New client transaction has failed %1")
                            .arg(toString(result)), cl_logERROR);
                    }
                });
        }
    #endif

    return ErrorCode::ok;
}

int Ec2DirectConnectionFactory::testDirectConnection(
    const QUrl& addr, impl::TestConnectionHandlerPtr handler)
{
    Q_UNUSED(addr);

    const int reqId = generateRequestID();
    QnConnectionInfo connectionInfo;
    fillConnectionInfo(ApiLoginData(), &connectionInfo);
    QnConcurrent::run(
        Ec2ThreadPool::instance(),
        std::bind(
            &impl::TestConnectionHandler::done,
            handler,
            reqId,
            ErrorCode::ok,
            connectionInfo));
    return reqId;
}

int Ec2DirectConnectionFactory::testRemoteConnection(
    const QUrl& addr, impl::TestConnectionHandlerPtr handler)
{
    const int reqId = generateRequestID();

    {
        QnMutexLocker lk(&m_mutex);
        if (m_terminated)
            return INVALID_REQ_ID;
        ++m_runningRequests;
    }

    ApiLoginData loginInfo;
    loginInfo.login = addr.userName();
    loginInfo.passwordHash = nx_http::calcHa1(
        loginInfo.login.toLower(), QnAppInfo::realm(), addr.password());
    auto func =
        [this, reqId, addr, handler](ErrorCode errorCode, const QnConnectionInfo& connectionInfo)
        {
            auto infoWithUrl = connectionInfo;
            infoWithUrl.ecUrl = addr;
            infoWithUrl.ecUrl.setQuery(QUrlQuery()); /*< Cleanup 'format' parameter. */
            remoteTestConnectionFinished(reqId, errorCode, infoWithUrl, addr, handler);
        };
    m_remoteQueryProcessor.processQueryAsync<nullptr_t, QnConnectionInfo>(
        addr, ApiCommand::testConnection, nullptr_t(), func);
    return reqId;
}

ErrorCode Ec2DirectConnectionFactory::getSettings(
    nullptr_t, ApiResourceParamDataList* const outData, const Qn::UserAccessData& accessData)
{
    if (!detail::QnDbManager::instance())
        return ErrorCode::ioError;
    return dbManager(accessData).doQuery(nullptr, *outData);
}

template<class InputDataType>
void Ec2DirectConnectionFactory::regUpdate(
    QnRestProcessorPool* const restProcessorPool,
    ApiCommand::Value cmd,
    Qn::GlobalPermission permission)
{
    restProcessorPool->registerHandler(
        lit("ec2/%1").arg(ApiCommand::toString(cmd)),
        new UpdateHttpHandler<InputDataType>(m_directConnection),
        permission);
}

template<class InputDataType, class CustomActionType>
void Ec2DirectConnectionFactory::regUpdate(
    QnRestProcessorPool* const restProcessorPool,
    ApiCommand::Value cmd,
    CustomActionType customAction,
    Qn::GlobalPermission permission)
{
    restProcessorPool->registerHandler(
        lit("ec2/%1").arg(ApiCommand::toString(cmd)),
        new UpdateHttpHandler<InputDataType>(m_directConnection, customAction),
        permission);
}

template<class InputDataType, class OutputDataType>
void Ec2DirectConnectionFactory::regGet(
    QnRestProcessorPool* const restProcessorPool,
    ApiCommand::Value cmd,
    Qn::GlobalPermission permission)
{
    restProcessorPool->registerHandler(
        lit("ec2/%1").arg(ApiCommand::toString(cmd)),
        new QueryHttpHandler<InputDataType, OutputDataType>(cmd, &m_serverQueryProcessor),
        permission);
}

template<class InputType, class OutputType>
void Ec2DirectConnectionFactory::regFunctor(
    QnRestProcessorPool* const restProcessorPool,
    ApiCommand::Value cmd,
    std::function<ErrorCode(InputType, OutputType*, const Qn::UserAccessData&)> handler, Qn::GlobalPermission permission)
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
void Ec2DirectConnectionFactory::regFunctorWithResponse(
    QnRestProcessorPool* const restProcessorPool,
    ApiCommand::Value cmd,
    std::function<ErrorCode(InputType, OutputType*, nx_http::Response*)> handler,
    Qn::GlobalPermission permission)
{
    restProcessorPool->registerHandler(
        lit("ec2/%1").arg(ApiCommand::toString(cmd)),
        new FlexibleQueryHttpHandler<InputType, OutputType>(cmd, std::move(handler)),
        permission);
}

} // namespace ec2
