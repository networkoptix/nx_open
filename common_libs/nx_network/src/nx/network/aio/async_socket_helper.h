#pragma once

#include <atomic>
#include <exception>
#include <functional>
#include <type_traits>
#include <queue>

#include <QtCore/QThread>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/std/future.h>

#include "../abstract_socket.h"
#include "../socket_global.h"

namespace nx {
namespace network {
namespace aio {

// TODO #ak Come up with new name for this class or remove it.
// TODO #ak Also, some refactor needed to use AsyncSocketImplHelper with server socket.
template<class SocketType>
class BaseAsyncSocketImplHelper
{
public:
    BaseAsyncSocketImplHelper(SocketType* _socket):
        m_socket(_socket)
    {
    }

    virtual ~BaseAsyncSocketImplHelper() {}

    void post(nx::utils::MoveOnlyFunc<void()> handler)
    {
        if (m_socket->impl()->terminated.load(std::memory_order_relaxed) > 0)
            return;

        nx::network::SocketGlobals::aioService().post(m_socket, std::move(handler));
    }

    void dispatch(nx::utils::MoveOnlyFunc<void()> handler)
    {
        if (m_socket->impl()->terminated.load(std::memory_order_relaxed) > 0)
            return;

        nx::network::SocketGlobals::aioService().dispatch(m_socket, std::move(handler));
    }

    /**
     * This call stops async I/O on socket and it can never be resumed!
     */
    void terminateAsyncIO()
    {
        ++m_socket->impl()->terminated;
    }

protected:
    SocketType* m_socket;
};

/**
 * Implements asynchronous socket operations (connect, send, recv) and async cancellation routines.
 */
template<class SocketType>
class AsyncSocketImplHelper:
    public BaseAsyncSocketImplHelper<SocketType>,
    public aio::AIOEventHandler
{
public:
    AsyncSocketImplHelper(
        SocketType* _socket,
        int _ipVersion)
        :
        BaseAsyncSocketImplHelper<SocketType>(_socket),
        m_connectSendAsyncCallCounter(0),
        m_recvBuffer(nullptr),
        m_recvAsyncCallCounter(0),
        m_sendBuffer(nullptr),
        m_sendBufPos(0),
        m_registerTimerCallCounter(0),
        m_connectSendHandlerTerminatedFlag(nullptr),
        m_recvHandlerTerminatedFlag(nullptr),
        m_timerHandlerTerminatedFlag(nullptr),
        m_threadHandlerIsRunningIn(NULL),
        m_asyncSendIssued(false),
        m_addressResolverIsInUse(false),
        m_ipVersion(_ipVersion)
    {
        NX_ASSERT(this->m_socket);
    }

    virtual ~AsyncSocketImplHelper()
    {
        //synchronization may be required here in case if recv handler and send/connect handler called simultaneously in different aio threads,
        //but even in described case no synchronization required, since before removing socket handler implementation MUST cancel ongoing
        //async I/O and wait for completion. That is, wait for eventTriggered to return.
        //So, socket can only be removed from handler called in aio thread. So, eventTriggered is down the stack if m_*TerminatedFlag is not nullptr

        if (m_connectSendHandlerTerminatedFlag)
            *m_connectSendHandlerTerminatedFlag = true;
        if (m_recvHandlerTerminatedFlag)
            *m_recvHandlerTerminatedFlag = true;
        if (m_timerHandlerTerminatedFlag)
            *m_timerHandlerTerminatedFlag = true;
    }

    void terminate()
    {
        //this method is to cancel all asynchronous operations when called within socket's aio thread

        this->terminateAsyncIO();
        //TODO #ak what's the difference of this method from cancelAsyncIO( aio::etNone ) ?

        //cancel ongoing async I/O. Doing this only if AsyncSocketImplHelper::eventTriggered is down the stack
        if (this->m_socket->impl()->aioThread.load() == QThread::currentThread())
        {
            stopPollingSocket(aio::etNone);
        }
        else
        {
            if (!nx::network::SocketGlobals::isInitialized())
                return;

            static const char* kFailureMessage =
                "You MUST cancel running async socket operation before "
                "deleting socket if you delete socket from non-aio thread";
            NX_CRITICAL(
                !(m_addressResolverIsInUse.load() &&
                    SocketGlobals::addressResolver().dnsResolver()
                    .isRequestIdKnown(this)), kFailureMessage);
            NX_CRITICAL(
                !SocketGlobals::aioService()
                .isSocketBeingWatched(this->m_socket), kFailureMessage);
        }
    }

    void resolve(
        const HostAddress& address,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, std::deque<HostAddress>)> handler)
    {
        m_addressResolverIsInUse = true;
        SocketGlobals::addressResolver().dnsResolver().resolveAsync(
            address.toString(),
            [this, handler = std::move(handler)](
                SystemError::ErrorCode code, std::deque<HostAddress> ips)
            {
                m_addressResolverIsInUse = false;
                handler(code, std::move(ips));
            },
            m_ipVersion,
            this);
    }

