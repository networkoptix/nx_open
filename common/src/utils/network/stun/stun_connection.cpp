/**********************************************************
* 30 sep 2014
* a.kolesnikov
***********************************************************/

#include "stun_connection.h"

namespace nx {
namespace stun {

AsyncClient::AsyncClient(
    const SocketAddress& stunServerEndpoint,
    bool useSsl )
:
    m_stunServerEndpoint( stunServerEndpoint ),
    m_state(State::NOT_CONNECTED)
{
    m_socket.reset( SocketFactory::createStreamSocket( useSsl, SocketFactory::nttDisabled ) );
    m_socket->setNonBlockingMode( true );
}

AsyncClient::~AsyncClient()
{
    {
        QnMutexLocker lock( &m_mutex );
        if( m_state == State::NOT_CONNECTED )
            return;
    }

    //TODO #ak pass completionHandler to m_socket->cancelAsyncIO
    //m_socket->cancelAsyncIO();
    m_baseConnection->closeConnection();
}

bool AsyncClient::openConnection( ConnectionHandler completionHandler,
                                       IndicationHandler indicationHandler )
{
    QnMutexLocker lock( &m_mutex );
    if( m_state != State::NOT_CONNECTED )
        return false;

    m_state = State::CONNECTING;
    m_indicationHandler = std::move(indicationHandler);

    auto handler = std::bind(
        &AsyncClient::onConnectionComplete,
        this, std::placeholders::_1, std::move(completionHandler) );

    return m_socket->connectAsync( m_stunServerEndpoint, std::move(handler) );
}

bool AsyncClient::sendRequest( Message request, RequestHandler requestHandler )
{
    QnMutexLocker lock( &m_mutex );
    if( m_state == State::NOT_CONNECTED )
        return false;

    m_requestQueue.push( std::make_pair( std::move( request ),
                                         std::move( requestHandler ) ) );

    if( m_state == State::CONNECTED )
        dispatchRequestsInQueue();

    return true;
}

void AsyncClient::closeConnection( BaseConnectionType* connection )
{
    Q_ASSERT( connection == m_baseConnection.get() );

    // TODO: think about calling handlers with errors and reconnect
}

void AsyncClient::onConnectionComplete(
    SystemError::ErrorCode code, ConnectionHandler handler )
{
    QnMutexLocker lock( &m_mutex );
    if( code != SystemError::noError ) {
        m_state = State::NOT_CONNECTED;
        return handler( code );
    }

    m_baseConnection.reset( new BaseConnectionType( this, std::move(m_socket) ) );
    m_baseConnection->setMessageHandler( std::bind(
        &AsyncClient::processMessage, this, std::placeholders::_1 ) );

    if( !m_baseConnection->startReadingConnection() )
        handler( SystemError::notConnected );

    m_state = State::CONNECTED;
    handler( SystemError::noError );

    dispatchRequestsInQueue();
}

void AsyncClient::dispatchRequestsInQueue()
{
    while( !m_requestQueue.empty() )
    {
        auto request = std::move( m_requestQueue.front() );
        m_requestQueue.pop();

        // Save handler to process when we have a response
        Q_ASSERT( m_requestsInProgress.insert( std::make_pair(
            request.first.header.transactionId, std::move( request.second )
        ) ).second );

        // TODO: #mu think what to do in case return value is false or callback
        // gets called with
        Q_ASSERT( m_baseConnection->sendMessage(
            std::move( request.first ),
            []( SystemError::ErrorCode code )
            { Q_ASSERT( code == SystemError::noError ); } ) );
    }
}

void AsyncClient::processMessage( Message message )
{
    QnMutexLocker lock( &m_mutex );
    switch( message.header.messageClass )
    {
        case MessageClass::errorResponse:
        case MessageClass::successResponse:
        {
            // Find corresponding handler by transactionId
            const auto it = m_requestsInProgress.find( message.header.transactionId );
            if( it == m_requestsInProgress.end() ) {
                // TODO: log this error in addition to assert
                Q_ASSERT( false );
            }

            // Use and erase the handler (transactionId is unique per transaction
            it->second( SystemError::noError, std::move( message ) );
            m_requestsInProgress.erase( it );
            return;
        }

        case MessageClass::indication:
            m_indicationHandler( std::move( message ) );
            return;

        default:
            Q_ASSERT( false );
            return;
    }
}

} // namespase stun
} // namespase nx
