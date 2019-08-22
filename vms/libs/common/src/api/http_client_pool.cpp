#include "http_client_pool.h"

namespace {
    static const int kDefaultPoolSize = 8;
    static const int kHttpDisconnectTimeout(60 * 1000);

    unsigned int httpTimeoutMs(std::chrono::milliseconds value)
    {
        return (unsigned int) value.count();
    }
}

namespace nx {
namespace network {
namespace http {

void ClientPool::Response::reset()
{
    systemError = SystemError::noError;
    statusLine = {};
    messageBody = {};
    contentType = {};
}

StatusCode::Value ClientPool::Context::getStatusCode() const
{
    NX_MUTEX_LOCKER lock(&mutex);
    return static_cast<StatusCode::Value>(response.statusLine.statusCode);
}

bool ClientPool::Context::hasResponse() const
{
    NX_MUTEX_LOCKER lock(&mutex);
    return state == State::hasResponse;
}

nx::utils::Url ClientPool::Context::getUrl() const
{
    NX_MUTEX_LOCKER lock(&mutex);
    return request.url;
}

struct ClientPool::HttpConnection
{
    HttpConnection(
        AsyncHttpClientPtr client = {},
        QSharedPointer<Context> context = {})
    :
        client(client),
        context(context)
    {
        idleTimeout.restart();
    }

    int getHandle() const
    {
        return context ? context->handle : 0;
    }

    /** Check if there is active request. */
    bool isActive() const
    {
        return context != nullptr;
    }

    AsyncHttpClientPtr client;
    QElapsedTimer idleTimeout;
    QSharedPointer<Context> context;
};

ClientPool::ClientPool(QObject *parent):
    QObject(parent),
    m_maxPoolSize(kDefaultPoolSize),
    m_requestId(0)
{
}

ClientPool::~ClientPool()
{
    decltype(m_connectionPool) dataCopy;
    {
        QnMutexLocker lock(&m_mutex);
        std::swap(dataCopy, m_connectionPool);
    }
    for (auto itr = dataCopy.begin(); itr != dataCopy.end(); ++itr)
        itr->second->client->pleaseStopSync();
}

void ClientPool::setPoolSize(int value)
{
    m_maxPoolSize = value;
}

int ClientPool::doGet(
    const nx::utils::Url& url,
    nx::network::http::HttpHeaders headers,
    std::optional<std::chrono::milliseconds> timeout)
{
    ContextPtr context(new Context());
    context->request.method = Method::get;
    context->request.url = url;
    context->request.headers = headers;
    context->request.timeout = timeout;
    return sendRequest(std::move(context));
}

int ClientPool::doPost(
    const nx::utils::Url& url,
    const QByteArray& contentType,
    const QByteArray& msgBody,
    nx::network::http::HttpHeaders headers,
    std::optional<std::chrono::milliseconds> timeout)
{
    ContextPtr context(new Context());
    context->request.method = Method::post;
    context->request.url = url;
    context->request.headers = headers;
    context->request.contentType = contentType;
    context->request.messageBody = msgBody;
    context->request.timeout = timeout;
    return sendRequest(std::move(context));
}

/*
int ClientPool::sendRequest(Request&& request)
{
    QnMutexLocker lock(&m_mutex);
    int requestId = ++m_requestId;
    auto context = QSharedPointer<Context>(new Context());
    context->request = std::move(request);
    context->handle = requestId;

    m_awaitingRequests.emplace(requestId, context);
    sendNextRequestUnsafe();
    return requestId;
}*/

int ClientPool::sendRequest(ContextPtr context)
{
    int requestId = 0;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        requestId = ++m_requestId;
        // Access to m_awaitingRequests is blocked, so no other thread can touch context as well.
        context->handle = requestId;
        m_awaitingRequests.emplace(requestId, context);
        sendNextRequestUnsafe();
    }