    void connectAsync(
        const SocketAddress& endpoint,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
    {
        NX_ASSERT(isNonBlockingMode());
        if (endpoint.address.isIpAddress())
            return connectToIpAsync(endpoint, std::move(handler));

        resolve(
            endpoint.address,
            [this, port = endpoint.port, handler = std::move(handler)](
                SystemError::ErrorCode code, std::deque<HostAddress> ips) mutable
            {
                if (code != SystemError::noError)
                    return this->post([h = std::move(handler), code]() { h(code); });

                connectToIpsAsync(std::move(ips), port, std::move(handler));
            });
    }

    void connectToIpsAsync(
        std::deque<HostAddress> ips, uint16_t port,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
    {
        SocketAddress firstAddress(std::move(ips.front()), port);
        ips.pop_front();
        connectToIpAsync(
            firstAddress,
            [this, ips = std::move(ips), port, handler = std::move(handler)](
                SystemError::ErrorCode code) mutable
            {
                if (code == SystemError::noError || ips.empty())
                    return handler(code);

                connectToIpsAsync(std::move(ips), port, std::move(handler));
            });
    }

    void connectToIpAsync(
        const SocketAddress& addr,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
    {
        NX_CRITICAL(addr.address.isIpAddress());
        //TODO with UDT we have to maintain pollset.add(socket), socket.connect, pollset.poll pipeline

        if (this->m_socket->impl()->terminated.load(std::memory_order_relaxed) > 0)
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
        if (!this->m_socket->getSendTimeout(&sendTimeout)
#ifdef _DEBUG
            || !this->m_socket->getNonBlockingMode(&isNonBlockingModeEnabled)
#endif
            )
        {
            this->post(
                [handler = std::move(handler),
                    errorCode = SystemError::getLastOSErrorCode()]() mutable
                { 
                    handler(errorCode);
                });
            return;
        }
#ifdef _DEBUG
        NX_ASSERT(isNonBlockingModeEnabled);
#endif

        m_connectHandler = std::move(handler);
        if (!startAsyncConnect(addr))
        {
            this->post(
                [handler = std::move(m_connectHandler),
                    code = SystemError::getLastOSErrorCode()]() mutable
                {
                    handler(code);
                });
        }
    }

    void readSomeAsync(
        nx::Buffer* const buf,
        std::function<void(SystemError::ErrorCode, size_t)> handler)
    {
        if (this->m_socket->impl()->terminated.load(std::memory_order_relaxed) > 0)
            return;

        NX_ASSERT(isNonBlockingMode());
        static const int DEFAULT_RESERVE_SIZE = 4 * 1024;

        //this assert is not critical but is a signal of possible 
        //ineffective memory usage in calling code
        NX_ASSERT(buf->capacity() > buf->size());

        if (buf->capacity() == buf->size())
            buf->reserve(DEFAULT_RESERVE_SIZE);
        m_recvBuffer = buf;
        m_recvHandler = std::move(handler);

        QnMutexLocker lk(nx::network::SocketGlobals::aioService().mutex());
        ++m_recvAsyncCallCounter;
        nx::network::SocketGlobals::aioService().startMonitoringNonSafe(
            &lk, this->m_socket, aio::etRead, this);
    }

    void sendAsync(
        const nx::Buffer& buf,
        std::function<void(SystemError::ErrorCode, size_t)> handler)
    {
        if (this->m_socket->impl()->terminated.load(std::memory_order_relaxed) > 0)
            return;

        NX_ASSERT(isNonBlockingMode());
        NX_ASSERT(buf.size() > 0);
        NX_CRITICAL(!m_asyncSendIssued.exchange(true));

        m_sendBuffer = &buf;
        m_sendHandler = std::move(handler);
        m_sendBufPos = 0;

        QnMutexLocker lk(nx::network::SocketGlobals::aioService().mutex());
        ++m_connectSendAsyncCallCounter;
        nx::network::SocketGlobals::aioService().startMonitoringNonSafe(
            &lk, this->m_socket, aio::etWrite, this);
    }

    void registerTimer(
        std::chrono::milliseconds timeoutMs,
        nx::utils::MoveOnlyFunc<void()> handler)
    {
        NX_CRITICAL(timeoutMs.count(), "Timer with timeout 0 does not make any sense");
        if (this->m_socket->impl()->terminated.load(std::memory_order_relaxed) > 0)
            return;

        m_timerHandler = std::move(handler);

        QnMutexLocker lk(nx::network::SocketGlobals::aioService().mutex());
        ++m_registerTimerCallCounter;
        nx::network::SocketGlobals::aioService().startMonitoringNonSafe(
            &lk,
            this->m_socket,
            aio::etTimedOut,
            this,
            timeoutMs);
    }

    void cancelIOAsync(
        const aio::EventType eventType,
        nx::utils::MoveOnlyFunc<void()> handler )
    {
        auto cancelImpl =
            [this, eventType, handler = move(handler)]() mutable
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
            nx::network::SocketGlobals::addressResolver().dnsResolver().cancel(this, true);

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
            NX_EXPECT(!nx::network::SocketGlobals::aioService().isInAnyAioThread());
            nx::utils::promise< bool > promise;
            cancelIOAsync(eventType, [&]() { promise.set_value(true); });
            promise.get_future().wait();
        }
    }

    void cancelAsyncIOWhileInAioThread(const aio::EventType eventType)
    {
        stopPollingSocket(eventType);
        std::atomic_thread_fence(std::memory_order_acquire);    //TODO #ak looks like it is not needed

        //we are in aio thread, CommunicatingSocketImpl::eventTriggered is down the stack
        //  avoiding unnecessary stopMonitoring calls in eventTriggered

        if (eventType == aio::etRead || eventType == aio::etNone)
            ++m_recvAsyncCallCounter;
        if (eventType == aio::etWrite || eventType == aio::etNone)
            ++m_connectSendAsyncCallCounter;
        if (eventType == aio::etTimedOut || eventType == aio::etNone)
            ++m_registerTimerCallCounter;
    }

private:
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_connectHandler;
    size_t m_connectSendAsyncCallCounter;

    std::function<void(SystemError::ErrorCode, size_t)> m_recvHandler;
    nx::Buffer* m_recvBuffer;
    size_t m_recvAsyncCallCounter;

    std::function<void(SystemError::ErrorCode, size_t)> m_sendHandler;
    const nx::Buffer* m_sendBuffer;
    int m_sendBufPos;

    nx::utils::MoveOnlyFunc<void()> m_timerHandler;
    size_t m_registerTimerCallCounter;

    bool* m_connectSendHandlerTerminatedFlag;
    bool* m_recvHandlerTerminatedFlag;
    bool* m_timerHandlerTerminatedFlag;

    std::atomic<Qt::HANDLE> m_threadHandlerIsRunningIn;
    std::atomic<bool> m_asyncSendIssued;
    std::atomic<bool> m_addressResolverIsInUse;
    const int m_ipVersion;

    bool isNonBlockingMode() const
    {
        bool value;
        if (!this->m_socket->getNonBlockingMode(&value))
        {
            // TODO: MUST return FALSE;
            // Currently here is a problem in UDT, getsockopt(...) can not find descriptor by some
            // reason, but send(...) and recv(...) work just fine.
            return true;
        }

        return value;
    }

    virtual void eventTriggered(
        Pollable* sock,
        aio::EventType eventType) throw() override
    {
        NX_ASSERT(static_cast<Pollable*>(this->m_socket) == sock);

        //TODO #ak split this method to multiple methods

        //NOTE aio garantees that all events on socket are handled in same aio thread, so no synchronization is required
        bool terminated = false;    //set to true just before object destruction

        m_threadHandlerIsRunningIn.store(QThread::currentThreadId(), std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        auto threadHandlerIsRunningInResetFunc =
            [this, &terminated](AsyncSocketImplHelper*)
            {
                if (terminated)
                    return;     //most likely, socket has been removed in handler
                m_threadHandlerIsRunningIn.store(nullptr, std::memory_order_relaxed);
                std::atomic_thread_fence(std::memory_order_release);
            };
        std::unique_ptr<AsyncSocketImplHelper, decltype(threadHandlerIsRunningInResetFunc)>
            threadHandlerIsRunningInResetGuard(this, threadHandlerIsRunningInResetFunc);

        const size_t connectSendAsyncCallCounterBak = m_connectSendAsyncCallCounter;
        auto connectHandlerLocal =
            [this, connectSendAsyncCallCounterBak, &terminated](SystemError::ErrorCode errorCode)
            {
                auto connectHandlerBak = std::move(m_connectHandler);
                m_asyncSendIssued = false;
                connectHandlerBak(errorCode);

                if (terminated)
                    return;     //most likely, socket has been removed in handler
                QnMutexLocker lk(nx::network::SocketGlobals::aioService().mutex());
                if (connectSendAsyncCallCounterBak == m_connectSendAsyncCallCounter)
                    nx::network::SocketGlobals::aioService().stopMonitoringNonSafe(
                        &lk, this->m_socket, aio::etWrite);
            };

        auto connectFinallyFunc =
            [this, &terminated](AsyncSocketImplHelper* /*pThis*/)
            {
                if (terminated)
                    return;     //most likely, socket has been removed in handler
                m_connectSendHandlerTerminatedFlag = nullptr;
            };

        const size_t recvAsyncCallCounterBak = m_recvAsyncCallCounter;
        auto recvHandlerLocal =
            [this, recvAsyncCallCounterBak, &terminated](
                SystemError::ErrorCode errorCode, size_t bytesRead)
            {
                m_recvBuffer = nullptr;
                auto recvHandlerBak = std::move(m_recvHandler);
                recvHandlerBak(errorCode, bytesRead);

                if (terminated)
                    return;     //most likely, socket has been removed in handler
                QnMutexLocker lk(nx::network::SocketGlobals::aioService().mutex());
                if (recvAsyncCallCounterBak == m_recvAsyncCallCounter)
                    nx::network::SocketGlobals::aioService().stopMonitoringNonSafe(
                        &lk, this->m_socket, aio::etRead);
            };

        auto readFinallyFunc =
            [this, &terminated](AsyncSocketImplHelper* /*pThis*/)
            {
                if (terminated)
                    return;     //most likely, socket has been removed in handler
                m_recvHandlerTerminatedFlag = nullptr;
            };

        auto sendHandlerLocal =
            [this, connectSendAsyncCallCounterBak, &terminated](
                SystemError::ErrorCode errorCode, size_t bytesSent)
            {
                m_sendBuffer = nullptr;
                m_sendBufPos = 0;
                auto sendHandlerBak = std::move(m_sendHandler);
                m_asyncSendIssued = false;
                sendHandlerBak(errorCode, bytesSent);

                if (terminated)
                    return;     //most likely, socket has been removed in handler
                QnMutexLocker lk(nx::network::SocketGlobals::aioService().mutex());
                if (connectSendAsyncCallCounterBak == m_connectSendAsyncCallCounter)
                    nx::network::SocketGlobals::aioService().stopMonitoringNonSafe(
                        &lk, this->m_socket, aio::etWrite);
            };

        auto writeFinallyFunc =
            [this, &terminated](AsyncSocketImplHelper* /*pThis*/)
            {
                if (terminated)
                    return;     //most likely, socket has been removed in handler
                m_connectSendHandlerTerminatedFlag = nullptr;
            };

        if (eventType & aio::etRead)
        {
            processRecvEvent(eventType, terminated, recvHandlerLocal, readFinallyFunc);
        }
        else if (eventType & aio::etWrite)
        {
            processWriteEvent(
                eventType, terminated,
                connectHandlerLocal, connectFinallyFunc,
                sendHandlerLocal, writeFinallyFunc);
        }
        else if (eventType == aio::etTimedOut)
        {
            processTimedOutEvent(this->m_socket, terminated);
        }
        else if (eventType == aio::etError)
        {
            processErrorEvent(
                terminated,
                connectHandlerLocal, connectFinallyFunc,
                recvHandlerLocal, readFinallyFunc,
                sendHandlerLocal, writeFinallyFunc);
        }
        else
        {
            NX_CRITICAL(false, lm("Unexpected value: 0b%1").arg(static_cast<int>(eventType), 0, 2));
        }
    }

    bool startAsyncConnect(const SocketAddress& resolvedAddress)
    {
        //connecting
        unsigned int sendTimeout = 0;
#ifdef _DEBUG
        bool isNonBlockingModeEnabled = false;
        if (!this->m_socket->getNonBlockingMode(&isNonBlockingModeEnabled))
            return false;
        NX_ASSERT(isNonBlockingModeEnabled);
#endif

        NX_ASSERT(!m_asyncSendIssued.exchange(true));

        if (!this->m_socket->getSendTimeout(&sendTimeout))
            return false;

        QnMutexLocker lk(nx::network::SocketGlobals::aioService().mutex());
        ++m_connectSendAsyncCallCounter;
        nx::network::SocketGlobals::aioService().startMonitoringNonSafe(
            &lk,
            this->m_socket,
            aio::etWrite,
            this,
            boost::none,
            [this, resolvedAddress, sendTimeout]()
            {
                NX_CRITICAL(resolvedAddress.address.isIpAddress());
                this->m_socket->connect(resolvedAddress, sendTimeout);
            });    //to be called between pollset.add and pollset.polladdress
        return true;
    }

    void processTimedOutEvent(SocketType* sock, bool& terminated)
    {
        const size_t registerTimerCallCounterBak = m_registerTimerCallCounter;
        auto timerHandlerLocal = 
            [this, registerTimerCallCounterBak, sock, &terminated]()
            {
                auto timerHandlerBak = std::move(m_timerHandler);
                timerHandlerBak();

                if (terminated)
                    return;     //most likely, socket has been removed in handler
                QnMutexLocker lk(nx::network::SocketGlobals::aioService().mutex());
                if (registerTimerCallCounterBak == m_registerTimerCallCounter)
                {
                    nx::network::SocketGlobals::aioService().stopMonitoringNonSafe(
                        &lk, sock, aio::etTimedOut);
                }
            };

        auto timerFinallyFunc =
            [this, &terminated](AsyncSocketImplHelper* /*pThis*/)
            {
                if (terminated)
                    return;     //most likely, socket has been removed in handler
                m_timerHandlerTerminatedFlag = nullptr;
            };

        //timer on socket (not read/write timeout, but some timer)
        m_timerHandlerTerminatedFlag = &terminated;

        if (m_timerHandler)
        {
            //async connect. If we are here than connect succeeded
            std::unique_ptr<AsyncSocketImplHelper, decltype(timerFinallyFunc)>
                cleanupGuard(this, timerFinallyFunc);
            timerHandlerLocal();
        }
    }

    template<typename Handler, typename FinallyHandler>
    void processRecvEvent(
        aio::EventType eventType,
        bool& terminated,
        const Handler& recvHandlerLocal,
        const FinallyHandler& readFinallyFunc)
    {
        if (eventType == aio::etRead)
        {
            m_recvHandlerTerminatedFlag = &terminated;

            std::unique_ptr<AsyncSocketImplHelper, decltype(readFinallyFunc)>
                cleanupGuard(this, readFinallyFunc);

            NX_ASSERT(m_recvHandler);
            if (!isNonBlockingMode())
                return recvHandlerLocal(SystemError::invalidData, (size_t) -1);

            //reading to buffer
            const auto bufSizeBak = m_recvBuffer->size();
            m_recvBuffer->resize(m_recvBuffer->capacity());
            const int bytesRead = this->m_socket->recv(
                m_recvBuffer->data() + bufSizeBak,
                m_recvBuffer->capacity() - bufSizeBak,
                0);
            if (bytesRead == -1)
            {
                const auto lastError = SystemError::getLastOSErrorCode();
                m_recvBuffer->resize(bufSizeBak);

                if (lastError == SystemError::again ||
                    lastError == SystemError::wouldBlock)
                {
                    return; //continuing waiting for data
                }

                recvHandlerLocal(lastError, (size_t)-1);
            }
            else
            {
                m_recvBuffer->resize(bufSizeBak + bytesRead);   //shrinking buffer
                recvHandlerLocal(SystemError::noError, bytesRead);
            }
        }
        else if (eventType == aio::etReadTimedOut)
        {
            NX_ASSERT(m_recvHandler);

            m_recvHandlerTerminatedFlag = &terminated;
            std::unique_ptr<AsyncSocketImplHelper, decltype(readFinallyFunc)> cleanupGuard(this, readFinallyFunc);
            recvHandlerLocal(SystemError::timedOut, (size_t)-1);
        }
    }

    template<
        typename ConnectHandler, typename ConnectFinallyHandler,
        typename SendHandler, typename SendFinallyHandler>
    void processWriteEvent(
        aio::EventType eventType,
        bool& terminated,
        const ConnectHandler& connectHandlerLocal,
        const ConnectFinallyHandler& connectFinallyFunc,
        const SendHandler& sendHandlerLocal,
        const SendFinallyHandler& writeFinallyFunc)
    {
        if (eventType == aio::etWrite)
        {
            m_connectSendHandlerTerminatedFlag = &terminated;

            if (m_connectHandler)
            {
                //async connect. If we are here than connect succeeded
                std::unique_ptr<AsyncSocketImplHelper, decltype(connectFinallyFunc)>
                    cleanupGuard(this, connectFinallyFunc);
                connectHandlerLocal(SystemError::noError);
            }
            else
            {
                //can send some bytes
                NX_ASSERT(m_sendHandler);
                if (!isNonBlockingMode())
                    return sendHandlerLocal(SystemError::invalidData, (size_t) -1);

                std::unique_ptr<AsyncSocketImplHelper, decltype(writeFinallyFunc)>
                    cleanupGuard(this, writeFinallyFunc);

                const int bytesWritten = this->m_socket->send(
                    m_sendBuffer->constData() + m_sendBufPos,
                    m_sendBuffer->size() - m_sendBufPos);
                if (bytesWritten == -1)
                {
                    const auto lastError = SystemError::getLastOSErrorCode();
                    if (lastError == SystemError::again ||
                        lastError == SystemError::wouldBlock)
                    {
                        return; // false positive
                    }

                    sendHandlerLocal(lastError, m_sendBufPos);
                }
                else if (bytesWritten == 0)
                {
                    sendHandlerLocal(SystemError::connectionReset, m_sendBufPos);
                }
                else
                {
                    m_sendBufPos += bytesWritten;
                    if (m_sendBufPos == m_sendBuffer->size())
                        sendHandlerLocal(SystemError::noError, m_sendBufPos);
                }
            }
        }
        else if (eventType == aio::etWriteTimedOut)
        {
            m_connectSendHandlerTerminatedFlag = &terminated;

            if (m_connectHandler)
            {
                std::unique_ptr<AsyncSocketImplHelper, decltype(connectFinallyFunc)> cleanupGuard(this, connectFinallyFunc);
                connectHandlerLocal(SystemError::timedOut);
            }
            else
            {
                NX_ASSERT(m_sendHandler);
                std::unique_ptr<AsyncSocketImplHelper, decltype(writeFinallyFunc)> cleanupGuard(this, writeFinallyFunc);
                sendHandlerLocal(SystemError::timedOut, (size_t)-1);
            }
        }
    }

    template<
        typename ConnectHandler, typename ConnectFinallyHandler,
        typename RecvHandler, typename RecvFinallyHandler,
        typename SendHandler, typename SendFinallyHandler>
    void processErrorEvent(
        bool& terminated,
        const ConnectHandler& connectHandlerLocal,
        const ConnectFinallyHandler& connectFinallyFunc,
        const RecvHandler& recvHandlerLocal,
        const RecvFinallyHandler& readFinallyFunc,
        const SendHandler& sendHandlerLocal,
        const SendFinallyHandler& writeFinallyFunc)
    {
        //TODO #ak distinguish read and write
        SystemError::ErrorCode sockErrorCode = SystemError::notConnected;
        if (!this->m_socket->getLastError(&sockErrorCode))
            sockErrorCode = SystemError::notConnected;
        else if (sockErrorCode == SystemError::noError)
            sockErrorCode = SystemError::notConnected;  //MUST report some error

        if (m_connectHandler)
        {
            m_connectSendHandlerTerminatedFlag = &terminated;
            std::unique_ptr<AsyncSocketImplHelper, decltype(connectFinallyFunc)>
                cleanupGuard(this, connectFinallyFunc);
            connectHandlerLocal(sockErrorCode);
        }

        if (terminated)
            return;

        if (m_recvHandler)
        {
            m_recvHandlerTerminatedFlag = &terminated;
            std::unique_ptr<AsyncSocketImplHelper, decltype(readFinallyFunc)>
                cleanupGuard(this, readFinallyFunc);
            recvHandlerLocal(sockErrorCode, (size_t)-1);
        }

        if (terminated)
            return;

        if (m_sendHandler)
        {
            m_connectSendHandlerTerminatedFlag = &terminated;
            std::unique_ptr<AsyncSocketImplHelper, decltype(writeFinallyFunc)>
                cleanupGuard(this, writeFinallyFunc);
            sendHandlerLocal(sockErrorCode, (size_t)-1);
        }
    }

    //!Call this from within aio thread only
    void stopPollingSocket(const aio::EventType eventType)
    {
        nx::network::SocketGlobals::addressResolver().dnsResolver().cancel(this, true);    //TODO #ak must not block here!

        if (eventType == aio::etNone)
            nx::network::SocketGlobals::aioService().cancelPostedCalls(this->m_socket, true);

        if (eventType == aio::etNone || eventType == aio::etRead)
        {
            nx::network::SocketGlobals::aioService().stopMonitoring(
                this->m_socket, aio::etRead, true);
            m_recvHandler = nullptr;
        }

        if (eventType == aio::etNone || eventType == aio::etWrite)
        {
            nx::network::SocketGlobals::aioService().stopMonitoring(
                this->m_socket, aio::etWrite, true);
            m_connectHandler = nullptr;
            m_sendHandler = nullptr;
            m_asyncSendIssued = false;
        }

        if (eventType == aio::etNone || eventType == aio::etTimedOut)
        {
            nx::network::SocketGlobals::aioService().stopMonitoring(
                this->m_socket, aio::etTimedOut, true);
            m_timerHandler = nullptr;
        }
    }
};

template <class SocketType>
class AsyncServerSocketHelper:
    public aio::AIOEventHandler
{
public:
    AsyncServerSocketHelper(SocketType* _sock):
        m_sock(_sock),
        m_threadHandlerIsRunningIn(NULL),
        m_acceptAsyncCallCount(0),
        m_terminatedFlagPtr(nullptr)
    {
    }

    ~AsyncServerSocketHelper()
    {
        // NOTE: Removing socket not from completion handler while completion handler is running 
        // in another thread is undefined behavour anyway, so no synchronization here.
        if (m_terminatedFlagPtr)
            *m_terminatedFlagPtr = true;
    }

    virtual void eventTriggered(Pollable* sock, aio::EventType eventType) throw() override
    {
        NX_ASSERT(m_acceptHandler);

        bool terminated = false;    //set to true just before object destruction
        m_terminatedFlagPtr = &terminated;
        m_threadHandlerIsRunningIn.store(QThread::currentThreadId(), std::memory_order_release);
        const int acceptAsyncCallCountBak = m_acceptAsyncCallCount;
        
        decltype(m_acceptHandler) acceptHandlerBak;
        acceptHandlerBak.swap(m_acceptHandler);

        auto acceptHandlerBakLocal = 
            [this, sock, acceptHandlerBak = std::move(acceptHandlerBak),
                    &terminated, acceptAsyncCallCountBak](
                SystemError::ErrorCode errorCode,
                std::unique_ptr<AbstractStreamSocket> newConnection)
            {
                acceptHandlerBak(errorCode, std::move(newConnection));
                if (terminated)
                    return;
                //if asyncAccept has been called from onNewConnection, no need to call stopMonitoring
                QnMutexLocker lk(nx::network::SocketGlobals::aioService().mutex());
                if (m_acceptAsyncCallCount == acceptAsyncCallCountBak)
                    nx::network::SocketGlobals::aioService().stopMonitoringNonSafe(&lk, sock, aio::etRead);
                m_threadHandlerIsRunningIn.store(nullptr, std::memory_order_release);
                m_terminatedFlagPtr = nullptr;
            };

        switch (eventType)
        {
            case aio::etRead:
            {
                //accepting socket
                std::unique_ptr<AbstractStreamSocket> newSocket(m_sock->systemAccept());
                const auto resultCode = newSocket != nullptr
                    ? SystemError::noError
                    : SystemError::getLastOSErrorCode();
                acceptHandlerBakLocal(resultCode, std::move(newSocket));
                break;
            }

            case aio::etReadTimedOut:
                acceptHandlerBakLocal(SystemError::timedOut, nullptr);
                break;

            case aio::etError:
            {
                SystemError::ErrorCode errorCode = SystemError::noError;
                sock->getLastError(&errorCode);
                acceptHandlerBakLocal(
                    errorCode != SystemError::noError ? errorCode : SystemError::invalidData,
                    nullptr);
                break;
            }

            default:
                NX_ASSERT(false);
                break;
        }
    }

    void acceptAsync(AcceptCompletionHandler handler)
    {
        m_acceptHandler = std::move(handler);

        QnMutexLocker lk(nx::network::SocketGlobals::aioService().mutex());
        ++m_acceptAsyncCallCount;
        // TODO: #ak Usually, acceptAsync is called repeatedly. 
        // SHOULD avoid unneccessary startMonitoring and stopMonitoring calls.
        return nx::network::SocketGlobals::aioService().startMonitoringNonSafe(
            &lk, m_sock, aio::etRead, this);
    }

    void cancelIOAsync(nx::utils::MoveOnlyFunc<void()> handler)
    {
        nx::network::SocketGlobals::aioService().dispatch(
            this->m_sock,
            [this, handler = move(handler)]() mutable
            {
                nx::network::SocketGlobals::aioService().stopMonitoring(
                    m_sock, aio::etRead, true);

                ++m_acceptAsyncCallCount;
                handler();
            });
    }

    void cancelIOSync()
    {
        // TODO: #ak Promise is not needed if we are already in aio thread.
        nx::utils::promise< bool > promise;
        cancelIOAsync([&]() { promise.set_value(true); });
        promise.get_future().wait();
    }

    void stopPolling()
    {
        nx::network::SocketGlobals::aioService().cancelPostedCalls(
            m_sock, true);
        nx::network::SocketGlobals::aioService().stopMonitoring(
            m_sock, aio::etRead, true);
        nx::network::SocketGlobals::aioService().stopMonitoring(
            m_sock, aio::etTimedOut, true);
    }

private:
    SocketType* m_sock;
    std::atomic<Qt::HANDLE> m_threadHandlerIsRunningIn;
    AcceptCompletionHandler m_acceptHandler;
    std::atomic<int> m_acceptAsyncCallCount;
    bool* m_terminatedFlagPtr;
};

} // namespace aio
} // namespace network
} // namespace nx
