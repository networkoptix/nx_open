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

#include <nx/api/updates2/updates2_status_data.h>
#include <nx/api/updates2/updates2_action_data.h>

/**
 * New class for HTTP requests to mediaServer. It should be used instead of deprecated class QnMediaServerConnection.
 * This class can be used either for client and server side.
 * Class calls callback methods from IO thread. So, caller code should be thread-safe.
 * Client MUST NOT make requests in callbacks as it will cause a deadlock.
 */

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

class ServerConnection:
    public QObject,
    public QnCommonModuleAware,
    public Qn::EnableSafeDirectConnection,
    public ServerConnectionBase
{
    Q_OBJECT

public:
    ServerConnection(QnCommonModule* commonModule, const QnUuid& serverId);
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

    using UpdateStatus = RestResultWithData<nx::api::Updates2StatusData>;

    Handle getUpdateStatus(Result<UpdateStatus>::type callback, QThread* targetThread = nullptr);

    Handle sendUpdateCommand(const nx::api::Updates2ActionData& request,
        Result<UpdateStatus>::type callback, QThread* targetThread = nullptr);

    using UpdateStatusAll = RestResultWithData<std::vector<nx::api::Updates2StatusData>>;

    Handle getUpdateStatusAll(Result<UpdateStatusAll>::type callback, QThread* targetThread = nullptr);

    Handle sendUpdateCommandAll(const nx::api::Updates2ActionData& request,
        Result<UpdateStatusAll>::type callback, QThread* targetThread = nullptr);

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

    Handle getStatisticsSettingsAsync(Result<QByteArray>::type callback
        , QThread *targetThread = nullptr);

    Handle sendStatisticsAsync(const QnSendStatisticsRequestData &request
        , PostCallback callback
        , QThread *targetThread = nullptr);

    /**
        * Detach the system from cloud. Admin password will be changed to provided (if provided).
        * @param resetAdminPassword Change administrator password to the provided one.
        */
    Handle detachSystemFromCloud(const QString& resetAdminPassword,
        Result<QnRestResult>::type callback, QThread *targetThread = nullptr);

    /**
        * Save the credentials returned by cloud to the database.
        */
    Handle saveCloudSystemCredentials(
        const QString &cloudSystemId,
        const QString &cloudAuthKey,
        const QString &cloudAccountName,
        Result<QnRestResult>::type callback,
        QThread *targetThread = nullptr);

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
     * Creates a new file upload that you can upload chunks into via `uploadFileChunk` function.
     *
     * A quick note on TTL. Countdown normally starts once you've uploaded the whole file, but
     * since clients are unreliable and might stop an upload prematurely, the server actually
     * restarts the countdown after each mutating command.
     *
     * @param fileName                  Unique file name that will be used in other calls.
     * @param size                      Size of the file, in bytes.
     * @param chunkSize                 Size of a single chunk as will be used in succeeding
     *                                  `uploadFileChunk` calls.
     * @param md5                       MD5 hash of the file, as text (32-character hex string).
     * @param ttl                       TTL for the upload, in milliseconds. Pass 0 for infinity.
     */
    Handle addFileUpload(
        const QString& fileName,
        qint64 size,
        qint64 chunkSize,
        const QByteArray& md5,
        qint64 ttl,
        PostCallback callback,
        QThread* targetThread = nullptr);

    Handle validateFileInformation(
        const QString& url,
        int expectedSize,
        GetCallback callback,
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
        QThread *targetThread = nullptr);

    Handle testEventRule(const QnUuid& ruleId, nx::vms::api::EventState toggleState,
        GetCallback callback, QThread* targetThread = nullptr);

    Handle getEvents(QnEventLogRequestData request,
        Result<EventLogData>::type callback,
        QThread *targetThread = nullptr);

    Handle getEvents(const QnEventLogMultiserverRequestData& request,
        Result<EventLogData>::type callback,
        QThread *targetThread = nullptr);

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
        QThread *targetThread = nullptr);

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
        const QString& startAddress,
        const QString& endAddress,
        const QString& userName,
        const QString& password,
        int port,
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

    /**
     * Cancel running request by known requestID. If request is canceled, callback isn't called.
     * If target thread has been used then callback may be called after 'cancelRequest' in case of data already received and queued to a target thread.
     * If QnServerRestConnection is destroyed all running requests are canceled, no callbacks called.
     *
     * Note that this effectively means that YOU ARE NEVER SAFE, even if you've cancelled all your requests
     * in your destructor. Better bulletproof your callbacks with `QnGuardedCallback`.
     */
    void cancelRequest(const Handle& requestId);


private slots:
    void onHttpClientDone(int requestId, nx::network::http::AsyncHttpClientPtr httpClient);

private:
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

    nx::network::http::ClientPool::Request prepareRequest(
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
private:
    QnUuid m_serverId;
    QMap<Handle, HttpCompletionFunc> m_runningRequests;
    mutable QnMutex m_mutex;
};

} // namespace rest
