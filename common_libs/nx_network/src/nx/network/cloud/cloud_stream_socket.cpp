#include "cloud_stream_socket.h"

#include <utils/common/systemerror.h>

#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>

#include "tunnel/outgoing_tunnel.h"
#include "../socket_global.h"
#include "../system_socket.h"

namespace nx {
namespace network {
namespace cloud {

CloudStreamSocket::CloudStreamSocket(int ipVersion):
    m_timer(std::make_unique<aio::Timer>()),
    m_readIoBinder(std::make_unique<aio::Timer>()),
    m_writeIoBinder(std::make_unique<aio::Timer>()),
    m_connectPromisePtr(nullptr),
    m_terminated(false),
    m_ipVersion(ipVersion)
{
    //TODO #ak user MUST be able to bind this object to any aio thread
    //getAioThread binds to an aio thread
    m_socketAttributes.aioThread = m_aioThreadBinder.getAioThread();

    bindToAioThread(m_aioThreadBinder.getAioThread());
}

CloudStreamSocket::~CloudStreamSocket()
{
    stopWhileInAioThread();
}

aio::AbstractAioThread* CloudStreamSocket::getAioThread() const
{
    return m_aioThreadBinder.getAioThread();
}

void CloudStreamSocket::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    BaseType::bindToAioThread(aioThread);

    m_aioThreadBinder.bindToAioThread(aioThread);
    if (m_socketDelegate)
        m_socketDelegate->bindToAioThread(aioThread);
    m_timer->bindToAioThread(aioThread);
    m_readIoBinder->bindToAioThread(aioThread);
    m_writeIoBinder->bindToAioThread(aioThread);

