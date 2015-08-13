/**********************************************************
* 30 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_STUN_CONNECTION_H
#define NX_STUN_CONNECTION_H

#include <functional>
#include <list>

#include <utils/thread/mutex.h>
#include <utils/network/connection_server/base_stream_protocol_connection.h>
#include <utils/network/connection_server/stream_socket_server.h>

#include "stun_message.h"
#include "stun_message_parser.h"
#include "stun_message_serializer.h"

namespace nx {
namespace stun {

//!Connects to STUN server, sends requests, receives responses and indications
/*!
    \note Methods of this class are not thread-safe
    \todo restore interrupted connection ?
*/
class AsyncClient
{
public:
    typedef nx_api::BaseStreamProtocolConnectionEmbeddable<
        AsyncClient,
        Message,
        MessageParser,
        MessageSerializer
    > BaseConnectionType;

    typedef BaseConnectionType ConnectionType;
    typedef std::unique_ptr< AbstractStreamSocket > SocketPtr;

    typedef std::function< void( SystemError::ErrorCode ) > ConnectionHandler;
    typedef std::function< void( Message ) > IndicationHandler;
    typedef std::function< void( SystemError::ErrorCode, Message )> RequestHandler;

    // TODO: pass timeouts
    AsyncClient( const SocketAddress& endpoint, bool useSsl = false );
    ~AsyncClient();
    Q_DISABLE_COPY( AsyncClient );

    //!Asynchronously openes connection to the server, specified during initialization
    /*!
        \param connectHandler Will be called once to deliver connection completeness
        \param indicationHandler Will be called for each indication message
        \param disconnectHandler Will be called on disconnect
        \return \a false, if could not start asynchronous operation
        \note It is valid to call after \fn sendRequest to setup handlers
    */
    bool openConnection( ConnectionHandler connectHandler,
                         IndicationHandler indicationHandler,
                         ConnectionHandler disconnectHandler );

    //!Sends message asynchronously
    /*!
        \param requestHandler Triggered after response has been received or error
            has occured. \a Message attribute is valid only if first attribute value
            is \a SystemError::noError
        \return \a false, if could not start asynchronous operation
        \note It is valid to call this method independent of \a openConnection
            (connection will be opened automaticly)
    */
    bool sendRequest( Message request, RequestHandler requestHandler );

    //! \note Required by \a nx_api::BaseServerConnection
    void closeConnection( BaseConnectionType* );

private:
    enum class State
    {
        NOT_CONNECTED,
        CONNECTING,
        CONNECTED
    };

    bool openConnectionImpl( QnMutexLockerBase* lock );
    bool dispatchRequestsInQueue( QnMutexLockerBase* lock );
    void onConnectionComplete( SocketPtr socket, SystemError::ErrorCode code );
    void processMessage( Message message );

private:
    const SocketAddress m_endpoint;
    const bool m_useSsl;
    std::unique_ptr<BaseConnectionType> m_baseConnection;

    QnMutex m_mutex;
    State m_state;
    ConnectionHandler m_connectHandler;
    IndicationHandler m_indicationHandler;
    ConnectionHandler m_disconnectHandler;

    std::list< std::pair< Message, RequestHandler > > m_requestQueue;
    std::map< Buffer, RequestHandler > m_requestsInProgress;
};

} // namespase stun
} // namespase nx

#endif  //NX_STUN_CONNECTION_H
