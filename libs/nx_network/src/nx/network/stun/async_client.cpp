// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "async_client.h"

#include <nx/network/socket_global.h>
#include <nx/network/url/url_parse_helper.h>

#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/string.h>

#include "stun_types.h"

namespace nx::network::stun {

AsyncClient::AsyncClient(Settings timeouts):
    m_settings(timeouts),
    m_reconnectTimer(std::make_unique<nx::network::RetryTimer>(m_settings.reconnectPolicy))
{
    SocketGlobals::instance().allocationAnalyzer().recordObjectCreation(this);
    ++SocketGlobals::instance().debugCounters().stunClientConnectionCount;

    bindToAioThread(getAioThread());
}

AsyncClient::AsyncClient(
    std::unique_ptr<AbstractStreamSocket> tcpConnection,
    Settings timeouts)
    :
    AsyncClient(timeouts)
{
    if (tcpConnection)
    {
        m_endpoint = tcpConnection->getForeignAddress();
        bindToAioThread(tcpConnection->getAioThread());
        initializeMessagePipeline(std::move(tcpConnection));
    }
}

AsyncClient::~AsyncClient()
{
    --SocketGlobals::instance().debugCounters().stunClientConnectionCount;
    SocketGlobals::instance().allocationAnalyzer().recordObjectDestruction(this);
}

void AsyncClient::bindToAioThread(network::aio::AbstractAioThread* aioThread)
{
    network::aio::BasicPollable::bindToAioThread(aioThread);

    m_reconnectTimer->bindToAioThread(aioThread);
    if (m_baseConnection)
        m_baseConnection->bindToAioThread(aioThread);
    if (m_connectingSocket)
        m_connectingSocket->bindToAioThread(aioThread);
}

void AsyncClient::connect(
    const nx::utils::Url& url,
    ConnectHandler completionHandler)
{
    if (url.scheme() != nx::network::stun::kUrlSchemeName &&
        url.scheme() != nx::network::stun::kSecureUrlSchemeName)
    {
        return post(
            [completionHandler = std::move(completionHandler)]()
            {
                completionHandler(SystemError::invalidData);
            });
    }

    NX_VERBOSE(this, "Connecting to URL %1", url);

    NX_MUTEX_LOCKER lock(&m_mutex);
    m_endpoint = nx::network::url::getEndpoint(url);
    m_useSsl = url.scheme() == nx::network::stun::kSecureUrlSchemeName;

    post(
        [this, completionHandler = std::move(completionHandler)]() mutable
        {
            NX_MUTEX_LOCKER lock(&m_mutex);

            if (m_state == State::connected)
            {
                lock.unlock();
                if (completionHandler)
                    completionHandler(SystemError::noError);
                return;
            }

            NX_ASSERT(!m_connectCompletionHandler);
            m_connectCompletionHandler = std::move(completionHandler);
            openConnectionImpl(&lock);
        });
}

void AsyncClient::setIndicationHandler(
    int method, IndicationHandler handler, void* client)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_indicationHandlers[method] = std::make_pair(client, std::move(handler));
}

void AsyncClient::addOnReconnectedHandler(
    ReconnectHandler handler, void* client)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_reconnectHandlers.emplace(client, std::move(handler));
}

void AsyncClient::setOnConnectionClosedHandler(
    OnConnectionClosedHandler onConnectionClosedHandler)
{
    m_onConnectionClosedHandler.swap(onConnectionClosedHandler);
}

void AsyncClient::sendRequest(
    Message request, RequestHandler handler, void* client)
{
    NX_VERBOSE(this, "Queuing request %1 (id %2)", request.header, client);

    NX_MUTEX_LOCKER lock(&m_mutex);
    m_requestQueue.push_back(std::make_pair(
        std::move(request), std::make_pair(client, std::move(handler))));

    switch (m_state)
    {
        case State::disconnected:
            return openConnectionImpl(&lock);

        case State::connecting:
            // dispatchRequestsInQueue will be called in onConnectionComplete
            return;

        case State::connected:
            dispatchRequestsInQueue(&lock);
            return;

        default:
            NX_ASSERT(false, nx::format("m_state has invalid value: %1")
                .arg(static_cast<int>(m_state)));
            return;
    };
}

