#include "listening_peer_pool.h"

#include <nx/network/cloud/tunnel/relay/api/relay_api_open_tunnel_notification.h>
#include <nx/network/socket_common.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/std/cpp14.h>

#include "../settings.h"

namespace nx {
namespace cloud {
namespace relay {
namespace model {

ListeningPeerPool::ListeningPeerPool(const conf::ListeningPeer& settings):
    m_settings(settings),
    m_terminated(false)
{
    m_periodicTasksTimer.start(
        m_settings.internalTimerPeriod,
        std::bind(&ListeningPeerPool::doPeriodicTasks, this));
}

ListeningPeerPool::~ListeningPeerPool()
{
    m_apiCallCounter.wait();

    m_unsuccessfulResultReporter.pleaseStopSync();
    m_periodicTasksTimer.pleaseStopSync();

    {
        QnMutexLocker lock(&m_mutex);
        m_terminated = true;
    }

    for (auto& peerContext: m_peers)
    {
        for (auto& connectionContext: peerContext.second.connections)
            connectionContext->connection->pleaseStopSync();
    }
}

void ListeningPeerPool::addConnection(
    const std::string& peerNameOriginal,
    std::unique_ptr<AbstractStreamSocket> connection)
{
    auto peerName = utils::reverseWords(peerNameOriginal, ".");

    QnMutexLocker lock(&m_mutex);

    processExpirationTimers(lock);

    auto insertionPair = m_peers.emplace(peerName, PeerContext());
    PeerContext& peerContext = insertionPair.first->second;

    if (peerContext.expirationTimer)
    {
        m_peerExpirationTimers.erase(*peerContext.expirationTimer);
        peerContext.expirationTimer = boost::none;
    }

    if (!connection->setKeepAlive(m_settings.tcpKeepAlive))
    {
        const auto sysErrorCode = SystemError::getLastOSErrorCode();
        NX_DEBUG(this, lm("Could not enable keep alive on connection from host %1 (%2). %3")
            .arg(peerName).arg(connection->getForeignAddress())
            .arg(SystemError::toString(sysErrorCode)));
    }
    
    auto connectionContext = std::make_unique<ConnectionContext>();
    connectionContext->connection = std::move(connection);

    if (someoneIsWaitingForPeerConnection(peerContext))
    {
        provideConnectionToTheClient(
            lock,
            peerName,
            &peerContext,
            std::move(connectionContext));
    }
    else
    {
        peerContext.connections.push_back(std::move(connectionContext));
        monitoringConnectionForClosure(
            peerName,
            peerContext.connections.back().get());
    }

    if (insertionPair.second)
    {
        scheduleEvent(std::bind(
            &nx::utils::Subscription<std::string>::notify,
            &m_peerConnectedSubscription,
            peerNameOriginal));
    }
}

std::size_t ListeningPeerPool::getConnectionCountByPeerName(
    const std::string& peerNameOriginal) const
{
    const auto peerName = utils::reverseWords(peerNameOriginal, ".");

    QnMutexLocker lock(&m_mutex);

    const_cast<ListeningPeerPool*>(this)->processExpirationTimers(lock);

    // Returning total number of connections of all matching peers.

    std::size_t totalConnectionCount = 0;
    auto iterRange = nx::utils::equalRangeByPrefix(m_peers, peerName);
    for (auto it = iterRange.first; it != iterRange.second; ++it)
        totalConnectionCount += it->second.connections.size();

    return totalConnectionCount;
}

bool ListeningPeerPool::isPeerOnline(const std::string& peerNameOriginal) const
{
    auto peerName = utils::reverseWords(peerNameOriginal, ".");

    QnMutexLocker lock(&m_mutex);

    const_cast<ListeningPeerPool*>(this)->processExpirationTimers(lock);

    auto peerContextIter = utils::findFirstElementWithPrefix(m_peers, peerName);
    return peerContextIter != m_peers.end();
}

std::string ListeningPeerPool::findListeningPeerByDomain(
    const std::string& domainName) const
{
    auto domainNameReversed = utils::reverseWords(domainName, ".");

    QnMutexLocker lock(&m_mutex);

    const_cast<ListeningPeerPool*>(this)->processExpirationTimers(lock);

    auto it = utils::findFirstElementWithPrefix(m_peers, domainNameReversed);
    if (it == m_peers.end())
        return std::string();
    return utils::reverseWords(it->first, ".");
}

void ListeningPeerPool::takeIdleConnection(
    const ClientInfo& clientInfo,
    const std::string& peerNameOriginal,
    TakeIdleConnectionHandler completionHandler)
{
    const auto peerName = utils::reverseWords(peerNameOriginal, ".");
    
    QnMutexLocker lock(&m_mutex);

    processExpirationTimers(lock);

    auto peerContextIter = utils::findFirstElementWithPrefix(m_peers, peerName);
    if (peerContextIter == m_peers.end())
    {
        m_unsuccessfulResultReporter.post(
            [completionHandler = std::move(completionHandler)]()
            {
                completionHandler(api::ResultCode::notFound, nullptr);
            });
        return;
    }

    PeerContext& peerContext = peerContextIter->second;

    if (peerContext.connections.empty())
    {
        startWaitingForNewConnection(
            lock,
            clientInfo,
            peerName,
            &peerContext,
            std::move(completionHandler));
        return;
    }

    auto connectionContext = std::move(peerContext.connections.front());
    peerContext.connections.pop_front();

    if (peerContext.connections.empty())
        startPeerExpirationTimer(lock, peerContextIter->first, &peerContext);

    giveAwayConnection(
        clientInfo,
        std::move(connectionContext),
        std::move(completionHandler));
}

nx::utils::Subscription<std::string>& ListeningPeerPool::peerConnectedSubscription()
{
    return m_peerConnectedSubscription;
}

nx::utils::Subscription<std::string>& ListeningPeerPool::peerDisconnectedSubscription()
{
    return m_peerDisconnectedSubscription;
}

bool ListeningPeerPool::someoneIsWaitingForPeerConnection(
    const PeerContext& peerContext) const
{
    return !peerContext.takeConnectionRequestQueue.empty();
}

void ListeningPeerPool::provideConnectionToTheClient(
    const QnMutexLockerBase& lock,
    const std::string& peerName,
    PeerContext* peerContext,
    std::unique_ptr<ConnectionContext> connectionContext)
{
    NX_ASSERT(peerContext->connections.empty());

    auto awaitContext = std::move(peerContext->takeConnectionRequestQueue.front());
    peerContext->takeConnectionRequestQueue.pop_front();

    m_takeIdleConnectionRequestTimers.erase(awaitContext.expirationTimerIter);

    startPeerExpirationTimer(lock, peerName, peerContext);

    giveAwayConnection(
        awaitContext.clientInfo,
        std::move(connectionContext),
        std::move(awaitContext.handler));
}

void ListeningPeerPool::startWaitingForNewConnection(
    const QnMutexLockerBase& /*lock*/,
    const ClientInfo& clientInfo,
    const std::string& peerName,
    PeerContext* peerContext,
    TakeIdleConnectionHandler completionHandler)
{
    auto expirationTime = 
        nx::utils::monotonicTime() + m_settings.takeIdleConnectionTimeout;
    // NOTE: Client request timer cannot last beyond peer offline timeout.
    if (peerContext->expirationTimer)
        expirationTime = std::min(expirationTime, (*peerContext->expirationTimer)->first);

    ConnectionAwaitContext connectionAwaitContext;
    connectionAwaitContext.clientInfo = clientInfo;
    connectionAwaitContext.handler = std::move(completionHandler);
    connectionAwaitContext.expirationTime = expirationTime;
    peerContext->takeConnectionRequestQueue.push_back(
        std::move(connectionAwaitContext));

    peerContext->takeConnectionRequestQueue.back().expirationTimerIter = 
        m_takeIdleConnectionRequestTimers.emplace(expirationTime, peerName);
}

void ListeningPeerPool::giveAwayConnection(
    const ClientInfo& clientInfo,
    std::unique_ptr<ConnectionContext> connectionContext,
    TakeIdleConnectionHandler completionHandler)
{
    // TODO: #ak Get rid of this struct when connectionPtr->sendAsync accepts nx::utils::MoveOnlyFunc
    struct Context
    {
        std::unique_ptr<ConnectionContext> connectionContext;
        TakeIdleConnectionHandler completionHandler;
    };

    api::OpenTunnelNotification notification;
    notification.setClientEndpoint(clientInfo.endpoint);
    notification.setClientPeerName(clientInfo.peerName.c_str());
    auto openTunnelNotificationBuffer = 
        std::make_shared<nx_http::StringType>(notification.toHttpMessage().toString());
    auto connectionPtr = connectionContext->connection.get();

    auto context = std::make_shared<Context>();
    context->connectionContext = std::move(connectionContext);
    context->completionHandler = std::move(completionHandler);

    connectionPtr->sendAsync(
        *openTunnelNotificationBuffer,
        [this, 
            clientInfo,
            openTunnelNotificationBuffer,
            context,
            scopedCallGuard = m_apiCallCounter.getScopedIncrement()](
                SystemError::ErrorCode sysErrorCode,
                std::size_t /*bytesSent*/) mutable
        {
            if (sysErrorCode != SystemError::noError)
            {
                NX_LOGX(lm("Session %1. Failed to send open tunnel notification to %2. %3")
                    .arg(clientInfo.relaySessionId)
                    .arg(context->connectionContext->connection->getForeignAddress())
                    .arg(SystemError::toString(sysErrorCode)), cl_logDEBUG1);
                return context->completionHandler(api::ResultCode::networkError, nullptr);
            }

            context->connectionContext->connection->cancelIOSync(network::aio::etNone);
            context->completionHandler(
                api::ResultCode::ok,
                std::move(context->connectionContext->connection));
        });
}

void ListeningPeerPool::monitoringConnectionForClosure(
    const std::string& peerName,
    ConnectionContext* connectionContext)
{
    using namespace std::placeholders;

    constexpr int readBufferSize = 1; //< We need only detect connection closure.

    connectionContext->readBuffer.clear();
    connectionContext->readBuffer.reserve(readBufferSize);
    connectionContext->connection->readSomeAsync(
        &connectionContext->readBuffer,
        std::bind(&ListeningPeerPool::onConnectionReadCompletion, this,
            peerName, connectionContext, _1, _2));
}

void ListeningPeerPool::onConnectionReadCompletion(
    const std::string& peerName,
    ConnectionContext* connectionContext,
    SystemError::ErrorCode sysErrorCode,
    std::size_t bytesRead)
{
    const bool isConnectionClosed =
        sysErrorCode == SystemError::noError && bytesRead == 0;

    if (isConnectionClosed ||
        (sysErrorCode != SystemError::noError && 
         nx::network::socketCannotRecoverFromError(sysErrorCode)))
    {
        return closeConnection(peerName, connectionContext, sysErrorCode);
    }

    // TODO: What if some data has been received?

    monitoringConnectionForClosure(peerName, connectionContext);
}

void ListeningPeerPool::closeConnection(
    const std::string& peerName,
    ConnectionContext* connectionContextToErase,
    SystemError::ErrorCode /*sysErrorCode*/)
{
    QnMutexLocker lock(&m_mutex);

    if (m_terminated)
        return;

    auto peerIter = m_peers.find(peerName);
    if (peerIter == m_peers.end())
        return;

    PeerContext& peerContext = peerIter->second;
    auto connectionContextIter = std::find_if(
        peerContext.connections.begin(), peerContext.connections.end(),
        [connectionContextToErase](const std::unique_ptr<ConnectionContext>& context)
        {
            return context.get() == connectionContextToErase;
        });
    if (connectionContextIter != peerContext.connections.end())
    {
        NX_ASSERT((*connectionContextIter)->connection->isInSelfAioThread());
        peerContext.connections.erase(connectionContextIter);
        if (peerContext.connections.empty())
            startPeerExpirationTimer(lock, peerName, &peerContext);
    }
}

void ListeningPeerPool::startPeerExpirationTimer(
    const QnMutexLockerBase& /*lock*/,
    const std::string& peerName,
    ListeningPeerPool::PeerContext* peerContext)
{
    NX_ASSERT(!static_cast<bool>(peerContext->expirationTimer));

    auto timerIter = m_peerExpirationTimers.emplace(
        nx::utils::monotonicTime() + m_settings.disconnectedPeerTimeout,
        peerName);
    peerContext->expirationTimer = timerIter;
}

void ListeningPeerPool::processExpirationTimers(
    const QnMutexLockerBase& /*lock*/,
    std::chrono::steady_clock::time_point currentTime)
{
    std::vector<std::string> disconnectedPeers;

    while (!m_peerExpirationTimers.empty() && 
        m_peerExpirationTimers.begin()->first <= currentTime)
    {
        const std::string& peerName = m_peerExpirationTimers.begin()->second;

        auto peerIter = m_peers.find(peerName);
        NX_ASSERT(peerIter != m_peers.end());
        NX_ASSERT(peerIter->second.takeConnectionRequestQueue.empty());
        NX_ASSERT(peerIter->second.connections.empty());
        m_peers.erase(peerIter);

        scheduleEvent(std::bind(
            &nx::utils::Subscription<std::string>::notify,
            &m_peerDisconnectedSubscription,
            utils::reverseWords(peerName, "."))); //< NOTE: peerName is actually "domainName.peerName".

        m_peerExpirationTimers.erase(m_peerExpirationTimers.begin());
    }
}

void ListeningPeerPool::processExpirationTimers(
    std::chrono::steady_clock::time_point currentTime)
{
    QnMutexLocker lock(&m_mutex);
    processExpirationTimers(lock, currentTime);
}

void ListeningPeerPool::doPeriodicTasks()
{
    const auto currentTime = nx::utils::monotonicTime();

    {
        QnMutexLocker lock(&m_mutex);
        handleTimedoutTakeConnectionRequests(lock, currentTime);
        processExpirationTimers(lock, currentTime);
    }
    raiseScheduledEvents();

    m_periodicTasksTimer.cancelSync();
    m_periodicTasksTimer.start(
        m_settings.internalTimerPeriod,
        std::bind(&ListeningPeerPool::doPeriodicTasks, this));
}

void ListeningPeerPool::handleTimedoutTakeConnectionRequests(
    const QnMutexLockerBase& lock,
    std::chrono::steady_clock::time_point currentTime)
{
    auto timedoutRequestHandlers = 
        findTimedoutTakeConnectionRequestHandlers(lock, currentTime);

    for (auto& handler: timedoutRequestHandlers)
    {
        scheduleEvent(
            [handler = std::move(handler)]()
            {
                handler(api::ResultCode::timedOut, nullptr);
            });
    }
}

std::vector<TakeIdleConnectionHandler> 
    ListeningPeerPool::findTimedoutTakeConnectionRequestHandlers(
        const QnMutexLockerBase& /*lock*/,
        std::chrono::steady_clock::time_point currentTime)
{
    std::vector<TakeIdleConnectionHandler> requestHandlers;

    while (!m_takeIdleConnectionRequestTimers.empty() &&
        m_takeIdleConnectionRequestTimers.begin()->first <= currentTime)
    {
        auto peerName = std::move(m_takeIdleConnectionRequestTimers.begin()->second);
        m_takeIdleConnectionRequestTimers.erase(
            m_takeIdleConnectionRequestTimers.begin());

        auto peerIter = m_peers.find(peerName);
        if (peerIter == m_peers.end())
            continue;

        PeerContext& peerContext = peerIter->second;
        for (auto it = peerContext.takeConnectionRequestQueue.begin();
            it != peerContext.takeConnectionRequestQueue.end();
            )
        {
            if (it->expirationTime > currentTime)
            {
                ++it;
                continue;
            }

            requestHandlers.push_back(std::move(it->handler));
            it = peerContext.takeConnectionRequestQueue.erase(it);
        }
    }

    return requestHandlers;
}

void ListeningPeerPool::scheduleEvent(nx::utils::MoveOnlyFunc<void()> raiseEventFunc)
{
    m_scheduledEvents.push_back(std::move(raiseEventFunc));
    forcePeriodicTasksProcessing();
}

void ListeningPeerPool::forcePeriodicTasksProcessing()
{
    m_periodicTasksTimer.post(std::bind(&ListeningPeerPool::doPeriodicTasks, this));
}

void ListeningPeerPool::raiseScheduledEvents()
{
    decltype (m_scheduledEvents) scheduledEvents;
    {
        QnMutexLocker lock(&m_mutex);
        scheduledEvents.swap(m_scheduledEvents);
    }
    for (auto& raiseEventFunc: scheduledEvents)
        raiseEventFunc();
}

//-------------------------------------------------------------------------------------------------

ListeningPeerPoolFactory::ListeningPeerPoolFactory():
    base_type(std::bind(&ListeningPeerPoolFactory::defaultFactoryFunction, 
        this, std::placeholders::_1))
{
}

ListeningPeerPoolFactory& ListeningPeerPoolFactory::instance()
{
    static ListeningPeerPoolFactory staticInstance;
    return staticInstance;
}

std::unique_ptr<AbstractListeningPeerPool>
    ListeningPeerPoolFactory::defaultFactoryFunction(const conf::ListeningPeer& settings)
{
    return std::make_unique<ListeningPeerPool>(settings);
}

} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx
