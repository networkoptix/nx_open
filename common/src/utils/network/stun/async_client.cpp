#include "async_client.h"

#include "utils/common/guard.h"

namespace nx {
namespace stun {

AsyncClient::AsyncClient( const SocketAddress& endpoint, bool useSsl )
    : m_endpoint( endpoint )
    , m_useSsl( useSsl )
    , m_state( State::NOT_CONNECTED )
{
}

AsyncClient::~AsyncClient()
{
    {
        QnMutexLocker lock( &m_mutex );
        if( m_state == State::NOT_CONNECTED )
            return;
    }

    m_baseConnection->closeConnection();
}

bool AsyncClient::openConnection( ConnectionHandler connectHandler,
                                  IndicationHandler indicationHandler,
                                  ConnectionHandler disconnectHandler )
{
    QnMutexLocker lock( &m_mutex );

    m_connectHandler = std::move( connectHandler );
    m_indicationHandler = std::move( indicationHandler );
    m_disconnectHandler = std::move( disconnectHandler );

    return openConnectionImpl( &lock );
}

bool AsyncClient::sendRequest( Message request, RequestHandler requestHandler )
{
    QnMutexLocker lock( &m_mutex );
    m_requestQueue.push_back( std::make_pair(
        std::move( request ), std::move( requestHandler ) ) );

    switch( m_state )
    {
        case State::NOT_CONNECTED:
            return openConnectionImpl( &lock );

        case State::CONNECTING:
            // dispatchRequestsInQueue will be called in onConnectionComplete
            return true;

        case State::CONNECTED:
            return dispatchRequestsInQueue( &lock );
    };

    Q_ASSERT_X( false, Q_FUNC_INFO, "m_state is corupted" );
    return false;
}

void AsyncClient::closeConnection( BaseConnectionType* connection )
{
    Q_ASSERT( connection == m_baseConnection.get() );

    ConnectionHandler disconnectHandler;
    std::list< RequestHandler > droppedRequests;
    {
        QnMutexLocker lock( &m_mutex );
        m_state = State::NOT_CONNECTED;

        for( auto& request : m_requestQueue )
            droppedRequests.push_back( std::move(request.second) );

        for( auto& request : m_requestsInProgress )
            droppedRequests.push_back( std::move(request.second) );

        m_requestQueue.clear();
        m_requestsInProgress.clear();

        std::swap( disconnectHandler, m_disconnectHandler );
        m_baseConnection = nullptr;
    }

    for( auto& handler : droppedRequests )
        handler( SystemError::connectionReset, Message() );

    if( disconnectHandler )
        disconnectHandler( SystemError::connectionReset );
}

bool AsyncClient::openConnectionImpl( QnMutexLockerBase* /*lock*/ )
{
    switch( m_state )
    {
        case State::NOT_CONNECTED: {
            // estabilish new connection
            const auto socket = SocketFactory::createStreamSocket(
                        m_useSsl, SocketFactory::nttDisabled );
            socket->setNonBlockingMode( true );

            m_state = State::CONNECTING;
            return socket->connectAsync(
                m_endpoint, [ = ]( SystemError::ErrorCode code ) {
                    onConnectionComplete( SocketPtr(socket), code );
                } );
        }

        case State::CONNECTED:
            m_baseConnection->executeInIoThread( std::bind(
                std::move( m_connectHandler ), SystemError::noError ) );
            return true;

        case State::CONNECTING:
            // m_connectHandler will be called in onConnectionComplete
            return true;
    }

    Q_ASSERT_X( false, Q_FUNC_INFO, "m_state is corupted" );
    return false;
}

bool AsyncClient::dispatchRequestsInQueue( QnMutexLockerBase* lock )
{
    while( !m_requestQueue.empty() )
    {
        auto request = std::move( m_requestQueue.front().first );
        auto handler = std::move( m_requestQueue.front().second );
        auto transactionId = request.header.transactionId;
        m_requestQueue.pop_front();

        const auto ret = m_baseConnection->sendMessage(
            std::move( request ),
            [ = ]( SystemError::ErrorCode code )
            {
                if( code != SystemError::noError )
                    return handler( code, Message() );

                QnMutexLocker lock( &m_mutex );
                const auto ret = m_requestsInProgress.insert( std::make_pair(
                    transactionId, std::move( handler ) ) );

                // TODO: NX_LOG
                Q_ASSERT_X( ret.second, Q_FUNC_INFO, "transactionId must be unique" );
            } );

        if( !ret )
        {
            lock->unlock();
            handler( SystemError::notConnected, Message() );
            return false;
        }
    }

    return true;
}

void AsyncClient::onConnectionComplete(
    SocketPtr socket, SystemError::ErrorCode code)
{
    ConnectionHandler connectionHandler;
    Guard onReturn( [ & ]()
    {
        if( connectionHandler )
            connectionHandler( code );
    } );

    QnMutexLocker lock( &m_mutex );
    std::swap( connectionHandler, m_connectHandler);

    if( code != SystemError::noError )
    {
        m_state = State::NOT_CONNECTED;
        return;
    }

    m_baseConnection.reset( new BaseConnectionType( this, std::move(socket) ) );
    m_baseConnection->setMessageHandler( 
        [ this ]( Message message ){ processMessage( std::move(message) ); } );

    if( !m_baseConnection->startReadingConnection() )
    {
        code = SystemError::notConnected;
        m_state = State::NOT_CONNECTED;
        return;
    }

    m_state = State::CONNECTED;
    dispatchRequestsInQueue( &lock );
}

void AsyncClient::processMessage( Message message )
{
    QnMutexLocker lock( &m_mutex );
    switch( message.header.messageClass )
    {
        case MessageClass::request:
            // TODO: NX_LOG
            Q_ASSERT_X( false, Q_FUNC_INFO, "client does not support requests" );
            return;

        case MessageClass::errorResponse:
        case MessageClass::successResponse:
        {
            // find corresponding handler by transactionId
            const auto it = m_requestsInProgress.find( message.header.transactionId );
            if( it == m_requestsInProgress.end() ) {
                // TODO: NX_LOG
                Q_ASSERT_X( false, Q_FUNC_INFO, "unexpected transactionId" );
                return;
            }

            // use and erase the handler (transactionId is unique per transaction)
            RequestHandler handler( std::move( it->second ) );
            m_requestsInProgress.erase( it );

            lock.unlock();
            handler( SystemError::noError, std::move( message ) );
            return;
        }

        case MessageClass::indication:
            if( m_indicationHandler )
            {
                IndicationHandler handler( m_indicationHandler );

                lock.unlock();
                handler( std::move( message ) );
            }
            return;
    }

    Q_ASSERT_X( false, Q_FUNC_INFO, "message.header.messageClass is corupted" );
    return;
}

} // namespase stun
} // namespase nx
