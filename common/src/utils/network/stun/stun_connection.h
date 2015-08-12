/**********************************************************
* 30 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_STUN_CONNECTION_H
#define NX_STUN_CONNECTION_H

#include <functional>
#include <queue>

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

    typedef std::function< void( SystemError::ErrorCode ) > ConnectionHandler;
    typedef std::function< void( Message ) > IndicationHandler;
    typedef std::function< void( SystemError::ErrorCode, Message )> RequestHandler;

    AsyncClient(
        const SocketAddress& stunServerEndpoint,
        bool useSsl = false );

    ~AsyncClient();

    Q_DISABLE_COPY(AsyncClient);

    //!Asynchronously openes connection to the server, specified during initialization
    /*!
        \param completionHandler will be called once to deliver connection completeness
        \param indicationHandler will be called for each indication message
        \return \a false, if could not start asynchronous operation
        \note It is valid to call \fn sendRequest when connection has not completed yet
    */
    bool openConnection( ConnectionHandler completionHandler,
                         IndicationHandler indicationHandler = []( Message ){} );

    //!Sends message asynchronously
    /*!
        \param requestHandler Triggered after response has been received or error has occured.
            \a Message attribute is valid only if first attribute value is \a SystemError::noError
        \return \a false, if could not start asynchronous operation (this can happen due to lack of resources on host machine)
        \note It is valid to call this method after If \a StunClientConnection::openConnection has been called and not completed yet
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

    void onConnectionComplete( SystemError::ErrorCode code, ConnectionHandler handler );
    void dispatchRequestsInQueue();
    void processMessage( Message message );

private:
    const SocketAddress m_stunServerEndpoint;
    std::unique_ptr<AbstractCommunicatingSocket> m_socket;
    std::unique_ptr<BaseConnectionType> m_baseConnection;

    QnMutex m_mutex;
    State m_state;
    IndicationHandler m_indicationHandler;
    std::queue< std::pair< Message, RequestHandler > > m_requestQueue;
    std::map< Buffer, RequestHandler > m_requestsInProgress;
};

} // namespase stun
} // namespase nx

#endif  //NX_STUN_CONNECTION_H
