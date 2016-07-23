#pragma once
#include <nx/network/http/asynchttpclient.h>
#include <nx/utils/thread/mutex.h>

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

    struct RequestInternal: public Request
    {
        RequestInternal(const Request& r = Request()):
            Request(r),
            handle(0)
        {
        }

        nx_http::AsyncHttpClientPtr httpClient;
        int handle;
    };
private:
    AsyncHttpClientPtr getHttpClientUnsafe(const QUrl& url);
    void sendRequestUnsafe(const RequestInternal& request, AsyncHttpClientPtr httpClient);
    void sendNextRequestUnsafe();
private:
    QnMutex m_mutex;
    std::vector<AsyncHttpClientPtr> m_clientPool;
    std::vector<RequestInternal> m_requestInProgress;
    int m_maxPoolSize;
    int m_requestId;
};

} // nx_http
