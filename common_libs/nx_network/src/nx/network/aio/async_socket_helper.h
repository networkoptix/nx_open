/**********************************************************
* 15 aug 2014
* a.kolesnikov
***********************************************************/

#ifndef ASYNC_SOCKET_HELPER_H
#define ASYNC_SOCKET_HELPER_H

#include <atomic>
#include <functional>
#include <type_traits>

#include <QtCore/QThread>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include "../abstract_socket.h"
#include "../socket_global.h"


namespace nx {
namespace network {
namespace aio {

//TODO #ak come up with new name for this class or remove it
//TODO #ak also, some refactor needed to use AsyncSocketImplHelper with server socket
//TODO #ak move timers to AbstractSocket
template<class SocketType>
class BaseAsyncSocketImplHelper
{
public:
    BaseAsyncSocketImplHelper( SocketType* _socket )
    :
        m_socket( _socket )
    {
    }

    virtual ~BaseAsyncSocketImplHelper() {}

    void post(nx::utils::MoveOnlyFunc<void()> handler)
    {
        if( m_socket->impl()->terminated.load( std::memory_order_relaxed ) > 0 )
            return;

        nx::network::SocketGlobals::aioService().post( m_socket, std::move(handler) );
    }

    void dispatch(nx::utils::MoveOnlyFunc<void()> handler)
    {
        if( m_socket->impl()->terminated.load( std::memory_order_relaxed ) > 0 )
            return;

        nx::network::SocketGlobals::aioService().dispatch( m_socket, std::move(handler) );
    }
    
