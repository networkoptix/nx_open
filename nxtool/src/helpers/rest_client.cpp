
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
        const rtu::BaseServerInfo &info = *request.target;

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
        /// TODO: remove when Roman fix server
        const QString tmp = (request.path.isEmpty() || request.path[0] != '/' ? request.path
            : request.path.right(request.path.length() - 1) + "?" + request.params.toString());
        result.url = tmp;
        result.serverId = request.target->id;
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
        qDebug() << " --- HTTP error callback " << request.path << " : " << static_cast<int>(errorCode);
        if (errorCode == RequestError::kUnauthorized)
        {
            request.target->discoveredByHttp = true;
            emit m_owner->accessMethodChanged(request.target->id, true);

            if (request.errorCallback)
                request.errorCallback(errorCode);
            return;
        }

        Request tmp(request);

        /// Multicast error replaced by initial http code (except unauthorized)
        const auto errorCallbackToCapture = request.errorCallback;
        tmp.errorCallback = [errorCallbackToCapture, errorCode](RequestError currentErrorCode)   
        {
            if (!errorCallbackToCapture)
                return;

            errorCallbackToCapture(currentErrorCode != RequestError::kUnauthorized 
                ? errorCode : RequestError::kUnauthorized);
        };

        /// Server not accessible by HTTP, but accessible by multicast
        const auto replyCallbackToCapture = request.replyCallback;
        tmp.replyCallback = [this, replyCallbackToCapture](const QByteArray &data)
        {
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

rtu::RestClient::SuccessCallback rtu::RestClient::Impl::makeHttpReplyCallback(const Request &request)
{
    const auto result = [this, request](const QByteArray &data)
    {
        request.target->discoveredByHttp = true;
        emit m_owner->accessMethodChanged(request.target->id, true);
        if (request.replyCallback)
            request.replyCallback(data);
    };

    return result;
}

void rtu::RestClient::Impl::sendHttpGet(const Request &request)
{
    m_httpClient.sendGet(makeHttpUrl(request), makeHttpReplyCallback(request)
        , makeHttpErrorCallback(request, true), request.timeout);
}

void rtu::RestClient::Impl::sendHttpPost(const Request &request
    , const QByteArray &data)
{
    m_httpClient.sendPost(makeHttpUrl(request), data, makeHttpReplyCallback(request)
        , makeHttpErrorCallback(request, false, data), request.timeout);
}

QnMulticast::ResponseCallback rtu::RestClient::Impl::makeCallbackForMulticast(const rtu::RestClient::Request &request)
{
    const auto callback = [this, request](const QUuid& /* requestId */
        , QnMulticast::ErrCode errCode, const QnMulticast::Response& response)
    {
        const rtu::RequestError requestError = 
            convertMulticastClientErrorToGlobal(errCode, response.httpResult);
        
        qDebug() << " --- Multicast callback " << request.path << " : " << static_cast<int>(requestError) << " : " << response.httpResult;

        if ((requestError == RequestError::kSuccess) 
            || (requestError == RequestError::kUnauthorized))
        {
            request.target->discoveredByHttp = false;
            emit m_owner->accessMethodChanged(request.target->id, false);
        }

        if (requestError == rtu::RequestError::kSuccess)
        {
            if (request.path.contains("iflist"))
                qDebug() << "\n" << response.messageBody << "\n";

            if (request.replyCallback)
                request.replyCallback(response.messageBody);

        }
        else if (request.errorCallback)
            request.errorCallback(requestError);
    };
    return callback;
}

void rtu::RestClient::Impl::sendMulticastGet(const Request &request)
{
    m_multicastClient.execRequest(makeMulticastRequest(request, true)
        , makeCallbackForMulticast(request) , request.timeout);
}

void rtu::RestClient::Impl::sendMulticastPost(const Request &request
    , const QByteArray &data)
{
    auto msecRequest = makeMulticastRequest(request, false);
    msecRequest.messageBody = data;
    m_multicastClient.execRequest(msecRequest
        , makeCallbackForMulticast(request) , request.timeout);
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
    qDebug() << "++++ Sending GET [ " << (request.target->discoveredByHttp ? "HTTP" : "MULTICAST") 
        << " ] : " << request.path << " with password " << request.password;

    if (request.target->discoveredByHttp)
        implInstance().sendHttpGet(request);
    else
        implInstance().sendMulticastGet(request);
}

void rtu::RestClient::sendPost(const Request &request
    , const QByteArray &data)
{
    if (request.target->discoveredByHttp)
        implInstance().sendHttpPost(request, data);
    else
        implInstance().sendMulticastPost(request, data);
}
        
///

rtu::RestClient::Request::Request(const rtu::BaseServerInfoPtr &initTarget
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
