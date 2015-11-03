#pragma once

#include <functional>
#include "utils/network/http/httptypes.h"
#include "utils/common/systemerror.h"
#include "utils/common/request_param.h"
#include "nx_ec/data/api_fwd.h"
#include <api/helpers/request_helpers_fwd.h>


/*
* New class for HTTP requests to mediaServer. It should be used instead of deprecated class QnMediaServerConnection.
* It class can be used either for client and server side.
* Class calls callback methods from IO thread. So, caller code should be thread-safe.
*/

namespace nx_http {
    class AsyncHttpClientPtr;
};

class QnServerRestConnection: public QObject
{
    Q_OBJECT
public:
    QnServerRestConnection(const QnUuid& serverId);
    virtual ~QnServerRestConnection();

    // use this type for POST requests without result data
    struct DummyContentType {};

    template <typename ResultType>
    struct Result { typedef std::function<void (bool, ResultType)> type; };

    /*
    * !Returns requestID or null guid if it isn't running
    */
    QnUuid cameraHistoryAsync(const QnChunksRequestData &request, Result<ec2::ApiCameraHistoryDataList>::type callback);

    /*
    * Cancel running request by known requestID. If request is canceled, callback isn't called.
    * If QnServerRestConnection is destroyed all running requests are canceled, no callbacks called.
    */
    void cancelRequest(const QnUuid& requestId);
private:
    enum class HttpMethod {
        Unknown,
        Get,
        Post
    };

    struct Request 
    {
        Request(): method(HttpMethod::Unknown) {}
        bool isValid() const { return method != HttpMethod::Unknown && url.isValid(); }

        HttpMethod method;
        QUrl url;
        nx_http::HttpHeaders headers;
        nx_http::StringType contentType;
        nx_http::StringType messageBody;
    };

    template <typename ResultType> QnUuid executeGet(const QString& path, const QnRequestParamList& params, std::function<void (bool, ResultType)> callback);
    template <typename ResultType> QnUuid executePost(
        const QString& path,
        const QnRequestParamList& params,
        const nx_http::StringType& contentType,
        const nx_http::StringType& messageBody,
        std::function<void (bool, ResultType)> callback);
    template <typename ResultType> QnUuid executeRequest(const Request& request, std::function<void (bool, ResultType)> callback);
    QnUuid executeRequest(const Request& request, std::function<void (bool, DummyContentType)> callback);

    QUrl prepareUrl(const QString& path, const QnRequestParamList& params) const;
    Request prepareRequest(HttpMethod method, const QUrl& url, const nx_http::StringType& contentType = nx_http::StringType(), const nx_http::StringType& messageBody = nx_http::StringType());

    typedef std::function<void (SystemError::ErrorCode, int, nx_http::StringType contentType, nx_http::BufferType msgBody)> HttpCompletionFunc;
    QnUuid sendRequest(const Request& request, HttpCompletionFunc callback);
private:
    QnUuid m_serverId;
    QMap<QnUuid, nx_http::AsyncHttpClientPtr> m_runningRequests;
    mutable QnMutex m_mutex;
};

typedef QSharedPointer<QnServerRestConnection> QnServerRestConnectionPtr;
