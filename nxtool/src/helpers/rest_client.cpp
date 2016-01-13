
#include "rest_client.h"

#include <cassert>
#include <QString>

#include <version.h>

#include <helpers/http_client.h>
#include <helpers/multicast/multicast_http_client.h>

namespace
{
    const QString kUserAgent = QString("%1/%2").arg(
        QLatin1String(QN_APPLICATION_DISPLAY_NAME), QLatin1String(QN_APPLICATION_VERSION));

    QUrl makeHttpUrl(const rtu::RestClient::Request &request)
    {
        const rtu::BaseServerInfo &info = *request.target;

        QUrl url;
        url.setScheme("http");
        url.setHost(info.hostAddress);
        url.setPort(info.port);
        url.setUserName(rtu::RestClient::defaultAdminUserName());
        url.setPassword(request.password);
        url.setPath(request.path);
        url.setQuery(request.params);
        return url;
    }

    const QnMulticast::Request makeMulticastRequest(const rtu::RestClient::Request &request
        , bool isGetRequest)
    {
        static const QString kUrlTemplate = "%1?%2";

        QnMulticast::Request result;
        result.method = (isGetRequest ? "GET" : "POST");
        result.url = kUrlTemplate.arg(request.path, request.params.toString());
        result.serverId = request.target->id;
        result.contentType = "application/json";
        result.auth.setUser(rtu::RestClient::defaultAdminUserName());
        result.auth.setPassword(request.password);
        return result;
    }
}

class rtu::RestClient::Impl
{
public:
    Impl(RestClient *owner);

    ~Impl();

    void sendHttpGet(const Request &request);

    void sendHttpPost(const Request &request
        , const QByteArray &data);

    void sendMulticastGet(const Request &request);

    void sendMulticastPost(const Request &request
        , const QByteArray &data);

private:
    static RequestResult convertMulticastClientRequestResult(
        QnMulticast::ErrCode errorCode, int httpCode);

    static RequestResult convertHttpClientRequestResult(
        HttpClient::RequestResult errorCode);

    rtu::RestClient::SuccessCallback makeHttpReplyCallback(const Request &request);

    rtu::HttpClient::ErrorCallback makeHttpErrorCallback(const rtu::RestClient::Request &request
        , bool isGetRequest
        , const QByteArray &data = QByteArray());

    QnMulticast::ResponseCallback makeCallbackForMulticast(const rtu::RestClient::Request &request);

private:
    RestClient * const m_owner;
    rtu::HttpClient m_httpClient;
    QnMulticast::HTTPClient m_multicastClient;
};

///

rtu::RestClient::Impl::Impl(RestClient *owner)
    : m_owner(owner)
    , m_httpClient()
    , m_multicastClient(kUserAgent)
{
    // Do nothing.
}

rtu::RestClient::Impl::~Impl()
{
}

rtu::RestClient::RequestResult rtu::RestClient::Impl::convertMulticastClientRequestResult(
    QnMulticast::ErrCode errorCode,
    int httpCode)
{
    switch(errorCode)
    {
    case QnMulticast::ErrCode::ok:
        {
            /// Checks Http codes
            const bool isSuccessHttp = ((httpCode >= rtu::HttpClient::kHttpSuccessCodeFirst) 
                && (httpCode <= rtu::HttpClient::kHttpSuccessCodeLast));

            if (isSuccessHttp)
                return RequestResult::kSuccess;
            else if (httpCode == rtu::HttpClient::kHttpUnauthorized)
                return RequestResult::kUnauthorized;
            return RequestResult::kUnspecified;
        }
    case QnMulticast::ErrCode::timeout:
        return RequestResult::kRequestTimeout;

    default:
        return RequestResult::kUnspecified;
    }
}

rtu::RestClient::RequestResult rtu::RestClient::Impl::convertHttpClientRequestResult(
    rtu::HttpClient::RequestResult httpResult)
{
    switch (httpResult)
    {
        case HttpClient::RequestResult::kSuccess: return RequestResult::kSuccess;
        case HttpClient::RequestResult::kRequestTimeout: return RequestResult::kRequestTimeout;
        case HttpClient::RequestResult::kUnauthorized: return RequestResult::kUnauthorized;
        case HttpClient::RequestResult::kUnspecified: return RequestResult::kUnspecified;
        default:
            assert(false);
            return RequestResult::kUnspecified;
    }
}

