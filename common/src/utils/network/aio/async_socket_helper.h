/**********************************************************
* 15 aug 2014
* a.kolesnikov
***********************************************************/

#ifndef ASYNC_SOCKET_HELPER_H
#define ASYNC_SOCKET_HELPER_H

#include <atomic>
#include <functional>
#include <future>

#include <QtCore/QThread>

#include <utils/thread/mutex.h>
#include <utils/thread/wait_condition.h>

#include "aioservice.h"
#include "../abstract_socket.h"
#include "../host_address_resolver.h"

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

    void post( std::function<void()>&& handler )
    {
        if( m_socket->impl()->terminated.load( std::memory_order_relaxed ) )
            return;

        aio::AIOService::instance()->post( m_socket, std::move(handler) );
    }

    void dispatch( std::function<void()>&& handler )
    {
        if( m_socket->impl()->terminated.load( std::memory_order_relaxed ) )
            return;

        aio::AIOService::instance()->dispatch( m_socket, std::move(handler) );
    }

    //!This call stops async I/O on socket and it can never be resumed!
    void terminateAsyncIO()
    {
        m_socket->impl()->terminated.store( true, std::memory_order_relaxed );
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
        int _ipVersion)
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
        m_asyncSendIssued( false ),
        m_ipVersion( _ipVersion )
    {
        assert( this->m_socket );
        assert( m_abstractSocketPtr );
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
        this->terminateAsyncIO();
        //TODO #ak what's the difference of this method from cancelAsyncIO( aio::etNone ) ?

        //cancel ongoing async I/O. Doing this only if AsyncSocketImplHelper::eventTriggered is down the stack
        if( this->m_socket->impl()->aioThread.load() == QThread::currentThread() )
        {
            HostAddressResolver::instance()->cancel( this, true );    //TODO #ak must not block here!

            if( m_connectSendHandlerTerminatedFlag )
                aio::AIOService::instance()->removeFromWatch( this->m_socket, aio::etWrite );
            if( m_recvHandlerTerminatedFlag )
                aio::AIOService::instance()->removeFromWatch( this->m_socket, aio::etRead );
            if( m_timerHandlerTerminatedFlag )
                aio::AIOService::instance()->removeFromWatch( this->m_socket, aio::etTimedOut );
            //TODO #ak not sure whether this call always necessary
            aio::AIOService::instance()->cancelPostedCalls( this->m_socket );
        }
        else
        {
            if (HostAddressResolver::instance()->isRequestIDKnown(this))
            {
                Q_ASSERT_X(
                    false, Q_FUNC_INFO,
                    "You MUST cancel running async socket operation before "
                    "deleting socket if you delete socket from non-aio thread (1)");

                // SIGSEGV in release to make usefull crash dump!
                *reinterpret_cast<volatile int*>(0) = 1;
            }

            if (aio::AIOService::instance()->isSocketBeingWatched(this->m_socket))
            {
                Q_ASSERT_X(
                    false, Q_FUNC_INFO,
                    "You MUST cancel running async socket operation before "
                    "deleting socket if you delete socket from non-aio thread (2)");

                // SIGSEGV in release to make usefull crash dump!
                *reinterpret_cast<volatile int*>(0) = 2;
            }
        }
    }

    bool connectAsyncImpl( const SocketAddress& addr, std::function<void( SystemError::ErrorCode )>&& handler )
    {
        if( this->m_socket->impl()->terminated.load( std::memory_order_relaxed ) )
        {
            //socket has been terminated, no async call possible.
            //Returning true to trick calling party: let it think everything OK and
            //finish its completion handler correctly.
            //TODO #ak is it really ok to trick someone?
            return true;
        }

        Q_ASSERT( isNonBlockingMode() ); //< NX_ASSERT/NX_CRITICAL by FlagConfig in 3.0.0
        m_connectHandler = std::move( handler );

        //TODO #ak if address is already resolved (or is an ip address) better make synchronous non-blocking call
        //NOTE: socket cannot be read from/written to if not connected yet. TODO #ak check that with assert

        if( HostAddressResolver::isAddressResolved(addr.address) )
            return startAsyncConnect( addr );

        //resolving address, if required
        return HostAddressResolver::instance()->resolveAddressAsync(
            addr.address,
            [this, addr]( SystemError::ErrorCode errorCode, const HostAddress& resolvedAddress )
            {
                //always calling m_connectHandler within aio thread socket is bound to
                if( errorCode != SystemError::noError )
                    this->post( std::bind( m_connectHandler, errorCode ) );
                else if( !startAsyncConnect( SocketAddress(resolvedAddress, addr.port) ) )
                    this->post( std::bind( m_connectHandler, SystemError::getLastOSErrorCode() ) );
            },
            m_ipVersion,
            this );
    }

    bool recvAsyncImpl( nx::Buffer* const buf, std::function<void( SystemError::ErrorCode, size_t )>&& handler )
    {
        if( this->m_socket->impl()->terminated.load( std::memory_order_relaxed ) )
            return true;

        Q_ASSERT( isNonBlockingMode() ); //< NX_ASSERT/NX_CRITICAL by FlagConfig in 3.0.0
        static const int DEFAULT_RESERVE_SIZE = 4*1024;

        //this assert is not critical but is a signal of possible
            //ineffective memory usage in calling code
        Q_ASSERT( buf->capacity() > buf->size() );

        if( buf->capacity() == buf->size() )
            buf->reserve( DEFAULT_RESERVE_SIZE );
        m_recvBuffer = buf;
        m_recvHandler = std::move( handler );

        QnMutexLocker lk( aio::AIOService::instance()->mutex() );
        ++m_recvAsyncCallCounter;
        return aio::AIOService::instance()->watchSocketNonSafe( this->m_socket, aio::etRead, this );
    }

    bool sendAsyncImpl( const nx::Buffer& buf, std::function<void( SystemError::ErrorCode, size_t )>&& handler )
    {
        if( this->m_socket->impl()->terminated.load( std::memory_order_relaxed ) )
            return true;

        Q_ASSERT( isNonBlockingMode() ); //< NX_ASSERT/NX_CRITICAL by FlagConfig in 3.0.0
        assert( buf.size() > 0 );

        //assert does not stop all threads immediately, so performing segmentation fault
        if( m_asyncSendIssued.exchange(true) )
            *((int*)nullptr) = 12;

        m_sendBuffer = &buf;
        m_sendHandler = std::move( handler );
        m_sendBufPos = 0;

        QnMutexLocker lk( aio::AIOService::instance()->mutex() );
        ++m_connectSendAsyncCallCounter;
        return aio::AIOService::instance()->watchSocketNonSafe( this->m_socket, aio::etWrite, this );
    }

    bool registerTimerImpl( unsigned int timeoutMs, std::function<void()>&& handler )
    {
        if( this->m_socket->impl()->terminated.load( std::memory_order_relaxed ) )
            return true;

        m_timerHandler = std::move( handler );

        QnMutexLocker lk( aio::AIOService::instance()->mutex() );
        ++m_registerTimerCallCounter;
        return aio::AIOService::instance()->watchSocketNonSafe(
            this->m_socket,
            aio::etTimedOut,
            this,
            timeoutMs );
    }

    void cancelAsyncIO( const aio::EventType eventType, bool waitForRunningHandlerCompletion )
    {
        if (eventType == aio::etWrite || eventType == aio::etNone)
            HostAddressResolver::instance()->cancel(this, waitForRunningHandlerCompletion);

        if (waitForRunningHandlerCompletion)
        {
            QnMutex mtx;
            QnWaitCondition cond;
            bool done = false;
            aio::AIOService::instance()->dispatch(
                this->m_socket,
                [this, &mtx, &cond, &done, eventType]() {
                    stopPollingSocket(this->m_socket, eventType);
                    QnMutexLocker lk(&mtx);
                    done = true;
                    cond.wakeAll();
                });

            QnMutexLocker lk(&mtx);
            while(!done)
                cond.wait(lk.mutex());
        }
        else
        {
            aio::AIOService::instance()->dispatch(
                this->m_socket,
                [this, eventType]() {
                    stopPollingSocket(this->m_socket, eventType);
                } );
        }

        if (eventType == aio::etWrite || eventType == aio::etNone)
            HostAddressResolver::instance()->cancel(this, waitForRunningHandlerCompletion);

        std::atomic_thread_fence(std::memory_order_acquire);
        if (m_threadHandlerIsRunningIn.load(std::memory_order_relaxed) == QThread::currentThreadId())
        {
            //we are in aio thread, CommunicatingSocketImpl::eventTriggered is down the stack
            //  avoiding unnecessary removeFromWatch calls in eventTriggered
            if (eventType == aio::etRead || eventType == aio::etNone)
                ++m_recvAsyncCallCounter;
            if (eventType == aio::etWrite || eventType == aio::etNone)
                ++m_connectSendAsyncCallCounter;
            if (eventType == aio::etTimedOut || eventType == aio::etNone)
                ++m_registerTimerCallCounter;
        }
        else
        {
            //TODO #ak AIO engine does not support truely async cancellation yet
            assert(waitForRunningHandlerCompletion);
        }
    }

