// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <variant>

#include <QtCore/QThread>

#include <analytics/db/analytics_db_types.h>
#include <api/helpers/request_helpers_fwd.h>
#include <api/http_client_pool.h>
#include <api/model/analytics_actions.h>
#include <api/model/audit/audit_record.h>
#include <api/model/camera_diagnostics_reply.h>
#include <api/model/camera_list_reply.h>
#include <api/model/getnonce_reply.h>
#include <api/model/manual_camera_seach_reply.h>
#include <api/model/storage_status_reply.h>
#include <api/model/test_email_settings_reply.h>
#include <api/model/time_reply.h>
#include <api/model/virtual_camera_prepare_data.h>
#include <core/resource/resource_fwd.h>
#include <nx/api/mediaserver/requests_fwd.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/http/http_types.h>
#include <nx/network/rest/params.h>
#include <nx/network/rest/result.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/system_error.h>
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
#include <nx/vms/api/data/saveable_system_settings.h>
#include <nx/vms/api/data/storage_init_result.h>
#include <nx/vms/api/data/storage_scan_info.h>
#include <nx/vms/api/data/storage_space_data.h>
#include <nx/vms/api/data/time_reply.h>
#include <nx/vms/api/data/user_data.h>
#include <nx/vms/api/data/user_group_model.h>
#include <nx/vms/api/data/user_model.h>
#include <nx/vms/api/rules/event_log_fwd.h>
#include <nx/vms/auth/auth_result.h>
#include <nx/vms/event/event_fwd.h>
#include <nx/vms/utils/abstract_session_token_helper.h>
#include <recording/time_period_list.h>
#include <utils/camera/camera_diagnostics.h>

#include "server_rest_connection_fwd.h"

namespace nx::vms::common {

class AbstractCertificateVerifier;
class SystemContext;

} // namespace nx::vms::common

namespace rest {

template <typename T>
struct RestResultWithData
{
    RestResultWithData() {}
    RestResultWithData(const nx::network::rest::Result& restResult, T data):
        error(restResult.error),
        errorString(restResult.errorString),
        data(std::move(data))
    {
    }

    RestResultWithData(const RestResultWithData&) = delete;
    RestResultWithData(RestResultWithData&&) = default;
    RestResultWithData& operator=(const RestResultWithData&) = delete;
    RestResultWithData& operator=(RestResultWithData&&) = default;

