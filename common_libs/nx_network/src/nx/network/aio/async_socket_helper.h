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

#include <nx/tool/thread/mutex.h>
#include <nx/tool/thread/wait_condition.h>

#include "../abstract_socket.h"
#include "../socket_global.h"


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
        if( m_socket->impl()->terminated.load( std::memory_order_relaxed ) > 0 )
            return;

        nx::SocketGlobals::aioService().post( m_socket, std::move(handler) );
    }

    void dispatch( std::function<void()>&& handler )
    {
        if( m_socket->impl()->terminated.load( std::memory_order_relaxed ) > 0 )
            return;

        nx::SocketGlobals::aioService().dispatch( m_socket, std::move(handler) );
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
        m_natTraversalEnabled( _natTraversalEnabled )
#ifdef _DEBUG
        , m_asyncSendIssued( false )
#endif
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
            nx::SocketGlobals::addressResolver().cancel( this, true );    //TODO #ak must not block here!

            if( m_connectSendHandlerTerminatedFlag )
                nx::SocketGlobals::aioService().removeFromWatch( this->m_socket, aio::etWrite );
            if( m_recvHandlerTerminatedFlag )
                nx::SocketGlobals::aioService().removeFromWatch( this->m_socket, aio::etRead );
            if( m_timerHandlerTerminatedFlag )
                nx::SocketGlobals::aioService().removeFromWatch( this->m_socket, aio::etTimedOut );
            //TODO #ak not sure whether this call always necessary
            nx::SocketGlobals::aioService().cancelPostedCalls( this->m_socket );
        }
        else
        {
            //checking that socket is not registered in aio
            Q_ASSERT_X(
                !nx::SocketGlobals::aioService().isSocketBeingWatched(this->m_socket),
                Q_FUNC_INFO,
                "You MUST cancel running async socket operation before deleting socket if you delete socket from non-aio thread (2)" );
        }
    }

    void connectAsyncImpl( const SocketAddress& addr, std::function<void( SystemError::ErrorCode )>&& handler )
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
        if (!m_abstractSocketPtr->getNonBlockingMode(&isNonBlockingModeEnabled))
        {
            this->post(std::bind(m_connectHandler, SystemError::getLastOSErrorCode()));
            return;
        }
        assert( isNonBlockingModeEnabled );