rtu::HttpClient::ErrorCallback rtu::RestClient::Impl::makeHttpErrorCallback(
    const rtu::RestClient::Request &request
    , bool isGetRequest
    , const QByteArray &data)
{
    return [this, request, isGetRequest, data](HttpClient::RequestResult httpResult)
    {
        if (httpResult == HttpClient::RequestResult::kUnauthorized)
        {
            request.target->accessibleByHttp = true;
            emit m_owner->accessMethodChanged(request.target->id, true);

            if (request.errorCallback)
                request.errorCallback(convertHttpClientRequestResult(httpResult));
            return;
        }

        Request multicastRequest(request);

        // Replace Multicast error with initial http code (except unauthorized).
        const auto requestErrorCallback = request.errorCallback;
        multicastRequest.errorCallback =
            [requestErrorCallback, httpResult](RequestResult multicastResult)   
            {
                if (!requestErrorCallback)
                    return;

                requestErrorCallback((multicastResult == RequestResult::kUnauthorized) ?
                    RequestResult::kUnauthorized : convertHttpClientRequestResult(httpResult));
            };

        // Server is not accessible by http, but accessible by multicast.
        const auto replyCallbackToCapture = request.replyCallback;
        multicastRequest.replyCallback =
            [this, replyCallbackToCapture](const QByteArray &data)
            {
                if (replyCallbackToCapture)
                    replyCallbackToCapture(data);
            };

        if (isGetRequest)
            sendMulticastGet(multicastRequest);
        else
            sendMulticastPost(multicastRequest, data);
    };
}

rtu::RestClient::SuccessCallback rtu::RestClient::Impl::makeHttpReplyCallback(const Request &request)
{
    const auto result = [this, request](const QByteArray &data)
    {
        request.target->accessibleByHttp = true;
        emit m_owner->accessMethodChanged(request.target->id, true);
        if (request.replyCallback)
            request.replyCallback(data);
    };

    return result;
}

void rtu::RestClient::Impl::sendHttpGet(const Request &request)
{
    /*if (request.time)*/
    m_httpClient.sendGet(makeHttpUrl(request), request.timeoutMs, makeHttpReplyCallback(request)
        , makeHttpErrorCallback(request, true));
}

void rtu::RestClient::Impl::sendHttpPost(const Request &request
    , const QByteArray &data)
{
    m_httpClient.sendPost(makeHttpUrl(request), request.timeoutMs, data, makeHttpReplyCallback(request)
        , makeHttpErrorCallback(request, false, data));
}

QnMulticast::ResponseCallback rtu::RestClient::Impl::makeCallbackForMulticast(
    const rtu::RestClient::Request &request)
{
    const auto callback = [this, request](const QUuid& /* requestId */
        , QnMulticast::ErrCode errCode, const QnMulticast::Response& response)
    {
        const RequestResult requestResult = 
            convertMulticastClientRequestResult(errCode, response.httpResult);
        
        if ((requestResult == RequestResult::kSuccess) 
            || (requestResult == RequestResult::kUnauthorized))
        {
            request.target->accessibleByHttp = false;
            emit m_owner->accessMethodChanged(request.target->id, false);
        }

        if (requestResult == RequestResult::kSuccess)
        {
            if (request.replyCallback)
                request.replyCallback(response.messageBody);
        }
        else if (request.errorCallback)
            request.errorCallback(requestResult);
    };
    return callback;
}

void rtu::RestClient::Impl::sendMulticastGet(const Request &request)
{
    m_multicastClient.execRequest(makeMulticastRequest(request, true)
        , makeCallbackForMulticast(request), request.timeoutMs);
}

void rtu::RestClient::Impl::sendMulticastPost(const Request &request
    , const QByteArray &data)
{
    auto msecRequest = makeMulticastRequest(request, false);
    msecRequest.messageBody = data;
    m_multicastClient.execRequest(msecRequest
        , makeCallbackForMulticast(request), request.timeoutMs);
}

///

rtu::RestClient *rtu::RestClient::instance()
{
    static RestClient client;
    return &client;
}


rtu::RestClient::Impl& rtu::RestClient::implInstance()
{
    return *(*instance()).m_impl;
}

rtu::RestClient::RestClient()
    : m_impl(new Impl(this))
{
}

rtu::RestClient::~RestClient()
{
}

void rtu::RestClient::sendGet(const Request &request)
{
    if (request.target->accessibleByHttp)
        implInstance().sendHttpGet(request);
    else
        implInstance().sendMulticastGet(request);
}

void rtu::RestClient::sendPost(const Request &request
    , const QByteArray &data)
{
    if (request.target->accessibleByHttp)
        implInstance().sendHttpPost(request, data);
    else
        implInstance().sendMulticastPost(request, data);
}
        
const QString &rtu::RestClient::defaultAdminUserName()
{
    static const QString result = QStringLiteral("admin");
    return result;
}

const QStringList &rtu::RestClient::defaultAdminPasswords()
{
    static auto result = QStringList() 
        << QStringLiteral("admin") << QStringLiteral("123");
    return result;
}

///

rtu::RestClient::Request::Request(const rtu::BaseServerInfoPtr &initTarget
    , const QString &initPassword
    , const QString &initPath
    , const QUrlQuery &initParams
    , int initTimeoutMs
    , const rtu::RestClient::SuccessCallback &initReplyCallback
    , const rtu::RestClient::ErrorCallback &initErrorCallback)
    
    : target(initTarget)
    , password(initPassword)
    , path(initPath)
    , params(initParams)
    , timeoutMs(initTimeoutMs)
    , replyCallback(initReplyCallback)
    , errorCallback(initErrorCallback)
{
}
