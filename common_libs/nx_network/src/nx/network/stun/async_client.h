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

    typedef std::function< void( SystemError::ErrorCode ) > ConnectionHandler;
    typedef std::function< void( const Message& ) > IndicationHandler;
    typedef std::function< void( SystemError::ErrorCode, Message )> RequestHandler;

    static const struct Timeouts { uint send, recv; } DEFAULT_TIMEOUTS;

    AsyncClient( const SocketAddress& endpoint, bool useSsl = false,
                 Timeouts timeouts = DEFAULT_TIMEOUTS );
    ~AsyncClient();

    Q_DISABLE_COPY( AsyncClient );

    //!Asynchronously openes connection to the server, specified during initialization
    /*!
        \param connectHandler Will be called once to deliver connection completeness
        \param disconnectHandler Will be called on disconnect (may be repeated if reconnected)
        \return \a false, if could not start asynchronous operation
        \note It is valid to call any time to add connect and disconnect handlers
    */
    void openConnection( ConnectionHandler connectHandler = nullptr,
                         ConnectionHandler disconnectHandler = nullptr );

    //!Subscribes for certain indications
    /*!
        \param method Is monitoring indication type
        \param handler Will be called for each indication message
    */
    void monitorIndocations( int method, IndicationHandler handler );

    //!Sends message asynchronously
    /*!
        \param requestHandler Triggered after response has been received or error
            has occured. \a Message attribute is valid only if first attribute value
            is \a SystemError::noError
        \return \a false, if could not start asynchronous operation
        \note It is valid to call this method independent of \a openConnection
            (connection will be opened automaticly)
    */
    void sendRequest( Message request, RequestHandler requestHandler );

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

    void openConnectionImpl( QnMutexLockerBase* lock,
                             ConnectionHandler* currentHandler = nullptr );

    void closeConnectionImpl( QnMutexLockerBase* lock,
                              SystemError::ErrorCode code );

    void dispatchRequestsInQueue( QnMutexLockerBase* lock );
    void onConnectionComplete( SystemError::ErrorCode code );
    void processMessage( Message message );

private:
    const SocketAddress m_endpoint;
    const bool m_useSsl;
    const Timeouts m_timeouts;
    std::unique_ptr< BaseConnectionType > m_baseConnection;

    QnMutex m_mutex;
    State m_state;
    std::vector< ConnectionHandler > m_connectHandlers;
    std::vector< ConnectionHandler > m_disconnectHandlers;
    #ifdef _DEBUG
        std::map< int, std::vector< IndicationHandler > > m_indicationHandlers;
    #else
        std::unordered_map< int, std::vector< IndicationHandler > > m_indicationHandlers;
    #endif

    struct Credentials { String userName; String key; };
    boost::optional< Credentials > m_credentials;

    std::unique_ptr< AbstractStreamSocket > m_connectingSocket;
    std::list< std::pair< Message, RequestHandler > > m_requestQueue;
    std::map< Buffer, RequestHandler > m_requestsInProgress;
};

} // namespase stun
} // namespase nx

#endif // NX_STUN_ASYNC_CLIENT_H
