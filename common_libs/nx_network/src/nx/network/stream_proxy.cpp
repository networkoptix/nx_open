#include "stream_proxy.h"

#include <nx/network/socket_factory.h>
#include <nx/utils/thread/barrier_handler.h>

namespace nx::network {

StreamProxy::StreamProxy():
    m_retryAcceptTimeout(std::chrono::seconds(1))
{
}

StreamProxy::~StreamProxy()
{
    pleaseStopSync();
}

void StreamProxy::pleaseStop(
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    nx::utils::BarrierHandler barrierHandler(std::move(completionHandler));

    m_sourceAcceptor->pleaseStop(
        [this, barrierHandler = barrierHandler.fork()]()
        {
            m_timer.pleaseStopSync();
            stopProxyChannels(std::move(barrierHandler));
        });
}

void StreamProxy::startProxy(
    std::unique_ptr<AbstractStreamSocketAcceptor> source,
    const SocketAddress& destinationEndpoint)
{
    using namespace std::placeholders;

    m_sourceAcceptor = std::move(source);
    m_destinationEndpoint = destinationEndpoint;

    m_timer.bindToAioThread(m_sourceAcceptor->getAioThread());
    m_sourceAcceptor->acceptAsync(
        std::bind(&StreamProxy::onAcceptCompletion, this, _1, _2));
}

void StreamProxy::setProxyDestination(
    const SocketAddress& newDestination)
{
    QnMutexLocker lock(&m_mutex);
    m_destinationEndpoint = newDestination;
}

void StreamProxy::setConnectToDestinationTimeout(
    std::optional<std::chrono::milliseconds> timeout)
{
    m_connectToDestinationTimeout = timeout;
}

void StreamProxy::onAcceptCompletion(
    SystemError::ErrorCode systemErrorCode,
    std::unique_ptr<AbstractStreamSocket> connection)
{
    using namespace std::placeholders;

    if (systemErrorCode != SystemError::noError &&
        systemErrorCode != SystemError::timedOut)
    {
        retryAcceptAfterTimeout();
        return;
    }

    m_sourceAcceptor->acceptAsync(
        std::bind(&StreamProxy::onAcceptCompletion, this, _1, _2));

    QnMutexLocker lock(&m_mutex);

    if (!connection || !connection->setNonBlockingMode(true))
    {
        NX_DEBUG(this, lm("Proxy to %1. Accept connection error. %2")
            .args(m_destinationEndpoint, SystemError::toString(systemErrorCode)));
        return;
    }

    initiateConnectionToTheDestination(lock, std::move(connection));
}

void StreamProxy::retryAcceptAfterTimeout()
{
    using namespace std::placeholders;

    m_timer.start(
        m_retryAcceptTimeout,
        [this]()
        {
            m_sourceAcceptor->acceptAsync(
                std::bind(&StreamProxy::onAcceptCompletion, this, _1, _2));
        });
}

void StreamProxy::initiateConnectionToTheDestination(
    const QnMutexLockerBase& /*lock*/,
    std::unique_ptr<AbstractStreamSocket> sourceConnection)
{
    using namespace std::placeholders;

    m_proxyChannels.push_back(
        std::make_unique<detail::StreamProxyChannel>(
            std::move(sourceConnection),
            m_destinationEndpoint));

    if (m_connectToDestinationTimeout)
    {
        m_proxyChannels.back()->setConnectTimeout(
            *m_connectToDestinationTimeout);
    }

    m_proxyChannels.back()->start(
        std::bind(&StreamProxy::removeProxyChannel, this,
            std::prev(m_proxyChannels.end()), _1));
}

void StreamProxy::removeProxyChannel(
    StreamProxyChannels::iterator proxyChannelIter,
    SystemError::ErrorCode completionCode)
{
    std::unique_ptr<detail::StreamProxyChannel> proxyChannel;

    {
        QnMutexLocker lock(&m_mutex);

        NX_VERBOSE(this,
            lm("Proxy to %1. Removing completed proxy channel. Completion code: %2")
            .args(m_destinationEndpoint, SystemError::toString(completionCode)));

        proxyChannel.swap(*proxyChannelIter);
        m_proxyChannels.erase(proxyChannelIter);
    }
}

void StreamProxy::stopProxyChannels(
    std::function<void()> completionHandler)
{
    nx::utils::BarrierHandler barrierHandler(std::move(completionHandler));

    QnMutexLocker lock(&m_mutex);

    for (auto& proxyChannel: m_proxyChannels)
        proxyChannel->pleaseStop(barrierHandler.fork());
}

//-------------------------------------------------------------------------------------------------

StreamProxyPool::~StreamProxyPool()
{
    pleaseStopSync();
}

void StreamProxyPool::pleaseStop(
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    nx::utils::BarrierHandler barrierHandler(std::move(completionHandler));

    for (auto& proxyContext: m_proxies)
        proxyContext.second->pleaseStop(barrierHandler.fork());
}

int StreamProxyPool::addProxy(
    std::unique_ptr<AbstractStreamSocketAcceptor> source,
    const SocketAddress& destinationEndpoint)
{
    using namespace std::placeholders;

    const int proxyId = ++m_lastProxyId;

    auto it = m_proxies.emplace(
        proxyId,
        std::make_unique<StreamProxy>()).first;

    if (m_connectToDestinationTimeout)
        it->second->setConnectToDestinationTimeout(*m_connectToDestinationTimeout);

    it->second->startProxy(std::move(source), destinationEndpoint);

    return proxyId;
}

void StreamProxyPool::setProxyDestination(
    int proxyId,
    const SocketAddress& newDestination)
{
    auto proxyIter = m_proxies.find(proxyId);
    if (proxyIter == m_proxies.end())
        return;

    QnMutexLocker lock(&m_mutex);
    proxyIter->second->setProxyDestination(newDestination);
}

void StreamProxyPool::stopProxy(int proxyId)
{
    auto proxyIter = m_proxies.find(proxyId);
    if (proxyIter == m_proxies.end())
        return;

    proxyIter->second->pleaseStopSync();

    m_proxies.erase(proxyIter);
}

void StreamProxyPool::setConnectToDestinationTimeout(
    std::optional<std::chrono::milliseconds> timeout)
{
    m_connectToDestinationTimeout = timeout;
}

//-------------------------------------------------------------------------------------------------

namespace detail {

StreamProxyChannel::StreamProxyChannel(
    std::unique_ptr<AbstractStreamSocket> sourceConnection,
    const SocketAddress& destinationEndpoint)
    :
    m_sourceConnection(std::move(sourceConnection)),
    m_destinationEndpoint(destinationEndpoint)
{
    bindToAioThread(m_sourceConnection->getAioThread());
}

void StreamProxyChannel::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_sourceConnection)
        m_sourceConnection->bindToAioThread(aioThread);
    if (m_destinationConnection)
        m_destinationConnection->bindToAioThread(aioThread);
    if (m_bridge)
        m_bridge->bindToAioThread(aioThread);
}

