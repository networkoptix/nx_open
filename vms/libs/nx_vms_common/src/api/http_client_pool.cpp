// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_client_pool.h"

#include <QtCore/QElapsedTimer>

#include <nx/network/http/buffer_source.h>

#include <nx/utils/thread/mutex.h>
#include <nx/network/nx_network_ini.h>

using namespace std::chrono;

namespace nx::network::http {

namespace {

static const int kDefaultPoolSize = 8;
static const int kHttpDisconnectTimeout(60 * 1000);
static const int kInvalidHandle = 0;
static constexpr AsyncClient::Timeouts kDefaultTimeouts{1min, 1min, 1min};

} // namespace

//--------------------------------------------------------------------------------------------------
// ClientPool::Response

void ClientPool::Response::reset()
{
    statusLine = {};
    messageBody = {};
    contentType = nx::String();
}

//--------------------------------------------------------------------------------------------------
// ClientPool::Context

StatusLine ClientPool::Context::getStatusLine() const
{
    NX_MUTEX_LOCKER lock(&mutex);
    return response.statusLine;
}

QThread* ClientPool::Context::targetThread() const
{
    return thread ? *thread : nullptr;
}

void ClientPool::Context::setTargetThread(QThread* targetThread)
{
    if (targetThread)
        thread = targetThread;
    else if (thread)
        thread->clear();
}

bool ClientPool::Context::targetThreadIsDead() const
{
    NX_MUTEX_LOCKER lock(&mutex);
    return thread.has_value() && *thread == nullptr;
}

bool ClientPool::Context::hasResponse() const
{
    NX_MUTEX_LOCKER lock(&mutex);
    return state == State::hasResponse;
}

bool ClientPool::Context::isFinished() const
{
    NX_MUTEX_LOCKER lock(&mutex);
    return state == State::hasResponse || state == State::noResponse;
}

bool ClientPool::Context::isCanceled() const
{
    NX_MUTEX_LOCKER lock(&mutex);
    return state == State::canceled;
}

bool ClientPool::Context::hasSuccessfulResponse() const
{
    return hasResponse()
        && systemError == SystemError::noError
        && nx::network::http::StatusCode::isSuccessCode(response.statusLine.statusCode);
}

void ClientPool::Context::setCanceled()
{
    NX_MUTEX_LOCKER lock(&mutex);
    state = State::canceled;
}

nx::utils::Url ClientPool::Context::getUrl() const
{
    NX_MUTEX_LOCKER lock(&mutex);
    return request.url;
}

std::chrono::milliseconds ClientPool::Context::getTimeElapsed() const
{
    NX_MUTEX_LOCKER lock(&mutex);
    return std::chrono::duration_cast<std::chrono::milliseconds>(stampReceived - stampSent);
}

void ClientPool::Context::sendRequest(AsyncClient* httpClient)
{
    {
        NX_MUTEX_LOCKER lock(&mutex);

        httpClient->setAdditionalHeaders(request.headers);
        if (request.credentials)
            httpClient->setCredentials(*request.credentials);
        else
            httpClient->setCredentials({});
        httpClient->setAuthType(request.authType);
        if (timeouts)
            httpClient->setTimeouts(*timeouts);

        stampSent = Clock::now();
        state = Context::State::waitingResponse;
    }

    // All of these methods can potentially issue immediate callback.
    if (request.method == Method::get)
    {
        httpClient->doGet(request.url);
    }
    else if (request.method == Method::post)
    {
        auto messageBody = std::make_unique<BufferSource>(
            request.contentType,
            std::move(request.messageBody));
        httpClient->setRequestBody(std::move(messageBody));
        httpClient->doPost(request.url);
    }
    else if (request.method == Method::put)
    {
        auto messageBody = std::make_unique<BufferSource>(
            request.contentType,
            std::move(request.messageBody));
        httpClient->setRequestBody(std::move(messageBody));
        httpClient->doPut(request.url);
    }
    else if (request.method == Method::patch)
    {
        auto messageBody = std::make_unique<BufferSource>(
            request.contentType,
            std::move(request.messageBody));
        httpClient->setRequestBody(std::move(messageBody));
        httpClient->doPatch(request.url);
    }
    else if (request.method == Method::delete_)
    {
        httpClient->doDelete(request.url);
    }
    else
    {
        NX_ASSERT(false, "Method %1 is not supported", request.method);
    }
}

void ClientPool::Context::readHttpResponse(AsyncClient* httpClient)
{
    NX_MUTEX_LOCKER lock(&mutex);
    stampReceived = Clock::now();
    response.reset();

    systemError = SystemError::noError;

    if (httpClient->response())
    {
        response.statusLine = httpClient->response()->statusLine;
        response.headers = httpClient->response()->headers;
    }
    else
    {
        response.statusLine = nx::network::http::StatusLine();
        state = Context::State::noResponse;
    }

    if (httpClient->failed())
    {
        systemError = SystemError::connectionReset;
    }
    else
    {
        response.contentType = httpClient->contentType();
        response.messageBody = httpClient->fetchMessageBodyBuffer().takeByteArray();
        state = Context::State::hasResponse;
    }
}

//--------------------------------------------------------------------------------------------------
// ClientPool::HttpConnection

/**
 * Contains http connection and data for active request.
 * HttpConnection instance is reused after request is complete.
 */
struct HttpConnection
{
    HttpConnection(
        std::unique_ptr<AsyncClient> client = nullptr,
        QSharedPointer<ClientPool::Context> context = {})
        :
        client(std::move(client)),
        context(context)
    {
        idleTimeout.restart();
    }

