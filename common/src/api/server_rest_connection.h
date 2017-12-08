#pragma once

#include <QtCore/QThread>

#include "server_rest_connection_fwd.h"

#include <nx/network/http/http_types.h>
#include <nx/utils/system_error.h>
#include "utils/common/request_param.h"
#include "nx_ec/data/api_fwd.h"
#include <api/helpers/request_helpers_fwd.h>
#include <nx/network/deprecated/asynchttpclient.h>

#include <rest/server/json_rest_result.h>
#include <nx/utils/safe_direct_connection.h>
#include <api/http_client_pool.h>
#include <nx/vms/event/event_fwd.h>
#include <core/resource/resource_fwd.h>
#include <common/common_module_aware.h>
#include <api/model/time_reply.h>
#include <analytics/detected_objects_storage/analytics_events_storage.h>

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

class ServerConnection:
    public QObject,
    public QnCommonModuleAware,
    public Qn::EnableSafeDirectConnection
{
    Q_OBJECT
public:
    ServerConnection(QnCommonModule* commonModule, const QnUuid& serverId);
    virtual ~ServerConnection();


    template <typename ResultType>
    struct Result { typedef std::function<void (bool, Handle, ResultType)> type; };

    struct EmptyResponseType {};
    typedef Result<EmptyResponseType>::type PostCallback;   // use this type for POST requests without result data

    typedef Result<QnJsonRestResult>::type GetCallback; /**< Default callback type for GET requests without result data. */

    /**
    * Load information about cross-server archive
    * @return value > 0 on success or <= 0 if it isn't started.
    * @param targetThread execute callback in a target thread if specified. It convenient for UI purpose.
    * By default callback is called in IO thread.
    */
    Handle cameraHistoryAsync(const QnChunksRequestData &request, Result<ec2::ApiCameraHistoryDataList>::type callback, QThread* targetThread = 0);

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
        const QnThumbnailRequestData& request,
        Result<QByteArray>::type callback,
        QThread* targetThread = 0);

    Handle twoWayAudioCommand(
        const QString& sourceId,
        const QnUuid& cameraId,
        bool start,
        GetCallback callback,
        QThread* targetThread = nullptr);

    Handle softwareTriggerCommand(const QnUuid& cameraId, const QString& triggerId,
            nx::vms::event::EventState toggleState, GetCallback callback, QThread* targetThread = nullptr);

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
        int size,
        const QByteArray& md5,
        const QUrl& url,
        GetCallback callback,
        QThread* targetThread = nullptr);

    Handle removeFileDownload(
        const QString& fileName,
        bool deleteData,
        GetCallback callback,
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
        GetCallback callback,
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

    Handle testEventRule(const QnUuid& ruleId, nx::vms::event::EventState toggleState,
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

    Handle lookupDetectedObjects(
        const nx::analytics::storage::Filter& request,
        Result<nx::analytics::storage::LookupResult>::type callback,
        QThread* targetThread = nullptr);

    /**
    * Cancel running request by known requestID. If request is canceled, callback isn't called.
    * If target thread has been used then callback may be called after 'cancelRequest' in case of data already received and queued to a target thread.
    * If QnServerRestConnection is destroyed all running requests are canceled, no callbacks called.
    */
    void cancelRequest(const Handle& requestId);

private slots:
    void onHttpClientDone(int requestId, nx_http::AsyncHttpClientPtr httpClient);
private:
    template <typename ResultType> Handle executeGet(
        const QString& path,
        const QnRequestParamList& params,
        Callback<ResultType> callback,
        QThread* targetThread);

    template <typename ResultType> Handle executePost(
        const QString& path,
        const QnRequestParamList& params,
        const nx_http::StringType& contentType,
        const nx_http::StringType& messageBody,
        Callback<ResultType> callback,
        QThread* targetThread);

    template <typename ResultType> Handle executePut(
        const QString& path,
        const QnRequestParamList& params,
        const nx_http::StringType& contentType,
        const nx_http::StringType& messageBody,
        Callback<ResultType> callback,
        QThread* targetThread);

    template <typename ResultType> Handle executeDelete(
        const QString& path,
        const QnRequestParamList& params,
        Callback<ResultType> callback,
        QThread* targetThread);

    template <typename ResultType>
    Handle executeRequest(const nx_http::ClientPool::Request& request,
        Callback<ResultType> callback,
        QThread* targetThread);

    Handle executeRequest(const nx_http::ClientPool::Request& request,
        Callback<QByteArray> callback,
        QThread* targetThread);

    Handle executeRequest(const nx_http::ClientPool::Request& request,
        Callback<EmptyResponseType> callback,
        QThread* targetThread);

    QUrl prepareUrl(const QString& path, const QnRequestParamList& params) const;

    nx_http::ClientPool::Request prepareRequest(
        nx_http::Method::ValueType method,
        const QUrl& url,
        const nx_http::StringType& contentType = nx_http::StringType(),
        const nx_http::StringType& messageBody = nx_http::StringType());

    typedef std::function<void (Handle, SystemError::ErrorCode, int, nx_http::StringType contentType, nx_http::BufferType msgBody)> HttpCompletionFunc;
    Handle sendRequest(const nx_http::ClientPool::Request& request, HttpCompletionFunc callback = HttpCompletionFunc());

    QnMediaServerResourcePtr getServerWithInternetAccess() const;

    void trace(int handle, const QString& message) const;
private:
    QnUuid m_serverId;
    QMap<Handle, HttpCompletionFunc> m_runningRequests;
    mutable QnMutex m_mutex;
};

} // namespace rest
