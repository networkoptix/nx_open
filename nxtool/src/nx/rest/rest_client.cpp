
#include "rest_client.h"

#include <cassert>
#include <QString>

#include <nx/rest/http_client.h>
#include <nx/rest/multicast_http_client.h>

namespace nx {
namespace rest {

class RestClient::Impl
{
public:
    Impl(RestClient *owner, const QString& userAgent, const QString& userName);

    ~Impl();

    void sendHttpGet(const Request &request);

    void sendHttpPost(const Request &request
        , const QByteArray &data);

    void sendMulticastGet(const Request &request);

    void sendMulticastPost(const Request &request
        , const QByteArray &data);

private:
    static ResultCode convertMulticastClientRequestResult(
        multicastHttp::ResultCode errorCode, int httpCode);

    static ResultCode convertHttpClientRequestResult(
        HttpClient::RequestResult errorCode);

    QUrl makeHttpUrl(const RestClient::Request &request);
    
    const multicastHttp::Request makeMulticastRequest(
        const RestClient::Request &request, bool isGetRequest);

    HttpClient::ReplyCallback makeHttpReplyCallback(const Request &request);

    HttpClient::ErrorCallback makeHttpErrorCallback(const RestClient::Request &request
        , bool isGetRequest
        , const QByteArray &data = QByteArray());

