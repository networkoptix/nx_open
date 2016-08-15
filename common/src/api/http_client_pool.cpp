#include "http_client_pool.h"
#include <common/common_module.h>

namespace {
    static const std::chrono::minutes kRequestSendTimeout(1);
    static const std::chrono::minutes kResponseReadTimeout(1);
    static const std::chrono::minutes kMessageBodyReadTimeout(10);
    static const int kDefaultPoolSize = 8;
    static const int kHttpDisconnectTimeout(60 * 1000);

    SocketAddress toSocketAddress(const QUrl& url)
    {
        return SocketAddress(url.host(), url.port(80));
    }
}

namespace nx_http {

static ClientPool* staticInstance;

ClientPool::ClientPool(QObject *parent):
    QObject(),
    m_maxPoolSize(kDefaultPoolSize),
    m_requestId(0)
{
    staticInstance = this;
}

ClientPool::~ClientPool()
{
    std::multimap<SocketAddress, HttpConnectionPtr> dataCopy;
    {
        QnMutexLocker lock(&m_mutex);
        std::swap(dataCopy, m_connectionPool);
    }
    for (auto itr = dataCopy.begin(); itr != dataCopy.end(); ++itr)
        itr->second->client->terminate();
    staticInstance = nullptr;
}

void ClientPool::setPoolSize(int value)
{
    m_maxPoolSize = value;
}

ClientPool* ClientPool::instance()
{
    NX_ASSERT(staticInstance, Q_FUNC_INFO, "Make sure http client pool exists");
    return staticInstance;
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
    const QByteArray& contentType,
    const QByteArray& msgBody,
    nx_http::HttpHeaders headers)
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
    QnMutexLocker lock(&m_mutex);
    int requestId = ++m_requestId;
    m_awaitingRequests.emplace(requestId, request);
    sendNextRequestUnsafe();
    return requestId;
}

void ClientPool::terminate(int handle)
{
    QnMutexLocker lock(&m_mutex);
    m_awaitingRequests.erase(handle);
    for (auto itr = m_connectionPool.begin(); itr != m_connectionPool.end(); ++itr)
    {
        HttpConnection* connection = itr->second.get();
        if (connection->handle == handle)
        {
            connection->client->terminate();
            connection->handle = 0;
            sendNextRequestUnsafe();
            break;
        }
    }
}

void ClientPool::sendRequestUnsafe(const Request& request, AsyncHttpClientPtr httpClient)
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
    for (auto itr = m_awaitingRequests.begin(); itr != m_awaitingRequests.end();)
    {
        const Request& request = itr->second;
        if (auto connection = getUnusedConnection(request.url))
        {
            connection->handle = itr->first;
            sendRequestUnsafe(request, connection->client);
            itr = m_awaitingRequests.erase(itr);
        }
        else {
            ++itr;
        }
    }
}

void ClientPool::cleanupDisconnectedUnsafe()
{
    for (auto itr = m_connectionPool.begin(); itr != m_connectionPool.end();)
    {
        HttpConnection* connection = itr->second.get();
        if (!connection->handle &&
            connection->idleTimeout.hasExpired(kHttpDisconnectTimeout))
        {
            connection->client->post(
                [connection = std::move(itr->second)]() mutable
                {
                    connection.reset();
                });
            itr = m_connectionPool.erase(itr);
        }
        else
        {
            ++itr;
        }
    }
}

ClientPool::HttpConnection* ClientPool::getUnusedConnection(const QUrl& url)
{
    cleanupDisconnectedUnsafe();

    HttpConnection* result = nullptr;
    QUrl clientUrl;
    SocketAddress requestAddress = toSocketAddress(url);

    auto range = m_connectionPool.equal_range(requestAddress);
    int count = 0;
    if (range.first != m_connectionPool.end() && range.first->first == requestAddress)
    {
        for (auto itr = range.first; itr != range.second; ++itr)
        {
            ++count;
            HttpConnection* connection = itr->second.get();
            if (!result && !connection->handle)
            {
                result = itr->second.get();
                connection->idleTimeout.restart();
            }
        }
    }

    if (!result && count < m_maxPoolSize)
    {

        result = new HttpConnection();
        result->client = nx_http::AsyncHttpClient::create();

        //setting appropriate timeouts
        using namespace std::chrono;
        result->client->setSendTimeoutMs(
            duration_cast<milliseconds>(kRequestSendTimeout).count());
        result->client->setResponseReadTimeoutMs(
            duration_cast<milliseconds>(kResponseReadTimeout).count());
        result->client->setMessageBodyReadTimeoutMs(
            duration_cast<milliseconds>(kMessageBodyReadTimeout).count());

        connect(
            result->client.get(), &nx_http::AsyncHttpClient::done,
            this, &ClientPool::at_HttpClientDone,
            Qt::DirectConnection);

        m_connectionPool.emplace(requestAddress, std::move(HttpConnectionPtr(result)));
    }

    return result;
}

void ClientPool::at_HttpClientDone(nx_http::AsyncHttpClientPtr clientPtr)
{
    int requestId = 0;

    {
        QnMutexLocker lock(&m_mutex);
        for (auto itr = m_connectionPool.begin(); itr != m_connectionPool.end(); ++itr)
        {
            HttpConnection* connection = itr->second.get();
            if (connection->client == clientPtr)
            {
                requestId = connection->handle;
                connection->handle = 0;
                break;
            }
        }
    }

    if (requestId > 0)
        emit done(requestId, clientPtr);

    QnMutexLocker lock(&m_mutex);
    sendNextRequestUnsafe();
    cleanupDisconnectedUnsafe();
}

} // namespace nx_http