    nx::network::rest::Result::Error error = nx::network::rest::Result::NoError;
    QString errorString;
    T data;
};

template<typename Data>
using ErrorOrData = std::variant<nx::network::rest::Result, Data>;

using EventLogData = RestResultWithData<nx::vms::event::ActionDataList>;
using MultiServerTimeData = RestResultWithData<nx::vms::api::ServerTimeReplyList>;

struct ServerConnectionBase
{
    template<typename ResultType>
    struct Result
    {
        using type = std::function<void (bool success, Handle requestId, ResultType result)>;
    };
};

template<>
struct ServerConnectionBase::Result<QByteArray>
{
    using type = std::function<void(bool success, Handle requestId, QByteArray result,
        const nx::network::http::HttpHeaders& headers)>;
};

/**
 * Class for HTTP requests to mediaServer.
 * This class can be used either for client and server side.
 * Class calls callback methods from IO thread. So, caller code should be thread-safe.
 * Client MUST NOT make requests in callbacks as it will cause a deadlock.
 * This class limits number of underlying requests using separate http clients pool with 8 threads.
 */
class NX_VMS_COMMON_API ServerConnection:
    public QObject,
    public ServerConnectionBase
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
        const QnUuid& serverId);

    /**
     * Used to send REST requests to the mediaserver when we still do not have proper resource pool
     * and messageBusConnection instance, but need to send requests to the server.
     *
     * @param serverId resource Id of the VMS server.
     * @param address Explicit address of the VMS server.
     * @param credentials Credentials to authorize requests.
     */
    ServerConnection(
        const QnUuid& serverId,
        const QnUuid& auditId,
        AbstractCertificateVerifier* certificateVerifier,
        nx::network::SocketAddress address,
        nx::network::http::Credentials credentials);

    void updateSessionId(const QnUuid& sessionId);

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

    /**
     * Set timeouts for all http requests.
     */
    void setDefaultTimeouts(Timeouts timeouts);

    struct EmptyResponseType {};
    typedef Result<EmptyResponseType>::type PostCallback;   // use this type for POST requests without result data

    /**
     * Default callback type for GET requests without result data.
     */
    typedef Result<nx::network::rest::JsonResult>::type GetCallback;

    using ContextPtr = nx::network::http::ClientPool::ContextPtr;

    using ErrorOrEmpty = ErrorOrData<EmptyResponseType>;

    /**
    * Load information about cross-server archive
    * @return value > 0 on success or <= 0 if it isn't started.
    * @param targetThread execute callback in a target thread if specified. It convenient for UI purpose.
    * By default callback is called in IO thread.
    */
    Handle cameraHistoryAsync(
        const QnChunksRequestData& request,
        Result<nx::vms::api::CameraHistoryDataList>::type callback,
        QThread* targetThread = nullptr);

    Handle backupPositionAsync(const QnUuid& serverId,
        const QnUuid& deviceId,
        Result<nx::vms::api::BackupPositionEx>::type callback,
        QThread* targetThread = nullptr);

    Handle setBackupPositionAsync(const QnUuid& serverId,
        const QnUuid& deviceId,
        const nx::vms::api::BackupPosition& backupPosition,
        Result<nx::vms::api::BackupPosition>::type callback,
        QThread* targetThread = nullptr);

    Handle setBackupPositionsAsync(const QnUuid& serverId,
        const nx::vms::api::BackupPosition& backupPosition,
        Result<nx::vms::api::BackupPosition>::type callback,
        QThread* targetThread = nullptr);

    Handle getServerLocalTime(
        const QnUuid& serverId,
        Result<nx::network::rest::JsonResult>::type callback,
        QThread* targetThread = nullptr);

    /**
     * Get camera thumbnail for specified time.
     *
     * Returns immediately. On request completion callback is called.
     *
     * @param targetThread Callback thread. If not specified, callback is called in IO thread.
     * @return Request handle.
     */
    Handle cameraThumbnailAsync(
        const nx::api::CameraImageRequest& request,
        Result<QByteArray>::type callback,
        QThread* targetThread = 0);

    Handle createGenericEvent(
        const QString& source,
        const QString& caption,
        const QString& description,
        const nx::vms::event::EventMetaData& metadata,
        nx::vms::api::EventState toggleState = nx::vms::api::EventState::undefined,
        GetCallback callback = nullptr,
        QThread* targetThread = nullptr);

    Handle sendStatisticsUsingServer(
        const QnUuid& proxyServerId,
        const QnSendStatisticsRequestData& statisticsData,
        PostCallback callback,
        QThread* targetThread = nullptr);

    /**
     * Binds the System to the cloud using cloud credentials.
     */
    Handle bindSystemToCloud(
        const QString& cloudSystemId,
        const QString& cloudAuthKey,
        const QString& cloudAccountName,
        const QString& organizationId,
        const std::string& ownerSessionToken,
        Result<ErrorOrEmpty>::type callback,
        QThread* targetThread = nullptr);

    /**
     * Unbind the System from the Cloud and reset local admin password (optionally).
     */
    Handle unbindSystemFromCloud(
        nx::vms::common::SessionTokenHelperPtr helper,
        const QString& password,
        Result<ErrorOrEmpty>::type callback,
        QThread* targetThread = nullptr);

    Handle dumpDatabase(
        nx::vms::common::SessionTokenHelperPtr helper,
        Result<ErrorOrData<QByteArray>>::type callback,
        QThread* targetThread);

    Handle restoreDatabase(
        nx::vms::common::SessionTokenHelperPtr helper,
        const QByteArray& data,
        Result<ErrorOrEmpty>::type callback,
        QThread* targetThread);

    Handle putServerLogSettings(
        nx::vms::common::SessionTokenHelperPtr helper,
        const QnUuid& serverId,
        const nx::vms::api::ServerLogSettings& settings,
        Result<ErrorOrEmpty>::type callback,
        QThread* targetThread);

    Handle patchSystemSettings(
        nx::vms::common::SessionTokenHelperPtr helper,
        const nx::vms::api::SaveableSystemSettings& settings,
        Result<ErrorOrEmpty>::type callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    /* DistributedFileDownloader API */
    Handle addFileDownload(
        const QString& fileName,
        qint64 size,
        const QByteArray& md5,
        const QUrl& url,
        const QString& peerPolicy,
        GetCallback callback,
        QThread* targetThread = nullptr);

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
    typedef Result<nx::network::rest::JsonResult>::type AddUploadCallback;

    Handle addFileUpload(
        const QnUuid& serverId,
        const QString& fileName,
        qint64 size,
        qint64 chunkSize,
        const QByteArray& md5,
        qint64 ttl,
        bool recreateIfExists,
        AddUploadCallback callback,
        QThread* targetThread = nullptr);

    Handle removeFileDownload(
        const QnUuid& serverId,
        const QString& fileName,
        bool deleteData,
        PostCallback callback = nullptr,
        QThread* targetThread = nullptr);

    Handle fileChunkChecksums(
        const QnUuid& serverId,
        const QString& fileName,
        GetCallback callback,
        QThread* targetThread = nullptr);

    Handle downloadFileChunk(
        const QnUuid& serverId,
        const QString& fileName,
        int chunkIndex,
        Result<QByteArray>::type callback,
        QThread* targetThread = nullptr);

    Handle downloadFileChunkFromInternet(
        const QnUuid& serverId,
        const QString& fileName,
        const nx::utils::Url &url,
        int chunkIndex,
        int chunkSize,
        qint64 fileSize,
        Result<QByteArray>::type callback,
        QThread* targetThread = nullptr);

    Handle uploadFileChunk(
        const QnUuid& serverId,
        const QString& fileName,
        int index,
        const QByteArray& data,
        PostCallback callback,
        QThread* targetThread = nullptr);

    Handle downloadsStatus(
        GetCallback callback,
        QThread* targetThread = nullptr);

    Handle fileDownloadStatus(
        const QnUuid& serverId,
        const QString& fileName,
        GetCallback callback,
        QThread* targetThread = nullptr);

    Handle getTimeOfServersAsync(
        Result<MultiServerTimeData>::type callback,
        QThread* targetThread = nullptr);

    Handle getEvents(
        const QnUuid& serverId,
        QnEventLogRequestData request,
        Result<EventLogData>::type callback,
        QThread* targetThread = nullptr);

    Handle getEvents(const QnEventLogMultiserverRequestData& request,
        Result<EventLogData>::type callback,
        QThread* targetThread = nullptr);

    Handle eventLog(
        const nx::vms::api::rules::EventLogFilter& filter,
        Result<ErrorOrData<nx::vms::api::rules::EventLogRecordList>>::type callback,
        QThread* targetThread = nullptr,
        std::optional<Timeouts> timeouts = std::nullopt);

    /* Get camera credentials. */
    Handle getCameraCredentials(
        const QnUuid& deviceId,
        Result<QAuthenticator>::type callback,
        QThread* targetThread = nullptr);

    /**
     * Change user's password on a camera. This method doesn't create new user.
     * Only cameras with capability Qn::SetUserPasswordCapability support it.
     * @param auth user name and new password. User name should exists on camera.
     */
    Handle changeCameraPassword(
        const QnVirtualCameraResourcePtr& camera,
        const QAuthenticator& auth,
        Result<ErrorOrEmpty>::type callback,
        QThread* targetThread = nullptr);

    /**
     * Check the list of cameras for discovery. Forms a new list which contains only accessible
     * cameras.
     * @return Request handle.
     */
    int checkCameraList(
        const QnUuid& serverId,
        const QnNetworkResourceList& cameras,
        Result<QnCameraListReply>::type callback,
        QThread* targetThread = nullptr);

    /**
     * Adds a new virtual camera to this server.
     * @param serverId Target server.
     * @param name Name of the camera.
     */
    Handle addVirtualCamera(
        const QnUuid& serverId,
        const QString& name,
        GetCallback callback,
        QThread* targetThread = nullptr);

    Handle prepareVirtualCameraUploads(
        const QnNetworkResourcePtr& camera,
        const QnVirtualCameraPrepareData& data,
        GetCallback callback,
        QThread* targetThread = nullptr);

    Handle virtualCameraStatus(
        const QnNetworkResourcePtr& camera,
        GetCallback callback,
        QThread* targetThread = nullptr);

    Handle lockVirtualCamera(
        const QnNetworkResourcePtr& camera,
        const QnUserResourcePtr& user,
        qint64 ttl,
        GetCallback callback,
        QThread* targetThread = nullptr);

    Handle extendVirtualCameraLock(
        const QnNetworkResourcePtr& camera,
        const QnUserResourcePtr& user,
        const QnUuid& token,
        qint64 ttl,
        GetCallback callback,
        QThread* targetThread = nullptr);

    Handle releaseVirtualCameraLock(
        const QnNetworkResourcePtr& camera,
        const QnUuid& token,
        GetCallback callback,
        QThread* targetThread = nullptr);

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
        const QnNetworkResourcePtr& camera,
        const QnUuid& token,
        const QString& uploadId,
        qint64 startTimeMs,
        PostCallback callback,
        QThread* targetThread = nullptr);

    /** Get statistics for server health monitor. */
    Handle getStatistics(
        const QnUuid& serverId,
        GetCallback callback,
        QThread* targetThread = nullptr);

    Handle getAuditLogRecords(
        std::chrono::milliseconds from,
        std::chrono::milliseconds to,
        UbJsonResultCallback callback,
        QThread* targetThread = nullptr,
        std::optional<QnUuid> proxyToServer = {});

    Handle lookupObjectTracks(
        const nx::analytics::db::Filter& request,
        bool isLocal,
        Result<nx::analytics::db::LookupResult>::type callback,
        QThread* targetThread = nullptr,
        std::optional<QnUuid> proxyToServer = {});

    Handle addCamera(
        const QnUuid& targetServerId,
        const nx::vms::api::DeviceModelForSearch& devices,
        nx::vms::common::SessionTokenHelperPtr helper,
        Result<ErrorOrData<nx::vms::api::DeviceModelForSearch>>::type&& callback,
        QThread* targetThread = nullptr);

    Handle searchCamera(
        const QnUuid& targetServerId,
        const nx::vms::api::DeviceSearch& deviceSearchData,
        nx::vms::common::SessionTokenHelperPtr helper,
        Result<ErrorOrData<nx::vms::api::DeviceSearch>>::type&& callback,
        QThread* targetThread = nullptr);

    Handle searchCameraStatus(
        const QnUuid& targetServerId,
        const QnUuid& searchId,
        nx::vms::common::SessionTokenHelperPtr helper,
        Result<ErrorOrData<nx::vms::api::DeviceSearch>>::type&& callback,
        QThread* targetThread = nullptr);

    Handle searchCameraStop(
        const QnUuid& targetServerId,
        const QnUuid& processUuid,
        nx::vms::common::SessionTokenHelperPtr helper,
        Result<ErrorOrEmpty>::type callback,
        QThread* targetThread = nullptr);

    Handle executeAnalyticsAction(
        const AnalyticsAction& action,
        Result<nx::network::rest::JsonResult>::type callback,
        QThread* targetThread = nullptr);

    Handle getRemoteArchiveSynchronizationStatus(
        Result<ErrorOrData<nx::vms::api::RemoteArchiveSynchronizationStatusList>>::type&& callback,
        QThread* targetThread = nullptr);

    Handle getOverlappedIds(
        const QString& nvrGroupId,
        Result<nx::vms::api::OverlappedIdResponse>::type callback,
        QThread* targetThread = nullptr);

    Handle setOverlappedId(
        const QString& nvrGroupId,
        int overlappedId,
        Result<nx::vms::api::OverlappedIdResponse>::type callback,
        QThread* targetThread = nullptr);

    Handle executeEventAction(
        const nx::vms::api::EventActionData& action,
        Result<nx::network::rest::Result>::type callback,
        QThread* targetThread = nullptr,
        std::optional<QnUuid> proxyToServer = {});

    Handle getModuleInformation(
        Result<RestResultWithData<nx::vms::api::ModuleInformation>>::type callback,
        QThread* targetThread = nullptr);

    Handle getModuleInformationAll(
        Result<RestResultWithData<QList<nx::vms::api::ModuleInformation>>>::type callback,
        QThread* targetThread = nullptr);

    Handle getMediaServers(
        Result<nx::vms::api::MediaServerDataList>::type callback,
        QThread* targetThread = nullptr);

    Handle getServersInfo(
        Result<ErrorOrData<nx::vms::api::ServerInformationList>>::type&& callback,
        QThread* targetThread);

    Handle getEngineAnalyticsSettings(
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        Result<nx::vms::api::analytics::EngineSettingsResponse>::type&& callback,
        QThread* targetThread = nullptr);

    Handle setEngineAnalyticsSettings(
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        const QJsonObject& settings,
        Result<nx::vms::api::analytics::EngineSettingsResponse>::type&& callback,
        QThread* targetThread = nullptr);

    Handle engineAnalyticsActiveSettingsChanged(
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        const QString& activeElement,
        const QJsonObject& settingsModel,
        const QJsonObject& settingsValues,
        const QJsonObject& paramValues,
        Result<nx::vms::api::analytics::EngineActiveSettingChangedResponse>::type&& callback,
        QThread* targetThread = nullptr);

    Handle getDeviceAnalyticsSettings(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        Result<nx::vms::api::analytics::DeviceAgentSettingsResponse>::type&& callback,
        QThread* targetThread = nullptr);

    Handle setDeviceAnalyticsSettings(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        const QJsonObject& settings,
        const QnUuid& settingsModelId,
        Result<nx::vms::api::analytics::DeviceAgentSettingsResponse>::type&& callback,
        QThread* targetThread = nullptr);

    Handle deviceAnalyticsActiveSettingsChanged(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        const QString& activeElement,
        const QJsonObject& settingsModel,
        const QJsonObject& settingsValues,
        const QJsonObject& paramValues,
        Result<nx::vms::api::analytics::DeviceAgentActiveSettingChangedResponse>::type&& callback,
        QThread* targetThread = nullptr);

    Handle getPluginInformation(
        const QnUuid& serverId,
        GetCallback callback,
        QThread* targetThread = nullptr);

    /**
     * Checks if the connection with SMTP server described in the corresponding data structure
     * passed as parameter is successful.
     */
    Handle testEmailSettings(
        const nx::vms::api::EmailSettings& settings,
        Result<RestResultWithData<QnTestEmailSettingsReply>>::type&& callback,
        QThread* targetThread = nullptr,
        std::optional<QnUuid> proxyToServer = {});

    /**
     * Checks if the connection with SMTP server described in the corresponding data structure
     * stored at server's system settings is successful.
     */
    Handle testEmailSettings(
        Result<RestResultWithData<QnTestEmailSettingsReply>>::type&& callback,
        QThread* targetThread = nullptr,
        std::optional<QnUuid> proxyToServer = {});

    Handle getExtendedPluginInformation(
        Result<nx::vms::api::ExtendedPluginInfoByServer>::type&& callback,
        QThread* targetThread = nullptr);

    Handle recordedTimePeriods(
        const QnChunksRequestData& request,
        Result<MultiServerPeriodDataList>::type&& callback,
        QThread* targetThread = nullptr);

    Handle getStorageStatus(
        const QnUuid& serverId,
        const QString& path,
        Result<RestResultWithData<StorageStatusReply>>::type&& callback,
        QThread* targetThread = nullptr);

    Handle setStorageEncryptionPassword(
        const QString& password,
        bool makeCurrent,
        const QByteArray& salt,
        PostCallback&& callback,
        QThread* targetThread = nullptr);

    Handle startArchiveRebuild(
        const QnUuid& serverId,
        const QString pool,
        Result<ErrorOrData<nx::vms::api::StorageScanInfoFull>>::type&& callback,
        QThread* targetThread);

    Handle getArchiveRebuildProgress(
        const QnUuid& serverId,
        const QString pool,
        Result<ErrorOrData<nx::vms::api::StorageScanInfoFull>>::type&& callback,
        QThread* targetThread);

    Handle stopArchiveRebuild(
        const QnUuid& serverId,
        const QString pool,
        Result<ErrorOrEmpty>::type&& callback,
        QThread* targetThread);

    /** Request the name of a system from the server. */
    Handle getSystemIdFromServer(
        const QnUuid& serverId,
        Result<QString>::type&& callback,
        QThread* targetThread = nullptr);

    /**
     * Request the server to run the camera diagnostics step following previousStep.
     * @param slot Slot MUST have signature (int, QnCameraDiagnosticsReply, int).
     * @return Request handle.
     */
    Handle doCameraDiagnosticsStep(
        const QnUuid& serverId,
        const QnUuid& cameraId,
        CameraDiagnostics::Step::Value previousStep,
        Result<RestResultWithData<QnCameraDiagnosticsReply>>::type&& callback,
        QThread* targetThread = nullptr);

    using LdapAuthenticateCallback = std::function<void(
        Handle requestId,
        ErrorOrData<nx::vms::api::UserModelV3> userOrError,
        nx::vms::common::AuthResult authResult)>;

    Handle ldapAuthenticateAsync(
        const nx::vms::api::Credentials& credentials,
        bool localOnly,
        LdapAuthenticateCallback&& callback,
        QThread* targetThread = nullptr);

    Handle loginInfoAsync(
        const QString& login,
        bool localOnly,
        Result<ErrorOrData<nx::vms::api::LoginUser>>::type&& callback,
        QThread* targetThread = nullptr);

    /**
     * Tests connection with the LDAP Server using settings provided. Returns first user DNs found
     * per each filter from `filters` into the callback.
     */
    Handle testLdapSettingsAsync(
        const nx::vms::api::LdapSettings& settings,
        Result<ErrorOrData<std::vector<QString>>>::type&& callback,
        QThread* targetThread = nullptr);

    /**
     * Overwrites existing settings with provided settings.
     */
    Handle setLdapSettingsAsync(
        const nx::vms::api::LdapSettings& settings,
        nx::vms::common::SessionTokenHelperPtr helper,
        Result<ErrorOrData<nx::vms::api::LdapSettings>>::type&& callback,
        QThread* targetThread = nullptr);

    /**
     * Modifies existing settings with provided non-empty settings and allows to clear existing LDAP
     * users/groups.
     */
    Handle modifyLdapSettingsAsync(
        const nx::vms::api::LdapSettings& settings,
        nx::vms::common::SessionTokenHelperPtr helper,
        Result<ErrorOrData<nx::vms::api::LdapSettings>>::type&& callback,
        QThread* targetThread = nullptr);

    Handle getLdapSettingsAsync(
        Result<ErrorOrData<nx::vms::api::LdapSettings>>::type&& callback,
        QThread* targetThread = nullptr);

    Handle getLdapStatusAsync(
        Result<ErrorOrData<nx::vms::api::LdapStatus>>::type&& callback,
        QThread* targetThread = nullptr);

    Handle syncLdapAsync(
        nx::vms::common::SessionTokenHelperPtr helper,
        Result<ErrorOrEmpty>::type&& callback,
        QThread* targetThread);

    Handle resetLdapAsync(
        nx::vms::common::SessionTokenHelperPtr helper,
        Result<ErrorOrEmpty>::type&& callback,
        QThread* targetThread);

    Handle saveUserAsync(
        bool newUser,
        const nx::vms::api::UserModelV3& userData,
        nx::vms::common::SessionTokenHelperPtr helper,
        Result<ErrorOrData<nx::vms::api::UserModelV3>>::type&& callback,
        QThread* targetThread);

    Handle removeUserAsync(
        const QnUuid& userId,
        nx::vms::common::SessionTokenHelperPtr helper,
        Result<ErrorOrEmpty>::type&& callback,
        QThread* targetThread);

    Handle saveGroupAsync(
        bool newGroup,
        const nx::vms::api::UserGroupModel& groupData,
        nx::vms::common::SessionTokenHelperPtr helper,
        Result<ErrorOrData<nx::vms::api::UserGroupModel>>::type&& callback,
        QThread* targetThread);

    Handle removeGroupAsync(
        const QnUuid& groupId,
        nx::vms::common::SessionTokenHelperPtr helper,
        Result<ErrorOrEmpty>::type&& callback,
        QThread* targetThread);

    Handle loginAsync(
        const nx::vms::api::LoginSessionRequest& data,
        Result<ErrorOrData<nx::vms::api::LoginSession>>::type callback,
        QThread* targetThread);

    Handle loginAsync(
        const nx::vms::api::TemporaryLoginSessionRequest& data,
        Result<ErrorOrData<nx::vms::api::LoginSession>>::type callback,
        QThread* targetThread);

    Handle replaceDevice(
        const QnUuid& deviceToBeReplacedId,
        const QString& replacementDevicePhysicalId,
        bool returnReportOnly,
        Result<nx::vms::api::DeviceReplacementResponse>::type&& callback,
        QThread* targetThread = nullptr);

    Handle undoReplaceDevice(
        const QnUuid& deviceId,
        PostCallback&& callback,
        QThread* targetThread = nullptr);

    Handle debug(
        const QString& action,
        const QString& value,
        PostCallback callback,
        QThread* targetThread = nullptr);

    Handle getLookupLists(
        Result<ErrorOrData<nx::vms::api::LookupListDataList>>::type&& callback,
        QThread* targetThread = nullptr);

    Handle jsonRpcBatchCall(
        nx::vms::common::SessionTokenHelperPtr helper,
        const std::vector<nx::vms::api::JsonRpcRequest>& requests,
        JsonRpcBatchResultCallback&& callback,
        QThread* targetThread = nullptr,
        std::optional<Timeouts> timeouts = {});

    /** Sends POST request with a response to be a JSON */
    Handle postJsonResult(
        const QString& action,
        const nx::network::rest::Params& params,
        const QByteArray& body,
        JsonResultCallback&& callback,
        QThread* targetThread = nullptr,
        std::optional<Timeouts> timeouts = {},
        std::optional<QnUuid> proxyToServer = {});

    /** Sends POST request with a response to be an Ubjson. */
    Handle postUbJsonResult(
        const QString& action,
        const nx::network::rest::Params& params,
        const QByteArray& body,
        UbJsonResultCallback&& callback,
        QThread* targetThread = nullptr);

    /** Sends POST request with a response to be an EmptyResult. */
    Handle postEmptyResult(
        const QString& action,
        const nx::network::rest::Params& params,
        const QByteArray& body,
        PostCallback&& callback,
        QThread* targetThread = nullptr,
        std::optional<QnUuid> proxyToServer = {},
        std::optional<Qn::SerializationFormat> contentType = Qn::SerializationFormat::ubjson);

    /** Sends PUT request with a response to be an EmptyResult. */
    Handle putEmptyResult(
        const QString& action,
        const nx::network::rest::Params& params,
        const QByteArray& body,
        PostCallback&& callback,
        QThread* targetThread = nullptr,
        std::optional<QnUuid> proxyToServer = {});

    Handle putRest(
        nx::vms::common::SessionTokenHelperPtr helper,
        const QString& action,
        const nx::network::rest::Params& params,
        const QByteArray& body,
        Result<ErrorOrEmpty>::type callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle patchRest(
        nx::vms::common::SessionTokenHelperPtr helper,
        const QString& action,
        const nx::network::rest::Params& params,
        const nx::String& body,
        Result<ErrorOrData<QByteArray>>::type callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle postRest(
        nx::vms::common::SessionTokenHelperPtr helper,
        const QString& action,
        const nx::network::rest::Params& params,
        const QByteArray& body,
        Result<ErrorOrEmpty>::type callback,
        nx::utils::AsyncHandlerExecutor executor = {});

    Handle getUbJsonResult(
        const QString& action,
        nx::network::rest::Params params,
        UbJsonResultCallback&& callback,
        QThread* targetThread = nullptr,
        std::optional<QnUuid> proxyToServer = {});

    Handle getJsonResult(
        const QString& action,
        nx::network::rest::Params params,
        JsonResultCallback&& callback,
        QThread* targetThread = nullptr,
        std::optional<QnUuid> proxyToServer = {});

    Handle getRawResult(
        const QString& action,
        const nx::network::rest::Params& params,
        Result<QByteArray>::type callback,
        QThread* targetThread = nullptr);

    Handle deleteEmptyResult(
        const QString& action,
        const nx::network::rest::Params& params,
        PostCallback&& callback,
        QThread* target = nullptr);

    Handle deleteRest(
        nx::vms::common::SessionTokenHelperPtr helper,
        const QString& action,
        const nx::network::rest::Params& params,
        Result<ErrorOrEmpty>::type callback,
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
    template<typename ResultType>
    Callback<ResultType> makeSessionAwareCallback(
        nx::vms::common::SessionTokenHelperPtr helper,
        nx::network::http::ClientPool::Request request,
        Callback<ResultType> callback,
        std::optional<nx::network::http::AsyncClient::Timeouts> timeouts = {});

    Result<QByteArray>::type makeSessionAwareCallback(
        nx::vms::common::SessionTokenHelperPtr helper,
        nx::network::http::ClientPool::Request request,
        Result<QByteArray>::type callback,
        nx::network::http::AsyncClient::Timeouts timeouts);

    template<typename ResultType, typename... CallbackParameters, typename... Args>
    typename Result<ResultType>::type makeSessionAwareCallbackInternal(
        nx::vms::common::SessionTokenHelperPtr helper,
        nx::network::http::ClientPool::Request request,
        typename Result<ResultType>::type callback,
        std::optional<nx::network::http::AsyncClient::Timeouts> timeouts,
        Args&&... args);

    template <typename ResultType>
    Handle executeRequest(
        const nx::network::http::ClientPool::Request& request,
        Callback<ResultType> callback,
        QThread* targetThread,
        std::optional<Timeouts> timeouts = {});

    template <typename ResultType>
    Handle executeRequest(
        const nx::network::http::ClientPool::Request& request,
        Callback<ResultType> callback,
        nx::utils::AsyncHandlerExecutor executor = {},
        std::optional<Timeouts> timeouts = {});

    Handle executeRequest(
        const nx::network::http::ClientPool::Request& request,
        Result<QByteArray>::type callback,
        QThread* targetThread,
        std::optional<Timeouts> timeouts = {});

    Handle executeRequest(
        const nx::network::http::ClientPool::Request& request,
        Callback<EmptyResponseType> callback,
        QThread* targetThread,
        std::optional<Timeouts> timeouts = {});

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

    /** Passes request to ClientPool. */
    Handle sendRequest(
        const nx::network::http::ClientPool::Request& request,
        std::function<void (ContextPtr)> callback = {},
        QThread* thread = nullptr,
        std::optional<Timeouts> timeouts = {});

    Handle sendRequest(
        const nx::network::http::ClientPool::Request& request,
        std::function<void (ContextPtr)> callback = {},
        nx::utils::AsyncHandlerExecutor executor = {},
        std::optional<Timeouts> timeouts = {});

    /** Passes Context to ClientPool. */
    Handle sendRequest(const nx::network::http::ClientPool::ContextPtr& context);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;

    /**
     * Generic requests, for the types, that should not be exposed to common library.
     */
    template<typename CallbackType> Handle executeGet(
        const QString& path,
        const nx::network::rest::Params& params,
        CallbackType callback,
        QThread* targetThread,
        std::optional<QnUuid> proxyToServer = {},
        std::optional<Timeouts> timeouts = std::nullopt);

    template <typename ResultType> Handle executePost(
        const QString& path,
        const nx::network::rest::Params& params,
        Callback<ResultType> callback,
        QThread* targetThread,
        std::optional<QnUuid> proxyToServer = {});

    /**
     * Send POST request with json-encoded data with no parameters.
     * @param path Url path.
     * @param messageBody Json document.
     * @param callback Callback.
     * @param targetThread Thread for the callback. If not passed, callback will be handled in any
     *     of the AIO threads.
     * @param proxyToServer Id of the server, which must handle the request.
     */
    template <typename ResultType>
    Handle executePost(
        const QString& path,
        const nx::String& messageBody,
        Callback<ResultType>&& callback,
        QThread* targetThread = nullptr,
        std::optional<QnUuid> proxyToServer = {});

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
        QThread* targetThread,
        std::optional<Timeouts> timeouts = {},
        std::optional<QnUuid> proxyToServer = {});

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
        QThread* targetThread,
        std::optional<QnUuid> proxyToServer = {});

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
        QThread* targetThread);

    template <typename ResultType> Handle executeDelete(
        const QString& path,
        const nx::network::rest::Params& params,
        Callback<ResultType> callback,
        QThread* targetThread,
        std::optional<QnUuid> proxyToServer = {});
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ServerConnection::DebugFlags);

using Empty = ServerConnection::EmptyResponseType;
using ErrorOrEmpty = ServerConnection::ErrorOrEmpty;

} // namespace rest
