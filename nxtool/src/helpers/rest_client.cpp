
#include "rest_client.h"

#include <helpers/http_client.h>

namespace
{
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
}

class rtu::RestClient::Impl
{
public:
    Impl();

    ~Impl();

    void sendHttpGet(const Request &request);

    void sendHttpPost(const Request &request
        , const QByteArray &data);

private:
    rtu::HttpClient m_httpClient;
};

///

rtu::RestClient::Impl::Impl()
    : m_httpClient(nullptr)
{
}

rtu::RestClient::Impl::~Impl()
{
}

/// TODO: #ynikitenkov Temporary, until string error reason be removed
namespace   
{
    rtu::HttpClient::ErrorCallback makeOldCallback(const rtu::RestClient::Request::ErrorCallback &error)
    {
        return [error](const QString &/*errorReason*/, int code)
        {
            if (error)
                error(code);
        };
    }
}

void rtu::RestClient::Impl::sendHttpGet(const Request &request)
{
    m_httpClient.sendGet(makeHttpUrl(request), request.replyCallback
        , makeOldCallback(request.errorCallback), request.timeout);
}

void rtu::RestClient::Impl::sendHttpPost(const Request &request
    , const QByteArray &data)
{
    m_httpClient.sendPost(makeHttpUrl(request), data
        , request.replyCallback, makeOldCallback(request.errorCallback), request.timeout);
}
        
///

rtu::RestClient& rtu::RestClient::instance()
{
    static RestClient client;
    return client;
}

rtu::RestClient::RestClient()
    : m_impl(new Impl())
{
}

rtu::RestClient::~RestClient()
{
}

void rtu::RestClient::sendGet(const Request &request)
{
    m_impl->sendHttpGet(request);
}

void rtu::RestClient::sendPost(const Request &request
    , const QByteArray &data)
{
    m_impl->sendHttpPost(request, data);
}
        
