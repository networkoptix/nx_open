#include "listening_peer_pool.h"

#include <nx/network/cloud/tunnel/relay/api/relay_api_open_tunnel_notification.h>
#include <nx/network/socket_common.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/log/log.h>

#include "settings.h"

namespace nx {
namespace cloud {
namespace relaying {

ListeningPeerPool::ListeningPeerPool(const Settings& settings):
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
            connectionContext->connectionWatcher->pleaseStopSync();
    }
}

void ListeningPeerPool::addConnection(
    const std::string& originalPeerName,
    const std::string& peerProtocolVersion,
    std::unique_ptr<network::AbstractStreamSocket> connection)
{
    auto peerName = convertHostnameToInternalFormat(originalPeerName);

    NX_VERBOSE(this, "Added connection from %1, protocol version %2",
        originalPeerName, peerProtocolVersion);

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
    connectionContext->peerEndpoint = connection->getForeignAddress();
    connectionContext->connectionWatcher =
        std::make_unique<ListeningPeerConnectionWatcher>(
            std::move(connection),
            peerProtocolVersion);

    m_statisticsCalculator.connectionAccepted();

    if (someoneIsWaitingForPeerConnection(peerContext))
    {
        m_statisticsCalculator.connectionUsed();

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
        insertionPair.first->second.originalPeerName = originalPeerName;
        scheduleEvent(std::bind(
            &nx::utils::Subscription<std::string>::notify,
            &m_peerConnectedSubscription,
            originalPeerName));
    }
}

std::size_t ListeningPeerPool::getConnectionCountByPeerName(
    const std::string& originalPeerName) const
{
    const auto peerName = convertHostnameToInternalFormat(originalPeerName);

    QnMutexLocker lock(&m_mutex);

    const_cast<ListeningPeerPool*>(this)->processExpirationTimers(lock);

    // Returning total number of connections of all matching peers.

    std::size_t totalConnectionCount = 0;
    auto iterRange = nx::utils::equalRangeByPrefix(m_peers, peerName);
    for (auto it = iterRange.first; it != iterRange.second; ++it)
        totalConnectionCount += it->second.connections.size();

    return totalConnectionCount;
}

bool ListeningPeerPool::isPeerOnline(const std::string& originalPeerName) const
{
    auto peerName = convertHostnameToInternalFormat(originalPeerName);

    QnMutexLocker lock(&m_mutex);

    const_cast<ListeningPeerPool*>(this)->processExpirationTimers(lock);

    auto peerContextIter = utils::findFirstElementWithPrefix(m_peers, peerName);
    return peerContextIter != m_peers.end();
}

std::string ListeningPeerPool::findListeningPeerByDomain(
    const std::string& domainName) const
{
    auto domainNameReversed = convertHostnameToInternalFormat(domainName);

    QnMutexLocker lock(&m_mutex);

    const_cast<ListeningPeerPool*>(this)->processExpirationTimers(lock);

    auto it = utils::findFirstElementWithPrefix(m_peers, domainNameReversed);
    if (it == m_peers.end())
        return std::string();
    return it->second.originalPeerName;
}

void ListeningPeerPool::takeIdleConnection(
    const ClientInfo& clientInfo,
    const std::string& originalPeerName,
    TakeIdleConnectionHandler completionHandler)
{
    const auto peerName = convertHostnameToInternalFormat(originalPeerName);

    QnMutexLocker lock(&m_mutex);

    processExpirationTimers(lock);

    auto peerContextIter = utils::findFirstElementWithPrefix(m_peers, peerName);
    if (peerName.empty() || peerContextIter == m_peers.end())
    {
        m_unsuccessfulResultReporter.post(
            [completionHandler = std::move(completionHandler)]()
            {
                completionHandler(relay::api::ResultCode::notFound, nullptr, std::string());
            });
        return;
    }

    PeerContext& peerContext = peerContextIter->second;

    if (peerContext.connections.empty())
    {
        startWaitingForNewConnection(
            lock,
            clientInfo,
            peerContextIter->first,
            &peerContext,
            std::move(completionHandler));
        return;
    }

    auto connectionContext = std::move(peerContext.connections.front());
    peerContext.connections.pop_front();

    m_statisticsCalculator.connectionUsed();

    if (peerContext.connections.empty())
        startPeerExpirationTimer(lock, peerContextIter->first, &peerContext);

    giveAwayConnection(
        clientInfo,
        peerContextIter->first,
        std::move(connectionContext),
        std::move(completionHandler));
}

Statistics ListeningPeerPool::statistics() const
{
    QnMutexLocker lock(&m_mutex);
    return m_statisticsCalculator.calculateStatistics(static_cast<int>(m_peers.size()));
}

nx::utils::Subscription<std::string>& ListeningPeerPool::peerConnectedSubscription()
{
    return m_peerConnectedSubscription;
}

nx::utils::Subscription<std::string>& ListeningPeerPool::peerDisconnectedSubscription()
{
    return m_peerDisconnectedSubscription;
}

std::string ListeningPeerPool::convertHostnameToInternalFormat(
    const std::string& hostname) const
{
    auto reversed = utils::reverseWords(hostname, ".");
    nx::utils::to_lower(&reversed);
    return reversed;
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
        peerName,
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
    const std::string& peerName,
    std::unique_ptr<ConnectionContext> connectionContext,
    TakeIdleConnectionHandler completionHandler)
{
    auto connectionWatcherPtr = connectionContext->connectionWatcher.get();

    connectionWatcherPtr->startTunnel(
        clientInfo,
        [this,
            clientInfo,
            peerName,
            connectionContext = std::move(connectionContext),
            completionHandler = std::move(completionHandler),
            scopedCallGuard = m_apiCallCounter.getScopedIncrement()](
                SystemError::ErrorCode sysErrorCode,
                std::unique_ptr<network::AbstractStreamSocket> connection) mutable
        {
            if (sysErrorCode != SystemError::noError)
            {
                NX_DEBUG(this, "Session %1. Failed to send open tunnel notification to %2. %3",
                    clientInfo.relaySessionId, connectionContext->peerEndpoint,
                        SystemError::toString(sysErrorCode));
                return completionHandler(
                    relay::api::ResultCode::networkError, nullptr, std::string());
            }

            completionHandler(
                relay::api::ResultCode::ok,
                std::move(connection),
                nx::utils::reverseWords(peerName, "."));
        });
}

void ListeningPeerPool::monitoringConnectionForClosure(
    const std::string& peerName,
    ConnectionContext* connectionContext)
{
    connectionContext->connectionWatcher->start(
        [this, peerName, connectionContext](SystemError::ErrorCode errorCode)
        {
            return closeConnection(peerName, connectionContext, errorCode);
        });
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
        NX_ASSERT((*connectionContextIter)->connectionWatcher->isInSelfAioThread());
        peerContext.connections.erase(connectionContextIter);

        m_statisticsCalculator.connectionClosed();

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

void ListeningPeerPool::processExpirationTimers()
{
    QnMutexLocker lock(&m_mutex);
    processExpirationTimers(lock);
}

void ListeningPeerPool::processExpirationTimers(const QnMutexLockerBase& lock)
{
    const auto currentTime = nx::utils::monotonicTime();

    handleTimedoutTakeConnectionRequests(lock, currentTime);
    removeExpiredListeningPeers(lock, currentTime);
}

void ListeningPeerPool::removeExpiredListeningPeers(
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
        auto originalPeerName = std::move(peerIter->second.originalPeerName);
        m_peers.erase(peerIter);

        scheduleEvent(std::bind(
            &nx::utils::Subscription<std::string>::notify,
            &m_peerDisconnectedSubscription,
            originalPeerName)); //< NOTE: peerName is actually "domainName.peerName".

        m_peerExpirationTimers.erase(m_peerExpirationTimers.begin());
    }
}

void ListeningPeerPool::doPeriodicTasks()
{
    processExpirationTimers();
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
                handler(relay::api::ResultCode::timedOut, nullptr, std::string());
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
    ListeningPeerPoolFactory::defaultFactoryFunction(const Settings& settings)
{
    return std::make_unique<ListeningPeerPool>(settings);
}

} // namespace relaying
} // namespace cloud
} // namespace nx
