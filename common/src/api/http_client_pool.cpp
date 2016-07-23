#include "http_client_pool.h"
#include <common/common_module.h>

namespace {
    static const std::chrono::minutes kRequestSendTimeout(1);
    static const std::chrono::minutes kResponseReadTimeout(1);
    static const std::chrono::minutes kMessageBodyReadTimeout(10);
    static const int kDefaultPoolSize = 8;
}

namespace nx_http {

ClientPool::ClientPool(QObject *parent):
    QObject(),
    m_maxPoolSize(kDefaultPoolSize),
    m_requestId(0)
{
}

ClientPool::~ClientPool()
{
    std::vector<AsyncHttpClientPtr> dataCopy;
    {
        QnMutexLocker lock(&m_mutex);
        m_requestInProgress.clear();
        std::swap(dataCopy, m_clientPool);
    }
    for (auto value: dataCopy)
        value->terminate();
}

void ClientPool::setPoolSize(int value)
{
    m_maxPoolSize = value;
}

ClientPool* ClientPool::instance()
{
    NX_ASSERT(qnCommon->instance<ClientPool>(), Q_FUNC_INFO, "Make sure http client pool exists");
    return qnCommon->instance<ClientPool>();
}

int ClientPool::doGet(const QUrl& url, nx_http::HttpHeaders headers)
{
    Request request;
    request.method = Method::GET;
    request.url = url;
    request.headers = headers;
    return sendRequest(request);
}

int ClientPool::doPost(
    const QUrl& url,
    nx_http::HttpHeaders headers,
    const QByteArray& contentType,
    const QByteArray& msgBody)
{
    Request request;
    request.method = Method::POST;
    request.url = url;
    request.headers = headers;
    request.contentType = contentType;
    request.messageBody = msgBody;
    return sendRequest(request);
}

int ClientPool::sendRequest(const Request& request)
{
    RequestInternal requestInternal(request);

    QnMutexLocker lock(&m_mutex);
    int requestId = ++m_requestId;
    requestInternal.handle = requestId;
    m_requestInProgress.push_back(std::move(requestInternal));
    sendNextRequestUnsafe();
    return requestId;
}

void ClientPool::terminate(int handle)
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

void ClientPool::sendRequestUnsafe(const RequestInternal& request, AsyncHttpClientPtr httpClient)
{
    httpClient->setAdditionalHeaders(request.headers);
    httpClient->setAuthType(request.authType);
    if (request.method == Method::GET)
        httpClient->doGet(request.url);
    else
        httpClient->doPost(request.url, request.contentType, request.messageBody);
}

void ClientPool::sendNextRequestUnsafe()
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

AsyncHttpClientPtr ClientPool::getHttpClientUnsafe(const QUrl& url)
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
            this, &ClientPool::at_HttpClientDone,
            Qt::DirectConnection);


        m_clientPool.emplace_back(result);
    }

    return result;
}

void ClientPool::at_HttpClientDone(nx_http::AsyncHttpClientPtr clientPtr)
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
