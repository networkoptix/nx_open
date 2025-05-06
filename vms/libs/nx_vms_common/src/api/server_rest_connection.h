// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <analytics/db/analytics_db_types.h>
#include <api/helpers/request_helpers_fwd.h>
#include <api/http_client_pool.h>
#include <api/model/camera_diagnostics_reply.h>
#include <api/model/camera_list_reply.h>
#include <api/model/getnonce_reply.h>
#include <api/model/legacy_audit_record.h>
#include <api/model/manual_camera_seach_reply.h>
#include <api/model/storage_status_reply.h>
#include <api/model/test_email_settings_reply.h>
#include <api/model/time_reply.h>
#include <api/model/virtual_camera_prepare_data.h>
#include <core/resource/resource_fwd.h>
#include <nx/api/mediaserver/requests_fwd.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/http/http_types.h>
#include <nx/network/rest/auth_result.h>
#include <nx/network/rest/params.h>
#include <nx/network/rest/result.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/std/expected.h>
#include <nx/utils/system_error.h>
#include <nx/vms/api/analytics/analytics_actions.h>
#include <nx/vms/api/analytics/analytics_engine_settings_data.h>
#include <nx/vms/api/analytics/device_agent_active_setting_changed_response.h>
#include <nx/vms/api/analytics/device_agent_settings_response.h>
#include <nx/vms/api/data/analytics_data.h>
#include <nx/vms/api/data/backup_position.h>
#include <nx/vms/api/data/camera_history_data.h>
#include <nx/vms/api/data/database_dump_data.h>
#include <nx/vms/api/data/device_model.h>
#include <nx/vms/api/data/device_replacement.h>
#include <nx/vms/api/data/device_search.h>
#include <nx/vms/api/data/email_settings.h>
#include <nx/vms/api/data/event_rule_data.h>
#include <nx/vms/api/data/json_rpc.h>
#include <nx/vms/api/data/ldap.h>
#include <nx/vms/api/data/log_settings.h>
#include <nx/vms/api/data/login.h>
#include <nx/vms/api/data/lookup_list_data.h>
#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/api/data/overlapped_id_data.h>
#include <nx/vms/api/data/remote_archive_synchronization_status.h>
#include <nx/vms/api/data/storage_init_result.h>
#include <nx/vms/api/data/storage_scan_info.h>
#include <nx/vms/api/data/storage_space_data.h>
#include <nx/vms/api/data/system_settings.h>
#include <nx/vms/api/data/time_reply.h>
#include <nx/vms/api/data/user_data.h>
#include <nx/vms/api/data/user_group_model.h>
#include <nx/vms/api/data/user_model.h>
#include <nx/vms/api/rules/acknowledge.h>
#include <nx/vms/api/rules/soft_trigger.h>
#include <nx/vms/event/event_fwd.h>
#include <nx/vms/utils/abstract_session_token_helper.h>
#include <recording/time_period_list.h>
#include <utils/camera/camera_diagnostics.h>

#include "rest_types.h"

namespace nx::vms::common {

class AbstractCertificateVerifier;
class SystemContext;

} // namespace nx::vms::common

namespace rest {

using MultiServerTimeData = ResultWithData<nx::vms::api::ServerTimeReplyList>;

/**
 * Class for HTTP requests to mediaServer.
 * This class can be used either for client and server side.
 * Class calls callback methods from IO thread. So, caller code should be thread-safe.
 * Client MUST NOT make requests in callbacks as it will cause a deadlock.
 * This class limits number of underlying requests using separate http clients pool with 8 threads.
 */
class NX_VMS_COMMON_API ServerConnection: public QObject
{
public:
    using Timeouts = nx::network::http::AsyncClient::Timeouts;
    using AbstractCertificateVerifier = nx::vms::common::AbstractCertificateVerifier;

public:
    /**
     * Used to send REST requests to the mediaserver. It uses messageBusConnection and
     * MediaServerResource to get authentication data.
     * @param systemContext ServerConnection takes a lot of objects from it.
     * @param serverId resource Id of the mediaserver.
     */
    ServerConnection(
        nx::vms::common::SystemContext* systemContext,
        const nx::Uuid& serverId);

