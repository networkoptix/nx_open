#pragma once

#include <nx/network/connection_server/base_stream_protocol_connection.h>
#include <nx/network/connection_server/stream_socket_server.h>
#include <nx/network/stun/abstract_async_client.h>
#include <nx/network/stun/message_parser.h>
#include <nx/network/stun/message_serializer.h>
#include <nx/utils/thread/mutex.h>

#include <map>

namespace nx {
namespace stun {

typedef nx_api::BaseStreamProtocolConnectionEmbeddable<
    Message,
    MessageParser,
    MessageSerializer> MessagePipeline;

/**
 * Connects to STUN server, sends requests, receives responses and indications
 */
class NX_NETWORK_API AsyncClient
:
    public AbstractAsyncClient,
    public StreamConnectionHolder<MessagePipeline>
{
public:
    typedef MessagePipeline BaseConnectionType;

    typedef BaseConnectionType ConnectionType;

    AsyncClient(Settings timeouts = kDefaultSettings);
    ~AsyncClient() override;

    Q_DISABLE_COPY( AsyncClient );

    void connect(SocketAddress endpoint, bool useSsl = false) override;
    bool setIndicationHandler(int method, IndicationHandler handler, void* client = 0) override;
    void addOnReconnectedHandler(ReconnectHandler handler, void* client = 0) override;
    void sendRequest(Message request, RequestHandler handler, void* client = 0) override;
    SocketAddress localAddress() const override;
    SocketAddress remoteAddress() const override;
    void closeConnection(SystemError::ErrorCode errorCode) override;
    void cancelHandlers(void* client, utils::MoveOnlyFunc<void()> handler) override;

    /*! \note Required by \a nx_api::BaseServerConnection */
    virtual void closeConnection(
        SystemError::ErrorCode errorCode, BaseConnectionType* connection) override;

private:
    enum class State
    {
        disconnected,
        connecting,
        connected,
        terminated,
    };

    void openConnectionImpl(QnMutexLockerBase* lock);
    void closeConnectionImpl(QnMutexLockerBase* lock, SystemError::ErrorCode code);

    void dispatchRequestsInQueue(const QnMutexLockerBase* lock);
    void onConnectionComplete(SystemError::ErrorCode code);
    void processMessage(Message message );

private:
    const Settings m_settings;

    mutable QnMutex m_mutex;
    boost::optional<SocketAddress> m_endpoint;
    bool m_useSsl;
    State m_state;

    nx::network::RetryTimer m_timer;
    std::unique_ptr<BaseConnectionType> m_baseConnection;
    std::unique_ptr<AbstractStreamSocket> m_connectingSocket;

    std::list<std::pair<Message, std::pair<void*, RequestHandler>>> m_requestQueue;
    std::map<int, std::pair<void*, IndicationHandler>> m_indicationHandlers;
    std::multimap<void*, ReconnectHandler> m_reconnectHandlers;
    std::map<Buffer, std::pair<void*, RequestHandler>> m_requestsInProgress;
};

} // namespase stun
} // namespase nx