#endif
        if (!m_abstractSocketPtr->getSendTimeout(&sendTimeout))
        {
            this->post(std::bind(m_connectHandler, SystemError::getLastOSErrorCode()));
            return;
        }

        m_connectHandler = std::move( handler );

        //TODO #ak if address is already resolved (or is an ip address) better make synchronous non-blocking call
        //NOTE: socket cannot be read from/written to if not connected yet. TODO #ak check that with assert

        if( addr.address.isResolved() )
        {
            if (!startAsyncConnect(addr))
                this->post(std::bind(m_connectHandler, SystemError::getLastOSErrorCode()));
            return;
        }

        nx::SocketGlobals::addressResolver().resolveAsync(
            addr.address,
            [this, addr]( SystemError::ErrorCode code,
                          std::vector< nx::cc::AddressEntry > addresses )
            {
                //always calling m_connectHandler within aio thread socket is bound to
                if( addresses.empty() )
                {
                    if (code == SystemError::noError)
                        code = SystemError::hostNotFound;
                    this->post( std::bind( m_connectHandler, code ) );
                    return;
                }


                // TODO: iterate over addresses and try to connect to each of them
                //       instead of just using the first one
                const auto& entry = addresses.front();
                switch( entry.type )
                {
                    case nx::cc::AddressType::regular:
                    {
                        SocketAddress target( entry.host, addr.port );
                        for( const auto& attr : entry.attributes )
                            if( attr.type == nx::cc::AddressAttributeType::nxApiPort )
                                target.port = static_cast< quint16 >( attr.value );
                        if( !startAsyncConnect( target ) )
                            this->post( std::bind( m_connectHandler,
                                                   SystemError::getLastOSErrorCode() ) );
                        break;
                    }
                    default:
                        this->post( std::bind( m_connectHandler, SystemError::hostNotFound ) );
                };
            },
            m_natTraversalEnabled,
            this );
    }

    void recvAsyncImpl( nx::Buffer* const buf, std::function<void( SystemError::ErrorCode, size_t )>&& handler )
    {
        if( this->m_socket->impl()->terminated.load( std::memory_order_relaxed ) > 0 )
            return;

        static const int DEFAULT_RESERVE_SIZE = 4*1024;

        //this assert is not critical but is a signal of possible 
            //ineffective memory usage in calling code
        Q_ASSERT( buf->capacity() > buf->size() );

        if( buf->capacity() == buf->size() )
            buf->reserve( DEFAULT_RESERVE_SIZE );
        m_recvBuffer = buf;
        m_recvHandler = std::move( handler );

        QnMutexLocker lk( nx::SocketGlobals::aioService().mutex() );
        ++m_recvAsyncCallCounter;
        nx::SocketGlobals::aioService().watchSocketNonSafe( &lk, this->m_socket, aio::etRead, this );
    }

    void sendAsyncImpl( const nx::Buffer& buf, std::function<void( SystemError::ErrorCode, size_t )>&& handler )
    {
        if( this->m_socket->impl()->terminated.load( std::memory_order_relaxed ) > 0 )
            return;

        assert( buf.size() > 0 );

#ifdef _DEBUG
        //assert does not stop all threads immediately, so performing segmentation fault
        if( m_asyncSendIssued )
            *((int*)nullptr) = 12;
        assert( !m_asyncSendIssued );
        m_asyncSendIssued = true;
#endif

        m_sendBuffer = &buf;
        m_sendHandler = std::move( handler );
        m_sendBufPos = 0;

        QnMutexLocker lk( nx::SocketGlobals::aioService().mutex() );
        ++m_connectSendAsyncCallCounter;
        nx::SocketGlobals::aioService().watchSocketNonSafe( &lk, this->m_socket, aio::etWrite, this );
    }

    void registerTimerImpl( unsigned int timeoutMs, std::function<void()>&& handler )
    {
        if( this->m_socket->impl()->terminated.load( std::memory_order_relaxed ) > 0 )
            return;

        m_timerHandler = std::move( handler );

        QnMutexLocker lk( nx::SocketGlobals::aioService().mutex() );
        ++m_registerTimerCallCounter;
        nx::SocketGlobals::aioService().watchSocketNonSafe(
            &lk,
            this->m_socket,
            aio::etTimedOut,
            this,
            timeoutMs );
    }

    void cancelAsyncIO( const aio::EventType eventType, bool waitForRunningHandlerCompletion )
    {
        if (eventType == aio::etWrite || eventType == aio::etNone)
            nx::SocketGlobals::addressResolver().cancel(this, waitForRunningHandlerCompletion);

         ++this->m_socket->impl()->terminated;
        if (waitForRunningHandlerCompletion)
        {
            QnMutex mtx;
            QnWaitCondition cond;
            bool done = false;
            nx::SocketGlobals::aioService().dispatch(
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
            nx::SocketGlobals::aioService().dispatch(
                this->m_socket,
                [this, eventType]() {
                    stopPollingSocket(this->m_socket, eventType);
                } );
        }
        --this->m_socket->impl()->terminated;

        if (eventType == aio::etWrite || eventType == aio::etNone)
            nx::SocketGlobals::addressResolver().cancel(this, waitForRunningHandlerCompletion);

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
            //TODO #ak AIO engine does not support truly async cancellation yet
            assert( waitForRunningHandlerCompletion );
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
    const bool m_natTraversalEnabled;

#ifdef _DEBUG
    bool m_asyncSendIssued;
#endif

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
#ifdef _DEBUG
            m_asyncSendIssued = false;
#endif
            connectHandlerBak( errorCode );

            if( terminated )
                return;     //most likely, socket has been removed in handler
            QnMutexLocker lk( nx::SocketGlobals::aioService().mutex() );
            if( connectSendAsyncCallCounterBak == m_connectSendAsyncCallCounter )
                nx::SocketGlobals::aioService().removeFromWatchNonSafe( &lk, sock, aio::etWrite );
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
            QnMutexLocker lk( nx::SocketGlobals::aioService().mutex() );
            if( recvAsyncCallCounterBak == m_recvAsyncCallCounter )
                nx::SocketGlobals::aioService().removeFromWatchNonSafe( &lk, sock, aio::etRead );
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
#ifdef _DEBUG
            m_asyncSendIssued = false;
#endif
            sendHandlerBak( errorCode, bytesSent );

            if( terminated )
                return;     //most likely, socket has been removed in handler
            QnMutexLocker lk( nx::SocketGlobals::aioService().mutex() );
            if( connectSendAsyncCallCounterBak == m_connectSendAsyncCallCounter )
                nx::SocketGlobals::aioService().removeFromWatchNonSafe( &lk, sock, aio::etWrite );
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
            QnMutexLocker lk( nx::SocketGlobals::aioService().mutex() );
            if( registerTimerCallCounterBak == m_registerTimerCallCounter )
                nx::SocketGlobals::aioService().removeFromWatchNonSafe( &lk, sock, aio::etTimedOut );
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

#ifdef _DEBUG
        assert( !m_asyncSendIssued );
        m_asyncSendIssued = true;
#endif

        if( !m_abstractSocketPtr->getSendTimeout( &sendTimeout ) )
            return false;

        QnMutexLocker lk( nx::SocketGlobals::aioService().mutex() );
        ++m_connectSendAsyncCallCounter;
        nx::SocketGlobals::aioService().watchSocketNonSafe(
            &lk,
            this->m_socket,
            aio::etWrite,
            this,
            boost::optional<unsigned int>(),
            [this, resolvedAddress, sendTimeout](){ m_abstractSocketPtr->connect( resolvedAddress, sendTimeout ); } );    //to be called between pollset.add and pollset.poll
        return true;
    }

    //!Call this from within aio thread only
    void stopPollingSocket(SocketType* sock, const aio::EventType eventType)
    {
        //TODO #ak move this method to aioservice?
        nx::SocketGlobals::aioService().cancelPostedCalls(sock, true);
        if (eventType == aio::etNone || eventType == aio::etRead)
            nx::SocketGlobals::aioService().removeFromWatch(sock, aio::etRead, true);
        if (eventType == aio::etNone || eventType == aio::etWrite)
            nx::SocketGlobals::aioService().removeFromWatch(sock, aio::etWrite, true);
        if (eventType == aio::etNone || eventType == aio::etTimedOut)
            nx::SocketGlobals::aioService().removeFromWatch(sock, aio::etTimedOut, true);
    }
};



