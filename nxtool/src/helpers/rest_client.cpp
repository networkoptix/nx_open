
#include "rest_client.h"

#include <helpers/http_client.h>
#include <helpers/multicast/multicast_http_client.h>

namespace
{
    const QString kUserAgent = "NX Tool";

    const QString &adminUserName()
    {
        static QString result("admin");
        return result;
    }

    ///

    const QStringList &defaultAdminPasswords()
    {
        static QStringList result = []() -> QStringList
        {
            QStringList result;
            result.push_back("123");
            result.push_back("admin");
            return result;
        }();

        return result;
    }

    QUrl makeHttpUrl(const rtu::RestClient::Request &request)
    {
        const rtu::BaseServerInfo &info = request.target;

        QUrl url;
        url.setScheme("http");
        url.setHost(info.hostAddress);
        url.setPort(info.port);
        url.setUserName(adminUserName());
        url.setPassword(request.password);
        url.setPath(request.path);
        url.setQuery(request.params);
        return url;
    }

    const QnMulticast::Request makeMulticastRequest(const rtu::RestClient::Request &request
        , bool isGetRequest)
    {
        QnMulticast::Request result;
        result.method = (isGetRequest ? "GET" : "POST");
        result.url = request.path;
        result.serverId = request.target.id;
        result.contentType = "application/json";

        result.auth.setUser(adminUserName());
        result.auth.setPassword(request.password);
        return result;
    }

