#pragma once

#include <map>

#include <nx/network/connection_server/base_stream_protocol_connection.h>
#include <nx/network/connection_server/stream_socket_server.h>
#include <nx/network/stun/abstract_async_client.h>
#include <nx/network/stun/message_parser.h>
#include <nx/network/stun/message_serializer.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/url.h>

namespace nx {
namespace network {
namespace stun {

using MessagePipeline = nx::network::server::BaseStreamProtocolConnectionEmbeddable<
    Message,
    MessageParser,
    MessageSerializer>;

/**
 * Connects to STUN server, sends requests, receives responses and indications.
 */
class NX_NETWORK_API AsyncClient:
    public AbstractAsyncClient,
    public nx::network::server::StreamConnectionHolder<MessagePipeline>
{
public:
    using BaseConnectionType = MessagePipeline;
    using ConnectionType = BaseConnectionType;
    using OnConnectionClosedHandler = nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)>;

    AsyncClient(Settings timeouts = Settings());
    /**
     * @param tcpConnection Already-established connection to the target server.
     */
    AsyncClient(
        std::unique_ptr<AbstractStreamSocket> tcpConnection,
        Settings timeouts = Settings());

    AsyncClient(const AsyncClient&) = delete;

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    virtual void connect(
        const nx::utils::Url& url,
        ConnectHandler completionHandler = nullptr) override;

    virtual bool setIndicationHandler(
        int method, IndicationHandler handler, void* client = nullptr) override;

    virtual void addOnReconnectedHandler(ReconnectHandler handler, void* client = nullptr) override;
    virtual void sendRequest(Message request, RequestHandler handler, void* client = nullptr) override;
    // TODO: #ak This method does not seem to belong here. Remove it.
    virtual bool addConnectionTimer(
        std::chrono::milliseconds period, TimerHandler handler, void* client) override;

    virtual SocketAddress localAddress() const override;
    virtual SocketAddress remoteAddress() const override;
    virtual void closeConnection(SystemError::ErrorCode errorCode) override;
    virtual void cancelHandlers(void* client, utils::MoveOnlyFunc<void()> handler) override;
    virtual void setKeepAliveOptions(KeepAliveOptions options) override;

    void setOnConnectionClosedHandler(OnConnectionClosedHandler onConnectionClosedHandler);

private:
    enum class State
    {
        disconnected,
        connecting,
        connected,
    };

    virtual void closeConnection(
        SystemError::ErrorCode errorCode, BaseConnectionType* connection) override;

    void openConnectionImpl(QnMutexLockerBase* lock);
    void closeConnectionImpl(QnMutexLockerBase* lock, SystemError::ErrorCode code);

    void dispatchRequestsInQueue(const QnMutexLockerBase* lock);
    void onConnectionComplete(SystemError::ErrorCode code);
    void initializeMessagePipeline(std::unique_ptr<AbstractStreamSocket> connection);
    void processMessage(Message message );

    typedef std::map<void*, std::unique_ptr<network::aio::Timer>> ConnectionTimers;
    void startTimer(
        ConnectionTimers::iterator timer,
        std::chrono::milliseconds period,
        TimerHandler handler);

    virtual void stopWhileInAioThread() override;

private:
    const Settings m_settings;

    mutable QnMutex m_mutex;
    boost::optional<SocketAddress> m_endpoint;
    boost::optional<SocketAddress> m_resolvedEndpoint;
    bool m_useSsl;
    State m_state;

    std::unique_ptr<nx::network::RetryTimer> m_reconnectTimer;
    std::unique_ptr<BaseConnectionType> m_baseConnection;
    std::unique_ptr<AbstractStreamSocket> m_connectingSocket;

    std::list<std::pair<Message, std::pair<void*, RequestHandler>>> m_requestQueue;
    std::map<int, std::pair<void*, IndicationHandler>> m_indicationHandlers;
    std::multimap<void*, ReconnectHandler> m_reconnectHandlers;
    std::map<Buffer, std::pair<void*, RequestHandler>> m_requestsInProgress;
    ConnectionTimers m_connectionTimers;

    OnConnectionClosedHandler m_onConnectionClosedHandler;
    ConnectHandler m_connectCompletionHandler;

    const char* toString(State state) const;
};

} // namespace stun
} // namespace network
} // namespace nx