    ~HttpConnection()
    {
        if (client)
            client->pleaseStopSync();
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

    std::unique_ptr<AsyncClient> client;
    QElapsedTimer idleTimeout;
    QSharedPointer<ClientPool::Context> context;
    QnUuid lastAdapterFuncId;
};

//--------------------------------------------------------------------------------------------------
// ClientPool::Private

struct ClientPool::Private
{
    AsyncClient::Timeouts timeouts = kDefaultTimeouts;
    int maxPoolSize = kDefaultPoolSize;
    int requestId = kInvalidHandle;

    mutable nx::Mutex mutex;

    std::multimap<QString /*endpointWithProtocol*/, std::unique_ptr<HttpConnection>> connectionPool;

    /** A sequence of requests to be passed to connection pool to process. */
    std::map<int, ContextPtr> awaitingRequests;

    bool stopped = false;

    std::unique_ptr<AsyncClient> createHttpConnection()
    {
        auto result = std::make_unique<AsyncClient>(ssl::kDefaultCertificateCheck);
        result->setTimeouts(timeouts);
        return result;
    }

    void cleanupDisconnectedUnsafe()
    {
        for (auto itr = connectionPool.begin(); itr != connectionPool.end();)
        {
            HttpConnection* connection = itr->second.get();
            if (connection->getHandle() == kInvalidHandle
                && connection->idleTimeout.hasExpired(kHttpDisconnectTimeout))
            {
                connection->client->post(
                    [connection = std::move(itr->second)]() mutable
                    {
                        connection.reset();
                    });
                itr = connectionPool.erase(itr);
            }
            else
            {
                ++itr;
            }
        }
    }

    HttpConnection* getUnusedConnection(const nx::utils::Url& url)
    {
        cleanupDisconnectedUnsafe();

        HttpConnection* result = nullptr;
        const auto requestAddress = AsyncClient::endpointWithProtocol(url);

        auto range = connectionPool.equal_range(requestAddress.c_str());
        int count = 0;
        if (range.first != connectionPool.end() && range.first->first == requestAddress)
        {
            for (auto itr = range.first; itr != range.second; ++itr)
            {
                ++count;
                HttpConnection* connection = itr->second.get();
                if (!result && connection->getHandle() == kInvalidHandle)
                {
                    result = itr->second.get();
                    connection->idleTimeout.restart();
                }
            }
        }

        if (!result && count < maxPoolSize)
        {
            NX_VERBOSE(this, "getUnusedConnection(%1) - creating new connection");
            result = new HttpConnection();
            result->client = createHttpConnection();

            connectionPool.emplace(requestAddress.c_str(), result);
        }

        return result;
    }

    void setAdapterFuncToConnection(
        HttpConnection* connection,
        const QnUuid& adapterFuncId,
        const ssl::AdapterFunc& adapterFunc)
    {
        if (adapterFuncId == connection->lastAdapterFuncId)
            return;
        if (!connection->lastAdapterFuncId.isNull())
            connection->client = createHttpConnection();
        if (adapterFunc)
        {
            if (NX_ASSERT(!adapterFuncId.isNull()))
                connection->lastAdapterFuncId = adapterFuncId;
            else
                connection->lastAdapterFuncId = QnUuid::createUuid();
            connection->client->setAdapterFunc(adapterFunc);
        }
    }

    void sendNextRequestUnsafe()
    {
        // It can be called from both main thread and AioThread
        for (auto itr = awaitingRequests.begin(); itr != awaitingRequests.end();)
        {
            ContextPtr context = itr->second;
            auto url = context->getUrl();
            if (auto connection = getUnusedConnection(url))
            {
                setAdapterFuncToConnection(connection,
                    context->adapterFuncId, context->adapterFunc);
                connection->context = context;
                NX_ASSERT(!context->isCanceled());

                auto completionHandler =
                    [this, handle = context->handle]
                    {
                        onHttpClientDone(handle);
                    };
                connection->client->setOnDone(completionHandler);
                context->sendRequest(connection->client.get());
                itr = awaitingRequests.erase(itr);
            }
            else
            {
                ++itr;
            }
        }
    }

