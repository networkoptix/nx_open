#pragma once

#include <QtCore/QThread>

#include "server_rest_connection_fwd.h"

#include <nx/network/http/http_types.h>
#include <nx/utils/system_error.h>
#include "utils/common/request_param.h"
#include "nx_ec/data/api_fwd.h"
#include <api/helpers/request_helpers_fwd.h>
#include <nx/api/mediaserver/requests_fwd.h>
#include <nx/network/deprecated/asynchttpclient.h>

#include <rest/server/json_rest_result.h>
#include <nx/utils/safe_direct_connection.h>
#include <api/http_client_pool.h>
#include <nx/vms/event/event_fwd.h>
#include <core/resource/resource_fwd.h>
#include <common/common_module_aware.h>
#include <api/model/time_reply.h>
#include <analytics/detected_objects_storage/analytics_events_storage.h>
#include <api/model/analytics_actions.h>
#include <api/model/wearable_prepare_data.h>
#include <api/model/manual_camera_seach_reply.h>
#include <nx/update/update_information.h>

#include <nx/vms/api/analytics/settings_response.h>

namespace rest {

struct RestResultWithDataBase
{
};

template <typename T>
struct RestResultWithData: public RestResultWithDataBase
{
    RestResultWithData() {}
    RestResultWithData(const QnRestResult& restResult, T data):
        error(restResult.error),
        errorString(restResult.errorString),
        data(std::move(data))
    {
    }

    RestResultWithData(const RestResultWithData&) = delete;
    RestResultWithData(RestResultWithData&&) = default;
    RestResultWithData& operator=(const RestResultWithData&) = delete;
    RestResultWithData& operator=(RestResultWithData&&) = default;