    /**
     * Used to send REST requests to the mediaserver when we still do not have proper resource pool
     * and messageBusConnection instance, but need to send requests to the server.
     *
     * @param serverId resource Id of the VMS server.
     * @param auditId Id of the session in the Audit Trail.
     * @param address Explicit address of the VMS server.
     * @param credentials Credentials to authorize requests.
     */
    ServerConnection(
        nx::network::http::ClientPool* httpClientPool,
        const nx::Uuid& serverId,
        const nx::Uuid& auditId,
        AbstractCertificateVerifier* certificateVerifier,
        nx::network::SocketAddress address,
        nx::network::http::Credentials credentials);

    virtual ~ServerConnection();

    /**
     * Changes address which client currently uses to connect to server. Required to handle
     * situations like server port change or systems merge. Also can be used to select the most
     * fast interface amongst available.
     */
    void updateAddress(nx::network::SocketAddress address);

    /**
     * Changes credentials which client currently uses to connect to server. Required to handle
     * situations like user password change or systems merge.
     */
    void updateCredentials(nx::network::http::Credentials credentials);

    void setUserId(const nx::Uuid& userId);

    using PostCallback = Callback<EmptyResponseType>;   // use this type for POST requests without result data

    /**
     * Default callback type for GET requests without result data.
     */
    using GetCallback = Callback<nx::network::rest::JsonResult>;

    using ContextPtr = nx::network::http::ClientPool::ContextPtr;

    using ErrorOrEmpty = ErrorOrData<EmptyResponseType>;

