#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <vector>

#include <nx/utils/thread/mutex.h>

#include "abstract_stream_socket_acceptor.h"
#include "async_stoppable.h"
#include "aio/async_channel_bridge.h"

namespace nx::network {

namespace detail { class StreamProxyChannel; }

/**
 * Proxies byte stream from socket acceptor to a destination address.
 * Accepts connections on a server socket.
 * For each accepted connection, a separate connection to the target is opened.
 * And traffic is forwarded in both ways.
 * On error to accept connection there will be a retry after some timeout.
 * NOTE: Not thread-safe.
 */
class NX_NETWORK_API StreamProxy:
    public QnStoppableAsync
{
public:
    ~StreamProxy();

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;

    /**
     * @return ID of proxy that may be used later to modify or stop the proxy. Always non-zero.
     */
    int addProxy(
        std::unique_ptr<AbstractStreamSocketAcceptor> source,
        const SocketAddress& destinationEndpoint);

    /**
     * @param proxyId ID provided by StreamProxy::addProxy call.
     */
    void setProxyDestination(int proxyId, const SocketAddress& newDestination);

    /**
     * @param proxyId ID provided by StreamProxy::addProxy call.
     */
    void stopProxy(int proxyId);

private:
    using StreamProxyChannels =
        std::list<std::unique_ptr<detail::StreamProxyChannel>>;

    struct ProxyDestinationContext
    {
        std::unique_ptr<AbstractStreamSocketAcceptor> sourceAcceptor;
        SocketAddress destinationEndpoint;
        StreamProxyChannels proxyChannels;
    };

    std::map<int, ProxyDestinationContext> m_proxies;
    std::atomic<int> m_lastProxyId{0};
    mutable QnMutex m_mutex;

    void onAcceptCompletion(
        ProxyDestinationContext* proxyContext,
        SystemError::ErrorCode systemErrorCode,
        std::unique_ptr<AbstractStreamSocket> connection);

    void initiateConnectionToTheDestination(
        const QnMutexLockerBase& /*lock*/,
        ProxyDestinationContext* proxyContext,
        std::unique_ptr<AbstractStreamSocket> connection);

    void removeBridge(
        ProxyDestinationContext* proxyContext,
        StreamProxyChannels::iterator proxyChannelIter,
        SystemError::ErrorCode completionCode);

    void stopProxyChannels(
        ProxyDestinationContext* proxyContext,
        std::function<void()> completionHandler);
};

//-------------------------------------------------------------------------------------------------

namespace detail {

class StreamProxyChannel:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    using ProxyCompletionHandler =
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)>;

    StreamProxyChannel(
        std::unique_ptr<AbstractStreamSocket> sourceConnection,
        const SocketAddress& destinationEndpoint);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    void start(ProxyCompletionHandler completionHandler);

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::unique_ptr<AbstractStreamSocket> m_sourceConnection;
    const SocketAddress m_destinationEndpoint;
    std::unique_ptr<AbstractStreamSocket> m_destinationConnection;
    std::unique_ptr<aio::AsyncChannelBridge> m_bridge;
    ProxyCompletionHandler m_completionHandler;

    void onConnectToTargetCompletion(SystemError::ErrorCode systemErrorCode);

    void onBridgeCompleted(SystemError::ErrorCode completionCode);
};

} // namespace detail

} // namespace nx::network