    return requestId;
}

void ClientPool::terminate(int handle)
{
    QnMutexLocker lock(&m_mutex);
    m_awaitingRequests.erase(handle);
    for (auto itr = m_connectionPool.begin(); itr != m_connectionPool.end(); ++itr)
    {
        HttpConnection* connection = itr->second.get();
        if (connection->getHandle() == handle)
        {
            AsyncHttpClientPtr client = connection->client;

            connection->client = createHttpConnection();
            connection->context.reset();
            sendNextRequestUnsafe();
            lock.unlock();
            client->pleaseStopSync();
            break;
        }
    }
}

ClientPool::RequestStats ClientPool::getRequestStats() const
{
    RequestStats stats;
    QnMutexLocker lock(&m_mutex);
    int total = m_awaitingRequests.size();
    stats.queued = total;
    for (auto itr = m_connectionPool.begin(); itr != m_connectionPool.end(); ++itr)
    {
        const HttpConnection* connection = itr->second.get();
        if (connection->getHandle())
            ++total;
    }
    stats.connections = m_connectionPool.size();
    stats.sent = total - stats.queued;
    return stats;
}

int ClientPool::size() const
{
    QnMutexLocker lock(&m_mutex);
    int result = m_awaitingRequests.size();
    for (auto itr = m_connectionPool.begin(); itr != m_connectionPool.end(); ++itr)
    {
        const HttpConnection* connection = itr->second.get();
        if (connection->getHandle())
            ++result;
    }
    return result;
}


void ClientPool::setDefaultTimeouts(
    std::chrono::milliseconds request,
    std::chrono::milliseconds response,
    std::chrono::milliseconds messageBody)
{
    QnMutexLocker lock(&m_mutex);
    NX_ASSERT(m_connectionPool.empty());
    m_defaultRequestTimeout = request;
    m_defaultResponseTimeout = response;
    m_defaultMessageBodyTimeout = messageBody;
}

void ClientPool::sendRequestUnsafe(ContextPtr context, AsyncHttpClientPtr httpClient)
{
    NX_MUTEX_LOCKER lock(&context->mutex);
    const Request& request = context->request;

    if (request.timeout)
    {
        httpClient->setResponseReadTimeoutMs(httpTimeoutMs(*request.timeout));
        httpClient->setMessageBodyReadTimeoutMs(httpTimeoutMs(*request.timeout));
    }
    else
    {
        httpClient->setResponseReadTimeoutMs(httpTimeoutMs(m_defaultResponseTimeout));
        httpClient->setMessageBodyReadTimeoutMs(httpTimeoutMs(m_defaultMessageBodyTimeout));
    }

    httpClient->setAdditionalHeaders(request.headers);
    httpClient->setAuthType(request.authType);

    context->stampSent = Clock::now();
    context->state = Context::State::waitingResponse;
    // All of these methods can issue immediate callback.
    if (request.method == Method::get)
        httpClient->doGet(request.url);
    else if (request.method == Method::put)
        httpClient->doPut(request.url, request.contentType, request.messageBody);
    else if (request.method == Method::post)
        httpClient->doPost(request.url, request.contentType, request.messageBody);
    else if (request.method == Method::delete_)
        httpClient->doDelete(request.url);
    else
        NX_ASSERT(false);
}

void ClientPool::sendNextRequestUnsafe()
{
    // It can be called from both main thread and AioThread
    for (auto itr = m_awaitingRequests.begin(); itr != m_awaitingRequests.end();)
    {
        ContextPtr context = itr->second;
        auto url = context->getUrl();
        if (auto connection = getUnusedConnection(url))
        {
            connection->context = context;
            sendRequestUnsafe(context, connection->client);
            itr = m_awaitingRequests.erase(itr);
        }
        else
        {
            ++itr;
        }
    }
}

void ClientPool::cleanupDisconnectedUnsafe()
{
    for (auto itr = m_connectionPool.begin(); itr != m_connectionPool.end();)
    {
        HttpConnection* connection = itr->second.get();
        if (!connection->getHandle() &&
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

ClientPool::HttpConnection* ClientPool::getUnusedConnection(const nx::utils::Url& url)
{
    cleanupDisconnectedUnsafe();

    HttpConnection* result = nullptr;
    const auto requestAddress = AsyncHttpClient::endpointWithProtocol(url);

    auto range = m_connectionPool.equal_range(requestAddress);
    int count = 0;
    if (range.first != m_connectionPool.end() && range.first->first == requestAddress)
    {
        for (auto itr = range.first; itr != range.second; ++itr)
        {
            ++count;
            HttpConnection* connection = itr->second.get();
            if (!result && !connection->getHandle())
            {
                result = itr->second.get();
                connection->idleTimeout.restart();
            }
        }
    }

    if (!result && count < m_maxPoolSize)
    {
        result = new HttpConnection();
        result->client = createHttpConnection();

        m_connectionPool.emplace(requestAddress, HttpConnectionPtr(result));
    }

    return result;
}

AsyncHttpClientPtr ClientPool::createHttpConnection()
{
    AsyncHttpClientPtr result = nx::network::http::AsyncHttpClient::create();

    //setting appropriate timeouts
    using namespace std::chrono;
    result->setSendTimeoutMs(httpTimeoutMs(m_defaultRequestTimeout));
    result->setResponseReadTimeoutMs(httpTimeoutMs(m_defaultResponseTimeout));
    result->setMessageBodyReadTimeoutMs(httpTimeoutMs(m_defaultMessageBodyTimeout));

    connect(
        result.get(), &nx::network::http::AsyncHttpClient::done,
        this, &ClientPool::at_HttpClientDone,
        Qt::DirectConnection);

    return result;
}

void ClientPool::readHttpResponse(Context& context, AsyncHttpClientPtr httpClient)
{
    NX_MUTEX_LOCKER lock(&context.mutex);
    context.stampReceived = Clock::now();
    Response& response = context.response;
    response.reset();

    response.systemError = SystemError::noError;

    if (httpClient->response())
    {
        response.statusLine = httpClient->response()->statusLine;
        response.headers = httpClient->response()->headers;
    }
    else
    {
        response.statusLine = nx::network::http::StatusLine();
        context.state = Context::State::noResponse;
    }

    if (httpClient->failed())
    {
        response.systemError = SystemError::connectionReset;
    }
    else
    {
        response.contentType = httpClient->contentType();
        response.messageBody = httpClient->fetchMessageBodyBuffer();
        context.state = Context::State::hasResponse;
    }
}

void ClientPool::at_HttpClientDone(nx::network::http::AsyncHttpClientPtr clientPtr)
{
    QSharedPointer<Context> context;
    // We are most probably inside AioThread.
    {
        QnMutexLocker lock(&m_mutex);
        for (auto itr = m_connectionPool.begin(); itr != m_connectionPool.end(); ++itr)
        {
            HttpConnection* connection = itr->second.get();
            if (connection->client == clientPtr)
            {
                //requestId = connection->request->handle;
                context = std::move(connection->context);
                break;
            }
        }
    }

    // We have complete request. We should take Context and clean up current connection
    if (context)
    {
        readHttpResponse(*context, clientPtr);
        emit requestIsDone(std::move(context));
    }

    {
        QnMutexLocker lock(&m_mutex);
        sendNextRequestUnsafe();
        cleanupDisconnectedUnsafe();
    }
}

} // namespace nx
} // namespace network
} // namespace http