    multicastHttp::ResponseCallback makeCallbackForMulticast(const RestClient::Request &request);

private:
    RestClient * const m_owner;
    QString m_userName;
    HttpClient m_httpClient;
    MulticastHttpClient m_multicastClient;
};

///

RestClient::Impl::Impl(
    RestClient *owner, const QString& userAgent, const QString& userName)
    : m_owner(owner)
    , m_userName(userName)
    , m_httpClient()
    , m_multicastClient(userAgent)
{
}

RestClient::Impl::~Impl()
{
}

QUrl RestClient::Impl::makeHttpUrl(const RestClient::Request &request)
{
    QUrl url;
    url.setScheme("http");
    url.setHost(request.hostAddress);
    url.setPort(request.port);
    url.setUserName(m_userName);
    url.setPassword(request.password);
    url.setPath(request.path);
    url.setQuery(request.params);
    return url;
}

const multicastHttp::Request RestClient::Impl::makeMulticastRequest(
    const RestClient::Request &request, bool isGetRequest)
{
    static const QString kUrlTemplate = "%1?%2";

    multicastHttp::Request result;
    result.method = (isGetRequest ? "GET" : "POST");
    result.url = kUrlTemplate.arg(request.path, request.params.toString());
    result.serverId = request.serverId;
    result.contentType = "application/json";
    result.auth.setUser(m_userName);
    result.auth.setPassword(request.password);
    return result;
}

RestClient::ResultCode RestClient::Impl::convertMulticastClientRequestResult(
    multicastHttp::ResultCode errorCode,
    int httpCode)
{
    switch(errorCode)
    {
    case multicastHttp::ResultCode::ok:
        {
            /// Checks Http codes
            const bool isSuccessHttp = ((httpCode >= HttpClient::kHttpSuccessCodeFirst)
                && (httpCode <= HttpClient::kHttpSuccessCodeLast));

            if (isSuccessHttp)
                return ResultCode::kSuccess;
            else if (httpCode == HttpClient::kHttpUnauthorized)
                return ResultCode::kUnauthorized;
            return ResultCode::kUnspecified;
        }
    case multicastHttp::ResultCode::timeout:
        return ResultCode::kRequestTimeout;

    default:
        return ResultCode::kUnspecified;
    }
}

RestClient::ResultCode RestClient::Impl::convertHttpClientRequestResult(
    HttpClient::RequestResult httpResult)
{
    switch (httpResult)
    {
        case HttpClient::RequestResult::kSuccess: return ResultCode::kSuccess;
        case HttpClient::RequestResult::kRequestTimeout: return ResultCode::kRequestTimeout;
        case HttpClient::RequestResult::kUnauthorized: return ResultCode::kUnauthorized;
        case HttpClient::RequestResult::kUnspecified: return ResultCode::kUnspecified;
        default:
            Q_ASSERT(false);
            return ResultCode::kUnspecified;
    }
}

HttpClient::ErrorCallback RestClient::Impl::makeHttpErrorCallback(
    const RestClient::Request &request
    , bool isGetRequest
    , const QByteArray &data)
{
    return
        [this, request, isGetRequest, data](HttpClient::RequestResult httpResult)
        {
            if (httpResult == HttpClient::RequestResult::kUnauthorized)
            {
                if (request.errorCallback)
                {
                    request.errorCallback(
                        convertHttpClientRequestResult(httpResult),
                        NewHttpAccessMethod::kTcp);
                }
                return;
            }

            Request multicastRequest(request);

            // Replace Multicast error with initial http code (except unauthorized).
            const auto requestErrorCallback = request.errorCallback;
            multicastRequest.errorCallback =
                [requestErrorCallback, httpResult](
                    ResultCode multicastResult, NewHttpAccessMethod newHttpAccessMethod)
                {
                    if (!requestErrorCallback)
                        return;

                    requestErrorCallback((multicastResult == ResultCode::kUnauthorized) ?
                        ResultCode::kUnauthorized : convertHttpClientRequestResult(httpResult),
                        newHttpAccessMethod);
                };

            // Server is not accessible by http, but accessible by multicast.
            const auto requestReplyCallback = request.successCallback;
            multicastRequest.successCallback =
                [this, requestReplyCallback](
                    const QByteArray &data, NewHttpAccessMethod newHttpAccessMethod)
                {
                    if (requestReplyCallback)
                        requestReplyCallback(data, newHttpAccessMethod);
                };

            if (isGetRequest)
                sendMulticastGet(multicastRequest);
            else
                sendMulticastPost(multicastRequest, data);
        };
}

HttpClient::ReplyCallback RestClient::Impl::makeHttpReplyCallback(const Request &request)
{
    const auto result = [this, request](const QByteArray &data)
    {
        if (request.successCallback)
            request.successCallback(data, NewHttpAccessMethod::kTcp);
    };

    return result;
}

void RestClient::Impl::sendHttpGet(const Request &request)
{
    /*if (request.time)*/
    m_httpClient.sendGet(makeHttpUrl(request), request.timeoutMs,
        makeHttpReplyCallback(request),
        makeHttpErrorCallback(request, true));
}

void RestClient::Impl::sendHttpPost(const Request &request
    , const QByteArray &data)
{
    m_httpClient.sendPost(makeHttpUrl(request), request.timeoutMs, data,
        makeHttpReplyCallback(request),
        makeHttpErrorCallback(request, false, data));
}

multicastHttp::ResponseCallback RestClient::Impl::makeCallbackForMulticast(
    const RestClient::Request &request)
{
    const auto callback = [this, request](const QUuid& /* requestId */
        , multicastHttp::ResultCode errCode, const multicastHttp::Response& response)
    {
        const ResultCode requestResult =
            convertMulticastClientRequestResult(errCode, response.httpResult);

        NewHttpAccessMethod newHttpAccessMethod = NewHttpAccessMethod::kUnchanged;

        if ((requestResult == ResultCode::kSuccess)
            || (requestResult == ResultCode::kUnauthorized))
        {
            newHttpAccessMethod = NewHttpAccessMethod::kUdp;
        }

        if (requestResult == ResultCode::kSuccess)
        {
            if (request.successCallback)
                request.successCallback(response.messageBody, newHttpAccessMethod);
        }
        else
        {
            if (request.errorCallback)
                request.errorCallback(requestResult, newHttpAccessMethod);
        }
    };
    return callback;
}

void RestClient::Impl::sendMulticastGet(const Request &request)
{
    m_multicastClient.execRequest(makeMulticastRequest(request, true)
        , makeCallbackForMulticast(request), request.timeoutMs);
}

void RestClient::Impl::sendMulticastPost(const Request &request
    , const QByteArray &data)
{
    auto msecRequest = makeMulticastRequest(request, false);
    msecRequest.messageBody = data;
    m_multicastClient.execRequest(msecRequest
        , makeCallbackForMulticast(request), request.timeoutMs);
}

///

RestClient::RestClient(const QString& userAgent, const QString& userName)
    : m_impl(new Impl(this, userAgent, userName))
{
}

RestClient::~RestClient()
{
}

void RestClient::sendGet(const Request &request)
{
    switch (request.httpAccessMethod)
    {
        case HttpAccessMethod::kTcp:
            m_impl->sendHttpGet(request);
            break;
        case HttpAccessMethod::kUdp:
            m_impl->sendMulticastGet(request);
            break;
        default:
            Q_ASSERT(false);
    }
}

void RestClient::sendPost(const Request &request
    , const QByteArray &data)
{
    switch (request.httpAccessMethod)
    {
        case HttpAccessMethod::kTcp:
            m_impl->sendHttpPost(request, data);
            break;
        case HttpAccessMethod::kUdp:
            m_impl->sendMulticastPost(request, data);
            break;
        default:
            Q_ASSERT(false);
    }
}

///

RestClient::Request::Request(QUuid initServerId
    , const QString &initHostAddress
    , int initPort
    , HttpAccessMethod initHttpAccessMethod
    , const QString &initPassword
    , const QString &initPath
    , const QUrlQuery &initParams
    , qint64 initTimeoutMs
    , const RestClient::SuccessCallback &initReplyCallback
    , const RestClient::ErrorCallback &initErrorCallback)

    : serverId(initServerId)
    , hostAddress(initHostAddress)
    , port(initPort)
    , httpAccessMethod(initHttpAccessMethod)
    , password(initPassword)
    , path(initPath)
    , params(initParams)
    , timeoutMs(initTimeoutMs)
    , successCallback(initReplyCallback)
    , errorCallback(initErrorCallback)
{
}

} // namespace nx
} // namespace rest