    QnRestResult::Error error;
    QString errorString;
    T data;
};

using EventLogData = RestResultWithData<nx::vms::event::ActionDataList>;
using MultiServerTimeData = RestResultWithData<ApiMultiserverServerDateTimeDataList>;
using UpdateStatusAllData = RestResultWithData<std::vector<nx::update::Status>>;
using UpdateInformationData = RestResultWithData<nx::update::Information>;

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
 * New class for HTTP requests to mediaServer. It should be used instead of deprecated class QnMediaServerConnection.
 * This class can be used either for client and server side.
 * Class calls callback methods from IO thread. So, caller code should be thread-safe.
 * Client MUST NOT make requests in callbacks as it will cause a deadlock.
 */
class ServerConnection:
    public QObject,
    public /*mixin*/ QnCommonModuleAware,
    public /*mixin*/ Qn::EnableSafeDirectConnection,
    public ServerConnectionBase
{
    Q_OBJECT

public:
    /**
     * Used to send REST requests to the mediaserver. It has two modes of authentication:
     *  - directUrl is empty. It will use ec2Connection and MediaServerResource to get
     *     authentication data.
     *  - directUrl has some valid data. ServerConnection will use this url for authentication.
     *
     * Second mode is used in cases, when we stil do not have proper resource pool and
     * ec2Conenction instance, but need to send requests to the server.
     *
     * @param commonModule ServerConnection takes a lot of objects from it.
     * @param serverId resource Id of the mediaserver.
     * @param directUrl direct Url to mediaserver.
     */
    ServerConnection(
        QnCommonModule* commonModule,
        const QnUuid& serverId,
        const nx::utils::Url& directUrl = nx::utils::Url());
    virtual ~ServerConnection();

    struct EmptyResponseType {};
    typedef Result<EmptyResponseType>::type PostCallback;   // use this type for POST requests without result data

    typedef Result<QnJsonRestResult>::type GetCallback; /**< Default callback type for GET requests without result data. */

    /**
    * Load information about cross-server archive
    * @return value > 0 on success or <= 0 if it isn't started.
    * @param targetThread execute callback in a target thread if specified. It convenient for UI purpose.
    * By default callback is called in IO thread.
    */
    Handle cameraHistoryAsync(
        const QnChunksRequestData& request,
        Result<nx::vms::api::CameraHistoryDataList>::type callback,
        QThread* targetThread = 0);

    Handle getServerLocalTime(Result<QnJsonRestResult>::type callback, QThread* targetThread = nullptr);

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

    Handle twoWayAudioCommand(
        const QString& sourceId,
        const QnUuid& cameraId,
        bool start,
        GetCallback callback,
        QThread* targetThread = nullptr);

    Handle softwareTriggerCommand(const QnUuid& cameraId, const QString& triggerId,
            nx::vms::api::EventState toggleState, GetCallback callback, QThread* targetThread = nullptr);

    Handle getStatisticsSettingsAsync(
        Result<QByteArray>::type callback, QThread* targetThread = nullptr);

    Handle sendStatisticsAsync(
        const QnSendStatisticsRequestData &request, PostCallback callback,
        QThread* targetThread = nullptr);

    /**
     * Detach the system from cloud. Admin password will be changed to provided (if provided).
     * @param resetAdminPassword Change administrator password to the provided one.
     */
    Handle detachSystemFromCloud(
        const QString& currentAdminPassword,
        const QString& resetAdminPassword,
        Result<QnRestResult>::type callback, QThread* targetThread = nullptr);

    /**
     * Save the credentials returned by cloud to the database.
     */
    Handle saveCloudSystemCredentials(
        const QString &cloudSystemId,
        const QString &cloudAuthKey,
        const QString &cloudAccountName,
        Result<QnRestResult>::type callback,
        QThread* targetThread = nullptr);

    Handle startLiteClient(
        GetCallback callback,
        QThread* targetThread = nullptr);

    Handle getFreeSpaceForUpdateFiles(
        Result<QnUpdateFreeSpaceReply>::type callback,
        QThread* targetThread = nullptr);

    Handle checkCloudHost(
        Result<QnCloudHostCheckReply>::type callback,
        QThread* targetThread = nullptr);

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
    typedef Result<QnJsonRestResult>::type AddUploadCallback;

    Handle addFileUpload(
        const QString& fileName,
        qint64 size,
        qint64 chunkSize,
        const QByteArray& md5,
        qint64 ttl,
        AddUploadCallback callback,
        QThread* targetThread = nullptr);

    Handle removeFileDownload(
        const QString& fileName,
        bool deleteData,
        PostCallback callback = nullptr,
        QThread* targetThread = nullptr);

    Handle fileChunkChecksums(
        const QString& fileName,
        GetCallback callback,
        QThread* targetThread = nullptr);

    Handle downloadFileChunk(
        const QString& fileName,
        int index,
        Result<QByteArray>::type callback,
        QThread* targetThread = nullptr);

    Handle downloadFileChunkFromInternet(const QString& fileName,
        const nx::utils::Url &url,
        int chunkIndex,
        int chunkSize,
        Result<QByteArray>::type callback,
        QThread* targetThread = nullptr);

    Handle uploadFileChunk(
        const QString& fileName,
        int index,
        const QByteArray& data,
        PostCallback callback,
        QThread* targetThread = nullptr);

    Handle downloadsStatus(
        GetCallback callback,
        QThread* targetThread = nullptr);

    Handle fileDownloadStatus(
        const QString& fileName,
        GetCallback callback,
        QThread* targetThread = nullptr);

    Handle getTimeOfServersAsync(
        Result<MultiServerTimeData>::type callback,
        QThread* targetThread = nullptr);

    Handle testEventRule(const QnUuid& ruleId, nx::vms::api::EventState toggleState,
        GetCallback callback, QThread* targetThread = nullptr);

    Handle getEvents(QnEventLogRequestData request,
        Result<EventLogData>::type callback,
        QThread* targetThread = nullptr);

    Handle getEvents(const QnEventLogMultiserverRequestData& request,
        Result<EventLogData>::type callback,
        QThread* targetThread = nullptr);

    /**
     * Change user's password on a camera. This method doesn't create new user.
     * Only cameras with capability Qn::SetUserPasswordCapability support it.
     * @param id camera id
     * @param auth user name and new password. User name should exists on camera.
     */
    Handle changeCameraPassword(
        const QnUuid& id,
        const QAuthenticator& auth,
        Result<QnRestResult>::type callback,
        QThread* targetThread = nullptr);

    /**
     * Adds a new wearable camera to this server.
     *
     * @param name                      Name of the camera.
     */
    Handle addWearableCamera(
        const QString& name,
        GetCallback callback,
        QThread* targetThread = nullptr);

    Handle prepareWearableUploads(
        const QnNetworkResourcePtr& camera,
        const QnWearablePrepareData& data,
        GetCallback callback,
        QThread* targetThread = nullptr);

    Handle wearableCameraStatus(
        const QnNetworkResourcePtr& camera,
        GetCallback callback,
        QThread* targetThread = nullptr);

    Handle lockWearableCamera(
        const QnNetworkResourcePtr& camera,
        const QnUserResourcePtr& user,
        qint64 ttl,
        GetCallback callback,
        QThread* targetThread = nullptr);

    Handle extendWearableCameraLock(
        const QnNetworkResourcePtr& camera,
        const QnUserResourcePtr& user,
        const QnUuid& token,
        qint64 ttl,
        GetCallback callback,
        QThread* targetThread = nullptr);

    Handle releaseWearableCameraLock(
        const QnNetworkResourcePtr& camera,
        const QnUuid& token,
        GetCallback callback,
        QThread* targetThread = nullptr);

    /**
     * Request to merge the systems
     */
    Handle mergeSystemAsync(
        const nx::utils::Url& url, const QString& getKey, const QString& postKey,
        bool ownSettings, bool oneServer, bool ignoreIncompatible,
        GetCallback callback, QThread* targetThread = nullptr);

    /**
     * Makes the server consume a media file as a footage for a wearable camera.
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
    Handle consumeWearableCameraFile(
        const QnNetworkResourcePtr& camera,
        const QnUuid& token,
        const QString& uploadId,
        qint64 startTimeMs,
        PostCallback callback,
        QThread* targetThread = nullptr);

    /** Get statistics for server health monitor. */
    Handle getStatistics(GetCallback callback, QThread* targetThread = nullptr);

    Handle lookupDetectedObjects(
        const nx::analytics::storage::Filter& request,
        bool isLocal,
        Result<nx::analytics::storage::LookupResult>::type callback,
        QThread* targetThread = nullptr);

    Handle addCamera(
        const QnManualResourceSearchList& cameras,
        const QString& userName,
        const QString& password,
        GetCallback callback,
        QThread* thread = nullptr);

    Handle searchCameraStart(
        const QString& cameraUrl,
        const QString& userName,
        const QString& password,
        std::optional<int> port,
        GetCallback callback,
        QThread* targetThread = nullptr);

    Handle searchCameraRangeStart(const QString& startAddress,
        const QString& endAddress,
        const QString& userName,
        const QString& password,
        std::optional<int> port,
        GetCallback callback,
        QThread* targetThread = nullptr);

    Handle searchCameraStatus(
        const QnUuid& processUuid,
        GetCallback callback,
        QThread* targetThread = nullptr);

    Handle searchCameraStop(
        const QnUuid& processUuid,
        GetCallback callback,
        QThread* targetThread = nullptr);

    Handle executeAnalyticsAction(
        const AnalyticsAction& action,
        Result<QnJsonRestResult>::type callback,
        QThread* targetThread = nullptr);

    Handle updateActionStart(
        const nx::update::Information& info,
        QThread* targetThread = nullptr);

    /** Get information for a current update. It requests /ec2/updateInformation. */
    Handle getUpdateInfo(
        Result<UpdateInformationData>::type&& callback,
        QThread* targetThread = nullptr);

    /** Get information for installed update. It requests /ec2/updateInformation?version=installed. */
    Handle getInstalledUpdateInfo(
        Result<UpdateInformationData>::type&& callback,
        QThread* targetThread = nullptr);

    /**
     * Asks mediaserver to run check for updates.
     * @param changeset - changeset to be checked. It is usually a build number
     * @param callback
     */
    Handle checkForUpdates(const QString& changeset,
        Result<UpdateInformationData>::type&& callback,
        QThread* targetThread = nullptr);

    Handle updateActionStop(
        std::function<void(Handle, bool)>&& callback,
        QThread* targetThread = nullptr);

    Handle updateActionFinish(bool skipActivePeers,
        std::function<void(Handle, bool)>&& callback,
        QThread* targetThread = nullptr);

    Handle updateActionInstall(const QSet<QnUuid>& participants,
        std::function<void(Handle, bool)>&& callback,
        QThread* targetThread = nullptr);

    Handle getUpdateStatus(
        Result<UpdateStatusAllData>::type callback,
        QThread* targetThread = nullptr);

    Handle getModuleInformation(
        Result<QList<nx::vms::api::ModuleInformation>>::type callback,
        QThread* targetThread = nullptr);

    Handle getEngineAnalyticsSettings(
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        Result<nx::vms::api::analytics::SettingsResponse>::type&& callback,
        QThread* targetThread = nullptr);

    Handle setEngineAnalyticsSettings(
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        const QJsonObject& settings,
        Result<nx::vms::api::analytics::SettingsResponse>::type&& callback,
        QThread* targetThread = nullptr);

    Handle getDeviceAnalyticsSettings(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        Result<nx::vms::api::analytics::SettingsResponse>::type&& callback,
        QThread* targetThread = nullptr);

    Handle setDeviceAnalyticsSettings(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        const QJsonObject& settings,
        Result<nx::vms::api::analytics::SettingsResponse>::type&& callback,
        QThread* targetThread = nullptr);

    Handle debug(
        const QString& action,
        const QString& value,
        PostCallback callback,
        QThread* targetThread = nullptr);

    /**
     * Cancel running request by known requestId. If request is canceled, callback isn't called.
     * If target thread has been used then callback may be called after 'cancelRequest' in case of data already received and queued to a target thread.
     * If QnServerRestConnection is destroyed all running requests are canceled, no callbacks called.
     *
     * Note that this effectively means that YOU ARE NEVER SAFE, even if you've cancelled all your requests
     * in your destructor. Better bulletproof your callbacks with `QnGuardedCallback`.
     */
    void cancelRequest(const Handle& requestId);

    // Get ID of the server we are connected to.
    QnUuid getServerId() const;

private slots:
    void onHttpClientDone(int requestId, nx::network::http::AsyncHttpClientPtr httpClient);

private:
    template <typename ResultType>
    Handle executeRequest(const nx::network::http::ClientPool::Request& request,
        Callback<ResultType> callback,
        QThread* targetThread);

    Handle executeRequest(const nx::network::http::ClientPool::Request& request,
        Result<QByteArray>::type callback,
        QThread* targetThread);

    Handle executeRequest(const nx::network::http::ClientPool::Request& request,
        Callback<EmptyResponseType> callback,
        QThread* targetThread);

    QUrl prepareUrl(const QString& path, const QnRequestParamList& params) const;

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
        nx::network::http::Method::ValueType method,
        const QUrl& url,
        const nx::network::http::StringType& contentType = nx::network::http::StringType(),
        const nx::network::http::StringType& messageBody = nx::network::http::StringType());