template <class SocketType>
class AsyncServerSocketHelper
:
    public aio::AIOEventHandler<SocketType>
{
public:
    AsyncServerSocketHelper(SocketType* _sock, AbstractStreamServerSocket* _abstractServerSocket)
    :
        m_sock(_sock),
        m_abstractServerSocket(_abstractServerSocket),
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

    void terminate()
    {
        cancelAsyncIO(true);
    }

    virtual void eventTriggered(SocketType* sock, aio::EventType eventType) throw() override
    {
        assert(m_acceptHandler);

        bool terminated = false;    //set to true just before object destruction
        m_terminatedFlagPtr = &terminated;
        m_threadHandlerIsRunningIn.store( QThread::currentThreadId(), std::memory_order_release );
        const int acceptAsyncCallCountBak = m_acceptAsyncCallCount;
        auto acceptHandlerBak = std::move( m_acceptHandler );

        auto acceptHandlerBakLocal = [this, sock, acceptHandlerBak, &terminated, acceptAsyncCallCountBak]( SystemError::ErrorCode errorCode, AbstractStreamSocket* newConnection ){
            acceptHandlerBak( errorCode, newConnection );
            if( terminated )
                return;
            //if asyncAccept has been called from onNewConnection, no need to call removeFromWatch
            QnMutexLocker lk( nx::SocketGlobals::aioService().mutex() );
            if( m_acceptAsyncCallCount == acceptAsyncCallCountBak )
                nx::SocketGlobals::aioService().removeFromWatchNonSafe(&lk, sock, aio::etRead);
            m_threadHandlerIsRunningIn.store( nullptr, std::memory_order_release );
        };

        switch( eventType )
        {
            case aio::etRead:
            {
                //accepting socket
                AbstractStreamSocket* newSocket = m_abstractServerSocket->accept();
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
                assert(false);
                break;
        }
    }

    void acceptAsync(std::function<void(SystemError::ErrorCode, AbstractStreamSocket*)>&& handler)
    {
        m_acceptHandler = std::move(handler);

        QnMutexLocker lk( nx::SocketGlobals::aioService().mutex() );
        ++m_acceptAsyncCallCount;
        //TODO: #ak usually acceptAsyncImpl is called repeatedly. SHOULD avoid unneccessary watchSocket and removeFromWatch calls
        return nx::SocketGlobals::aioService().watchSocketNonSafe(&lk, m_sock, aio::etRead, this);
    }

    void cancelAsyncIO(bool waitForRunningHandlerCompletion)
    {
        nx::SocketGlobals::aioService().removeFromWatch(m_sock, aio::etRead, waitForRunningHandlerCompletion);
        if( m_threadHandlerIsRunningIn.load( std::memory_order_acquire ) == QThread::currentThreadId() )
            ++m_acceptAsyncCallCount;     //we are in aio thread, eventTriggered is down the stack
        else
            assert( waitForRunningHandlerCompletion );      //TODO #ak AIO engine does not support truly async cancellation yet
    }

private:
    SocketType* m_sock;
    AbstractStreamServerSocket* m_abstractServerSocket;
    std::atomic<Qt::HANDLE> m_threadHandlerIsRunningIn;
    std::function<void(SystemError::ErrorCode, AbstractStreamSocket*)> m_acceptHandler;
    std::atomic<int> m_acceptAsyncCallCount;
    bool* m_terminatedFlagPtr;
};

#endif  //ASYNC_SOCKET_HELPER_H
