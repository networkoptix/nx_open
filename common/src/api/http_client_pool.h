#pragma once
#include <nx/network/http/asynchttpclient.h>
#include <nx/utils/thread/mutex.h>

#include <QElapsedTimer>
#include <nx/utils/timer_manager.h>

namespace nx_http {

class ClientPool : public QObject
{
    Q_OBJECT
public:

    struct Request
    {
        Request():
            authType(nx_http::AsyncHttpClient::authBasicAndDigest)
        {
        }

        bool isValid() const
        {
            return !method.isEmpty() && url.isValid();
        }

        Method::ValueType method;
        QUrl url;
        nx_http::HttpHeaders headers;
        nx_http::StringType contentType;
        nx_http::StringType messageBody;
        nx_http::AsyncHttpClient::AuthType authType;
    };

    ClientPool(QObject *parent = nullptr);
    virtual ~ClientPool();
    static ClientPool* instance();

    int doGet(const QUrl& url, nx_http::HttpHeaders headers);

    int doPost(
        const QUrl& url,
        nx_http::HttpHeaders headers,
        const QByteArray& contentType,
        const QByteArray& msgBody);

    int sendRequest(const Request& request);

    void terminate(int handle);
    void setPoolSize(int value);
signals:
    void done(int requestId, AsyncHttpClientPtr httpClient);
private slots:
    void at_HttpClientDone(AsyncHttpClientPtr httpClient);
private:

    /*
    struct RequestInternal: public Request
    {
        RequestInternal(const Request& r = Request()):
            Request(r),
            handle(0)
        {
        }

        //nx_http::AsyncHttpClientPtr httpClient;
        int handle;
    };
    */

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
    HttpConnection* getUnusedConnection(const QUrl& url);
    void sendRequestUnsafe(const Request& request, AsyncHttpClientPtr httpClient);
    void sendNextRequestUnsafe();
    void cleanupDisconnectedUnsafe();
private:
    QnMutex m_mutex;
    typedef std::unique_ptr<HttpConnection> HttpConnectionPtr;
    std::multimap<SocketAddress, HttpConnectionPtr> m_connectionPool;
    std::map<int, Request> m_awaitingRequests;
    //std::map<AsyncHttpClientPtr, RequestInternal> m_requestInProgress;
    int m_maxPoolSize;
    int m_requestId;
};

} // nx_http
