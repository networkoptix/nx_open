#pragma once

#include "server_rest_connection_fwd.h"

#include "utils/network/http/httptypes.h"
#include "utils/common/systemerror.h"
#include "utils/common/request_param.h"
#include "nx_ec/data/api_fwd.h"
#include <api/helpers/request_helpers_fwd.h>
#include "utils/network/http/asynchttpclient.h"
#include <rest/server/json_rest_result.h>

/*
* New class for HTTP requests to mediaServer. It should be used instead of deprecated class QnMediaServerConnection.
* It class can be used either for client and server side.
* Class calls callback methods from IO thread. So, caller code should be thread-safe.
* Client MUST NOT make requests in callbacks as it will cause a deadlock.
*/

namespace rest
{
    class ServerConnection: public QObject
    {
        Q_OBJECT
    public:
        ServerConnection(const QnUuid& serverId);
        virtual ~ServerConnection();


        template <typename ResultType>
        struct Result { typedef std::function<void (bool, Handle, ResultType)> type; };


        struct EmptyResponseType {};
        typedef Result<EmptyResponseType>::type PostCallback;   // use this type for POST requests without result data

        typedef Result<QnJsonRestResult>::type GetCallback; /**< Default callback type for GET requests without result data. */

        /*
        * Load information about cross-server archive
        * !Returns value > 0 on success or <= 0 if it isn't started.
        * @param targetThread execute callback in a target thread if specified. It convenient for UI purpose.
        * By default callback is called in IO thread.
        */
        Handle cameraHistoryAsync(const QnChunksRequestData &request, Result<ec2::ApiCameraHistoryDataList>::type callback, QThread* targetThread = 0);


        Handle cameraThumbnailAsync(const QnThumbnailRequestData &request, Result<QByteArray>::type callback, QThread* targetThread = 0);

        Handle twoWayAudioCommand(const QnUuid& cameraId, bool start, GetCallback callback, QThread* targetThread = 0);

        Handle getStatisticsSettingsAsync(Result<QByteArray>::type callback
            , QThread *targetThread = nullptr);

        Handle sendStatisticsAsync(const QnSendStatisticsRequestData &request
            , PostCallback callback
            , QThread *targetThread = nullptr);

        /*
        * Cancel running request by known requestID. If request is canceled, callback isn't called.
        * If target thread has been used then callback may be called after 'cancelRequest' in case of data already received and queued to a target thread.
        * If QnServerRestConnection is destroyed all running requests are canceled, no callbacks called.
        */
        void cancelRequest(const Handle& requestId);

    private:
        enum class HttpMethod
        {
            Unknown,
            Get,
            Post
        };

        struct Request
        {
            Request(): method(HttpMethod::Unknown), authType(nx_http::AsyncHttpClient::authBasicAndDigest) {}
            bool isValid() const { return method != HttpMethod::Unknown && url.isValid(); }

            HttpMethod method;
            QUrl url;
            nx_http::AsyncHttpClient::AuthType authType;
            nx_http::HttpHeaders headers;
            nx_http::StringType contentType;
            nx_http::StringType messageBody;
        };

        template <typename ResultType> Handle executeGet(
            const QString& path,
            const QnRequestParamList& params,
            REST_CALLBACK(ResultType) callback,
            QThread* targetThread);

        template <typename ResultType> Handle executePost(
            const QString& path,
            const QnRequestParamList& params,
            const nx_http::StringType& contentType,
            const nx_http::StringType& messageBody,
            REST_CALLBACK(ResultType) callback,
            QThread* targetThread);

        template <typename ResultType>
        Handle executeRequest(const Request& request, REST_CALLBACK(ResultType) callback, QThread* targetThread);
        Handle executeRequest(const Request& request, REST_CALLBACK(QByteArray) callback, QThread* targetThread);
        Handle executeRequest(const Request& request, REST_CALLBACK(EmptyResponseType) callback, QThread* targetThread);

        QUrl prepareUrl(const QString& path, const QnRequestParamList& params) const;
        Request prepareRequest(HttpMethod method, const QUrl& url, const nx_http::StringType& contentType = nx_http::StringType(), const nx_http::StringType& messageBody = nx_http::StringType());

        typedef std::function<void (Handle, SystemError::ErrorCode, int, nx_http::StringType contentType, nx_http::BufferType msgBody)> HttpCompletionFunc;
        Handle sendRequest(const Request& request, HttpCompletionFunc callback = HttpCompletionFunc());
    private:
        QnUuid m_serverId;
        QMap<Handle, nx_http::AsyncHttpClientPtr> m_runningRequests;
        mutable QnMutex m_mutex;
    };
}
