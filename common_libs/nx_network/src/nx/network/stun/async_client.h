#ifndef NX_STUN_ASYNC_CLIENT_H
#define NX_STUN_ASYNC_CLIENT_H

#include <nx/utils/thread/mutex.h>
#include <nx/network/connection_server/base_stream_protocol_connection.h>
#include <nx/network/connection_server/stream_socket_server.h>

#include "message.h"
#include "message_parser.h"
#include "message_serializer.h"
#include "nx/network/aio/timer.h"
#include <nx/network/retry_timer.h>

#ifdef _DEBUG
    #include <map>
#else
    #include <unordered_map>
#endif

namespace nx {
namespace stun {

class NX_NETWORK_API AbstractAsyncClient
{
public:
    struct Settings
    {
        std::chrono::milliseconds sendTimeout;
        std::chrono::milliseconds recvTimeout;
        nx::network::RetryPolicy reconnectPolicy;

        Settings() /* Defaults */
        :
            sendTimeout(3000),
            recvTimeout(3000),
            reconnectPolicy(
                nx::network::RetryPolicy::kInfiniteRetries,
                std::chrono::milliseconds(500),
                nx::network::RetryPolicy::kDefaultDelayMultiplier,
                std::chrono::minutes(1))
        {}
    };

    static const Settings kDefaultSettings;

    virtual ~AbstractAsyncClient() = default;

    typedef std::function<void(Message)> IndicationHandler;
    typedef std::function<void()> ReconnectHandler;
    typedef utils::MoveOnlyFunc<void(
        SystemError::ErrorCode, Message)> RequestHandler;

    //!Asynchronously openes connection to the server
    /*!
        \param endpoint Address to use
        \note shall be called only once (to provide address) reconnect will
              happen automatically
    */
    virtual void connect(SocketAddress endpoint, bool useSsl = false) = 0;

    //!Subscribes for certain indications
    /*!
        \param method Is monitoring indication type
        \param handler Will be called for each indication message
        \return true on success, false if this methed is already monitored
    */
    virtual bool setIndicationHandler(int method, IndicationHandler handler) = 0;

    //!Subscribes for the event of successful reconnect
    /*!
        \param handler is called on every successfull reconnect
    */
    virtual void addOnReconnectedHandler(ReconnectHandler handler) = 0;

    //!Sends message asynchronously
    /*!
        \param requestHandler Triggered after response has been received or error
            has occured. \a Message attribute is valid only if first attribute value
            is \a SystemError::noError
        \return \a false, if could not start asynchronous operation
        \note It is valid to call this method independent of \a openConnection
            (connection will be opened automaticly)
    */
    virtual void sendRequest(Message request, RequestHandler handler) = 0;

    //!Returns local address if client is connected to the server
    virtual SocketAddress localAddress() const = 0;

    //!Returns server address if client knows one
    virtual SocketAddress remoteAddress() const = 0;

    //!Closes connection, also engage reconnect
    virtual void closeConnection(SystemError::ErrorCode errorCode) = 0;

    static boost::optional<QString>
        hasError(SystemError::ErrorCode code, const Message& message);
};

typedef nx_api::BaseStreamProtocolConnectionEmbeddable<
    Message,
    MessageParser,
    MessageSerializer> MessagePipeline;

//!Connects to STUN server, sends requests, receives responses and indications
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
    bool setIndicationHandler(int method, IndicationHandler handler) override;
    void addOnReconnectedHandler(ReconnectHandler handler) override;
    void sendRequest(Message request, RequestHandler handler) override;
    SocketAddress localAddress() const override;
    SocketAddress remoteAddress() const override;
    void closeConnection(SystemError::ErrorCode errorCode) override;

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

    std::list<std::pair<Message, RequestHandler>> m_requestQueue;
    std::map<int, IndicationHandler> m_indicationHandlers;
    std::vector<ReconnectHandler> m_reconnectHandlers;
    std::map<Buffer,RequestHandler> m_requestsInProgress;
};

} // namespase stun
} // namespase nx

#endif // NX_STUN_ASYNC_CLIENT_H
