// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <optional>

#include <nx/network/connection_server/base_stream_protocol_connection.h>
#include <nx/network/connection_server/stream_socket_server.h>
#include <nx/network/stun/abstract_async_client.h>
#include <nx/network/stun/message_parser.h>
#include <nx/network/stun/message_serializer.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/url.h>

namespace nx::network::stun {

using MessagePipeline = nx::network::server::StreamProtocolConnection<
    Message,
    MessageParser,
    MessageSerializer>;

/**
 * Connects to STUN server, sends requests, receives responses and indications.
 */
class NX_NETWORK_API AsyncClient:
    public AbstractAsyncClient
{
public:
    using BaseConnectionType = MessagePipeline;
    using ConnectionType = BaseConnectionType;

    AsyncClient(Settings timeouts = Settings());
    /**
     * @param tcpConnection Already-established connection to the target server.
     */
    AsyncClient(
        std::unique_ptr<AbstractStreamSocket> tcpConnection,
        Settings timeouts = Settings());
    virtual ~AsyncClient() override;

    AsyncClient(const AsyncClient&) = delete;

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    virtual void connect(
        const nx::utils::Url& url,
        ConnectHandler completionHandler = nullptr) override;

    virtual void setIndicationHandler(
        int method, IndicationHandler handler, void* client = nullptr) override;

    virtual void addOnReconnectedHandler(
        ReconnectHandler handler,
        void* client = nullptr) override;
    virtual void addOnConnectionClosedHandler(
        OnConnectionClosedHandler onConnectionClosedHandler, void* client) override;

    virtual void sendRequest(Message request, RequestHandler handler, void* client = nullptr) override;
    // TODO: #akolesnikov This method does not seem to belong here. Remove it.
    virtual bool addConnectionTimer(
        std::chrono::milliseconds period, TimerHandler handler, void* client) override;

    virtual SocketAddress localAddress() const override;
    virtual SocketAddress remoteAddress() const override;
    virtual void closeConnection(SystemError::ErrorCode errorCode) override;
    virtual void cancelHandlers(void* client, nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void cancelHandlersSync(void* client) override;
    virtual void setKeepAliveOptions(KeepAliveOptions options) override;

private:
    enum class State
    {
        disconnected,
        connecting,
        connected,
    };

    void closeConnection(
        SystemError::ErrorCode errorCode,
        BaseConnectionType* connection);

    void openConnectionImpl(nx::Locker<nx::Mutex>* lock);
    void closeConnectionImpl(nx::Locker<nx::Mutex>* lock, SystemError::ErrorCode code);

    void dispatchRequestsInQueue(const nx::Locker<nx::Mutex>* lock);
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

    mutable nx::Mutex m_mutex;
    std::optional<SocketAddress> m_endpoint;
    std::optional<SocketAddress> m_resolvedEndpoint;
    bool m_useSsl = false;
    State m_state = State::disconnected;

    std::unique_ptr<nx::network::RetryTimer> m_reconnectTimer;
    std::unique_ptr<BaseConnectionType> m_baseConnection;
    std::unique_ptr<AbstractStreamSocket> m_connectingSocket;

    std::list<std::pair<Message, std::pair<void*, RequestHandler>>> m_requestQueue;
    std::map<int, std::pair<void*, IndicationHandler>> m_indicationHandlers;
    std::multimap<void*, ReconnectHandler> m_reconnectHandlers;
    std::map<Buffer, std::pair<void*, RequestHandler>> m_requestsInProgress;
    ConnectionTimers m_connectionTimers;

    nx::utils::InterruptionFlag destructionFlag;
    std::map<void* /*client*/, OnConnectionClosedHandler> m_connectionClosedHandlers;
    ConnectHandler m_connectCompletionHandler;

    const char* toString(State state) const;
};

} // namespace nx::network::stun
