#pragma once

#include <optional>

#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/utils/thread/mutex.h>

#include <QtCore/QElapsedTimer>
#include <nx/utils/timer_manager.h>

namespace nx {
namespace network {
namespace http {

class ClientPool: public QObject
{
    Q_OBJECT
public:

    struct Request
    {
        Request():
            authType(nx::network::http::AuthType::authBasicAndDigest)
        {
        }

        bool isValid() const
        {
            return !method.isEmpty() && url.isValid();
        }

        Method::ValueType method;
        nx::utils::Url url;
        nx::network::http::HttpHeaders headers;
        nx::network::http::StringType contentType;
        nx::network::http::StringType messageBody;
        nx::network::http::AuthType authType;
        std::optional<std::chrono::milliseconds> timeout{std::nullopt};
    };

    ClientPool(QObject *parent = nullptr);
    virtual ~ClientPool();

    int doGet(
        const nx::utils::Url& url,
        nx::network::http::HttpHeaders headers = nx::network::http::HttpHeaders(),
        std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    int doPost(
        const nx::utils::Url& url,
        const QByteArray& contentType,
        const QByteArray& msgBody,
        nx::network::http::HttpHeaders headers = nx::network::http::HttpHeaders(),
        std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    int sendRequest(const Request& request);

    void terminate(int handle);
    void setPoolSize(int value);

    /** Returns amount of requests which are running or awaiting to be run */
    int size() const;

    void setDefaultTimeouts(
        std::chrono::milliseconds request,
        std::chrono::milliseconds response,
        std::chrono::milliseconds messageBody);

signals:
    void done(int requestId, AsyncHttpClientPtr httpClient);
private slots:
    void at_HttpClientDone(AsyncHttpClientPtr httpClient);
private:
    AsyncHttpClientPtr createHttpConnection();

    struct HttpConnection
    {
        HttpConnection(AsyncHttpClientPtr client = AsyncHttpClientPtr(), int handle = 0):
            client(client),
            handle(handle)
        {
            idleTimeout.restart();
        }

        AsyncHttpClientPtr client;
        QElapsedTimer idleTimeout;
        int handle;
    };
private:
    HttpConnection* getUnusedConnection(const nx::utils::Url &url);
    void sendRequestUnsafe(const Request& request, AsyncHttpClientPtr httpClient);
    void sendNextRequestUnsafe();
    void cleanupDisconnectedUnsafe();
private:
    mutable QnMutex m_mutex;
    typedef std::unique_ptr<HttpConnection> HttpConnectionPtr;
    std::multimap<QString /*endpointWithProtocol*/, HttpConnectionPtr> m_connectionPool;
    std::map<int, Request> m_awaitingRequests;
    //std::map<AsyncHttpClientPtr, RequestInternal> m_requestInProgress;
    int m_maxPoolSize;
    int m_requestId;
    std::chrono::milliseconds m_defaultRequestTimeout{std::chrono::minutes(1)};
    std::chrono::milliseconds m_defaultResponseTimeout{std::chrono::minutes(1)};
    std::chrono::milliseconds m_defaultMessageBodyTimeout{std::chrono::minutes(10)};
};

} // namespace nx
} // namespace network
} // namespace http
