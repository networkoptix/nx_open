#pragma once

#include <map>

#include <nx/network/connection_server/base_stream_protocol_connection.h>
#include <nx/network/connection_server/stream_socket_server.h>
#include <nx/network/stun/abstract_async_client.h>
#include <nx/network/stun/message_parser.h>
#include <nx/network/stun/message_serializer.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace stun {

typedef nx::network::server::BaseStreamProtocolConnectionEmbeddable<
    Message,
    MessageParser,
    MessageSerializer> MessagePipeline;

/**
 * Connects to STUN server, sends requests, receives responses and indications
 */
class NX_NETWORK_API AsyncClient:
    public AbstractAsyncClient,
    public nx::network::server::StreamConnectionHolder<MessagePipeline>
{
public:
    typedef MessagePipeline BaseConnectionType;
    typedef BaseConnectionType ConnectionType;
    typedef nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> OnConnectionClosedHandler;

    AsyncClient(Settings timeouts = Settings());
    virtual ~AsyncClient() override;

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    Q_DISABLE_COPY( AsyncClient );

    virtual void connect(
        SocketAddress endpoint,
        bool useSsl = false,
        ConnectHandler completionHandler = nullptr) override;

    virtual bool setIndicationHandler(
        int method, IndicationHandler handler, void* client = 0) override;
    
    virtual void addOnReconnectedHandler(ReconnectHandler handler, void* client = 0) override;
    virtual void sendRequest(Message request, RequestHandler handler, void* client = 0) override;
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
    void processMessage(Message message );

    typedef std::map<void*, std::unique_ptr<network::aio::Timer>> ConnectionTimers;
    void startTimer(ConnectionTimers::iterator timer, std::chrono::milliseconds period, TimerHandler handler);

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

} // namespase stun
} // namespase nx
