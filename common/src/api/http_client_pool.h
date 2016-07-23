#pragma once
#include <nx/network/http/asynchttpclient.h>
#include <nx/utils/thread/mutex.h>

namespace nx_http {

class HttpClientPool : public QObject
{
    Q_OBJECT
public:
    HttpClientPool(QObject *parent = nullptr);
    virtual ~HttpClientPool();
    static HttpClientPool* instance();

    int doGet(const QUrl& url, nx_http::HttpHeaders headers);

    int doPost(
        const QUrl& url,
        nx_http::HttpHeaders headers,
        const QByteArray& contentType,
        const QByteArray& msgBody);

    void terminate(int handle);
    void setPoolSize(int value);
signals:
    void done(int requestId, AsyncHttpClientPtr httpClient);
private slots:
    void at_HttpClientDone(AsyncHttpClientPtr httpClient);
private:
    struct RequestInfo
    {
        RequestInfo() : handle(0) {}

        QUrl url;
        HttpHeaders headers;
        QByteArray contentType;
        QByteArray msgBody;
        Method::ValueType method;
        nx_http::AsyncHttpClientPtr httpClient;
        int handle;
    };
private:
    AsyncHttpClientPtr getHttpClientUnsafe(const QUrl& url);
    void sendRequestUnsafe(const RequestInfo& request, AsyncHttpClientPtr httpClient);
    void sendNextRequestUnsafe();
private:
    QnMutex m_mutex;
    std::vector<AsyncHttpClientPtr> m_clientPool;
    std::vector<RequestInfo> m_requestInProgress;
    int m_maxPoolSize;
    int m_requestId;
};

} // nx_http