bool AsyncClient::addConnectionTimer(
    std::chrono::milliseconds period, TimerHandler handler, void* client)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_state != State::connected)
    {
        NX_DEBUG(this, nx::format("Ignore timer from client(%1), state is %2")
            .args(client, static_cast<int>(m_state)));

        return false;
    }

    const auto timer = m_connectionTimers.emplace(
        client, std::make_unique<network::aio::Timer>(getAioThread())).first;

    startTimer(timer, period, std::move(handler));
    return true;
}

SocketAddress AsyncClient::localAddress() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_baseConnection
        ? m_baseConnection->socket()->getLocalAddress()
        : SocketAddress();
}

SocketAddress AsyncClient::remoteAddress() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_resolvedEndpoint)
        return *m_resolvedEndpoint;

    return SocketAddress();
}

void AsyncClient::closeConnection(SystemError::ErrorCode errorCode)
{
    dispatch([this, errorCode]() { closeConnection(errorCode, nullptr); });
}

template<typename Container>
void removeByClient(Container* container, void* client)
{
    // std::remove_if does not work for std::map and multimap O_o
    for (auto it = container->begin(); it != container->end(); )
    {
        if (it->second.first == client)
            it = container->erase(it);
        else
            ++it;
    }
}

void AsyncClient::cancelHandlers(void* client, nx::utils::MoveOnlyFunc<void()> handler)
{
    NX_ASSERT(client);
    dispatch(
        [this, client, handler = std::move(handler)]()
        {
            cancelHandlersSync(client);
            handler();
        });
}

void AsyncClient::cancelHandlersSync(void* client)
{
    if (isInSelfAioThread())
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        removeByClient(&m_requestQueue, client);
        removeByClient(&m_indicationHandlers, client);
        m_reconnectHandlers.erase(client);
        removeByClient(&m_requestsInProgress, client);
        NX_VERBOSE(this, nx::format("Cancel requests from %1").arg(client));
    }
    else
    {
        std::promise<void> done;
        cancelHandlers(client, [&done]() { done.set_value(); });
        done.get_future().wait();
    }
}

void AsyncClient::setKeepAliveOptions(KeepAliveOptions options)
{
    dispatch(
        [this, options = std::move(options)]()
        {
            NX_DEBUG(this, nx::format("Set keep alive to: %1").arg(options));
            if (!m_baseConnection ||
                !m_baseConnection->socket()->setKeepAlive(std::move(options)))
            {
                auto systemErrorCode = SystemError::getLastOSErrorCode();
                NX_DEBUG(this, nx::format("Unable to set keep alive, connection is probably closed. %1")
                    .arg(SystemError::toString(systemErrorCode)));
                return;
            }
        });
}

void AsyncClient::closeConnection(
    SystemError::ErrorCode errorCode,
    BaseConnectionType* connection)
{
    std::unique_ptr< BaseConnectionType > baseConnection;
    {
        NX_MUTEX_LOCKER lock( &m_mutex );
        closeConnectionImpl( &lock, errorCode );
        baseConnection = std::move( m_baseConnection );
    }

    NX_ASSERT(!baseConnection || !connection || connection == baseConnection.get(),
        "Incorrect closeConnection call");

    if (baseConnection)
    {
        baseConnection->pleaseStopSync();

        // Reporting "connection closed" only if there was a connection to close.
        if (m_onConnectionClosedHandler)
            m_onConnectionClosedHandler(errorCode);
    }
}

void AsyncClient::openConnectionImpl(nx::Locker<nx::Mutex>* lock)
{
    if (!m_endpoint)
    {
        NX_VERBOSE(this, nx::format("Cannot open connection: no address"));
        lock->unlock();
        post(std::bind(&AsyncClient::onConnectionComplete, this, SystemError::notConnected));
        return;
    }

    switch (m_state)
    {
        case State::disconnected:
        {
            // establish new connection
            m_connectingSocket = SocketFactory::createStreamSocket(
                ssl::kAcceptAnyCertificate, m_useSsl, NatTraversalSupport::disabled);
            m_connectingSocket->bindToAioThread(getAioThread());

            auto onComplete = [this](SystemError::ErrorCode code)
            { onConnectionComplete(code); };

            if (!m_connectingSocket->setNonBlockingMode(true) ||
                !m_connectingSocket->setSendTimeout(m_settings.sendTimeout) ||
                // TODO: #muskov Use m_timeouts.recvTimeout on timer when request is sent.
                !m_connectingSocket->setRecvTimeout(0))
            {
                const auto sysErrorCode = SystemError::getLastOSErrorCode();
                NX_VERBOSE(this, nx::format("Failed to open connection to %1: Failed to configure socket: %2")
                    .arg(*m_endpoint).arg(SystemError::toString(sysErrorCode)));
                m_connectingSocket->post(
                    std::bind(onComplete, sysErrorCode));
                return;
            }

            NX_VERBOSE(this, "Connecting to %1", *m_endpoint);
            m_connectingSocket->connectAsync(*m_endpoint, std::move(onComplete));

            m_state = State::connecting;
            return;
        }

        case State::connected:
        case State::connecting:
            NX_DEBUG(this, nx::format("Cannot open connection while in state %1")
                .arg(toString(m_state)));
            return;

        default:
            NX_ASSERT(false, nx::format("m_state has invalid value: %1").arg(static_cast<int>(m_state)));
            return;
    }
}