    m_socketAttributes.aioThread = aioThread;
}

bool CloudStreamSocket::bind(const SocketAddress& localAddress)
{
    //TODO #ak just ignoring for now. 
        //Usually, we do not care about exact port on tcp client socket
    static_cast<void>(localAddress);
    return true;
}

SocketAddress CloudStreamSocket::getLocalAddress() const
{
    if (m_socketDelegate)
        return m_socketDelegate->getLocalAddress();

    return SocketAddress();
}

bool CloudStreamSocket::close()
{
    shutdown();
    if (m_socketDelegate)
        return m_socketDelegate->close();

    return true;
}

bool CloudStreamSocket::isClosed() const
{
    if (m_socketDelegate)
        return m_socketDelegate->isClosed();

    return true;
}

bool CloudStreamSocket::shutdown()
{
    {
        QnMutexLocker lk(&m_mutex);
        if (m_terminated)
            return true;
        m_terminated = true;
    }

    //interrupting blocking calls
    nx::utils::promise<void> stoppedPromise;
    pleaseStop(
        [this, &stoppedPromise]()
        {
            if (auto promisePtr = m_connectPromisePtr.exchange(nullptr))
                promisePtr->set_value(std::make_pair(SystemError::interrupted, 0));

            stoppedPromise.set_value();
        });

    stoppedPromise.get_future().wait();
    if (m_socketDelegate)
        m_socketDelegate->shutdown();

    return true;
}

AbstractSocket::SOCKET_HANDLE CloudStreamSocket::handle() const
{
    if (m_socketDelegate)
        return m_socketDelegate->handle();

    SystemError::setLastErrorCode(SystemError::notSupported);
    return -1;
}

bool CloudStreamSocket::reopen()
{
    if (m_socketDelegate)
        return m_socketDelegate->reopen();

    return false;
}

bool CloudStreamSocket::connect(
    const SocketAddress& remoteAddress,
    unsigned int timeoutMillis)
{
    NX_ASSERT(!SocketGlobals::aioService().isInAnyAioThread());

    unsigned int sendTimeoutBak = 0;
    if (!getSendTimeout(&sendTimeoutBak))
        return false;
    if (!setSendTimeout(timeoutMillis))
        return false;

    nx::utils::promise<std::pair<SystemError::ErrorCode, size_t>> promise;
    {
        QnMutexLocker lk(&m_mutex);
        if (m_terminated)
        {
            SystemError::setLastErrorCode(SystemError::interrupted);
            return false;
        }

        SocketResultPrimisePtr expected = nullptr;
        if (!m_connectPromisePtr.compare_exchange_strong(expected, &promise))
        {
            NX_ASSERT(false);
            SystemError::setLastErrorCode(SystemError::already);
            return false;
        }
    }

    connectAsync(
        remoteAddress,
        [this, &promise](SystemError::ErrorCode code)
        {
            //to ensure that socket is not used by aio sub-system anymore, we use post
            m_writeIoBinder->post(
                [this, code]()
                {
                    if (auto promisePtr = m_connectPromisePtr.exchange(nullptr))
                        promisePtr->set_value(std::make_pair(code, 0));
                });
        });
    
    auto result = promise.get_future().get().first;
    if (result != SystemError::noError)
    {
        SystemError::setLastErrorCode(result);
        return false;
    }

    return setSendTimeout(sendTimeoutBak);
}

int CloudStreamSocket::recv(void* buffer, unsigned int bufferLen, int flags)
{
    NX_ASSERT(!SocketGlobals::aioService().isInAnyAioThread());

    if (!m_socketDelegate)
    {
        SystemError::setLastErrorCode(SystemError::notConnected);
        return -1;
    }

    return m_socketDelegate->recv(buffer, bufferLen, flags);
}

int CloudStreamSocket::send(const void* buffer, unsigned int bufferLen)
{
    NX_ASSERT(!SocketGlobals::aioService().isInAnyAioThread());

    if (!m_socketDelegate)
    {
        SystemError::setLastErrorCode(SystemError::notConnected);
        return -1;
    }

    return m_socketDelegate->send(buffer, bufferLen);
}

SocketAddress CloudStreamSocket::getForeignAddress() const
{
    if (m_socketDelegate)
        return m_socketDelegate->getForeignAddress();

    SystemError::setLastErrorCode(SystemError::notConnected);
    return SocketAddress();
}

bool CloudStreamSocket::isConnected() const
{
    if (m_socketDelegate)
        return m_socketDelegate->isConnected();

    return false;
}

void CloudStreamSocket::cancelIOAsync(
    aio::EventType eventType,
    nx::utils::MoveOnlyFunc<void()> handler)
{
    post(
        [this, eventType, handler = std::move(handler)]()
        {
            cancelIoWhileInAioThread(eventType);
            handler();
        });
}

void CloudStreamSocket::cancelIOSync(aio::EventType eventType)
{
    if (isInSelfAioThread())
    {
        cancelIoWhileInAioThread(eventType);
    }
    else
    {
        nx::utils::promise<void> ioCancelled;
        cancelIOAsync(
            eventType,
            [&ioCancelled]() { ioCancelled.set_value(); });
        ioCancelled.get_future().wait();
    }
}

void CloudStreamSocket::post(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_aioThreadBinder.post(std::move(handler));
}

void CloudStreamSocket::dispatch(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_aioThreadBinder.dispatch(std::move(handler));
}

void CloudStreamSocket::connectAsync(
    const SocketAddress& address,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    NX_LOGX(lm("connectAsync. %1").str(address), cl_logDEBUG2);

    nx::network::SocketGlobals::addressResolver().resolveAsync(
        address.address,
        [this, operationGuard = m_asyncConnectGuard.sharedGuard(),
            port = address.port, handler = std::move(handler)](
                SystemError::ErrorCode code, std::deque<AddressEntry> dnsEntries) mutable
        {
            NX_LOGX(lm("done resolve. %1, %2").str(code).arg(dnsEntries.size()), cl_logDEBUG2);

            if (operationGuard->lock())
            {
                m_writeIoBinder->post(
                    [this, port, handler = std::move(handler),
                        code, dnsEntries = std::move(dnsEntries)]() mutable
                    {
                        if (code != SystemError::noError)
                            return handler(code);

                        if (dnsEntries.empty())
                        {
                            NX_ASSERT(false);
                            return handler(SystemError::hostNotFound);
                        }

                        connectToEntriesAsync(std::move(dnsEntries), port, std::move(handler));
                    });
            }
        },
        NatTraversalSupport::enabled,
        m_ipVersion,
        this);
}

void CloudStreamSocket::readSomeAsync(
    nx::Buffer* const buf,
    std::function<void(SystemError::ErrorCode, size_t)> handler)
{
    if (m_socketDelegate)
        m_socketDelegate->readSomeAsync(buf, std::move(handler));
    else
        m_readIoBinder->post(std::bind(handler, SystemError::notConnected, 0));
}

void CloudStreamSocket::sendAsync(
    const nx::Buffer& buf,
    std::function<void(SystemError::ErrorCode, size_t)> handler)
{
    if (m_socketDelegate)
        m_socketDelegate->sendAsync(buf, std::move(handler));
    else
        m_writeIoBinder->post(std::bind(handler, SystemError::notConnected, 0));
}

void CloudStreamSocket::registerTimer(
    std::chrono::milliseconds timeout,
    nx::utils::MoveOnlyFunc<void()> handler)
{
    m_timer->start(timeout, std::move(handler));
}

void CloudStreamSocket::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_aioThreadBinder.pleaseStop(
        [this, handler = std::move(handler)]()
        {
            stopWhileInAioThread();
            m_aioThreadBinder.pleaseStopSync();
            handler();
        });
}

void CloudStreamSocket::pleaseStopSync(bool /*checkForLocks*/)
{
    if (isInSelfAioThread())
    {
        stopWhileInAioThread();
        m_aioThreadBinder.pleaseStopSync();
    }
    else
    {
        nx::utils::promise<void> stoppedPromise;
        pleaseStop([&stoppedPromise]() { stoppedPromise.set_value(); });
        stoppedPromise.get_future().wait();
    }
}

bool CloudStreamSocket::isInSelfAioThread() const
{
    return m_aioThreadBinder.isInSelfAioThread();
}

void CloudStreamSocket::connectToEntriesAsync(
    std::deque<AddressEntry> dnsEntries,
    int port,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    NX_LOGX(lm("connectToEntriesAsync. %1, port %2").str(dnsEntries.front()).arg(port), cl_logDEBUG2);

    AddressEntry firstEntry(std::move(dnsEntries.front()));
    dnsEntries.pop_front();
    connectToEntryAsync(
        firstEntry, port,
        [this, dnsEntries = std::move(dnsEntries), port, handler = std::move(handler)](
            SystemError::ErrorCode code) mutable
        {
            if (code == SystemError::noError || dnsEntries.empty())
                return handler(code);

            connectToEntriesAsync(std::move(dnsEntries), port, std::move(handler));
        });
}

void CloudStreamSocket::connectToEntryAsync(
    const AddressEntry& dnsEntry,
    int port,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    NX_LOGX(lm("connectToEntryAsync. %1, port %2").str(dnsEntry).arg(port), cl_logDEBUG2);

    using namespace std::placeholders;
    switch (dnsEntry.type)
    {
        case AddressType::direct:
            NX_LOGX(lm("connectToEntryAsync. direct %1:%2").str(dnsEntry.host).arg(port), cl_logDEBUG2);
            //using tcp connection
            m_socketDelegate.reset(new TCPSocket(m_ipVersion));
            setDelegate(m_socketDelegate.get());
            if (!m_socketDelegate->setNonBlockingMode(true))
                return handler(SystemError::getLastOSErrorCode());

            for (const auto& attr: dnsEntry.attributes)
            {
                if (attr.type == AddressAttributeType::port)
                    port = static_cast<quint16>(attr.value);
            }

            m_connectHandler = std::move(handler);
            return m_socketDelegate->connectAsync(
                SocketAddress(std::move(dnsEntry.host), port),
                std::bind(&CloudStreamSocket::onDirectConnectDone, this, _1));

        case AddressType::cloud:
        case AddressType::unknown:  //if peer is unknown, trying to establish cloud connect
        {
            NX_LOGX(lm("connectToEntryAsync. cloud %1:%2").str(dnsEntry.host).arg(port), cl_logDEBUG2);

            //establishing cloud connect
            unsigned int sendTimeoutMillis = 0;
            if (!getSendTimeout(&sendTimeoutMillis))
                return handler(SystemError::getLastOSErrorCode());

            auto sharedOperationGuard = m_asyncConnectGuard.sharedGuard();
            m_connectHandler = std::move(handler);

            // TODO: Need to pass m_ipVersion for IPv6 support
            SocketGlobals::outgoingTunnelPool().establishNewConnection(
                dnsEntry,
                std::chrono::milliseconds(sendTimeoutMillis),
                getSocketAttributes(),
                [this, sharedOperationGuard](
                    SystemError::ErrorCode errorCode,
                    std::unique_ptr<AbstractStreamSocket> cloudConnection)
                {
                    //NOTE: this handler is called from unspecified thread
                    auto operationLock = sharedOperationGuard->lock();
                    if (!operationLock)
                        return; //operation has been cancelled

                    if (errorCode == SystemError::noError)
                        NX_ASSERT(cloudConnection->getAioThread() == m_aioThreadBinder.getAioThread());
                    else
                        NX_ASSERT(!cloudConnection);

                    dispatch(
                        [this, errorCode, cloudConnection = std::move(cloudConnection)]() mutable
                        {
                            onCloudConnectDone(
                                errorCode,
                                std::move(cloudConnection));
                        });
                });
            return;
        }

        default:
            NX_ASSERT(false);
            handler(SystemError::hostUnreach);
    }
}

SystemError::ErrorCode CloudStreamSocket::applyRealNonBlockingMode(
    AbstractStreamSocket* streamSocket)
{
    SystemError::ErrorCode errorCode = SystemError::noError;

    if (!m_socketAttributes.nonBlockingMode)
    {
        //restoring default non blocking mode on socket
        if (!streamSocket->setNonBlockingMode(false))
        {
            errorCode = SystemError::getLastOSErrorCode();
            NX_ASSERT(errorCode != SystemError::noError);
        }
    }

    return errorCode;
}

void CloudStreamSocket::onDirectConnectDone(SystemError::ErrorCode errorCode)
{
    NX_LOGX(lm("onDirectConnectDone. %1").str(errorCode), cl_logDEBUG2);

    if (errorCode == SystemError::noError)
        errorCode = applyRealNonBlockingMode(m_socketDelegate.get());

    auto userHandler = std::move(m_connectHandler);
    userHandler(errorCode);  //this object can be freed in handler, so using local variable for handler
}

void CloudStreamSocket::onCloudConnectDone(
    SystemError::ErrorCode errorCode,
    std::unique_ptr<AbstractStreamSocket> cloudConnection)
{
    NX_LOGX(lm("onCloudConnectDone. %1").str(errorCode), cl_logDEBUG2);
    
    if (errorCode == SystemError::noError)
        errorCode = applyRealNonBlockingMode(cloudConnection.get());

    if (errorCode == SystemError::noError)
    {
        NX_ASSERT(cloudConnection->getAioThread() == m_aioThreadBinder.getAioThread());
        m_socketDelegate = std::move(cloudConnection);
        setDelegate(m_socketDelegate.get());
    }
    else
    {
        NX_ASSERT(!cloudConnection);
    }
    auto userHandler = std::move(m_connectHandler);
    userHandler(errorCode);  //this object can be freed in handler, so using local variable for handler
}

void CloudStreamSocket::cancelIoWhileInAioThread(aio::EventType eventType)
{
    if (eventType == aio::etWrite || eventType == aio::etNone)
    {
        m_asyncConnectGuard->terminate(); // Breaks outgoing connects.
        nx::network::SocketGlobals::addressResolver().cancel(this);
    }

    if (eventType == aio::etNone)   // It means we need to cancel all I/O.
        m_aioThreadBinder.pleaseStopSync();

    if (eventType == aio::etNone || eventType == aio::etTimedOut)
        m_timer->cancelSync();

    if (eventType == aio::etNone || eventType == aio::etRead)
        m_readIoBinder->pleaseStopSync();

    if (eventType == aio::etNone || eventType == aio::etWrite)
        m_writeIoBinder->pleaseStopSync();

    if (m_socketDelegate)
        m_socketDelegate->cancelIOSync(eventType);
}

void CloudStreamSocket::stopWhileInAioThread()
{
    m_asyncConnectGuard->terminate(); // Breaks outgoing connects.
    nx::network::SocketGlobals::addressResolver().cancel(this);

    m_timer.reset();
    m_readIoBinder.reset();
    m_writeIoBinder.reset();
    m_socketDelegate.reset();
}

} // namespace cloud
} // namespace network
} // namespace nx
