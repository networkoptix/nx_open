#include "async_client.h"

#include "common/common_globals.h"
#include "utils/common/guard.h"
#include "utils/common/log.h"

namespace nx {
namespace stun {

const AsyncClient::Timeouts AsyncClient::DEFAULT_TIMEOUTS = { 3000, 3000 };

AsyncClient::AsyncClient( const SocketAddress& endpoint, bool useSsl, Timeouts timeouts )
    : m_endpoint( endpoint )
    , m_useSsl( useSsl )
    , m_timeouts( timeouts )
    , m_state( State::disconnected )
{
}

AsyncClient::~AsyncClient()
{
    {
        QnMutexLocker lock( &m_mutex );
        if( m_state == State::disconnected )
            return;

        m_state = State::terminated;
    }

    m_baseConnection.reset();
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
        case State::disconnected:
            return openConnectionImpl( &lock );

        case State::connecting:
            // dispatchRequestsInQueue will be called in onConnectionComplete
            return true;

        case State::connected:
            dispatchRequestsInQueue( &lock );
            return true;

        default:
            Q_ASSERT_X( false, Q_FUNC_INFO, "m_state is invalid" );
            NX_LOG( lit( "%1 m_state has invalid value: %2" )
                    .arg( QString::fromUtf8( Q_FUNC_INFO ) )
                    .arg( static_cast< int >( m_state ) ), cl_logERROR );
            return false;
    };
}

void AsyncClient::closeConnection( BaseConnectionType* connection )
{
    Q_ASSERT( !m_baseConnection || connection == m_baseConnection.get() );

    ConnectionHandler disconnectHandler;
    {
        QnMutexLocker lock( &m_mutex );
        std::swap( disconnectHandler, m_disconnectHandler );

        closeConnectionImpl( &lock, SystemError::getLastOSErrorCode() );
    }

    if( disconnectHandler )
        disconnectHandler( SystemError::connectionReset );

    m_baseConnection = nullptr;
}

boost::optional< QString >
    AsyncClient::hasError( SystemError::ErrorCode code, const Message& message )
{
    if( code != SystemError::noError )
        return lit( "System error %1: %2" )
            .arg( code ).arg( SystemError::toString( code ) );

    if( message.header.messageClass != MessageClass::successResponse )
    {
        if( const auto err = message.getAttribute< attrs::ErrorDescription >() )
            return lit( "STUN error %1: %2" )
                .arg( err->code ).arg( QString::fromUtf8( err->reason ) );
        else
            return lit( "STUN error without ErrorDescription" );
    }

    return boost::none;
}

bool AsyncClient::openConnectionImpl( QnMutexLockerBase* /*lock*/ )
{
    switch( m_state )
    {
        case State::disconnected: {
            // estabilish new connection
            m_connectingSocket = SocketFactory::createStreamSocket(
                        m_useSsl, SocketFactory::nttDisabled );

            auto onComplete = [ this ]( SystemError::ErrorCode code )
                { onConnectionComplete( code ); };

            if( !m_connectingSocket->setNonBlockingMode( true ) ||
                !m_connectingSocket->setSendTimeout( m_timeouts.send ) ||
                !m_connectingSocket->setRecvTimeout( m_timeouts.recv ) ||
                !m_connectingSocket->connectAsync( m_endpoint,
                                                   std::move(onComplete) ) )
            {
                m_connectingSocket = nullptr;
                return false;
            }

            m_state = State::connecting;
            return true;
        }

        case State::connected:
            m_baseConnection->socket()->post( std::bind(
                std::move( m_connectHandler ), SystemError::noError ) );
            return true;

        case State::connecting:
            // m_connectHandler will be called in onConnectionComplete
            return true;

        default:
            Q_ASSERT_X( false, Q_FUNC_INFO, "m_state is invalid" );
            NX_LOG( lit( "%1 m_state has invalid value: %2" )
                    .arg( QString::fromUtf8( Q_FUNC_INFO ) )
                    .arg( static_cast< int >( m_state ) ), cl_logERROR );
            return false;
    }
}

void AsyncClient::closeConnectionImpl( QnMutexLockerBase* lock,
                                       SystemError::ErrorCode code )
{
    if( m_state != State::terminated )
        m_state = State::disconnected;

    std::list< std::pair< Message, RequestHandler > > requestQueue;
    std::swap( requestQueue, m_requestQueue );

    std::map< Buffer, RequestHandler > requestsInProgress;
    std::swap( requestsInProgress, m_requestsInProgress );

    lock->unlock();

    for( auto& req : requestsInProgress )   req.second( code, Message() );
    for( auto& req : requestQueue )         req.second( code, Message() );
}

void AsyncClient::dispatchRequestsInQueue( QnMutexLockerBase* lock )
{
    static_cast< void >( lock );
    while( !m_requestQueue.empty() )
    {
        auto request = std::move( m_requestQueue.front().first );
        auto handler = std::move( m_requestQueue.front().second );
        auto transactionId = request.header.transactionId;
        m_requestQueue.pop_front();

        m_baseConnection->sendMessage(
            std::move( request ),
            [ = ]( SystemError::ErrorCode code ) mutable
            {
                if( code != SystemError::noError )
                    return handler( code, Message() );

                QnMutexLocker lock( &m_mutex );
                if ( !m_requestsInProgress.emplace(
                         transactionId, std::move( handler ) ).second )
                {
                    Q_ASSERT_X( false, Q_FUNC_INFO, "transactionId is not unique" );
                    NX_LOG( lit( "%1 transactionId is not unique: %2" )
                            .arg( QString::fromUtf8( Q_FUNC_INFO ) )
                            .arg( QString::fromUtf8( transactionId.toHex() ) ),
                            cl_logERROR );
                }

                dispatchRequestsInQueue( &lock );
            } );
    }
}

void AsyncClient::onConnectionComplete( SystemError::ErrorCode code)
{
    ConnectionHandler connectionHandler;
    Guard onReturn( [ & ]()
    {
        if( connectionHandler )
            connectionHandler( code );
    } );

    QnMutexLocker lock( &m_mutex );
    if( m_state == State::terminated )
        return;

    std::swap( connectionHandler, m_connectHandler);
    if( code != SystemError::noError )
    {
        closeConnectionImpl( &lock, code );
        m_connectingSocket = nullptr;
        return;
    }

    m_baseConnection.reset( new BaseConnectionType( this, std::move(m_connectingSocket) ) );
    m_baseConnection->setMessageHandler( 
        [ this ]( Message message ){ processMessage( std::move(message) ); } );

    if( !m_baseConnection->startReadingConnection() )
    {
        code = SystemError::getLastOSErrorCode();
        m_state = State::disconnected;
        return;
    }

    m_state = State::connected;
    dispatchRequestsInQueue( &lock );
}

void AsyncClient::processMessage( Message message )
{
    QnMutexLocker lock( &m_mutex );
    if( m_state == State::terminated )
        return;

    switch( message.header.messageClass )
    {
        case MessageClass::request:
            Q_ASSERT_X( false, Q_FUNC_INFO, "client does not support requests" );
            NX_LOG( lit( "%1 client does not support requests" )
                    .arg( QString::fromUtf8( Q_FUNC_INFO ) ), cl_logERROR );
            return;

        case MessageClass::errorResponse:
        case MessageClass::successResponse:
        {
            // find corresponding handler by transactionId
            const auto it = m_requestsInProgress.find( message.header.transactionId );
            if( it == m_requestsInProgress.end() )
            {
                Q_ASSERT_X( false, Q_FUNC_INFO, "unexpected transactionId" );
                NX_LOG( lit( "%1 unexpected transactionId: %2" )
                        .arg( QString::fromUtf8( Q_FUNC_INFO ) )
                        .arg( QString::fromUtf8( message.header.transactionId.toHex() ) ),
                        cl_logERROR );
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

        default:
            Q_ASSERT_X( false, Q_FUNC_INFO, "messageClass is invalid" );
            NX_LOG( lit( "%1 messageClass has invalid value: %2" )
                    .arg( QString::fromUtf8( Q_FUNC_INFO ) )
                    .arg( static_cast< int >( message.header.messageClass ) ),
                    cl_logERROR );
            return;
    }
}

} // namespase stun
} // namespase nx
