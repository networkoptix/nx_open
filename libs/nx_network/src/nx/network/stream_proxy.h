#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <vector>

#include <nx/network/aio/timer.h>
#include <nx/utils/byte_stream/pipeline.h>
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
    StreamProxy();
    ~StreamProxy();

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;

    template<typename T>
    // requires std::is_base_of<T, nx::utils::bstream::AbstractOutputConverter>::value
    void setUpStreamConverter()
    {
        m_upStreamConverterFactory = []() { return std::make_unique<T>(); };
    }

    template<typename T>
    // requires std::is_base_of<T, nx::utils::bstream::AbstractInputConverter>::value
    void setDownStreamConverter()
    {
        m_downStreamConverterFactory = []() { return std::make_unique<T>(); };
    }

    void startProxy(
        std::unique_ptr<AbstractStreamSocketAcceptor> source,
        const SocketAddress& destinationEndpoint);

    void setProxyDestination(const SocketAddress& newDestination);

    void setConnectToDestinationTimeout(
        std::optional<std::chrono::milliseconds> timeout);

private:
    using StreamProxyChannels =
        std::list<std::unique_ptr<detail::StreamProxyChannel>>;

    mutable QnMutex m_mutex;
    std::unique_ptr<AbstractStreamSocketAcceptor> m_sourceAcceptor;
    aio::Timer m_timer;
    SocketAddress m_destinationEndpoint;
    StreamProxyChannels m_proxyChannels;
    std::optional<std::chrono::milliseconds> m_connectToDestinationTimeout;
    std::chrono::milliseconds m_retryAcceptTimeout;
    std::function<std::unique_ptr<nx::utils::bstream::AbstractOutputConverter>()>
        m_upStreamConverterFactory;
    std::function<std::unique_ptr<nx::utils::bstream::AbstractInputConverter>()>
        m_downStreamConverterFactory;

    void onAcceptCompletion(
        SystemError::ErrorCode systemErrorCode,
        std::unique_ptr<AbstractStreamSocket> connection);

    void retryAcceptAfterTimeout();

    void initiateConnectionToTheDestination(
        const QnMutexLockerBase& /*lock*/,
        std::unique_ptr<AbstractStreamSocket> connection);

    void removeProxyChannel(
        StreamProxyChannels::iterator proxyChannelIter,
        SystemError::ErrorCode completionCode);

    void stopProxyChannels(
        std::function<void()> completionHandler);
};

//-------------------------------------------------------------------------------------------------

/**
 * NOTE: Not thread-safe.
 */
class NX_NETWORK_API StreamProxyPool:
    public QnStoppableAsync
{
public:
    ~StreamProxyPool();

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;

    /**
     * @return ID of proxy that may be used later to modify or stop the proxy. Always non-zero.
     */
    int addProxy(
        std::unique_ptr<AbstractStreamSocketAcceptor> source,
        const SocketAddress& destinationEndpoint);

    /**
     * @param proxyId ID provided by StreamProxyPool::addProxy call.
     */
    void setProxyDestination(int proxyId, const SocketAddress& newDestination);

    /**
     * @param proxyId ID provided by StreamProxyPool::addProxy call.
     * NOTE: Blocks until proxy has been stopped.
     */
    void stopProxy(int proxyId);

    void setConnectToDestinationTimeout(
        std::optional<std::chrono::milliseconds> timeout);

private:
    std::map<int, std::unique_ptr<StreamProxy>> m_proxies;
    std::atomic<int> m_lastProxyId{0};
    mutable QnMutex m_mutex;
    std::optional<std::chrono::milliseconds> m_connectToDestinationTimeout;
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

    void setConnectTimeout(
        std::optional<std::chrono::milliseconds> timeout);

    void setUpStreamConverter(
        std::unique_ptr<nx::utils::bstream::AbstractOutputConverter> converter);

    void setDownStreamConverter(
        std::unique_ptr<nx::utils::bstream::AbstractInputConverter> converter);

    void start(ProxyCompletionHandler completionHandler);

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::unique_ptr<AbstractStreamSocket> m_sourceConnection;
    const SocketAddress m_destinationEndpoint;
    std::unique_ptr<AbstractStreamSocket> m_destinationConnection;
    std::unique_ptr<aio::AsyncChannelBridge> m_bridge;
    ProxyCompletionHandler m_completionHandler;
    std::optional<std::chrono::milliseconds> m_connectTimeout;
    nx::utils::bstream::CompositeConverter m_converter;
    std::unique_ptr<nx::utils::bstream::AbstractOutputConverter> m_upStreamConverter;
    std::unique_ptr<nx::utils::bstream::AbstractInputConverter> m_downStreamConverter;

    bool tuneDestinationConnectionAttributes();

    void onConnectToTargetCompletion(SystemError::ErrorCode systemErrorCode);

    void onBridgeCompleted(SystemError::ErrorCode completionCode);
};

} // namespace detail

} // namespace nx::network
