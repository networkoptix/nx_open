// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <exception>
#include <functional>
#include <type_traits>
#include <queue>

#include <QtCore/QThread>

#include <nx/utils/interruption_flag.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/format.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/future.h>

#include "aio_event_handler.h"
#include "basic_pollable.h"
#include "../abstract_socket.h"
#include "../address_resolver.h"
#include "../socket_global.h"

namespace nx {
namespace network {
namespace aio {

// TODO #akolesnikov Come up with new name for this class or remove it.
// TODO #akolesnikov Also, some refactor needed to use AsyncSocketImplHelper with server socket.
template<class SocketType>
class BaseAsyncSocketImplHelper
{
public:
    BaseAsyncSocketImplHelper(SocketType* _socket):
        m_socket(_socket)
    {
    }

    virtual ~BaseAsyncSocketImplHelper() = default;

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
    {
        NX_ASSERT(
            this->m_socket->impl()->aioThread->load() == aioThread ||
            !this->m_socket->impl()->aioThread->load() ||
            !this->m_socket->impl()->aioThread->load()->isSocketBeingMonitored(this->m_socket));

        if (this->m_socket->impl()->aioThread->load() != aioThread)
            m_socketInterruptionFlag.interrupt();
    }

    void post(nx::utils::MoveOnlyFunc<void()> handler)
    {
        if (m_socket->impl()->terminated.load(std::memory_order_relaxed) > 0)
            return;

        m_socket->impl()->aioThread->load()->post(m_socket, std::move(handler));
    }

    void dispatch(nx::utils::MoveOnlyFunc<void()> handler)
    {
        if (m_socket->impl()->terminated.load(std::memory_order_relaxed) > 0)
            return;

        m_socket->impl()->aioThread->load()->dispatch(m_socket, std::move(handler));
    }

    /**
     * This call stops async I/O on socket and it can never be resumed!
     */
    void terminateAsyncIO()
    {
        ++m_socket->impl()->terminated;
    }

protected:
    SocketType* m_socket = nullptr;
    nx::utils::InterruptionFlag m_socketInterruptionFlag;
};

/**
 * Implements asynchronous socket operations (connect, send, recv) and async cancellation routines using AIOService.
 * NOTE: When adding support for win32 I/O completion ports this class should
 *   become an abstraction level on top of completion ports and AIOService.
 */
template<class SocketType>
class AsyncSocketImplHelper:
    public BaseAsyncSocketImplHelper<SocketType>,
    public aio::AIOEventHandler
{
    using base_type = BaseAsyncSocketImplHelper<SocketType>;

public:
    AsyncSocketImplHelper(
        SocketType* _socket,
        int _ipVersion)
        :
        base_type(_socket),
        m_addressResolver(&SocketGlobals::addressResolver()),
        m_connectSendAsyncCallCounter(0),
        m_recvBuffer(nullptr),
        m_recvAsyncCallCounter(0),
        m_sendBuffer(nullptr),
        m_registerTimerCallCounter(0),
        m_asyncSendIssued(false),
        m_addressResolverIsInUse(false),
        m_ipVersion(_ipVersion),
        m_resolveResultScheduler(_socket->getAioThread())
    {
        NX_ASSERT(this->m_socket);
    }

    virtual ~AsyncSocketImplHelper()
    {
        // NOTE eventTriggered can be down the stack because socket
        // is allowed to be removed from I/O completion handler.
    }

    void terminate()
    {
        // This method is to cancel all asynchronous operations when called within socket's aio thread.
        // TODO: #akolesnikov What's the difference of this method from cancelAsyncIO(aio::etNone)?

        this->terminateAsyncIO();

        // Cancel ongoing async I/O. Doing this only if AsyncSocketImplHelper::eventTriggered is down the stack.
        if (this->m_socket->impl()->aioThread->load() == QThread::currentThread())
        {
            stopPollingSocket(aio::etNone);
            this->m_socket->impl()->aioThread->load()->cancelPostedCalls(this->m_socket);
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
                    m_addressResolver->isRequestIdKnown(this)), kFailureMessage);

            if (this->m_socket->impl()->aioThread->load())
            {
                NX_CRITICAL(
                    !this->m_socket->impl()->aioThread->load()->isSocketBeingMonitored(this->m_socket),
                    kFailureMessage);
            }
        }
    }

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);

