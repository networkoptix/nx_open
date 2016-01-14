#ifndef NX_STUN_ASYNC_CLIENT_H
#define NX_STUN_ASYNC_CLIENT_H

#include <nx/utils/thread/mutex.h>
#include <nx/network/connection_server/base_stream_protocol_connection.h>
#include <nx/network/connection_server/stream_socket_server.h>

#include "message.h"
#include "message_parser.h"
#include "message_serializer.h"

#ifdef _DEBUG
    #include <map>
#else
    #include <unordered_map>
#endif

namespace nx {
namespace stun {

//!Connects to STUN server, sends requests, receives responses and indications
class NX_NETWORK_API AsyncClient
    : public StreamConnectionHolder<
		nx_api::BaseStreamProtocolConnectionEmbeddable<
			Message,
			MessageParser,
			MessageSerializer
		>>
{
public:
    typedef nx_api::BaseStreamProtocolConnectionEmbeddable<
        Message,
        MessageParser,
        MessageSerializer
    > BaseConnectionType;

    typedef BaseConnectionType ConnectionType;

    typedef std::function< void( Message ) > IndicationHandler;
    typedef std::function< void( SystemError::ErrorCode, Message )> RequestHandler;

    static const struct Timeouts { uint send, recv, reconnect; } DEFAULT_TIMEOUTS;

    AsyncClient(Timeouts timeouts = DEFAULT_TIMEOUTS);
    ~AsyncClient();

    Q_DISABLE_COPY( AsyncClient );

    //!Asynchronously openes connection to the server
    /*!
        \param endpoint Address to use
        \note shell be called only once (to provide address) reconnect will
              happen automatically
    */
    void connect( SocketAddress endpoint, bool useSsl = false );

    //!Subscribes for certain indications
    /*!
        \param method Is monitoring indication type
        \param handler Will be called for each indication message
        \return true on success, false if this methed is already monitored
    */
    bool monitorIndications( int method, IndicationHandler handler );

    //!Stops monitoring for certain indications
    /*!
        \param method Is monitoring indication type
        \note does not affect indications in progress
        \return true on success, false if this method was not monitored
    */
    bool ignoreIndications( int method );

    //!Sends message asynchronously
    /*!
        \param requestHandler Triggered after response has been received or error
            has occured. \a Message attribute is valid only if first attribute value
            is \a SystemError::noError
        \return \a false, if could not start asynchronous operation
        \note It is valid to call this method independent of \a openConnection
            (connection will be opened automaticly)
    */
    void sendRequest( Message request, RequestHandler handler );

    //!Returns local address if client is connected to be server
    SocketAddress localAddress() const;

    //! \note Required by \a nx_api::BaseServerConnection
    virtual void closeConnection(
            SystemError::ErrorCode errorCode,
            BaseConnectionType* connection = nullptr ) override;

    static boost::optional< QString >
        hasError( SystemError::ErrorCode code, const Message& message );

private:
    enum class State
    {
        disconnected,
        connecting,
        connected,
        terminated,
    };

    void openConnectionImpl( QnMutexLockerBase* lock );
    void closeConnectionImpl( QnMutexLockerBase* lock,
                              SystemError::ErrorCode code );

    void dispatchRequestsInQueue( QnMutexLockerBase* lock );
    void onConnectionComplete( SystemError::ErrorCode code );
    void processMessage( Message message );

private:
    const Timeouts m_timeouts;

    mutable QnMutex m_mutex;
    boost::optional<SocketAddress> m_endpoint;
    bool m_useSsl;
    State m_state;

    std::unique_ptr<AbstractCommunicatingSocket> m_timerSocket;
    std::unique_ptr<BaseConnectionType> m_baseConnection;
    std::unique_ptr<AbstractStreamSocket> m_connectingSocket;

    std::list<std::pair<Message, RequestHandler>> m_requestQueue;
    #ifdef _DEBUG
        std::map<int, IndicationHandler> m_indicationHandlers;
        std::map<Buffer,RequestHandler> m_requestsInProgress;
    #else
        std::unordered_map<int, IndicationHandler> m_indicationHandlers;
        std::unordered_map<Buffer, RequestHandler> m_requestsInProgress;
    #endif
};

} // namespase stun
} // namespase nx

#endif // NX_STUN_ASYNC_CLIENT_H