void StreamProxyChannel::setConnectTimeout(
    std::optional<std::chrono::milliseconds> timeout)
{
    m_connectTimeout = timeout;
}

void StreamProxyChannel::start(ProxyCompletionHandler completionHandler)
{
    using namespace std::placeholders;

    m_completionHandler = std::move(completionHandler);

    m_destinationConnection = SocketFactory::createStreamSocket();
    m_destinationConnection->bindToAioThread(getAioThread());

    if (!tuneDestinationConnectionAttributes())
    {
        const auto lastError = SystemError::getLastOSErrorCode();
        NX_DEBUG(this, lm("Proxy to %1. Failed to prepare destination connection. %2")
            .args(m_destinationEndpoint, SystemError::toString(lastError)));
        post(std::bind(&StreamProxyChannel::onBridgeCompleted, this, lastError));
        return;
    }

    m_destinationConnection->connectAsync(
        m_destinationEndpoint,
        std::bind(&StreamProxyChannel::onConnectToTargetCompletion, this, _1));
}

bool StreamProxyChannel::tuneDestinationConnectionAttributes()
{
    if (!m_destinationConnection->setNonBlockingMode(true))
        return false;

    if (!m_connectTimeout)
    {
        if (!m_destinationConnection->setSendTimeout(*m_connectTimeout))
            return false;
    }

    return true;
}

void StreamProxyChannel::stopWhileInAioThread()
{
    m_sourceConnection.reset();
    m_destinationConnection.reset();
    m_bridge.reset();
}

void StreamProxyChannel::onConnectToTargetCompletion(
    SystemError::ErrorCode systemErrorCode)
{
    using namespace std::placeholders;

    if (systemErrorCode != SystemError::noError)
    {
        NX_DEBUG(this, lm("Proxy to %1. Failed to connect to the destination. %2")
            .args(m_destinationEndpoint, SystemError::toString(systemErrorCode)));
        nx::utils::swapAndCall(m_completionHandler, systemErrorCode);
        return;
    }

    m_bridge = aio::makeAsyncChannelBridge(
        std::move(m_sourceConnection),
        std::move(m_destinationConnection));
    m_bridge->start(std::bind(&StreamProxyChannel::onBridgeCompleted, this, _1));
}

void StreamProxyChannel::onBridgeCompleted(
    SystemError::ErrorCode completionCode)
{
    stopWhileInAioThread();

    nx::utils::swapAndCall(m_completionHandler, completionCode);
}

} // namespace detail

} // namespace nx::network
