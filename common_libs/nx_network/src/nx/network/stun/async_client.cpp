#include "async_client.h"

#include "common/common_globals.h"
#include "utils/common/guard.h"
#include <nx/utils/log/log.h>

namespace nx {
namespace stun {

const AbstractAsyncClient::Settings AbstractAsyncClient::kDefaultSettings;

AsyncClient::AsyncClient(Settings timeouts):
    m_settings(timeouts),
    m_useSsl(false),
    m_state(State::disconnected),
    m_timer(m_settings.reconnectPolicy)
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

bool AsyncClient::setIndicationHandler(
    int method, IndicationHandler handler, void* client)
{
    QnMutexLocker lock(&m_mutex);
    return m_indicationHandlers.emplace(
        method, std::make_pair(client, std::move(handler))).second;
}

void AsyncClient::addOnReconnectedHandler(
    ReconnectHandler handler, void* client)
{
    QnMutexLocker lock(&m_mutex);
    m_reconnectHandlers.emplace(client, std::move(handler));
}

void AsyncClient::sendRequest(
    Message request, RequestHandler handler, void* client)
{
    QnMutexLocker lock( &m_mutex );
    m_requestQueue.push_back( std::make_pair(
        std::move( request ), std::make_pair( client, std::move( handler ) ) ) );

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

template<typename Container>
void removeByClient(Container* container, void* client)
{
    // std::remove_if does not work for std::map and multimap O_o
    for (auto it = container->begin(); it != container->end(); )
    {
        if (it->second.first == client)
            it = container->erase(it);
        else
            ++it;
    }
}

void AsyncClient::cancelHandlers(void* client, utils::MoveOnlyFunc<void()> handler)
{
    NX_ASSERT(client);
    m_timer.dispatch(
        [this, client, handler = std::move(handler)]()
        {
            QnMutexLocker lock(&m_mutex);
            removeByClient(&m_requestQueue, client);
            removeByClient(&m_indicationHandlers, client);
            m_reconnectHandlers.erase(client);
            removeByClient(&m_requestsInProgress, client);
            NX_LOGX(lm("Cancel requests from %1").arg(client), cl_logDEBUG2);

            lock.unlock();
            handler();
        });
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

    if (baseConnection)
        baseConnection->pleaseStopSync();

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
                !m_connectingSocket->setSendTimeout( m_settings.sendTimeout ) ||
                // TODO: #mu Use m_timeouts.recvTimeout on timer when request is sent
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
    auto connectingSocket = std::move(m_connectingSocket);
    auto requestQueue = std::move(m_requestQueue);
    auto requestsInProgress = std::move(m_requestsInProgress);

    if (m_state != State::terminated)
        m_state = State::disconnected;

    lock->unlock();
    {
        if (connectingSocket)
            connectingSocket->pleaseStopSync();

        for (const auto& r: requestsInProgress) r.second.second(code, Message());
        for (const auto& r: requestQueue) r.second.second(code, Message());
    }
    lock->relock();

    if (m_state != State::terminated)
    {
        m_timer.scheduleNextTry(
            [this]
            {
                NX_LOGX(lm("Try to restore mediator connection..."), cl_logDEBUG1);
                QnMutexLocker lock(&m_mutex);
                openConnectionImpl(&lock);
            });
    }
}

void AsyncClient::dispatchRequestsInQueue(const QnMutexLockerBase* lock)
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

    m_timer.reset();
    NX_ASSERT(!m_baseConnection);
    NX_LOGX(lm("Connected to %1").str(*m_endpoint), cl_logDEBUG1);

    m_baseConnection.reset( new BaseConnectionType( this, std::move(m_connectingSocket) ) );
    m_baseConnection->setMessageHandler( 
        [ this ]( Message message ){ processMessage( std::move(message) ); } );

    m_baseConnection->startReadingConnection();

    m_state = State::connected;
    dispatchRequestsInQueue( &lock );

    const auto reconnectHandlers = m_reconnectHandlers;
    lock.unlock();
    for( const auto& handler: reconnectHandlers )
        handler.second();
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
                NX_LOGX(lm("Response to canceled request %1")
                    .arg(message.header.transactionId.toHex()), cl_logDEBUG2);
                return;
            }

            // use and erase the handler (transactionId is unique per transaction)
            RequestHandler handler( std::move( it->second.second ) );
            m_requestsInProgress.erase( it );

            lock.unlock();
            return handler( SystemError::noError, std::move( message ) );
        }

        case MessageClass::indication:
        {
            auto it = m_indicationHandlers.find( message.header.method );
            if( it != m_indicationHandlers.end() )
            {
                auto handler = it->second;
                lock.unlock();
                handler.second( std::move( message ) );
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
