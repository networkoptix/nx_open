#include "async_client.h"

#include "common/common_globals.h"
#include "utils/common/guard.h"
#include <nx/utils/log/log.h>

namespace nx {
namespace stun {

const AbstractAsyncClient::Timeouts AbstractAsyncClient::DEFAULT_TIMEOUTS = { 3000, 3000, 60000 };

boost::optional< QString >
    AbstractAsyncClient::hasError(SystemError::ErrorCode code, const Message& message)
{
    if( code != SystemError::noError )
        return lit( "System error %1: %2" )
            .arg( code ).arg( SystemError::toString( code ) );

    if( message.header.messageClass != MessageClass::successResponse )
    {
        if( const auto err = message.getAttribute< attrs::ErrorDescription >() )
            return lit( "STUN error %1: %2" )
                .arg( err->getCode() ).arg( QString::fromUtf8( err->getString() ) );
        else
            return lit( "STUN error without ErrorDescription" );
    }

    return boost::none;
}

AsyncClient::AsyncClient(Timeouts timeouts):
    m_timeouts(timeouts),
    m_useSsl(false),
    m_state(State::disconnected)
{
}

AsyncClient::~AsyncClient()
{
    std::unique_ptr< AbstractStreamSocket > connectingSocket;
    std::unique_ptr< BaseConnectionType > baseConnection;
    {
        QnMutexLocker lock( &m_mutex );
        connectingSocket = std::move( m_connectingSocket );
		baseConnection = std::move( m_baseConnection );
        m_state = State::terminated;
    }

    if (baseConnection)
        baseConnection->pleaseStopSync();

    if( connectingSocket )
        connectingSocket->pleaseStopSync();

    m_timer.pleaseStopSync();
}

void AsyncClient::connect(SocketAddress endpoint, bool useSsl)
{
    QnMutexLocker lock( &m_mutex );
    m_endpoint = std::move( endpoint );
    m_useSsl = useSsl;
    openConnectionImpl( &lock );
}

bool AsyncClient::setIndicationHandler(int method, IndicationHandler handler)
{
    QnMutexLocker lock(&m_mutex);
    return m_indicationHandlers.emplace(method, std::move(handler)).second;
}

bool AsyncClient::ignoreIndications(int method)
{
    QnMutexLocker lock(&m_mutex);
    return m_indicationHandlers.erase(method);
}

void AsyncClient::sendRequest(Message request, RequestHandler handler)
{
    QnMutexLocker lock( &m_mutex );
    m_requestQueue.push_back( std::make_pair(
        std::move( request ), std::move( handler ) ) );

    switch( m_state )
    {
        case State::disconnected:
            return openConnectionImpl( &lock );

        case State::connecting:
            // dispatchRequestsInQueue will be called in onConnectionComplete
            return;

        case State::connected:
            dispatchRequestsInQueue( &lock );
            return;

        default:
            NX_ASSERT( false, Q_FUNC_INFO, "m_state is invalid" );
            NX_LOGX( lit( "m_state has invalid value: %1" )
                    .arg( static_cast< int >( m_state ) ), cl_logERROR );
            return;
    };
}

SocketAddress AsyncClient::localAddress() const
{
    QnMutexLocker lock(&m_mutex);
    return m_baseConnection
        ? m_baseConnection->socket()->getLocalAddress()
        : SocketAddress();
}

SocketAddress AsyncClient::remoteAddress() const
{
    QnMutexLocker lock(&m_mutex);
    if (m_endpoint)
        return *m_endpoint;

    return SocketAddress();
}

void AsyncClient::closeConnection(SystemError::ErrorCode errorCode)
{
    closeConnection(errorCode, nullptr);
}

void AsyncClient::closeConnection(
    SystemError::ErrorCode errorCode,
    BaseConnectionType* connection)
{
	std::unique_ptr< BaseConnectionType > baseConnection;
    {
        QnMutexLocker lock( &m_mutex );
        closeConnectionImpl( &lock, errorCode );
		baseConnection = std::move( m_baseConnection );
    }

    NX_ASSERT( !baseConnection || !connection ||
                connection == baseConnection.get(),
                Q_FUNC_INFO, "Incorrect closeConnection call" );
}

void AsyncClient::openConnectionImpl(QnMutexLockerBase* lock)
{
    if( !m_endpoint )
    {
        lock->unlock();
        m_timer.post(std::bind(
            &AsyncClient::onConnectionComplete, this, SystemError::notConnected));
        return;
    }

    switch( m_state )
    {
        case State::disconnected: {
            // estabilish new connection
            m_connectingSocket = 
                SocketFactory::createStreamSocket(
                    m_useSsl, SocketFactory::NatTraversalType::nttDisabled );
            m_connectingSocket->bindToAioThread(m_timer.getAioThread());

            auto onComplete = [ this ]( SystemError::ErrorCode code )
                { onConnectionComplete( code ); };

            if (!m_connectingSocket->setNonBlockingMode( true ) ||
                !m_connectingSocket->setSendTimeout( m_timeouts.send ) ||
                // TODO: #mu Use m_timeouts.recv on timer when request is sent
                !m_connectingSocket->setRecvTimeout( 0 ))
            {
                m_connectingSocket->post(
                    std::bind(onComplete, SystemError::getLastOSErrorCode()));
                return;
            }

            m_connectingSocket->connectAsync(*m_endpoint, std::move(onComplete));

            m_state = State::connecting;
            return;
        }

        case State::connected:
        case State::connecting:
        case State::terminated:
            return;

        default:
            NX_ASSERT( false, Q_FUNC_INFO, "m_state is invalid" );
            NX_LOGX( lit( "m_state has invalid value: %1" )
                     .arg( static_cast< int >( m_state ) ), cl_logERROR );
            return;
    }
}

void AsyncClient::closeConnectionImpl(
        QnMutexLockerBase* lock, SystemError::ErrorCode code)
{
    auto connectingSocket = std::move( m_connectingSocket );
    auto requestQueue = std::move( m_requestQueue );
    auto requestsInProgress = std::move( m_requestsInProgress );

    if( m_state != State::terminated )
        m_state = State::disconnected;

    lock->unlock();

    if( connectingSocket )
        connectingSocket->pleaseStopSync();

    for( const auto& req : requestsInProgress )   req.second( code, Message() );
    for( const auto& req : requestQueue )         req.second( code, Message() );

    lock->relock();

    if( m_state != State::terminated )
        m_timer.start(
            std::chrono::milliseconds(m_timeouts.reconnect),
            [this]
            {
                QnMutexLocker lock( &m_mutex );
                openConnectionImpl( &lock );
            });
}

void AsyncClient::dispatchRequestsInQueue(QnMutexLockerBase* lock)
{
    static_cast< void >( lock );
    while( !m_requestQueue.empty() )
    {
        auto request = std::move( m_requestQueue.front().first );
        auto handler = std::move( m_requestQueue.front().second );
        auto& tid = request.header.transactionId;

        m_requestQueue.pop_front();
        if ( !m_requestsInProgress.emplace(
                 tid, std::move( handler ) ).second )
        {
            NX_ASSERT( false, Q_FUNC_INFO,
                        "transactionId is not unique" );

            NX_LOGX( lit( "transactionId is not unique: %1" )
                    .arg( QString::fromUtf8( tid.toHex() ) ),
                    cl_logERROR );
        }
        else
        {
            m_baseConnection->sendMessage(
                std::move( request ),
                [ this ]( SystemError::ErrorCode code ) mutable
                {
                    QnMutexLocker lock( &m_mutex );
                    if( code != SystemError::noError )
                        dispatchRequestsInQueue( &lock );
                } );
        }
    }
}

void AsyncClient::onConnectionComplete(SystemError::ErrorCode code)
{
    QnMutexLocker lock( &m_mutex );
    if( m_state == State::terminated )
        return;

    if( code != SystemError::noError )
        return closeConnectionImpl( &lock, code );

    NX_ASSERT(!m_baseConnection);

    m_baseConnection.reset( new BaseConnectionType( this, std::move(m_connectingSocket) ) );
    m_baseConnection->setMessageHandler( 
        [ this ]( Message message ){ processMessage( std::move(message) ); } );

    m_baseConnection->startReadingConnection();

    m_state = State::connected;
    dispatchRequestsInQueue( &lock );
}

void AsyncClient::processMessage(Message message)
{
    QnMutexLocker lock( &m_mutex );
    if( m_state == State::terminated )
        return;

    switch( message.header.messageClass )
    {
        case MessageClass::request:
            NX_ASSERT( false, Q_FUNC_INFO, "client does not support requests" );
            NX_LOGX( lit( "Client does not support requests" ), cl_logERROR );
            return;

        case MessageClass::errorResponse:
        case MessageClass::successResponse:
        {
            // find corresponding handler by transactionId
            const auto it = m_requestsInProgress.find( message.header.transactionId );
            if( it == m_requestsInProgress.end() )
            {
                NX_ASSERT( false, Q_FUNC_INFO, "unexpected transactionId" );
                NX_LOGX( lit( "Unexpected transactionId: %2" )
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
        {
            auto it = m_indicationHandlers.find( message.header.method );
            if( it != m_indicationHandlers.end() )
            {
                auto handler = it->second;
                lock.unlock();
                handler( std::move( message ) );
            }
            else
            {
                NX_LOGX( lit( "Unexpected/unsupported indication: %2" )
                         .arg( message.header.method ),
                         cl_logWARNING );
            }
            return;
        }

        default:
            NX_ASSERT( false, Q_FUNC_INFO, "messageClass is invalid" );
            NX_LOGX( lit( "messageClass has invalid value: %1" )
                     .arg( static_cast< int >( message.header.messageClass ) ),
                     cl_logERROR );
            return;
    }
}

} // namespase stun
} // namespase nx