    /**
    * Load information about cross-server archive
    * @return value > 0 on success or <= 0 if it isn't started.
    * @param executor execute callback in a target thread if specified.
    * By default callback is called in IO thread.
    */
    Handle cameraHistoryAsync(
        const QnChunksRequestData& request,
        Callback<nx::vms::api::CameraHistoryDataList> callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle backupPositionAsyncV1(const nx::Uuid& serverId,
        const nx::Uuid& deviceId,
        Callback<nx::vms::api::BackupPositionExV1> callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle setBackupPositionAsyncV1(const nx::Uuid& serverId,
        const nx::Uuid& deviceId,
        const nx::vms::api::BackupPositionV1& backupPosition,
        Callback<nx::vms::api::BackupPositionV1> callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle setBackupPositionsAsyncV1(const nx::Uuid& serverId,
        const nx::vms::api::BackupPositionV1& backupPosition,
        Callback<nx::vms::api::BackupPositionV1> callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle getServerLocalTime(
        const nx::Uuid& serverId,
        Callback<nx::network::rest::JsonResult> callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    /**
     * Get camera thumbnail for specified time.
     *
     * Returns immediately. On request completion callback is called.
     *
     * @param executor Callback thread. If not specified, callback is called in IO thread.
     * @return Request handle.
     */
    Handle cameraThumbnailAsync(
        const nx::api::CameraImageRequest& request,
        DataCallback callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle sendStatisticsUsingServer(
        const nx::Uuid& proxyServerId,
        const QnSendStatisticsRequestData& statisticsData,
        PostCallback callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    /**
     * Binds the System to the cloud using cloud credentials.
     */
    Handle bindSystemToCloud(
        const QString& cloudSystemId,
        const QString& cloudAuthKey,
        const QString& cloudAccountName,
        const QString& organizationId,
        const std::string& ownerSessionToken,
        Callback<ErrorOrEmpty> callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    /**
     * Unbind the System from the Cloud and reset local admin password (optionally).
     */
    Handle unbindSystemFromCloud(
        nx::vms::common::SessionTokenHelperPtr helper,
        const QString& password,
        Callback<ErrorOrEmpty> callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle dumpDatabase(
        nx::vms::common::SessionTokenHelperPtr helper,
        Callback<ErrorOrData<QByteArray>> callback,
        nx::utils::AsyncHandlerExecutor executor);

    Handle restoreDatabase(
        nx::vms::common::SessionTokenHelperPtr helper,
        const QByteArray& data,
        Callback<ErrorOrEmpty> callback,
        nx::utils::AsyncHandlerExecutor executor);

    Handle putServerLogSettings(
        nx::vms::common::SessionTokenHelperPtr helper,
        const nx::Uuid& serverId,
        const nx::vms::api::ServerLogSettings& settings,
        Callback<ErrorOrEmpty> callback,
        nx::utils::AsyncHandlerExecutor executor);

    Handle patchSystemSettings(
        nx::vms::common::SessionTokenHelperPtr helper,
        const nx::vms::api::SaveableSystemSettings& settings,
        Callback<ErrorOrEmpty> callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    /* DistributedFileDownloader API */
    Handle addFileDownload(
        const QString& fileName,
        qint64 size,
        const QByteArray& md5,
        const QUrl& url,
        const QString& peerPolicy,
        GetCallback callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    /**
     * Creates a new file upload that you can upload chunks into via uploadFileChunk().
     *
     * A quick note on TTL. Countdown normally starts once you've uploaded the whole file, but
     * since clients are unreliable and might stop an upload prematurely, the server actually
     * restarts the countdown after each mutating command.
     *
     * @param fileName Unique file name that will be used in other calls.
     * @param size Size of the file, in bytes.
     * @param chunkSize Size of a single chunk as will be used in succeeding uploadFileChunk() calls.
     * @param md5 MD5 hash of the file, as text (32-character hex string).
     * @param ttl TTL for the upload, in milliseconds. Pass 0 for infinity.
     */
    using AddUploadCallback = Callback<nx::network::rest::JsonResult>;

    Handle addFileUpload(
        const nx::Uuid& serverId,
        const QString& fileName,
        qint64 size,
        qint64 chunkSize,
        const QByteArray& md5,
        qint64 ttl,
        bool recreateIfExists,
        AddUploadCallback callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle removeFileDownload(
        const nx::Uuid& serverId,
        const QString& fileName,
        bool deleteData,
        PostCallback callback = nullptr,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle fileChunkChecksums(
        const nx::Uuid& serverId,
        const QString& fileName,
        GetCallback callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle downloadFileChunk(
        const nx::Uuid& serverId,
        const QString& fileName,
        int chunkIndex,
        DataCallback callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle downloadFileChunkFromInternet(
        const nx::Uuid& serverId,
        const QString& fileName,
        const nx::utils::Url &url,
        int chunkIndex,
        int chunkSize,
        qint64 fileSize,
        DataCallback callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle uploadFileChunk(
        const nx::Uuid& serverId,
        const QString& fileName,
        int index,
        const QByteArray& data,
        PostCallback callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle downloadsStatus(
        GetCallback callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle fileDownloadStatus(
        const nx::Uuid& serverId,
        const QString& fileName,
        GetCallback callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle getTimeOfServersAsync(
        Callback<MultiServerTimeData> callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle eventLog(
        const nx::vms::api::rules::EventLogFilter& filter,
        Callback<ErrorOrData<nx::vms::api::rules::EventLogRecordList>> callback,
        nx::utils::AsyncHandlerExecutor executor = {},
        std::optional<Timeouts> timeouts = std::nullopt);

    Handle createSoftTrigger(
        const nx::vms::api::rules::SoftTriggerData& data,
        Callback<ErrorOrEmpty> callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle getEventsToAcknowledge(
        Callback<ErrorOrData<nx::vms::api::rules::EventLogRecordList>> callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle acknowledge(
        const nx::vms::api::rules::AcknowledgeBookmark& bookmark,
        Callback<ErrorOrData<nx::vms::api::BookmarkV3>> callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    /* Get camera credentials. */
    Handle getCameraCredentials(
        const nx::Uuid& deviceId,
        Callback<QAuthenticator> callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    /**
     * Change user's password on a camera. This method doesn't create new user.
     * Only cameras with capability Qn::SetUserPasswordCapability support it.
     * @param auth user name and new password. User name should exists on camera.
     */
    Handle changeCameraPassword(
        const QnVirtualCameraResourcePtr& camera,
        const QAuthenticator& auth,
        Callback<ErrorOrEmpty> callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    /**
     * Check the list of cameras for discovery. Forms a new list which contains only accessible
     * cameras.
     * @return Request handle.
     */
    int checkCameraList(
        const nx::Uuid& serverId,
        const QnVirtualCameraResourceList& cameras,
        Callback<QnCameraListReply> callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    /**
     * Adds a new virtual camera to this server.
     * @param serverId Target server.
     * @param name Name of the camera.
     */
    Handle addVirtualCamera(
        const nx::Uuid& serverId,
        const QString& name,
        GetCallback callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle prepareVirtualCameraUploads(
        const QnVirtualCameraResourcePtr& camera,
        const QnVirtualCameraPrepareData& data,
        GetCallback callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle virtualCameraStatus(
        const QnVirtualCameraResourcePtr& camera,
        GetCallback callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle lockVirtualCamera(
        const QnVirtualCameraResourcePtr& camera,
        const QnUserResourcePtr& user,
        qint64 ttl,
        GetCallback callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle extendVirtualCameraLock(
        const QnVirtualCameraResourcePtr& camera,
        const QnUserResourcePtr& user,
        const nx::Uuid& token,
        qint64 ttl,
        GetCallback callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle releaseVirtualCameraLock(
        const QnVirtualCameraResourcePtr& camera,
        const nx::Uuid& token,
        GetCallback callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    /**
     * Makes the server consume a media file as a footage for a virtual camera.
     * The file itself should be uploaded (or downloaded) to the server beforehand via
     * file upload API.
     *
     * Note that once the import is completed, the server will delete the original
     * uploaded file.
     *
     * @param camera                    Camera to add footage to.
     * @param token                     Lock token.
     * @param uploadId                  Name of the uploaded file to use.
     * @param startTimeMs               Start time of the footage, in msecs since epoch.
     *
     * @see addFileUpload
     */
    Handle consumeVirtualCameraFile(
        const QnVirtualCameraResourcePtr& camera,
        const nx::Uuid& token,
        const QString& uploadId,
        qint64 startTimeMs,
        PostCallback callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    /** Get statistics for server health monitor. */
    Handle getStatistics(
        const nx::Uuid& serverId,
        GetCallback callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle getAuditLogRecords(
        std::chrono::milliseconds from,
        std::chrono::milliseconds to,
        UbJsonResultCallback callback,
        nx::utils::AsyncHandlerExecutor executor = {},
        std::optional<nx::Uuid> proxyToServer = {});

    Handle lookupObjectTracks(
        const nx::analytics::db::Filter& request,
        bool isLocal,
        Callback<nx::analytics::db::LookupResult> callback,
        nx::utils::AsyncHandlerExecutor executor = {},
        std::optional<nx::Uuid> proxyToServer = {});

    Handle addCamera(
        const nx::Uuid& targetServerId,
        const nx::vms::api::DeviceModelForSearch& devices,
        nx::vms::common::SessionTokenHelperPtr helper,
        Callback<ErrorOrData<nx::vms::api::DeviceModelForSearch>>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle patchCamera(
        const nx::Uuid& targetServerId,
        const nx::vms::api::DeviceModelGeneral& devices,
        nx::vms::common::SessionTokenHelperPtr helper,
        Callback<ErrorOrData<nx::vms::api::DeviceModelForSearch>>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle searchCamera(
        const nx::Uuid& targetServerId,
        const nx::vms::api::DeviceSearch& deviceSearchData,
        nx::vms::common::SessionTokenHelperPtr helper,
        Callback<ErrorOrData<nx::vms::api::DeviceSearch>>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle searchCameraStatus(
        const nx::Uuid& targetServerId,
        const QString& searchId,
        nx::vms::common::SessionTokenHelperPtr helper,
        Callback<ErrorOrData<nx::vms::api::DeviceSearch>>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle searchCameraStop(
        const nx::Uuid& targetServerId,
        const QString& searchId,
        nx::vms::common::SessionTokenHelperPtr helper,
        Callback<ErrorOrEmpty> callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle executeAnalyticsAction(
        const nx::vms::api::AnalyticsAction& action,
        Callback<nx::network::rest::JsonResult> callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle getRemoteArchiveSynchronizationStatus(
        Callback<ErrorOrData<nx::vms::api::RemoteArchiveSynchronizationStatusList>>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle getOverlappedIds(
        const QString& nvrGroupId,
        Callback<nx::vms::api::OverlappedIdResponse> callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle setOverlappedId(
        const QString& nvrGroupId,
        int overlappedId,
        Callback<nx::vms::api::OverlappedIdResponse> callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle executeEventAction(
        const nx::vms::api::EventActionData& action,
        Callback<nx::network::rest::Result> callback,
        nx::utils::AsyncHandlerExecutor executor = {},
        std::optional<nx::Uuid> proxyToServer = {});

    Handle getModuleInformation(
        Callback<ResultWithData<nx::vms::api::ModuleInformation>> callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle getModuleInformationAll(
        Callback<ResultWithData<QList<nx::vms::api::ModuleInformation>>> callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    /**
     * Request servers information.
     * @param onlyFreshInfo Flag to filter out Server Info from offline Servers. If `false` is
     *     passed, info from database will be returned, otherwise a request to each server will be
     *     sent separately.
     */
    Handle getServersInfo(
        bool onlyFreshInfo,
        Callback<ErrorOrData<nx::vms::api::ServerInformationV1List>>&& callback,
        nx::utils::AsyncHandlerExecutor executor);

    Handle getEngineAnalyticsSettings(
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        Callback<nx::vms::api::analytics::EngineSettingsResponse>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle setEngineAnalyticsSettings(
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        const QJsonObject& settings,
        Callback<nx::vms::api::analytics::EngineSettingsResponse>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle engineAnalyticsActiveSettingsChanged(
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        const QString& activeElement,
        const QJsonObject& settingsModel,
        const QJsonObject& settingsValues,
        const QJsonObject& paramValues,
        Callback<nx::vms::api::analytics::EngineActiveSettingChangedResponse>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle getDeviceAnalyticsSettings(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        Callback<nx::vms::api::analytics::DeviceAgentSettingsResponse>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle setDeviceAnalyticsSettings(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        const QJsonObject& settingsValues,
        const QJsonObject& settingsModel,
        Callback<nx::vms::api::analytics::DeviceAgentSettingsResponse>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle deviceAnalyticsActiveSettingsChanged(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        const QString& activeElement,
        const QJsonObject& settingsModel,
        const QJsonObject& settingsValues,
        const QJsonObject& paramValues,
        Callback<nx::vms::api::analytics::DeviceAgentActiveSettingChangedResponse>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle postMetadata(
        const QString& path,
        const QByteArray& messageBody,
        PostCallback&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle getPluginInformation(
        const nx::Uuid& serverId,
        GetCallback callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    /**
     * Checks if the connection with SMTP server described in the corresponding data structure
     * passed as parameter is successful.
     */
    Handle testEmailSettings(
        const nx::vms::api::EmailSettings& settings,
        Callback<ResultWithData<QnTestEmailSettingsReply>>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {},
        std::optional<nx::Uuid> proxyToServer = {});

    /**
     * Checks if the connection with SMTP server described in the corresponding data structure
     * stored at server's system settings is successful.
     */
    Handle testEmailSettings(
        Callback<ResultWithData<QnTestEmailSettingsReply>>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {},
        std::optional<nx::Uuid> proxyToServer = {});

    Handle getExtendedPluginInformation(
        Callback<nx::vms::api::ExtendedPluginInfoByServer>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle recordedTimePeriods(
        const QnChunksRequestData& request,
        Callback<MultiServerPeriodDataList>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle getStorageStatus(
        const nx::Uuid& serverId,
        const QString& path,
        Callback<ResultWithData<StorageStatusReply>>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle checkStoragePath(
        const QString& path,
        Callback<ErrorOrData<nx::vms::api::StorageSpaceDataWithDbInfoV3>>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle setStorageEncryptionPassword(
        const QString& password,
        bool makeCurrent,
        const QByteArray& salt,
        PostCallback&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle startArchiveRebuild(
        const nx::Uuid& serverId,
        const QString pool,
        Callback<ErrorOrData<nx::vms::api::StorageScanInfoFull>>&& callback,
        nx::utils::AsyncHandlerExecutor executor);

    Handle getArchiveRebuildProgress(
        const nx::Uuid& serverId,
        const QString pool,
        Callback<ErrorOrData<nx::vms::api::StorageScanInfoFull>>&& callback,
        nx::utils::AsyncHandlerExecutor executor);

    Handle stopArchiveRebuild(
        const nx::Uuid& serverId,
        const QString pool,
        Callback<ErrorOrEmpty>&& callback,
        nx::utils::AsyncHandlerExecutor executor);

    /** Request the name of a system from the server. */
    Handle getSystemIdFromServer(
        const nx::Uuid& serverId,
        Callback<QString>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    /**
     * Request the server to run the camera diagnostics step following previousStep.
     * @param slot Slot MUST have signature (int, QnCameraDiagnosticsReply, int).
     * @return Request handle.
     */
    Handle doCameraDiagnosticsStep(
        const nx::Uuid& serverId,
        const nx::Uuid& cameraId,
        CameraDiagnostics::Step::Value previousStep,
        Callback<ResultWithData<QnCameraDiagnosticsReply>>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    using LdapAuthenticateCallback = nx::utils::MoveOnlyFunc<void(
        Handle requestId,
        ErrorOrData<nx::vms::api::UserModelV3> userOrError,
        nx::network::rest::AuthResult authResult)>;

    Handle ldapAuthenticateAsync(
        const nx::vms::api::Credentials& credentials,
        bool localOnly,
        LdapAuthenticateCallback&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle loginInfoAsync(
        const QString& login,
        bool localOnly,
        Callback<ErrorOrData<nx::vms::api::LoginUser>>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    /**
     * Tests connection with the LDAP Server using settings provided. Returns first user DNs found
     * per each filter from `filters` into the callback.
     */
    Handle testLdapSettingsAsync(
        const nx::vms::api::LdapSettings& settings,
        Callback<ErrorOrData<std::vector<QString>>>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    /**
     * Overwrites existing settings with provided settings.
     */
    Handle setLdapSettingsAsync(
        const nx::vms::api::LdapSettings& settings,
        nx::vms::common::SessionTokenHelperPtr helper,
        Callback<ErrorOrData<nx::vms::api::LdapSettings>>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    /**
     * Modifies existing settings with provided non-empty settings and allows to clear existing LDAP
     * users/groups.
     */
    Handle modifyLdapSettingsAsync(
        const nx::vms::api::LdapSettings& settings,
        nx::vms::common::SessionTokenHelperPtr helper,
        Callback<ErrorOrData<nx::vms::api::LdapSettings>>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle getLdapSettingsAsync(
        Callback<ErrorOrData<nx::vms::api::LdapSettings>>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle getLdapStatusAsync(
        Callback<ErrorOrData<nx::vms::api::LdapStatus>>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle syncLdapAsync(
        nx::vms::common::SessionTokenHelperPtr helper,
        Callback<ErrorOrEmpty>&& callback,
        nx::utils::AsyncHandlerExecutor executor);

    Handle resetLdapAsync(
        nx::vms::common::SessionTokenHelperPtr helper,
        Callback<ErrorOrEmpty>&& callback,
        nx::utils::AsyncHandlerExecutor executor);

    Handle saveUserAsync(
        bool newUser,
        const nx::vms::api::UserModelV3& userData,
        nx::vms::common::SessionTokenHelperPtr helper,
        Callback<ErrorOrData<nx::vms::api::UserModelV3>>&& callback,
        nx::utils::AsyncHandlerExecutor executor);

    Handle patchUserSettings(
        nx::Uuid id,
        const nx::vms::api::UserSettings& settings,
        Callback<ErrorOrData<nx::vms::api::UserModelV3>>&& callback,
        nx::utils::AsyncHandlerExecutor executor);

    Handle removeUserAsync(
        const nx::Uuid& userId,
        nx::vms::common::SessionTokenHelperPtr helper,
        Callback<ErrorOrEmpty>&& callback,
        nx::utils::AsyncHandlerExecutor executor);

    Handle saveGroupAsync(
        bool newGroup,
        const nx::vms::api::UserGroupModel& groupData,
        nx::vms::common::SessionTokenHelperPtr helper,
        Callback<ErrorOrData<nx::vms::api::UserGroupModel>>&& callback,
        nx::utils::AsyncHandlerExecutor executor);

    Handle removeGroupAsync(
        const nx::Uuid& groupId,
        nx::vms::common::SessionTokenHelperPtr helper,
        Callback<ErrorOrEmpty>&& callback,
        nx::utils::AsyncHandlerExecutor executor);

    /**
     *  Requests the one-time authorization ticket which can be used in rest API requests.
     *  As ticket works only on the server which returned it we need to specify targetServerId.
     */
    Handle createTicket(
        const nx::Uuid& targetServerId,
        Callback<ErrorOrData<nx::vms::api::LoginSession>> callback,
        nx::utils::AsyncHandlerExecutor executor);

    Handle getCurrentSession(
        Callback<ErrorOrData<nx::vms::api::LoginSession>> callback,
        nx::utils::AsyncHandlerExecutor executor);

    Handle loginAsync(
        const nx::vms::api::LoginSessionRequest& data,
        Callback<ErrorOrData<nx::vms::api::LoginSession>> callback,
        nx::utils::AsyncHandlerExecutor executor);

    Handle loginAsync(
        const nx::vms::api::TemporaryLoginSessionRequest& data,
        Callback<ErrorOrData<nx::vms::api::LoginSession>> callback,
        nx::utils::AsyncHandlerExecutor executor);

    Handle replaceDevice(
        const nx::Uuid& deviceToBeReplacedId,
        const QString& replacementDevicePhysicalId,
        bool returnReportOnly,
        Callback<nx::vms::api::DeviceReplacementResponse>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle undoReplaceDevice(
        const nx::Uuid& deviceId,
        PostCallback&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle debug(
        const QString& action,
        const QString& value,
        PostCallback callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle getLookupLists(
        Callback<ErrorOrData<nx::vms::api::LookupListDataList>>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle saveLookupList(
        const nx::vms::api::LookupListData& lookupList,
        Callback<ErrorOrEmpty> callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle jsonRpcBatchCall(
        nx::vms::common::SessionTokenHelperPtr helper,
        const std::vector<nx::vms::api::JsonRpcRequest>& requests,
        JsonRpcBatchResultCallback&& callback,
        nx::utils::AsyncHandlerExecutor executor = {},
        std::optional<Timeouts> timeouts = {});

    /** Sends POST request with a response to be a JSON */
    Handle postJsonResult(
        const QString& action,
        const nx::network::rest::Params& params,
        const QByteArray& body,
        JsonResultCallback&& callback,
        nx::utils::AsyncHandlerExecutor executor = {},
        std::optional<Timeouts> timeouts = {},
        std::optional<nx::Uuid> proxyToServer = {});

    template <typename ResultType>
    Handle sendRequest(
        nx::vms::common::SessionTokenHelperPtr helper,
        nx::network::http::Method method,
        const QString& action,
        const nx::network::rest::Params& params,
        const nx::String& body,
        Callback<ResultType>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {},
        const nx::network::http::HttpHeaders& customHeaders = {});

    Handle getUbJsonResult(
        const QString& action,
        nx::network::rest::Params params,
        UbJsonResultCallback&& callback,
        nx::utils::AsyncHandlerExecutor executor = {},
        std::optional<nx::Uuid> proxyToServer = {});

    Handle getJsonResult(
        const QString& action,
        nx::network::rest::Params params,
        JsonResultCallback&& callback,
        nx::utils::AsyncHandlerExecutor executor = {},
        std::optional<nx::Uuid> proxyToServer = {});

    Handle getRawResult(
        const QString& action,
        const nx::network::rest::Params& params,
        DataCallback callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    /**
     * Cancel running request by known requestId. If request is canceled, callback isn't called.
     * If target thread has been used then callback may be called after 'cancelRequest' in case of
     * data already received and queued to a target thread.
     * If ServerConnection is destroyed all running requests are canceled, no callbacks called.
     *
     * Note that this effectively means that YOU ARE NEVER SAFE, even if you've cancelled all your
     * requests in your destructor. Better bulletproof your callbacks with `guarded`.
     */
    void cancelRequest(const Handle& requestId);

    enum class DebugFlag
    {
        none = 0,
        disableThumbnailRequests = 0x01
    };
    Q_DECLARE_FLAGS(DebugFlags, DebugFlag);

    /**
     * Flags altering functionality for debugging purposes.
     * Affect all ServerConnection instances.
     * These functions are not thread-safe.
     */
    static DebugFlags debugFlags();
    static void setDebugFlag(DebugFlag flag, bool on);

private:
    ContextPtr prepareContext(
        const nx::network::http::ClientPool::Request& request,
        nx::utils::MoveOnlyFunc<void (ContextPtr)> callback,
        std::optional<Timeouts> timeouts = std::nullopt);

    QUrl prepareUrl(const QString& path, const nx::network::rest::Params& params) const;

    /**
     * Fills in http request header with parameters like authentication or proxy.
     * It will fall back to prepareDirectRequest if directUrl is not empty.
     *
     * @param method HTTP method, like {GET, PUT, POST, ...}
     * @param url path to requested resource, like /ec2/updateStatus/
     * @param contentType content type according to HTTP reference.
     * @param messageBody message payload.
     * @return generated request.
     */
    nx::network::http::ClientPool::Request prepareRequest(
        nx::network::http::Method method,
        const QUrl& url,
        const nx::String& contentType = nx::String(),
        const nx::String& messageBody = nx::String());

    /** Same as prepareRequest, but JSON-oriented. */
    nx::network::http::ClientPool::Request prepareRestRequest(
        nx::network::http::Method method,
        const QUrl& url,
        const nx::String& messageBody = {});

    /** Passes Context to ClientPool. */
    Handle sendRequest(const nx::network::http::ClientPool::ContextPtr& context);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;

    /**
     * Generic requests, for the types, that should not be exposed to common library.
     */
    template<typename ResultType> Handle executeGet(
        const QString& path,
        const nx::network::rest::Params& params,
        Callback<ResultType> callback,
        nx::utils::AsyncHandlerExecutor executor,
        std::optional<nx::Uuid> proxyToServer = {},
        std::optional<Timeouts> timeouts = std::nullopt);

    template <typename ResultType> Handle executePost(
        const QString& path,
        const nx::network::rest::Params& params,
        Callback<ResultType> callback,
        nx::utils::AsyncHandlerExecutor executor,
        std::optional<nx::Uuid> proxyToServer = {});

    /**
     * Send POST request with json-encoded data with no parameters.
     * @param path Url path.
     * @param messageBody Json document.
     * @param callback Callback.
     * @param executor Thread for the callback. If not passed, callback will be handled in any
     *     of the AIO threads.
     * @param proxyToServer Id of the server, which must handle the request.
     */
    template <typename ResultType>
    Handle executePost(
        const QString& path,
        const nx::String& messageBody,
        Callback<ResultType>&& callback,
        nx::utils::AsyncHandlerExecutor executor = {},
        std::optional<nx::Uuid> proxyToServer = {});

    /**
     * This overload should only be used if API requires custom message body so parameters can only
     * be passed by URL.
     */
    template <typename ResultType> Handle executePost(
        const QString& path,
        const nx::network::rest::Params& params,
        const nx::String& contentType,
        const nx::String& messageBody,
        Callback<ResultType> callback,
        nx::utils::AsyncHandlerExecutor executor,
        std::optional<Timeouts> timeouts = {},
        std::optional<nx::Uuid> proxyToServer = {});

    /**
     * This overload should only be used if API requires custom message body so parameters can only
     * be passed by URL.
     */
    template <typename ResultType> Handle executePut(
        const QString& path,
        const nx::network::rest::Params& params,
        const nx::String& contentType,
        const nx::String& messageBody,
        Callback<ResultType> callback,
        nx::utils::AsyncHandlerExecutor executor,
        std::optional<nx::Uuid> proxyToServer = {});

    /**
     * This overload should be used only if an API requires a custom message body so its parameters
     * can be passed only by URL.
     */
    template <typename ResultType> Handle executePatch(
        const QString& path,
        const nx::network::rest::Params& params,
        const nx::String& contentType,
        const nx::String& messageBody,
        Callback<ResultType> callback,
        nx::utils::AsyncHandlerExecutor executor);

    template <typename ResultType> Handle executeDelete(
        const QString& path,
        const nx::network::rest::Params& params,
        Callback<ResultType> callback,
        nx::utils::AsyncHandlerExecutor executor,
        std::optional<nx::Uuid> proxyToServer = {});
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ServerConnection::DebugFlags);

using Empty = EmptyResponseType;
using ErrorOrEmpty = ServerConnection::ErrorOrEmpty;

} // namespace rest
