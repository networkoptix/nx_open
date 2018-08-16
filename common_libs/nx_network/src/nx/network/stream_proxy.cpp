#include "stream_proxy.h"

#include <nx/network/socket_factory.h>
#include <nx/utils/thread/barrier_handler.h>

namespace nx::network {

StreamProxy::~StreamProxy()
{
    std::promise<void> stopped;
    pleaseStop(
        [&stopped]()
        {
            stopped.set_value();
        });
    stopped.get_future().wait();
}

void StreamProxy::pleaseStop(
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    nx::utils::BarrierHandler barrierHandler(std::move(completionHandler));

    for (auto& proxyContext: m_proxies)
    {
        proxyContext.second.sourceAcceptor->pleaseStop(
            std::bind(&StreamProxy::stopProxyChannels, this,
                &proxyContext.second, barrierHandler.fork()));
    }
}

int StreamProxy::addProxy(
    std::unique_ptr<AbstractStreamSocketAcceptor> source,
    const SocketAddress& destinationEndpoint)
{
    using namespace std::placeholders;

    const int proxyId = ++m_lastProxyId;

    auto it = m_proxies.emplace(
        proxyId,
        ProxyDestinationContext{std::move(source), destinationEndpoint}).first;
    it->second.sourceAcceptor->acceptAsync(
        std::bind(&StreamProxy::onAcceptCompletion, this, &it->second, _1, _2));

    return proxyId;
}

void StreamProxy::setProxyDestination(
    int proxyId,
    const SocketAddress& newDestination)
{
    auto proxyIter = m_proxies.find(proxyId);
    if (proxyIter == m_proxies.end())
        return;

    QnMutexLocker lock(&m_mutex);
    proxyIter->second.destinationEndpoint = newDestination;
}

void StreamProxy::stopProxy(int /*proxyId*/)
{
    // TODO
}

void StreamProxy::onAcceptCompletion(
    ProxyDestinationContext* proxyContext,
    SystemError::ErrorCode systemErrorCode,
    std::unique_ptr<AbstractStreamSocket> connection)
{
    using namespace std::placeholders;

    proxyContext->sourceAcceptor->acceptAsync(
        std::bind(&StreamProxy::onAcceptCompletion, this, proxyContext, _1, _2));

    QnMutexLocker lock(&m_mutex);

    if (!connection || !connection->setNonBlockingMode(true))
    {
        NX_DEBUG(this, lm("Proxy to %1. Accept connection error. %2")
            .args(proxyContext->destinationEndpoint, SystemError::toString(systemErrorCode)));
        return;
    }

    initiateConnectionToTheDestination(
        lock,
        proxyContext,
        std::move(connection));
}

void StreamProxy::initiateConnectionToTheDestination(
    const QnMutexLockerBase& /*lock*/,
    ProxyDestinationContext* proxyContext,
    std::unique_ptr<AbstractStreamSocket> sourceConnection)
{
    using namespace std::placeholders;

    proxyContext->proxyChannels.push_back(
        std::make_unique<detail::StreamProxyChannel>(
            std::move(sourceConnection),
            proxyContext->destinationEndpoint));

    proxyContext->proxyChannels.back()->start(
        std::bind(&StreamProxy::removeBridge, this,
            proxyContext, std::prev(proxyContext->proxyChannels.end()), _1));
}

void StreamProxy::removeBridge(
    ProxyDestinationContext* proxyContext,
    StreamProxyChannels::iterator proxyChannelIter,
    SystemError::ErrorCode completionCode)
{
    std::unique_ptr<detail::StreamProxyChannel> proxyChannel;

    {
        QnMutexLocker lock(&m_mutex);

        NX_VERBOSE(this, lm("Proxy to %1. Removing completed bridge. Completion code: %2")
            .args(proxyContext->destinationEndpoint, SystemError::toString(completionCode)));

        proxyChannel.swap(*proxyChannelIter);
        proxyContext->proxyChannels.erase(proxyChannelIter);
    }
}

void StreamProxy::stopProxyChannels(
    ProxyDestinationContext* proxyContext,
    std::function<void()> completionHandler)
{
    nx::utils::BarrierHandler barrierHandler(std::move(completionHandler));

    QnMutexLocker lock(&m_mutex);

    for (auto& proxyChannel: proxyContext->proxyChannels)
        proxyChannel->pleaseStop(barrierHandler.fork());
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

void StreamProxyChannel::start(ProxyCompletionHandler completionHandler)
{
    using namespace std::placeholders;

    m_completionHandler = std::move(completionHandler);

    m_destinationConnection = SocketFactory::createStreamSocket();
    m_destinationConnection->bindToAioThread(getAioThread());
    if (!m_destinationConnection->setNonBlockingMode(true))
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