void AsyncClient::closeConnectionImpl(
    nx::Locker<nx::Mutex>* lock, SystemError::ErrorCode code)
{
    NX_INFO(this, nx::format("Connection is closed: %1").arg(SystemError::toString(code)));
    m_state = State::disconnected;

    decltype(m_connectingSocket) connectingSocket;
    connectingSocket.swap(m_connectingSocket);

    decltype(m_requestQueue) requestQueue;
    requestQueue.swap(m_requestQueue);

    decltype(m_requestsInProgress) requestsInProgress;
    requestsInProgress.swap(m_requestsInProgress);

    m_connectionTimers.clear();

    lock->unlock();
    {
        if (connectingSocket)
            connectingSocket->pleaseStopSync();

        for (const auto& r : requestsInProgress)
            r.second.second(code, Message());
        for (const auto& r : requestQueue)
            r.second.second(code, Message());
    }
    lock->relock();

    NX_DEBUG(this, nx::format("Scheduling reconnect attempt"));
    m_reconnectTimer->scheduleNextTry(
        [this]
        {
            NX_DEBUG(this, nx::format("Trying to restore connection to STUN server %1 ...")
                .arg(m_endpoint ? *m_endpoint : SocketAddress()));

            NX_MUTEX_LOCKER lock(&m_mutex);
            openConnectionImpl(&lock);
        });
}

void AsyncClient::dispatchRequestsInQueue(const nx::Locker<nx::Mutex>* /*lock*/)
{
    while (!m_requestQueue.empty())
    {
        auto request = std::move(m_requestQueue.front().first);
        auto handler = std::move(m_requestQueue.front().second);
        auto& tid = request.header.transactionId;
        m_requestQueue.pop_front();

        const auto emplace = m_requestsInProgress.emplace(tid, std::pair<void*, RequestHandler>());
        if (!emplace.second)
        {
            NX_ASSERT(false, nx::format("transactionId is not unique: %1").arg(nx::utils::toHex(tid)));
            post(
                [handler = std::move(handler.second)]()
                {
                    handler(SystemError::invalidData, Message());
                });
            continue;
        }

        NX_VERBOSE(this, "Sending request %1", request.header);

        emplace.first->second = std::move(handler);
        m_baseConnection->sendMessage(
            std::move(request),
            [this](SystemError::ErrorCode code) mutable
            {
                NX_MUTEX_LOCKER lock(&m_mutex);
                // TODO #muskov following code looks redundant since handler will be triggered
                //   on connection closure (which is imminent).
                if (code != SystemError::noError)
                {
                    NX_VERBOSE(this, nx::format("Failed to send request to %1. %2")
                        .arg(m_baseConnection->socket()->getForeignAddress())
                        .arg(SystemError::toString(code)));
                    dispatchRequestsInQueue(&lock);
                }
            });
    }
}

void AsyncClient::onConnectionComplete(SystemError::ErrorCode code)
{
    NX_VERBOSE(this, nx::format("Connect to %1 completed with result: %2")
        .arg(m_endpoint ? *m_endpoint : SocketAddress())
        .arg(SystemError::toString(code)));

    ConnectHandler connectCompletionHandler;
    const auto executeOnConnectedHandlerGuard = nx::utils::makeScopeGuard(
        [&connectCompletionHandler, code]()
        {
            if (connectCompletionHandler)
                connectCompletionHandler(code);
        });

    NX_MUTEX_LOCKER lock(&m_mutex);
    connectCompletionHandler.swap(m_connectCompletionHandler);

    if (code != SystemError::noError)
        return closeConnectionImpl(&lock, code);

    NX_ASSERT(m_connectingSocket);
    m_reconnectTimer->cancelSync();

    initializeMessagePipeline(std::move(m_connectingSocket));

    dispatchRequestsInQueue(&lock);

    const auto reconnectHandlers = m_reconnectHandlers;
    lock.unlock();
    for (const auto& handler : reconnectHandlers)
        handler.second();
}