        m_resolveResultScheduler.bindToAioThread(aioThread);
    }

    void resolve(
        const HostAddress& address,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, std::deque<HostAddress>)> handler)
    {
        m_addressResolverIsInUse = true;
        m_addressResolver->resolveAsync(
            address.toString(),
            [this, handler = std::move(handler)](
                SystemError::ErrorCode code, std::deque<AddressEntry> addresses) mutable
            {
                std::deque<HostAddress> ips;
                for (auto& entry: addresses)
                {
                    if (NX_ASSERT(entry.host.isIpAddress(), entry.host))
                        ips.push_back(std::move(entry.host));
                }

                m_addressResolverIsInUse = false;
                this->m_resolveResultScheduler.dispatch(
                    [handler = std::move(handler), code, ips = std::move(ips)]() mutable
                    {
                        handler(code, std::move(ips));
                    });
            },
            NatTraversalSupport::disabled,
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
            [this, endpoint, port = endpoint.port, handler = std::move(handler)](
                SystemError::ErrorCode code, std::deque<HostAddress> ips) mutable
            {
                if (code != SystemError::noError)
                {
                    NX_VERBOSE(this, "%1 resolve failed. %2",
                        endpoint, SystemError::toString(code));

                    return this->m_resolveResultScheduler.post(
                        [h = std::move(handler), code]() { h(code); });
                }

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
        if (!NX_ASSERT(addr.address.isIpAddress(), addr))
            return handler(SystemError::dnsServerFailure);

        if (this->m_socket->impl()->terminated.load(std::memory_order_relaxed) > 0)
        {
            // Socket has been terminated, no async call possible.
            return;
        }

        m_connectHandler = std::move(handler);
        if (!startAsyncConnect(addr))
        {
            this->m_resolveResultScheduler.post(
                [handler = std::move(m_connectHandler),
                    code = SystemError::getLastOSErrorCode()]() mutable
                {
                    handler(code);
                });
        }
    }

    void readSomeAsync(
        nx::Buffer* const buf,
        IoCompletionHandler handler)
    {
        if (this->m_socket->impl()->terminated.load(std::memory_order_relaxed) > 0)
            return;

        NX_ASSERT(isNonBlockingMode());

        // This assert is not critical but is a signal of a possible
        // ineffective memory usage in calling code.
        NX_ASSERT(buf->capacity() > buf->size());

        if (buf->capacity() == buf->size())
            buf->reserve(kDefaultReserveSize);
        m_recvBuffer = buf;
        m_recvHandler = std::move(handler);

        this->dispatch(
            [this]()
            {
                ++m_recvAsyncCallCounter;
                this->m_socket->impl()->aioThread->load()->startMonitoring(
                    this->m_socket, aio::etRead, this);
            });
    }

    void sendAsync(
        const nx::Buffer* buf,
        IoCompletionHandler handler)
    {
        if (this->m_socket->impl()->terminated.load(std::memory_order_relaxed) > 0)
            return;

        NX_ASSERT(isNonBlockingMode());
        NX_ASSERT(buf->size() > 0);
        NX_CRITICAL(!m_asyncSendIssued.exchange(true));

        m_sendBuffer = buf;
        m_sendHandler = std::move(handler);
        m_sendBufPos = 0;

        this->dispatch(
            [this]()
            {
                ++m_connectSendAsyncCallCounter;
                this->m_socket->impl()->aioThread->load()->startMonitoring(
                    this->m_socket, aio::etWrite, this);
            });
    }

    void registerTimer(
        std::chrono::milliseconds timeoutMs,
        nx::utils::MoveOnlyFunc<void()> handler)
    {
        NX_CRITICAL(timeoutMs.count(), "Timer with timeout 0 does not make any sense");
        if (this->m_socket->impl()->terminated.load(std::memory_order_relaxed) > 0)
            return;

        m_timerHandler = std::move(handler);

        this->dispatch(
            [this, timeoutMs]()
            {
                ++m_registerTimerCallCounter;
                this->m_socket->impl()->aioThread->load()->startMonitoring(
                    this->m_socket,
                    aio::etTimedOut,
                    this,
                    timeoutMs);
            });
    }

    void cancelIOSync(aio::EventType eventType)
    {
        if (this->m_socket->impl()->aioThread->load() == QThread::currentThread())
        {
            // TODO: #akolesnikov We must cancel resolve task here, but must do it without blocking!
            cancelAsyncIOWhileInAioThread(eventType);
        }
        else
        {
            //NX_ASSERT(!this->m_socket->impl()->aioThread->load()->isInAnyAioThread());

            nx::utils::promise<void> promise;
            this->m_socket->impl()->aioThread->load()->post(
                this->m_socket,
                [this, eventType, &promise]()
                {
                    cancelAsyncIOWhileInAioThread(eventType);
                    promise.set_value();
                });
            promise.get_future().wait();
        }
    }

    void cancelAsyncIOWhileInAioThread(const aio::EventType eventType)
    {
        stopPollingSocket(eventType);
        std::atomic_thread_fence(std::memory_order_acquire);    //< TODO: #akolesnikov Looks like it is not needed.

        // We are in aio thread, CommunicatingSocketImpl::eventTriggered is down the stack
        //  avoiding unnecessary stopMonitoring calls in eventTriggered.

        if (eventType == aio::etRead || eventType == aio::etNone)
            ++m_recvAsyncCallCounter;
        if (eventType == aio::etWrite || eventType == aio::etNone)
            ++m_connectSendAsyncCallCounter;
        if (eventType == aio::etTimedOut || eventType == aio::etNone)
            ++m_registerTimerCallCounter;

        if (eventType == aio::EventType::etNone)
            this->m_socketInterruptionFlag.interrupt();
    }

private:
    static constexpr int kDefaultReserveSize = 4 * 1024;

    AddressResolver* m_addressResolver;

    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_connectHandler;
    size_t m_connectSendAsyncCallCounter;

    IoCompletionHandler m_recvHandler;
    nx::Buffer* m_recvBuffer;
    size_t m_recvAsyncCallCounter;

    IoCompletionHandler m_sendHandler;
    const nx::Buffer* m_sendBuffer;
    std::size_t m_sendBufPos = 0;

    nx::utils::MoveOnlyFunc<void()> m_timerHandler;
    size_t m_registerTimerCallCounter;

    std::atomic<bool> m_asyncSendIssued;
    std::atomic<bool> m_addressResolverIsInUse;
    const int m_ipVersion;

    BasicPollable m_resolveResultScheduler;

    bool isNonBlockingMode() const
    {
        bool value;
        if (!this->m_socket->getNonBlockingMode(&value))
        {
            // TODO: #akolesnikov MUST return FALSE;
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
        try
        {
            NX_ASSERT(static_cast<Pollable*>(this->m_socket) == sock);

            // NOTE: aio garantees that all events on socket are handled in same aio thread,
            // so no synchronization is required.

            if (eventType & aio::etRead)
            {
                processRecvEvent(eventType);
            }
            else if (eventType & aio::etWrite)
            {
                processWriteEvent(eventType);
            }
            else if (eventType == aio::etTimedOut)
            {
                processTimedOutEvent();
            }
            else if (eventType == aio::etError)
            {
                processErrorEvent();
            }
            else
            {
                NX_CRITICAL(false, nx::format("Unexpected value: 0b%1").arg(static_cast<int>(eventType), 0, 2));
            }
        }
        catch (const std::exception& e)
        {
            NX_WARNING(typeid(decltype(*this)),
                nx::format("User exception caught while processing socket I/O event %1. %2")
                    .args(eventType, e.what()));
        }
        catch (...)
        {
            NX_WARNING(typeid(decltype(*this)),
                nx::format("Unknown user exception caught while processing socket I/O event %1")
                    .args(eventType));
        }
    }

    bool startAsyncConnect(const SocketAddress& resolvedAddress)
    {
#ifdef _DEBUG
        bool isNonBlockingModeEnabled = false;
        if (!this->m_socket->getNonBlockingMode(&isNonBlockingModeEnabled))
            return false;
        NX_ASSERT(isNonBlockingModeEnabled);
#endif

        NX_ASSERT(!m_asyncSendIssued.exchange(true));

        unsigned int sendTimeout = 0;
        if (!this->m_socket->getSendTimeout(&sendTimeout))
            return false;

        this->dispatch(
            [this, resolvedAddress, sendTimeout]()
            {
                ++m_connectSendAsyncCallCounter;
                this->m_socket->impl()->aioThread->load()->startMonitoring(
                    this->m_socket,
                    aio::etWrite,
                    this,
                    std::nullopt,
                    [this, resolvedAddress, sendTimeout]()
                    {
                        this->m_socket->connect(
                            resolvedAddress,
                            std::chrono::milliseconds(sendTimeout));
                    }); //< This functor will be called between pollset.add and pollset.poll.
            });
        return true;
    }

    void processTimedOutEvent()
    {
        if (!m_timerHandler)
            return;

        // Timer on socket (not read/write timeout, but some timer).

        nx::utils::InterruptionFlag::Watcher watcher(&this->m_socketInterruptionFlag);

        auto execFinally = nx::utils::makeScopeGuard(
            [this, &watcher, registerTimerCallCounterBak = m_registerTimerCallCounter]()
            {
                if (watcher.interrupted())
                    return;

                if (registerTimerCallCounterBak == m_registerTimerCallCounter)
                {
                    this->m_socket->impl()->aioThread->load()->stopMonitoring(
                        this->m_socket, aio::etTimedOut);
                }
            });

        nx::utils::swapAndCall(m_timerHandler);
    }

    void processRecvEvent(aio::EventType eventType)
    {
        if (eventType == aio::etRead)
        {
            NX_ASSERT(m_recvHandler);
            if (!isNonBlockingMode())
            {
                reportReadCompletion(SystemError::invalidData, (size_t) -1);
                return;
            }

            // Reading to buffer.
            auto newSize = m_recvBuffer->capacity();

            NX_ASSERT(newSize > m_recvBuffer->size(),
                nx::format("%1, %2").args(newSize, m_recvBuffer->size()));

            if (newSize == m_recvBuffer->size())
                newSize += kDefaultReserveSize;

            const auto bufSizeBak = m_recvBuffer->size();
            m_recvBuffer->resize(newSize);
            const auto bytesToRead = m_recvBuffer->size() - bufSizeBak;
            const int bytesRead = this->m_socket->recv(
                m_recvBuffer->data() + bufSizeBak,
                bytesToRead,
                0);
            if (bytesRead == -1)
            {
                const auto lastError = SystemError::getLastOSErrorCode();
                m_recvBuffer->resize(bufSizeBak);

                if (lastError == SystemError::again ||
                    lastError == SystemError::wouldBlock)
                {
                    return; //< Continuing waiting for data.
                }

                reportReadCompletion(lastError, (size_t)-1);
            }
            else
            {
                m_recvBuffer->resize(bufSizeBak + bytesRead); //< Shrinking buffer.
                reportReadCompletion(SystemError::noError, bytesRead);
            }
        }
        else if (eventType == aio::etReadTimedOut)
        {
            NX_ASSERT(m_recvHandler);

            reportReadCompletion(SystemError::timedOut, (size_t)-1);
        }
    }

    void reportReadCompletion(
        SystemError::ErrorCode errorCode, size_t bytesRead)
    {
        m_recvBuffer = nullptr;

        nx::utils::InterruptionFlag::Watcher watcher(&this->m_socketInterruptionFlag);

        auto execFinally = nx::utils::makeScopeGuard(
            [this, &watcher, recvAsyncCallCounterBak = m_recvAsyncCallCounter]()
            {
                if (watcher.interrupted())
                    return;

                if (recvAsyncCallCounterBak == m_recvAsyncCallCounter)
                {
                    this->m_socket->impl()->aioThread->load()->stopMonitoring(
                        this->m_socket, aio::etRead);
                }
            });

        nx::utils::swapAndCall(m_recvHandler, errorCode, bytesRead);
    }

    void processWriteEvent(aio::EventType eventType)
    {
        if (eventType == aio::etWrite)
        {
            if (m_connectHandler)
            {
                // Async connect. If we are here than connect succeeded.
                reportConnectCompletion(SystemError::noError);
            }
            else
            {
                // Can send some bytes.
                NX_ASSERT(m_sendHandler);
                if (!isNonBlockingMode())
                {
                    reportSendCompletion(SystemError::invalidData, (size_t) -1);
                    return;
                }

                const int bytesWritten = this->m_socket->send(
                    m_sendBuffer->data() + m_sendBufPos,
                    m_sendBuffer->size() - m_sendBufPos);
                if (bytesWritten == -1)
                {
                    const auto lastError = SystemError::getLastOSErrorCode();
                    if (lastError == SystemError::again ||
                        lastError == SystemError::wouldBlock)
                    {
                        return; //< False positive.
                    }

                    reportSendCompletion(lastError, m_sendBufPos);
                }
                else if (bytesWritten == 0)
                {
                    reportSendCompletion(SystemError::connectionReset, m_sendBufPos);
                }
                else
                {
                    m_sendBufPos += bytesWritten;
                    if (m_sendBufPos == m_sendBuffer->size())
                        reportSendCompletion(SystemError::noError, m_sendBufPos);
                }
            }
        }
        else if (eventType == aio::etWriteTimedOut)
        {
            if (m_connectHandler)
            {
                NX_VERBOSE(this, "Socket %1. Reporting connect timeout", this->m_socket);
                reportConnectCompletion(SystemError::timedOut);
            }
            else
            {
                NX_ASSERT(m_sendHandler);
                reportSendCompletion(SystemError::timedOut, (size_t)-1);
            }
        }
    }

    void reportConnectCompletion(SystemError::ErrorCode errorCode)
    {
        reportConnectOrSendCompletion(m_connectHandler, errorCode);
    }

    void reportSendCompletion(
        SystemError::ErrorCode errorCode, size_t bytesSent)
    {
        m_sendBuffer = nullptr;
        m_sendBufPos = 0;

        reportConnectOrSendCompletion(m_sendHandler, errorCode, bytesSent);
    }

    template<typename Handler, typename ... Args>
    void reportConnectOrSendCompletion(Handler& handler, Args... args)
    {
        m_asyncSendIssued = false;

        nx::utils::InterruptionFlag::Watcher watcher(&this->m_socketInterruptionFlag);

        auto execFinally = nx::utils::makeScopeGuard(
            [this, &watcher, connectSendAsyncCallCounterBak = m_connectSendAsyncCallCounter]()
            {
                if (watcher.interrupted())
                    return;

                if (connectSendAsyncCallCounterBak == m_connectSendAsyncCallCounter)
                {
                    this->m_socket->impl()->aioThread->load()->stopMonitoring(
                        this->m_socket, aio::etWrite);
                }
            });

        nx::utils::swapAndCall(handler, args...);
    }

    void processErrorEvent()
    {
        // TODO: #akolesnikov Distinguish read and write.
        SystemError::ErrorCode sockErrorCode = SystemError::notConnected;
        if (!this->m_socket->getLastError(&sockErrorCode))
            sockErrorCode = SystemError::notConnected;
        else if (sockErrorCode == SystemError::noError)
            sockErrorCode = SystemError::notConnected;  //< MUST report some error.

        nx::utils::InterruptionFlag::Watcher watcher(&this->m_socketInterruptionFlag);

        if (m_connectHandler)
        {
            NX_VERBOSE(this, "Socket %1. Reporting connect failure. %2",
                this->m_socket, SystemError::toString(sockErrorCode));
            reportConnectCompletion(sockErrorCode);
            if (watcher.interrupted())
                return;
        }

        if (m_recvHandler)
        {
            reportReadCompletion(sockErrorCode, (size_t)-1);
            if (watcher.interrupted())
                return;
        }

        if (m_sendHandler)
        {
            reportSendCompletion(sockErrorCode, (size_t)-1);
            if (watcher.interrupted())
                return;
        }
    }

    /**
     * NOTE: Can be called within socket's aio thread only.
     */
    void stopPollingSocket(const aio::EventType eventType)
    {
        m_addressResolver->cancel(this); //< TODO: #akolesnikov Must not block here!

        if (eventType == aio::EventType::etNone)
        {
            // TODO: #akolesnikov For now doing this only in case of cancellation of every event type
            // to support popular use case: cancel socket operation, pass socket to another thread.
            // But, same should be done separately for every event type.
            this->m_socketInterruptionFlag.interrupt();
        }

        if (eventType == aio::etNone || eventType == aio::etRead)
        {
            this->m_socket->impl()->aioThread->load()->stopMonitoring(this->m_socket, aio::etRead);
            m_recvHandler = nullptr;
        }

        if (eventType == aio::etNone || eventType == aio::etWrite)
        {
            // Cancelling dispatch call in resolve handler.
            this->m_resolveResultScheduler.cancelPostedCallsSync();
            this->m_socket->impl()->aioThread->load()->stopMonitoring(this->m_socket, aio::etWrite);
            m_connectHandler = nullptr;
            m_sendHandler = nullptr;
            m_asyncSendIssued = false;
        }

        if (eventType == aio::etNone || eventType == aio::etTimedOut)
        {
            this->m_socket->impl()->aioThread->load()->stopMonitoring(this->m_socket, aio::etTimedOut);
            m_timerHandler = nullptr;
        }
    }
};

// TODO: #akolesnikov Part of code of this class should be shared with AsyncSocketImplHelper.
template <class SocketType>
class AsyncServerSocketHelper:
    public BaseAsyncSocketImplHelper<SocketType>,
    public aio::AIOEventHandler
{
    using base_type = BaseAsyncSocketImplHelper<SocketType>;

public:
    AsyncServerSocketHelper(
        SocketType* _sock)
        :
        base_type(_sock),
        m_acceptAsyncCallCount(0)
    {
    }

    ~AsyncServerSocketHelper()
    {
        // NOTE: Removing socket not from completion handler while completion handler is running
        // in another thread is undefined behavour anyway, so no synchronization here.
    }

    virtual void eventTriggered(Pollable* sock, aio::EventType eventType) throw() override
    {
        NX_ASSERT(m_acceptHandler);

        try
        {
            switch (eventType)
            {
                case aio::etRead:
                {
                    // Accepting socket.
                    std::unique_ptr<AbstractStreamSocket> newSocket(this->m_socket->systemAccept());
                    const auto resultCode = newSocket != nullptr
                        ? SystemError::noError
                        : SystemError::getLastOSErrorCode();
                    reportAcceptResult(resultCode, std::move(newSocket));
                    break;
                }

                case aio::etReadTimedOut:
                    reportAcceptResult(SystemError::timedOut, nullptr);
                    break;

                case aio::etError:
                {
                    SystemError::ErrorCode errorCode = SystemError::noError;
                    sock->getLastError(&errorCode);
                    reportAcceptResult(
                        errorCode != SystemError::noError ? errorCode : SystemError::invalidData,
                        nullptr);
                    break;
                }

                default:
                    NX_ASSERT(false);
                    break;
            }
        }
        catch (const std::exception& e)
        {
            NX_WARNING(this,
                nx::format("User exception caught while processing server socket I/O event %1. %2")
                    .args(eventType, e.what()));
        }
        catch (...)
        {
            NX_WARNING(this,
                nx::format("Unknown user exception caught while processing server socket I/O event %1")
                    .args(eventType));
        }
    }

    void acceptAsync(AcceptCompletionHandler handler)
    {
        m_acceptHandler = std::move(handler);

        this->dispatch(
            [this]()
            {
                ++m_acceptAsyncCallCount;
                // TODO: #akolesnikov Usually, acceptAsync is called repeatedly.
                // SHOULD avoid unneccessary startMonitoring and stopMonitoring calls.
                return this->m_socket->impl()->aioThread->load()->startMonitoring(
                    this->m_socket, aio::etRead, this);
            });
    }

    void cancelIOSync()
    {
        if (this->m_socket->impl()->aioThread->load() == QThread::currentThread())
        {
            this->cancelIoWhileInAioThread();
        }
        else
        {
            nx::utils::promise<void> promise;
            this->post(
                [this, &promise]()
                {
                    this->cancelIoWhileInAioThread();
                    promise.set_value();
                });
            promise.get_future().wait();
        }
    }

    void stopPolling()
    {
        AioThread* aioThread = this->m_socket->impl()->aioThread->load();
        if (!aioThread)
            return;

        aioThread->cancelPostedCalls(this->m_socket);
        aioThread->stopMonitoring(this->m_socket, aio::etRead);
        aioThread->stopMonitoring(this->m_socket, aio::etTimedOut);
    }

private:
    AcceptCompletionHandler m_acceptHandler;
    std::atomic<int> m_acceptAsyncCallCount;

    // TODO: #akolesnikov Merge this method with stopPolling().
    void cancelIoWhileInAioThread()
    {
        this->m_socketInterruptionFlag.interrupt();

        this->m_socket->impl()->aioThread->load()->stopMonitoring(this->m_socket, aio::etRead);
        ++m_acceptAsyncCallCount;
    }

    void reportAcceptResult(
        SystemError::ErrorCode errorCode,
        std::unique_ptr<AbstractStreamSocket> newConnection)
    {
        nx::utils::InterruptionFlag::Watcher watcher(&this->m_socketInterruptionFlag);

        auto execFinally = nx::utils::makeScopeGuard(
            [this, &watcher, acceptAsyncCallCountBak = m_acceptAsyncCallCount.load()]()
            {
                if (watcher.interrupted())
                    return;

                // If asyncAccept has been called from onNewConnection, no need to call stopMonitoring.
                if (m_acceptAsyncCallCount == acceptAsyncCallCountBak)
                    this->m_socket->impl()->aioThread->load()->stopMonitoring(this->m_socket, aio::etRead);
            });

        nx::utils::swapAndCall(m_acceptHandler, errorCode, std::move(newConnection));
    }
};

} // namespace aio
} // namespace network
} // namespace nx