    //!This call stops async I/O on socket and it can never be resumed!
    void terminateAsyncIO()
    {
        ++m_socket->impl()->terminated;
    }

protected:
    SocketType* m_socket;
};

//!Implements asynchronous socket operations (connect, send, recv) and async cancellation routines
template<class SocketType>
class AsyncSocketImplHelper
:
    public BaseAsyncSocketImplHelper<SocketType>,
    public aio::AIOEventHandler<SocketType>
{
public:
    AsyncSocketImplHelper(
        SocketType* _socket,
        AbstractCommunicatingSocket* _abstractSocketPtr,
        bool _natTraversalEnabled )
    :
        BaseAsyncSocketImplHelper<SocketType>( _socket ),
        m_abstractSocketPtr( _abstractSocketPtr ),
        m_connectSendAsyncCallCounter( 0 ),
        m_recvBuffer( nullptr ),
        m_recvAsyncCallCounter( 0 ),
        m_sendBuffer( nullptr ),
        m_sendBufPos( 0 ),
        m_registerTimerCallCounter( 0 ),
        m_connectSendHandlerTerminatedFlag( nullptr ),
        m_recvHandlerTerminatedFlag( nullptr ),
        m_timerHandlerTerminatedFlag( nullptr ),
        m_threadHandlerIsRunningIn( NULL ),
        m_natTraversalEnabled( _natTraversalEnabled ),
        m_asyncSendIssued( false ),
        m_addressResolverIsInUse( false )
    {
        NX_ASSERT( this->m_socket );
        NX_ASSERT( m_abstractSocketPtr );
    }

    virtual ~AsyncSocketImplHelper()
    {
        //synchronization may be required here in case if recv handler and send/connect handler called simultaneously in different aio threads,
        //but even in described case no synchronization required, since before removing socket handler implementation MUST cancel ongoing 
        //async I/O and wait for completion. That is, wait for eventTriggered to return.
        //So, socket can only be removed from handler called in aio thread. So, eventTriggered is down the stack if m_*TerminatedFlag is not nullptr

        if( m_connectSendHandlerTerminatedFlag )
            *m_connectSendHandlerTerminatedFlag = true;
        if( m_recvHandlerTerminatedFlag )
            *m_recvHandlerTerminatedFlag = true;
        if( m_timerHandlerTerminatedFlag )
            *m_timerHandlerTerminatedFlag = true;
    }

    void terminate()
    {
        //this method is to cancel all asynchronous operations when called within socket's aio thread

        this->terminateAsyncIO();
        //TODO #ak what's the difference of this method from cancelAsyncIO( aio::etNone ) ?

        //cancel ongoing async I/O. Doing this only if AsyncSocketImplHelper::eventTriggered is down the stack
        if( this->m_socket->impl()->aioThread.load() == QThread::currentThread() )
        {
            nx::network::SocketGlobals::addressResolver().cancel( this );    //TODO #ak must not block here!

            nx::network::SocketGlobals::aioService().removeFromWatch( this->m_socket, aio::etWrite );
            nx::network::SocketGlobals::aioService().removeFromWatch( this->m_socket, aio::etRead );
            nx::network::SocketGlobals::aioService().removeFromWatch( this->m_socket, aio::etTimedOut );
            //TODO #ak not sure whether this call always necessary
            nx::network::SocketGlobals::aioService().cancelPostedCalls( this->m_socket );
        }
        else
        {
            // checking that socket is not registered in aio
            if ((m_addressResolverIsInUse.load() &&
                    nx::network::SocketGlobals::addressResolver()
                        .isRequestIdKnown(this)) ||
                nx::network::SocketGlobals::aioService()
                    .isSocketBeingWatched(this->m_socket))
            {
                NX_CRITICAL(
                    false,
                    "You MUST cancel running async socket operation before "
                    "deleting socket if you delete socket from non-aio thread");
            }
        }
    }

    void connectAsync(
        const SocketAddress& addr,
        nx::utils::MoveOnlyFunc<void( SystemError::ErrorCode )> handler )
    {
        //TODO with UDT we have to maintain pollset.add(socket), socket.connect, pollset.poll pipeline

        if( this->m_socket->impl()->terminated.load( std::memory_order_relaxed ) > 0 )
        {
            //socket has been terminated, no async call possible. 
            //Returning true to trick calling party: let it think everything OK and
            //finish its completion handler correctly.
            //TODO #ak is it really ok to trick someone?
            return;
        }

        unsigned int sendTimeout = 0;
#ifdef _DEBUG
        bool isNonBlockingModeEnabled = false;
#endif
        if (!m_abstractSocketPtr->getSendTimeout(&sendTimeout)
#ifdef _DEBUG
            || !m_abstractSocketPtr->getNonBlockingMode(&isNonBlockingModeEnabled)
#endif
            )
        {
            this->post(
                [handler = move(handler), 
                    errorCode = SystemError::getLastOSErrorCode()]() mutable
                { 
                    handler(errorCode);
                });
            return;
        }
#ifdef _DEBUG
        NX_ASSERT( isNonBlockingModeEnabled );
#endif

        m_connectHandler = std::move( handler );

        //TODO #ak if address is already resolved (or is an ip address) better make synchronous non-blocking call
        //NOTE: socket cannot be read from/written to if not connected yet. TODO #ak check that with NX_ASSERT

        if( addr.address.isResolved() )
        {
            if (!startAsyncConnect(addr))
                this->post(
                    [handler = move(m_connectHandler),
                        errorCode = SystemError::getLastOSErrorCode()]() mutable
                    {
                        handler(errorCode);
                    });
            return;
        }

        m_addressResolverIsInUse = true;
        nx::network::SocketGlobals::addressResolver().resolveAsync(
            addr.address,
            [this, addr]( SystemError::ErrorCode errorCode,
                          std::vector< nx::network::cloud::AddressEntry > addresses )
            {
                //always calling m_connectHandler within aio thread socket is bound to
                if( addresses.empty() )
                {
                    if (errorCode == SystemError::noError)
                        errorCode = SystemError::hostNotFound;
                    this->post(
                        [handler = move(m_connectHandler), errorCode]() mutable
                        {
                            handler(errorCode);
                        });
                    return;
                }


                // TODO: iterate over addresses and try to connect to each of them
                //       instead of just using the first one
                const auto& entry = addresses.front();
                switch( entry.type )
                {
                    case nx::network::cloud::AddressType::direct:
                    {
                        SocketAddress target( entry.host, addr.port );
                        for( const auto& attr : entry.attributes )
                            if( attr.type == nx::network::cloud::AddressAttributeType::port )
                                target.port = static_cast< quint16 >( attr.value );
                        if( startAsyncConnect( target ) )
                            return;
                        errorCode = SystemError::getLastOSErrorCode();
                        break;
                    }
                    default:
                        errorCode = SystemError::hostNotFound;
                        break;
                }

                this->post(
                    [handler = move(m_connectHandler), errorCode]() mutable
                    {
                        handler(errorCode);
                    });
            },
            m_natTraversalEnabled,
            this );
    }

    void readSomeAsync( nx::Buffer* const buf, std::function<void( SystemError::ErrorCode, size_t )>&& handler )
    {
        if( this->m_socket->impl()->terminated.load( std::memory_order_relaxed ) > 0 )
            return;

        static const int DEFAULT_RESERVE_SIZE = 4*1024;

        //this NX_ASSERT is not critical but is a signal of possible 
            //ineffective memory usage in calling code
        NX_ASSERT( buf->capacity() > buf->size() );

        if( buf->capacity() == buf->size() )
            buf->reserve( DEFAULT_RESERVE_SIZE );
        m_recvBuffer = buf;
        m_recvHandler = std::move( handler );

        QnMutexLocker lk( nx::network::SocketGlobals::aioService().mutex() );
        ++m_recvAsyncCallCounter;
        nx::network::SocketGlobals::aioService().watchSocketNonSafe( &lk, this->m_socket, aio::etRead, this );
    }

    void sendAsync( const nx::Buffer& buf, std::function<void( SystemError::ErrorCode, size_t )>&& handler )
    {
        if( this->m_socket->impl()->terminated.load( std::memory_order_relaxed ) > 0 )
            return;

        NX_ASSERT( buf.size() > 0 );

        //NX_ASSERT does not stop all threads immediately, so performing segmentation fault
        if( m_asyncSendIssued.exchange(true) )
            *((int*)nullptr) = 12;

        m_sendBuffer = &buf;
        m_sendHandler = std::move( handler );
        m_sendBufPos = 0;

        QnMutexLocker lk( nx::network::SocketGlobals::aioService().mutex() );
        ++m_connectSendAsyncCallCounter;
        nx::network::SocketGlobals::aioService().watchSocketNonSafe( &lk, this->m_socket, aio::etWrite, this );
    }

    void registerTimer(
        std::chrono::milliseconds timeoutMs,
        nx::utils::MoveOnlyFunc<void()> handler )
    {
        if( this->m_socket->impl()->terminated.load( std::memory_order_relaxed ) > 0 )
            return;

        m_timerHandler = std::move( handler );

        QnMutexLocker lk( nx::network::SocketGlobals::aioService().mutex() );
        ++m_registerTimerCallCounter;
        nx::network::SocketGlobals::aioService().watchSocketNonSafe(
            &lk,
            this->m_socket,
            aio::etTimedOut,
            this,
            timeoutMs );
    }

    void cancelIOAsync(
        const aio::EventType eventType,
        nx::utils::MoveOnlyFunc<void()> handler )
    {
        auto cancelImpl = [this, eventType, handler = move(handler)]() mutable
        {
            // cancelIOSync will be instant from socket's IO thread
            nx::network::SocketGlobals::aioService().dispatch(
                this->m_socket,
                [this, eventType, handler = move(handler)]() mutable
                {
                    cancelAsyncIOWhileInAioThread(eventType);
                    handler();
                });
        };

        if (eventType == aio::etWrite || eventType == aio::etNone)
            nx::network::SocketGlobals::addressResolver().cancel(
                this,
                std::move(cancelImpl));
        else
            cancelImpl();
    }

    void cancelIOSync(aio::EventType eventType)
    {
        if (this->m_socket->impl()->aioThread.load() == QThread::currentThread())
        {
            //TODO #ak we must cancel resolve task here, but must do it without blocking!
            cancelAsyncIOWhileInAioThread(eventType);
        }
        else
        {
            std::promise< bool > promise;
            cancelIOAsync(eventType, [&]() { promise.set_value(true); });
            promise.get_future().wait();
        }
    }

    void cancelAsyncIOWhileInAioThread(const aio::EventType eventType)
    {
        stopPollingSocket(eventType);
        std::atomic_thread_fence(std::memory_order_acquire);    //TODO #ak looks like it is not needed

        //we are in aio thread, CommunicatingSocketImpl::eventTriggered is down the stack
        //  avoiding unnecessary removeFromWatch calls in eventTriggered

        if (eventType == aio::etRead || eventType == aio::etNone)
            ++m_recvAsyncCallCounter;
        if (eventType == aio::etWrite || eventType == aio::etNone)
            ++m_connectSendAsyncCallCounter;
        if (eventType == aio::etTimedOut || eventType == aio::etNone)
            ++m_registerTimerCallCounter;
    }

private:
    AbstractCommunicatingSocket* m_abstractSocketPtr;

    nx::utils::MoveOnlyFunc<void( SystemError::ErrorCode )> m_connectHandler;
    size_t m_connectSendAsyncCallCounter;

    std::function<void( SystemError::ErrorCode, size_t )> m_recvHandler;
    nx::Buffer* m_recvBuffer;
    size_t m_recvAsyncCallCounter;

    std::function<void( SystemError::ErrorCode, size_t )> m_sendHandler;
    const nx::Buffer* m_sendBuffer;
    int m_sendBufPos;

    nx::utils::MoveOnlyFunc<void()> m_timerHandler;
    size_t m_registerTimerCallCounter;

    bool* m_connectSendHandlerTerminatedFlag;
    bool* m_recvHandlerTerminatedFlag;
    bool* m_timerHandlerTerminatedFlag;

    std::atomic<Qt::HANDLE> m_threadHandlerIsRunningIn;
    const bool m_natTraversalEnabled;
    std::atomic<bool> m_asyncSendIssued;
    std::atomic<bool> m_addressResolverIsInUse;

    virtual void eventTriggered( SocketType* sock, aio::EventType eventType ) throw() override
    {
        NX_ASSERT( this->m_socket == sock );

        //TODO #ak split this method to multiple methods

        //NOTE aio garantees that all events on socket are handled in same aio thread, so no synchronization is required
        bool terminated = false;    //set to true just before object destruction

        m_threadHandlerIsRunningIn.store( QThread::currentThreadId(), std::memory_order_relaxed );
        std::atomic_thread_fence( std::memory_order_release );
        auto __threadHandlerIsRunningInResetFunc = [this, &terminated]( AsyncSocketImplHelper* ){
            if( terminated )
                return;     //most likely, socket has been removed in handler
            m_threadHandlerIsRunningIn.store( nullptr, std::memory_order_relaxed );
            std::atomic_thread_fence( std::memory_order_release );
        };
        std::unique_ptr<AsyncSocketImplHelper, decltype(__threadHandlerIsRunningInResetFunc)>
            __threadHandlerIsRunningInReset( this, __threadHandlerIsRunningInResetFunc );

        const size_t connectSendAsyncCallCounterBak = m_connectSendAsyncCallCounter;
        auto connectHandlerLocal = [this, connectSendAsyncCallCounterBak, sock, &terminated]( SystemError::ErrorCode errorCode )
        {
            auto connectHandlerBak = std::move( m_connectHandler );
            m_asyncSendIssued = false;
            connectHandlerBak( errorCode );

            if( terminated )
                return;     //most likely, socket has been removed in handler
            QnMutexLocker lk( nx::network::SocketGlobals::aioService().mutex() );
            if( connectSendAsyncCallCounterBak == m_connectSendAsyncCallCounter )
                nx::network::SocketGlobals::aioService().removeFromWatchNonSafe( &lk, sock, aio::etWrite );
        };

        auto __finally_connect = [this, &terminated]( AsyncSocketImplHelper* /*pThis*/ )
        {
            if( terminated )
                return;     //most likely, socket has been removed in handler
            m_connectSendHandlerTerminatedFlag = nullptr;
        };

        const size_t recvAsyncCallCounterBak = m_recvAsyncCallCounter;
        auto recvHandlerLocal = [this, recvAsyncCallCounterBak, sock, &terminated]( SystemError::ErrorCode errorCode, size_t bytesRead )
        {
            m_recvBuffer = nullptr;
            auto recvHandlerBak = std::move( m_recvHandler );
            recvHandlerBak( errorCode, bytesRead );

            if( terminated )
                return;     //most likely, socket has been removed in handler
            QnMutexLocker lk( nx::network::SocketGlobals::aioService().mutex() );
            if( recvAsyncCallCounterBak == m_recvAsyncCallCounter )
                nx::network::SocketGlobals::aioService().removeFromWatchNonSafe( &lk, sock, aio::etRead );
        };

        auto __finally_read = [this, &terminated]( AsyncSocketImplHelper* /*pThis*/ )
        {
            if( terminated )
                return;     //most likely, socket has been removed in handler
            m_recvHandlerTerminatedFlag = nullptr;
        };

        auto sendHandlerLocal = [this, connectSendAsyncCallCounterBak, sock, &terminated]( SystemError::ErrorCode errorCode, size_t bytesSent )
        {
            m_sendBuffer = nullptr;
            m_sendBufPos = 0;
            auto sendHandlerBak = std::move( m_sendHandler );
            m_asyncSendIssued = false;
            sendHandlerBak( errorCode, bytesSent );

            if( terminated )
                return;     //most likely, socket has been removed in handler
            QnMutexLocker lk( nx::network::SocketGlobals::aioService().mutex() );
            if( connectSendAsyncCallCounterBak == m_connectSendAsyncCallCounter )
                nx::network::SocketGlobals::aioService().removeFromWatchNonSafe( &lk, sock, aio::etWrite );
        };

        auto __finally_write = [this, &terminated]( AsyncSocketImplHelper* /*pThis*/ )
        {
            if( terminated )
                return;     //most likely, socket has been removed in handler
            m_connectSendHandlerTerminatedFlag = nullptr;
        };

        const size_t registerTimerCallCounterBak = m_registerTimerCallCounter;
        auto timerHandlerLocal = [this, registerTimerCallCounterBak, sock, &terminated]()
        {
            auto timerHandlerBak = std::move( m_timerHandler );
            timerHandlerBak();

            if( terminated )
                return;     //most likely, socket has been removed in handler
            QnMutexLocker lk( nx::network::SocketGlobals::aioService().mutex() );
            if( registerTimerCallCounterBak == m_registerTimerCallCounter )
                nx::network::SocketGlobals::aioService().removeFromWatchNonSafe( &lk, sock, aio::etTimedOut );
        };

        auto __finally_timer = [this, &terminated]( AsyncSocketImplHelper* /*pThis*/ )
        {
            if( terminated )
                return;     //most likely, socket has been removed in handler
            m_timerHandlerTerminatedFlag = nullptr;
        };

        switch( eventType )
        {
            case aio::etRead:
            {
                m_recvHandlerTerminatedFlag = &terminated;

                std::unique_ptr<AsyncSocketImplHelper, decltype(__finally_read)> cleanupGuard( this, __finally_read );

                NX_ASSERT( m_recvHandler );

                //reading to buffer
                const auto bufSizeBak = m_recvBuffer->size();
                m_recvBuffer->resize( m_recvBuffer->capacity() );
                const int bytesRead = m_abstractSocketPtr->recv(
                    m_recvBuffer->data() + bufSizeBak,
                    m_recvBuffer->capacity() - bufSizeBak );
                if( bytesRead == -1 )
                {
                    const auto lastError = SystemError::getLastOSErrorCode();
                    m_recvBuffer->resize( bufSizeBak );

                    if( lastError == SystemError::again ||
                        lastError == SystemError::wouldBlock )
                    {
                        return; // false positive
                    }

                    recvHandlerLocal( lastError, (size_t)-1 );
                }
                else
                {
                    m_recvBuffer->resize( bufSizeBak + bytesRead );   //shrinking buffer
                    recvHandlerLocal( SystemError::noError, bytesRead );
                }
                break;
            }

            case aio::etWrite:
            {
                m_connectSendHandlerTerminatedFlag = &terminated;

                if( m_connectHandler )
                {
                    //async connect. If we are here than connect succeeded
                    std::unique_ptr<AsyncSocketImplHelper, decltype(__finally_connect)> cleanupGuard( this, __finally_connect );
                    connectHandlerLocal(SystemError::noError);
                }
                else
                {
                    //can send some bytes
                    NX_ASSERT( m_sendHandler );

                    std::unique_ptr<AsyncSocketImplHelper, decltype(__finally_write)> cleanupGuard( this, __finally_write );

                    const int bytesWritten = m_abstractSocketPtr->send(
                        m_sendBuffer->constData() + m_sendBufPos,
                        m_sendBuffer->size() - m_sendBufPos );
                    if( bytesWritten == -1 )
                    {
                        const auto lastError = SystemError::getLastOSErrorCode();
                        if( lastError == SystemError::again ||
                            lastError == SystemError::wouldBlock )
                        {
                            return; // false positive
                        }

                        sendHandlerLocal( lastError, m_sendBufPos );
                    }
                    else
                    if( bytesWritten == 0 )
                    {
                        sendHandlerLocal( SystemError::connectionReset, m_sendBufPos );
                    }
                    else
                    {
                        m_sendBufPos += bytesWritten;
                        if( m_sendBufPos == m_sendBuffer->size() )
                            sendHandlerLocal( SystemError::noError, m_sendBufPos );
                    }
                }
                break;
            }

            case aio::etTimedOut:
            {
                //timer on socket (not read/write timeout, but some timer)
                m_timerHandlerTerminatedFlag = &terminated;

                if( m_timerHandler )
                {
                    //async connect. If we are here than connect succeeded
                    std::unique_ptr<AsyncSocketImplHelper, decltype(__finally_timer)> cleanupGuard( this, __finally_timer );
                    timerHandlerLocal();
                }
                break;
            }

            case aio::etReadTimedOut:
            {
                NX_ASSERT( m_recvHandler );

                m_recvHandlerTerminatedFlag = &terminated;
                std::unique_ptr<AsyncSocketImplHelper, decltype(__finally_read)> cleanupGuard( this, __finally_read );
                recvHandlerLocal( SystemError::timedOut, (size_t)-1 );
                break;
            }

            case aio::etWriteTimedOut:
            {
                m_connectSendHandlerTerminatedFlag = &terminated;

                if( m_connectHandler )
                {
                    std::unique_ptr<AsyncSocketImplHelper, decltype(__finally_connect)> cleanupGuard( this, __finally_connect );
                    connectHandlerLocal( SystemError::timedOut );
                }
                else
                {
                    NX_ASSERT( m_sendHandler );
                    std::unique_ptr<AsyncSocketImplHelper, decltype(__finally_write)> cleanupGuard( this, __finally_write );
                    sendHandlerLocal( SystemError::timedOut, (size_t)-1 );
                }
                break;
            }

            case aio::etError:
            {
                //TODO #ak distinguish read and write
                SystemError::ErrorCode sockErrorCode = SystemError::notConnected;
                if( !this->m_socket->getLastError( &sockErrorCode ) )
                    sockErrorCode = SystemError::notConnected;
                else if( sockErrorCode == SystemError::noError )
                    sockErrorCode = SystemError::notConnected;  //MUST report some error
                if( m_connectHandler )
                {
                    m_connectSendHandlerTerminatedFlag = &terminated;
                    std::unique_ptr<AsyncSocketImplHelper, decltype(__finally_connect)> cleanupGuard( this, __finally_connect );
                    connectHandlerLocal( sockErrorCode );
                }
                if( terminated )
                    return;
                if( m_recvHandler )
                {
                    m_recvHandlerTerminatedFlag = &terminated;
                    std::unique_ptr<AsyncSocketImplHelper, decltype(__finally_read)> cleanupGuard( this, __finally_read );
                    recvHandlerLocal( sockErrorCode, (size_t)-1 );
                }
                if( terminated )
                    return;
                if( m_sendHandler )
                {
                    m_connectSendHandlerTerminatedFlag = &terminated;
                    std::unique_ptr<AsyncSocketImplHelper, decltype(__finally_write)> cleanupGuard( this, __finally_write );
                    sendHandlerLocal( sockErrorCode, (size_t)-1 );
                }
                break;
            }

            default:
                NX_ASSERT( false );
                break;
        }
    }

    bool startAsyncConnect( const SocketAddress& resolvedAddress )
    {
        //connecting
        unsigned int sendTimeout = 0;
#ifdef _DEBUG
        bool isNonBlockingModeEnabled = false;
        if( !m_abstractSocketPtr->getNonBlockingMode( &isNonBlockingModeEnabled ) )
            return false;
        NX_ASSERT( isNonBlockingModeEnabled );
#endif

        NX_ASSERT( !m_asyncSendIssued.exchange( true ) );

        if( !m_abstractSocketPtr->getSendTimeout( &sendTimeout ) )
            return false;

        QnMutexLocker lk( nx::network::SocketGlobals::aioService().mutex() );
        ++m_connectSendAsyncCallCounter;
        nx::network::SocketGlobals::aioService().watchSocketNonSafe(
            &lk,
            this->m_socket,
            aio::etWrite,
            this,
            boost::none,
            [this, resolvedAddress, sendTimeout]()
            {
                m_abstractSocketPtr->connect( resolvedAddress, std::chrono::milliseconds(sendTimeout) );
            });    //to be called between pollset.add and pollset.poll
        return true;
    }

    //!Call this from within aio thread only
    void stopPollingSocket(const aio::EventType eventType)
    {
        //TODO #ak move this method to aioservice?
        if (eventType == aio::etNone)
            nx::network::SocketGlobals::aioService().cancelPostedCalls(this->m_socket, true);
        if (eventType == aio::etNone || eventType == aio::etRead)
            nx::network::SocketGlobals::aioService().removeFromWatch(this->m_socket, aio::etRead, true);
        if (eventType == aio::etNone || eventType == aio::etWrite)
            nx::network::SocketGlobals::aioService().removeFromWatch(this->m_socket, aio::etWrite, true);
        if (eventType == aio::etNone || eventType == aio::etTimedOut)
            nx::network::SocketGlobals::aioService().removeFromWatch(this->m_socket, aio::etTimedOut, true);
    }
};



template <class SocketType>
class AsyncServerSocketHelper
:
    public aio::AIOEventHandler<Pollable>
{
public:
    AsyncServerSocketHelper(SocketType* _sock)
    :
        m_sock(_sock),
        m_threadHandlerIsRunningIn(NULL),
        m_acceptAsyncCallCount(0),
        m_terminatedFlagPtr(nullptr)
    {
    }

    ~AsyncServerSocketHelper()
    {
        //NOTE removing socket not from completion handler while completion handler is running in another thread is undefined behavour, so no synchronization here
        if( m_terminatedFlagPtr )
            *m_terminatedFlagPtr = true;
    }

    virtual void eventTriggered(Pollable* sock, aio::EventType eventType) throw() override
    {
        NX_ASSERT(m_acceptHandler);

        bool terminated = false;    //set to true just before object destruction
        m_terminatedFlagPtr = &terminated;
        m_threadHandlerIsRunningIn.store( QThread::currentThreadId(), std::memory_order_release );
        const int acceptAsyncCallCountBak = m_acceptAsyncCallCount;
        auto acceptHandlerBak = std::move( m_acceptHandler );

        auto acceptHandlerBakLocal = 
            [this, sock, acceptHandlerBak = std::move(acceptHandlerBak),
                    &terminated, acceptAsyncCallCountBak](
                SystemError::ErrorCode errorCode,
                AbstractStreamSocket* newConnection )
            {
                acceptHandlerBak( errorCode, newConnection );
                if( terminated )
                    return;
                //if asyncAccept has been called from onNewConnection, no need to call removeFromWatch
                QnMutexLocker lk( nx::network::SocketGlobals::aioService().mutex() );
                if( m_acceptAsyncCallCount == acceptAsyncCallCountBak )
                    nx::network::SocketGlobals::aioService().removeFromWatchNonSafe(&lk, sock, aio::etRead);
                m_threadHandlerIsRunningIn.store( nullptr, std::memory_order_release );
                m_terminatedFlagPtr = nullptr;
            };

        switch( eventType )
        {
            case aio::etRead:
            {
                //accepting socket
                AbstractStreamSocket* newSocket = m_sock->systemAccept();
                acceptHandlerBakLocal(
                    newSocket != nullptr ? SystemError::noError : SystemError::getLastOSErrorCode(),
                    newSocket);
                break;
            }

            case aio::etReadTimedOut:
                acceptHandlerBakLocal(SystemError::timedOut, nullptr);
                break;

            case aio::etError:
            {
                SystemError::ErrorCode errorCode = SystemError::noError;
                sock->getLastError(&errorCode);
                acceptHandlerBakLocal(errorCode, nullptr);
                break;
            }

            default:
                NX_ASSERT(false);
                break;
        }
    }

    void acceptAsync(
        nx::utils::MoveOnlyFunc<void(
            SystemError::ErrorCode,
            AbstractStreamSocket*)> handler)
    {
        m_acceptHandler = std::move(handler);

        QnMutexLocker lk( nx::network::SocketGlobals::aioService().mutex() );
        ++m_acceptAsyncCallCount;
        //TODO: #ak usually acceptAsync is called repeatedly. SHOULD avoid unneccessary watchSocket and removeFromWatch calls
        return nx::network::SocketGlobals::aioService().watchSocketNonSafe(&lk, m_sock, aio::etRead, this);
    }

    void cancelIOAsync(nx::utils::MoveOnlyFunc< void() > handler)
    {
        nx::network::SocketGlobals::aioService().dispatch(
            this->m_sock,
            [this, handler = move(handler)]() mutable
            {
                nx::network::SocketGlobals::aioService().removeFromWatch(
                    m_sock, aio::etRead, true);
                ++m_acceptAsyncCallCount;
                handler();
            });
    }

    void cancelIOSync()
    {
        //TODO #ak promise not needed if we are already in aio thread
        std::promise< bool > promise;
        cancelIOAsync( [ & ](){ promise.set_value( true ); } );
        promise.get_future().wait();
    }

private:
    SocketType* m_sock;
    std::atomic<Qt::HANDLE> m_threadHandlerIsRunningIn;
    nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode,
        AbstractStreamSocket*)> m_acceptHandler;
    std::atomic<int> m_acceptAsyncCallCount;
    bool* m_terminatedFlagPtr;
};

}   //aio
}   //network
}   //nx

#endif  //ASYNC_SOCKET_HELPER_H