void AsyncClient::initializeMessagePipeline(
    std::unique_ptr<AbstractStreamSocket> connection)
{
    m_resolvedEndpoint = connection->getForeignAddress();

    NX_ASSERT(!m_baseConnection);
    NX_INFO(this, nx::format("Connected to %1").arg(*m_endpoint));

    m_baseConnection = std::make_unique<BaseConnectionType>(std::move(connection));
    m_baseConnection->registerCloseHandler(
        [this, connection = m_baseConnection.get()](auto closeReason, auto /*connectionDestroyed*/)
        {
            closeConnection(closeReason, connection);
        });
    m_baseConnection->bindToAioThread(getAioThread());
    m_baseConnection->setMessageHandler(
        [this](Message message) { processMessage(std::move(message)); });

    m_state = State::connected;

    m_baseConnection->startReadingConnection();
}

void AsyncClient::processMessage(Message message)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    message.transportHeader.requestedEndpoint = m_baseConnection->socket()->getForeignAddress();
    message.transportHeader.locationEndpoint = message.transportHeader.requestedEndpoint;

    NX_VERBOSE(this, "Received message %1", message.header);

    switch (message.header.messageClass)
    {
        case MessageClass::request:
            NX_ASSERT(false, "Client does not support requests");
            return;

        case MessageClass::errorResponse:
        case MessageClass::successResponse:
        {
            // find corresponding handler by transactionId
            const auto it = m_requestsInProgress.find(message.header.transactionId);
            if (it == m_requestsInProgress.end())
            {
                NX_VERBOSE(this, nx::format("Response to canceled request %1")
                    .arg(nx::utils::toHex(message.header.transactionId)));
                return;
            }

            // use and erase the handler (transactionId is unique per transaction)
            RequestHandler handler(std::move(it->second.second));
            m_requestsInProgress.erase(it);

            lock.unlock();
            return handler(SystemError::noError, std::move(message));
        }

        case MessageClass::indication:
        {
            auto it = m_indicationHandlers.find(message.header.method);
            if (it == m_indicationHandlers.end())
                it = m_indicationHandlers.find(kEveryIndicationMethod); //< Default indication handler.

            if (it != m_indicationHandlers.end())
            {
                auto handler = it->second;
                lock.unlock();
                handler.second(std::move(message));
            }
            else
            {
                NX_WARNING(this, nx::format("Unexpected/unsupported indication: %2")
                    .arg(message.header.method));
            }
            return;
        }

        default:
            NX_ASSERT(false, nx::format("messageClass has invalid value: %1")
                .arg(static_cast< int >(message.header.messageClass)));
            return;
    }
}

void AsyncClient::startTimer(
    ConnectionTimers::iterator timer,
    std::chrono::milliseconds period,
    TimerHandler handler)
{
    NX_VERBOSE(this, nx::format("Set timer(%1) for client(%2) after %3")
        .args(timer->second, timer->first, period));

    timer->second->start(
        period,
        [this, client = timer->first, period, handler = std::move(handler)]() mutable
        {
            handler();

            NX_MUTEX_LOCKER lock(&m_mutex);
            const auto timer = m_connectionTimers.find(client);
            if (timer == m_connectionTimers.end())
                return; //< Timer is canceled.

            startTimer(timer, period, std::move(handler));
        });
}

void AsyncClient::stopWhileInAioThread()
{
    NX_INFO(this, nx::format("Stopped"));
    m_reconnectTimer.reset();
    m_baseConnection.reset();
    m_connectingSocket.reset();
    m_connectionTimers.clear();
}

const char* AsyncClient::toString(State state) const
{
    switch (state)
    {
        case State::disconnected:
            return "disconnected";
        case State::connecting:
            return "connecting";
        case State::connected:
            return "connected";
    }

    return "unknown";
}

} // namespace nx::network::stun