    nx::network::http::ClientPool::Request prepareDirectRequest(
        nx::network::http::Method::ValueType method,
        const QUrl& url,
        const nx::network::http::StringType& contentType = nx::network::http::StringType(),
        const nx::network::http::StringType& messageBody = nx::network::http::StringType());

    using HttpCompletionFunc = std::function<void (
        Handle handle,
        SystemError::ErrorCode errorCode,
        int statusCode,
        nx::network::http::StringType contentType,
        nx::network::http::BufferType msgBody,
        const nx::network::http::HttpHeaders& headers)>;

    Handle sendRequest(const nx::network::http::ClientPool::Request& request,
        HttpCompletionFunc callback = HttpCompletionFunc());

    QnMediaServerResourcePtr getServerWithInternetAccess() const;

    void trace(int handle, const QString& message) const;
    std::pair<QString, QString> getRequestCredentials(
        const QnMediaServerResourcePtr& targetServer) const;

private:
    QnUuid m_serverId;
    QMap<Handle, HttpCompletionFunc> m_runningRequests;
    mutable QnMutex m_mutex;
    nx::utils::Url m_directUrl;

    /**
     * Generic requests, for the types, that should not be exposed to common library.
     */
    template<typename CallbackType> Handle executeGet(
        const QString& path,
        const QnRequestParamList& params,
        CallbackType callback,
        QThread* targetThread);

    template <typename ResultType> Handle executePost(
        const QString& path,
        const QnRequestParamList& params,
        const nx::network::http::StringType& contentType,
        const nx::network::http::StringType& messageBody,
        Callback<ResultType> callback,
        QThread* targetThread);

    template <typename ResultType> Handle executePut(
        const QString& path,
        const QnRequestParamList& params,
        const nx::network::http::StringType& contentType,
        const nx::network::http::StringType& messageBody,
        Callback<ResultType> callback,
        QThread* targetThread);

    template <typename ResultType> Handle executeDelete(
        const QString& path,
        const QnRequestParamList& params,
        Callback<ResultType> callback,
        QThread* targetThread);
};

} // namespace rest
