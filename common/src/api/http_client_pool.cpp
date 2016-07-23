#include "http_client_pool.h"
#include <common/common_module.h>

namespace {
    static const std::chrono::minutes kRequestSendTimeout(1);
    static const std::chrono::minutes kResponseReadTimeout(1);
    static const std::chrono::minutes kMessageBodyReadTimeout(10);
    static const int kDefaultPoolSize = 8;
}

namespace nx_http {

HttpClientPool::HttpClientPool(QObject *parent):
    QObject(),
    m_maxPoolSize(kDefaultPoolSize),
    m_requestId(0)
{
}

HttpClientPool::~HttpClientPool()
{
}

void HttpClientPool::setPoolSize(int value)
{
    m_maxPoolSize = value;
}

HttpClientPool* HttpClientPool::instance()
{
    NX_ASSERT(qnCommon->instance<HttpClientPool>(), Q_FUNC_INFO, "Make sure http client pool exists");
    return qnCommon->instance<HttpClientPool>();
}

int HttpClientPool::doGet(const QUrl& url, nx_http::HttpHeaders headers)
{
    QnMutexLocker lock(&m_mutex);
    int requestId = ++m_requestId;
    RequestInfo request;
    request.method = Method::GET;
    request.url = url;
    request.headers = headers;
    request.handle = requestId;

    m_requestInProgress.push_back(std::move(request));
    sendNextRequestUnsafe();
    return requestId;
}

int HttpClientPool::doPost(
    const QUrl& url,
    nx_http::HttpHeaders headers,
    const QByteArray& contentType,
    const QByteArray& msgBody)
{
    QnMutexLocker lock(&m_mutex);
    int requestId = ++m_requestId;
    RequestInfo request;
    request.method = Method::POST;
    request.url = url;
    request.headers = headers;
    request.handle = requestId;
    request.contentType = contentType;
    request.msgBody = msgBody;

    m_requestInProgress.push_back(std::move(request));
    sendNextRequestUnsafe();
    return requestId;
}

void HttpClientPool::terminate(int handle)
{
    QnMutexLocker lock(&m_mutex);
    for (auto itr = m_requestInProgress.begin(); itr != m_requestInProgress.end(); ++itr)
    {
        auto& request = *itr;
        if (request.handle == handle)
        {
            if (request.httpClient)
                request.httpClient->terminate();
            m_requestInProgress.erase(itr);
            sendNextRequestUnsafe();
            break;
        }
    }
}

void HttpClientPool::sendRequestUnsafe(const RequestInfo& request, AsyncHttpClientPtr httpClient)
{
    httpClient->setAdditionalHeaders(request.headers);
    if (request.method == Method::GET)
        httpClient->doGet(request.url);
    else
        httpClient->doPost(request.url, request.contentType, request.msgBody);
}

void HttpClientPool::sendNextRequestUnsafe()
{
    for (auto& request: m_requestInProgress)
    {
        if (!request.httpClient)
        {
            if (auto httpClient = getHttpClientUnsafe(request.url))
            {
                request.httpClient = httpClient;
                sendRequestUnsafe(request, httpClient);
                break;
            }
        }
    }
}

AsyncHttpClientPtr HttpClientPool::getHttpClientUnsafe(const QUrl& url)
{
    AsyncHttpClientPtr result;

    for (const AsyncHttpClientPtr& httpClient : m_clientPool)
    {
        if (httpClient.use_count() > 1)
            continue; //< object is using now

        result = httpClient; //< any free client
        QUrl clientUrl(httpClient->url());
        if (clientUrl.host() == url.host() && clientUrl.port() == url.port())
        {
            result = httpClient; //< better match. client with the same HostAddress
            break;
        }
    }

    if (!result && m_clientPool.size() < m_maxPoolSize)
    {

        result = nx_http::AsyncHttpClient::create();

        //setting appropriate timeouts
        using namespace std::chrono;
        result->setSendTimeoutMs(
            duration_cast<milliseconds>(kRequestSendTimeout).count());
        result->setResponseReadTimeoutMs(
            duration_cast<milliseconds>(kResponseReadTimeout).count());
        result->setMessageBodyReadTimeoutMs(
            duration_cast<milliseconds>(kMessageBodyReadTimeout).count());

        connect(
            result.get(), &nx_http::AsyncHttpClient::done,
            this, &HttpClientPool::at_HttpClientDone,
            Qt::DirectConnection);


        m_clientPool.emplace_back(result);
    }

    return result;
}

void HttpClientPool::at_HttpClientDone(nx_http::AsyncHttpClientPtr clientPtr)
{
    int requestId = -1;

    {
        QnMutexLocker lock(&m_mutex);
        for (auto itr = m_requestInProgress.begin(); itr != m_requestInProgress.end(); ++itr)
        {
            const auto& request = *itr;
            if (request.httpClient == clientPtr)
            {
                requestId = request.handle;
                m_requestInProgress.erase(itr);
                break;
            }
        }
    }

    if (requestId > 0)
        emit done(requestId, clientPtr);

    QnMutexLocker lock(&m_mutex);
    sendNextRequestUnsafe();
}

} // namespace nx_http