    rtu::RequestError convertMulticastClientErrorToGlobal(QnMulticast::ErrCode errorCode
        , int httpCode)
    {
        switch(errorCode)
        {
        case QnMulticast::ErrCode::ok:
        {
            /// Checks Http codes
            const bool isSuccessHttp = ((httpCode >= rtu::HttpClient::kHttpSuccessCodeFirst) 
                && (httpCode <= rtu::HttpClient::kHttpSuccessCodeLast));

            if (isSuccessHttp)
                return rtu::RequestError::kSuccess;
            else if (httpCode == rtu::HttpClient::kHttpUnauthorized)
                return rtu::RequestError::kUnauthorized;
            return rtu::RequestError::kUnspecified;
        }
        case QnMulticast::ErrCode::timeout:
            return rtu::RequestError::kRequestTimeout;

        default:
            return rtu::RequestError::kUnspecified;
        }
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
    rtu::HttpClient::ErrorCallback makeHttpErrorCallback(const rtu::RestClient::Request &request
        , bool isGetRequest
        , const QByteArray &data = QByteArray());

private:
    RestClient * const m_owner;
    rtu::HttpClient m_httpClient;
    QnMulticast::HTTPClient m_multicastClient;
};

///

rtu::RestClient::Impl::Impl(RestClient *owner)
    : m_owner(owner)
    , m_httpClient(nullptr)
    , m_multicastClient(kUserAgent)
{
    enum { kDefaultTimeout = 10 * 1000 };   /// move http client timeout initialization here
    m_multicastClient.setDefaultTimeout(kDefaultTimeout);
}

rtu::RestClient::Impl::~Impl()
{
}

rtu::HttpClient::ErrorCallback rtu::RestClient::Impl::makeHttpErrorCallback(const rtu::RestClient::Request &request
    , bool isGetRequest
    , const QByteArray &data)
{
    const auto httpTryErrorCallback = 
        [this, request, isGetRequest, data](RequestError errorCode)
    {
        if (request.path.contains("configure"))
            int i = 0;

        qDebug() << " --- Error callback " << request.path;
        if (errorCode == RequestError::kUnauthorized)
        {
            if (request.errorCallback)
                request.errorCallback(errorCode);
        }

        Request tmp(request);

        /// Multicast error replaced by initial http code (except unauthorized)
        const auto errorCallbackToCapture = request.errorCallback;
        tmp.errorCallback = [errorCallbackToCapture, errorCode](RequestError errorCode)   
        {
            if (!errorCallbackToCapture)
                return;

            errorCallbackToCapture(errorCode != RequestError::kUnauthorized 
                ? errorCode : RequestError::kUnauthorized);
        };

        /// Server not accessible by HTTP, but accessible by multicast
        const auto replyCallbackToCapture = request.replyCallback;
        const auto id = request.target.id;
        tmp.replyCallback = [this, replyCallbackToCapture, id](const QByteArray &data)
        {
            emit m_owner->accessibleOnlyByMulticast(id);
            if (replyCallbackToCapture)
                replyCallbackToCapture(data);
        };

        if (isGetRequest)
            sendMulticastGet(tmp);
        else
            sendMulticastPost(tmp, data);
    };

    return httpTryErrorCallback;
}

void rtu::RestClient::Impl::sendHttpGet(const Request &request)
{
    m_httpClient.sendGet(makeHttpUrl(request), request.replyCallback
        , makeHttpErrorCallback(request, true), request.timeout);
}

void rtu::RestClient::Impl::sendHttpPost(const Request &request
    , const QByteArray &data)
{
    m_httpClient.sendPost(makeHttpUrl(request), data, request.replyCallback
        , makeHttpErrorCallback(request, false, data), request.timeout);
}

QnMulticast::ResponseCallback makeCallback(const rtu::RestClient::Request &request)
{
    const auto callback = [request](const QUuid& /* requestId */
        , QnMulticast::ErrCode errCode, const QnMulticast::Response& response)
    {
        if (request.path.contains("configure"))
            int i = 0;
        if (errCode == QnMulticast::ErrCode::ok)
        {
            if (request.replyCallback)
                request.replyCallback(response.messageBody);
        }
        else if (request.errorCallback)
            request.errorCallback(convertMulticastClientErrorToGlobal(errCode, response.httpResult));
    };
    return callback;
}

void rtu::RestClient::Impl::sendMulticastGet(const Request &request)
{
    if (request.path.contains("configure"))
        int i = 0;
    m_multicastClient.execRequest(makeMulticastRequest(request, true)
        , makeCallback(request) , request.timeout);
}

void rtu::RestClient::Impl::sendMulticastPost(const Request &request
    , const QByteArray &data)
{
    m_multicastClient.execRequest(makeMulticastRequest(request, false)
        , makeCallback(request) , request.timeout);
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
    QObject::disconnect(nullptr, nullptr, nullptr, nullptr);
}

void rtu::RestClient::sendGet(const Request &request)
{
    if (request.target.discoveredByHttp)
        implInstance().sendHttpGet(request);
    else
        implInstance().sendMulticastGet(request);
}

void rtu::RestClient::sendPost(const Request &request
    , const QByteArray &data)
{
    if (request.target.discoveredByHttp)
        implInstance().sendHttpPost(request, data);
    else
        implInstance().sendMulticastPost(request, data);
}
        
///

rtu::RestClient::Request::Request(const rtu::BaseServerInfo &initTarget
    , const QString &initPassword
    , const QString &initPath
    , const QUrlQuery &initParams
    , int initTimeout
    , const rtu::RestClient::SuccessCallback &initReplyCallback
    , const rtu::RestClient::ErrorCallback &initErrorCallback)
    
    : target(initTarget)
    , password(initPassword)
    , path(initPath)
    , params(initParams)
    , timeout(initTimeout)
    , replyCallback(initReplyCallback)
    , errorCallback(initErrorCallback)
{
}

rtu::RestClient::Request::Request(const rtu::ServerInfo &initServerInfo
    , const QString &initPath
    , const QUrlQuery &initParams
    , int initTimeout
    , const rtu::RestClient::SuccessCallback &initReplyCallback
    , const rtu::RestClient::ErrorCallback &initErrorCallback)
    
    : target(initServerInfo.baseInfo())
    , password(initServerInfo.hasExtraInfo() ? initServerInfo.extraInfo().password : QString())
    , path(initPath)
    , params(initParams)
    , timeout(initTimeout)
    , replyCallback(initReplyCallback)
    , errorCallback(initErrorCallback)
{
}