private:
    AbstractCommunicatingSocket* m_abstractSocketPtr;

    std::function<void( SystemError::ErrorCode )> m_connectHandler;
    size_t m_connectSendAsyncCallCounter;

    std::function<void( SystemError::ErrorCode, size_t )> m_recvHandler;
    nx::Buffer* m_recvBuffer;
    size_t m_recvAsyncCallCounter;

    std::function<void( SystemError::ErrorCode, size_t )> m_sendHandler;
    const nx::Buffer* m_sendBuffer;
    int m_sendBufPos;

    std::function<void()> m_timerHandler;
    size_t m_registerTimerCallCounter;

    bool* m_connectSendHandlerTerminatedFlag;
    bool* m_recvHandlerTerminatedFlag;
    bool* m_timerHandlerTerminatedFlag;

    std::atomic<Qt::HANDLE> m_threadHandlerIsRunningIn;
    std::atomic<bool> m_asyncSendIssued;
    const int m_ipVersion;

    bool isNonBlockingMode()
    {
        bool value;
        bool result = m_abstractSocketPtr->getNonBlockingMode(&value);
        if (result && value)
            return true;

        return false;
    }

    virtual void eventTriggered( SocketType* sock, aio::EventType eventType ) throw() override
    {
        assert( this->m_socket == sock );

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
            QnMutexLocker lk( aio::AIOService::instance()->mutex() );
            if( connectSendAsyncCallCounterBak == m_connectSendAsyncCallCounter )
                aio::AIOService::instance()->removeFromWatchNonSafe( sock, aio::etWrite );
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
            QnMutexLocker lk( aio::AIOService::instance()->mutex() );
            if( recvAsyncCallCounterBak == m_recvAsyncCallCounter )
                aio::AIOService::instance()->removeFromWatchNonSafe( sock, aio::etRead );
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
            QnMutexLocker lk( aio::AIOService::instance()->mutex() );
            if( connectSendAsyncCallCounterBak == m_connectSendAsyncCallCounter )
                aio::AIOService::instance()->removeFromWatchNonSafe( sock, aio::etWrite );
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
            QnMutexLocker lk( aio::AIOService::instance()->mutex() );
            if( registerTimerCallCounterBak == m_registerTimerCallCounter )
                aio::AIOService::instance()->removeFromWatchNonSafe( sock, aio::etTimedOut );
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

                assert( m_recvHandler );
                if( !isNonBlockingMode() )
                {
                    recvHandlerLocal( SystemError::invalidData, (size_t) -1 );
                    break;
                }

                //reading to buffer
                const auto bufSizeBak = m_recvBuffer->size();
                m_recvBuffer->resize( m_recvBuffer->capacity() );
                const int bytesRead = m_abstractSocketPtr->recv(
                    m_recvBuffer->data() + bufSizeBak,
                    m_recvBuffer->capacity() - bufSizeBak );
                if( bytesRead == -1 )
                {
                    m_recvBuffer->resize( bufSizeBak );
                    recvHandlerLocal( SystemError::getLastOSErrorCode(), (size_t)-1 );
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
                    connectHandlerLocal( SystemError::noError );
                }
                else
                {
                    //can send some bytes
                    assert( m_sendHandler );
                    if( !isNonBlockingMode() )
                    {
                        recvHandlerLocal( SystemError::invalidData, (size_t) -1 );
                        break;
                    }

                    std::unique_ptr<AsyncSocketImplHelper, decltype(__finally_write)> cleanupGuard( this, __finally_write );

                    const int bytesWritten = m_abstractSocketPtr->send(
                        m_sendBuffer->constData() + m_sendBufPos,
                        m_sendBuffer->size() - m_sendBufPos );
                    if( bytesWritten == -1 || bytesWritten == 0 )
                    {
                        sendHandlerLocal(
                            bytesWritten == 0 ? SystemError::connectionReset : SystemError::getLastOSErrorCode(),
                            m_sendBufPos );
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
                assert( m_recvHandler );

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
                    assert( m_sendHandler );
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
                assert( false );
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
        assert( isNonBlockingModeEnabled );
#endif
        if( !m_abstractSocketPtr->getSendTimeout( &sendTimeout ) )
            return false;
        //if( !m_abstractSocketPtr->connect( addr.address.toString(), addr.port, sendTimeout ) )
        if( !m_abstractSocketPtr->connect( resolvedAddress, sendTimeout ) )
            return false;

        assert( !m_asyncSendIssued.exchange( true ) );

        QnMutexLocker lk( aio::AIOService::instance()->mutex() );
        ++m_connectSendAsyncCallCounter;
        return aio::AIOService::instance()->watchSocketNonSafe( this->m_socket, aio::etWrite, this );
    }

    //!Call this from within aio thread only
    void stopPollingSocket(SocketType* sock, const aio::EventType eventType)
    {
        //TODO #ak move this method to aioservice?
        aio::AIOService::instance()->cancelPostedCalls(sock, true);
        if (eventType == aio::etNone || eventType == aio::etRead)
            aio::AIOService::instance()->removeFromWatch(sock, aio::etRead, true);
        if (eventType == aio::etNone || eventType == aio::etWrite)
            aio::AIOService::instance()->removeFromWatch(sock, aio::etWrite, true);
        if (eventType == aio::etNone || eventType == aio::etTimedOut)
            aio::AIOService::instance()->removeFromWatch(sock, aio::etTimedOut, true);
    }
};

#endif  //ASYNC_SOCKET_HELPER_H