    void onHttpClientDone(int handle)
    {
        QSharedPointer<Context> context;
        // We are most probably inside AioThread.
        {
            NX_MUTEX_LOCKER lock(&mutex);
            for (auto itr = connectionPool.begin(); itr != connectionPool.end(); ++itr)
            {
                HttpConnection* connection = itr->second.get();
                if (connection->getHandle() == handle)
                {
                    context = std::move(connection->context);
                    // HttpConnection is considered free (connection->getHandle() returns 0) after
                    // taking away context from it. We must complete all the actions with
                    // AsyncHttpClient here. Context copies all necessary data from AsyncHttpClient
                    // to itself. There are no references to AsyncHttpClient after that.
                    if (context && !context->isCanceled())
                        context->readHttpResponse(connection->client.get());
                    // HttpConnection will be reused by another thread when we leave this mutex.
                    break;
                }
            }
        }

        if (context)
        {
            if (!context->targetThreadIsDead() && context->completionFunc)
                context->completionFunc(context);
            context.reset();
        }

        {
            NX_MUTEX_LOCKER lock(&mutex);
            sendNextRequestUnsafe();
            cleanupDisconnectedUnsafe();
        }
    }

};

//--------------------------------------------------------------------------------------------------
// ClientPool

ClientPool::ClientPool(QObject *parent):
    QObject(parent),
    d(new Private)
{
    SocketGlobals::instance().allocationAnalyzer().recordObjectCreation(this);
}

ClientPool::~ClientPool()
{
    stop(/*invokeCallbacks*/ false);
    SocketGlobals::instance().allocationAnalyzer().recordObjectDestruction(this);
}

void ClientPool::setPoolSize(int value)
{
    d->maxPoolSize = value;
}

void ClientPool::stop(bool invokeCallbacks)
{
    std::vector<ContextPtr> requests;

    decltype(d->connectionPool) dataCopy;
    {
        NX_MUTEX_LOCKER lock(&d->mutex);
        d->stopped = true;

        requests.reserve(dataCopy.size() + d->awaitingRequests.size());
        std::swap(dataCopy, d->connectionPool);

        for (const auto&[_, connection]: dataCopy)
            requests.push_back(connection->context);
        for (const auto&[_, context]: d->awaitingRequests)
            requests.push_back(context);

        d->awaitingRequests.clear(); //< We must not create new connections.
    }

    for (const auto& [_, connection]: dataCopy)
        connection->client->pleaseStopSync();

    if (invokeCallbacks)
    {
        for (const auto& context: requests)
        {
            if (context && !context->targetThreadIsDead() && context->completionFunc)
                context->completionFunc(context);
        }
    }
}

int ClientPool::sendRequest(ContextPtr context)
{
    int requestId = 0;
    {
        NX_MUTEX_LOCKER lock(&d->mutex);

        if (d->stopped)
            return 0;

        requestId = ++d->requestId;
        // Access to d->awaitingRequests is blocked, so no other thread can touch context as well.
        context->handle = requestId;
        d->awaitingRequests.emplace(requestId, context);
        d->sendNextRequestUnsafe();
    }

    return requestId;
}

bool ClientPool::terminate(int handle)
{
    QSharedPointer<Context> context;
    std::unique_ptr<AsyncClient> client;
    {
        NX_MUTEX_LOCKER lock(&d->mutex);
        if (d->awaitingRequests.count(handle))
        {
            context = d->awaitingRequests[handle];
            d->awaitingRequests.erase(handle);
        }
        else
        {
            for (auto itr = d->connectionPool.begin(); itr != d->connectionPool.end(); ++itr)
            {
                HttpConnection* connection = itr->second.get();
                if (connection->getHandle() == handle)
                {
                    client.swap(connection->client);
                    if (connection->context)
                        context.swap(connection->context);
                    else
                        NX_VERBOSE(this, "terminate(%1) - terminating orphaned connection", handle);

                    connection->client = d->createHttpConnection();
                    connection->lastAdapterFuncId = QnUuid();
                    break;
                }
            }
        }
    } // end of d->mutex lock

    // pleaseStopAsync should be safe to be called unguarded. Calling it inside d->mutex will
    // cause a deadlock, since it happen just when ClientPool::onHttpClientDone is called
    // from exactly the same client.
    if (client)
        client->pleaseStopSync();

    if (!context)
        return false;

    auto url = context->getUrl();
    NX_VERBOSE(this, "terminate(%1) - terminating request to \"%2\"", handle, url);
    context->setCanceled();
    if (!context->targetThreadIsDead() && context->completionFunc)
        context->completionFunc(context);
    context.reset();

    {
        NX_MUTEX_LOCKER lock(&d->mutex);
        d->sendNextRequestUnsafe();
        d->cleanupDisconnectedUnsafe();
    }
    return true;
}

int ClientPool::size() const
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    int result = d->awaitingRequests.size();
    for (auto itr = d->connectionPool.begin(); itr != d->connectionPool.end(); ++itr)
    {
        const HttpConnection* connection = itr->second.get();
        if (connection->getHandle() != kInvalidHandle)
            ++result;
    }
    return result;
}

AsyncClient::Timeouts ClientPool::defaultTimeouts() const
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    return d->timeouts;
}

void ClientPool::setDefaultTimeouts(AsyncClient::Timeouts timeouts)
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    NX_ASSERT(d->connectionPool.empty());
    d->timeouts = timeouts;
}

} // namespace nx::network::http
