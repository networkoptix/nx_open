#include "cloud_stream_socket.h"

#include <future>

#include <utils/common/systemerror.h>

#include "cloud_tunnel.h"
#include "../socket_global.h"
#include "../system_socket.h"


namespace nx {
namespace network {
namespace cloud {

CloudStreamSocket::CloudStreamSocket()
:
    m_aioThreadBinder(SocketFactory::createDatagramSocket())
{
    //getAioThread binds to an aio thread
    m_socketAttributes.aioThread = m_aioThreadBinder->getAioThread();
}

CloudStreamSocket::~CloudStreamSocket()
{
}

bool CloudStreamSocket::bind(const SocketAddress& localAddress)
{
    //TODO #ak 
    return false;
}

SocketAddress CloudStreamSocket::getLocalAddress() const
{
    if (m_socketDelegate)
        return m_socketDelegate->getLocalAddress();

    return SocketAddress();
}

void CloudStreamSocket::close()
{
    if (m_socketDelegate)
        m_socketDelegate->close();
}

bool CloudStreamSocket::isClosed() const
{
    if (m_socketDelegate)
        return m_socketDelegate->isClosed();

    return true;
}

void CloudStreamSocket::shutdown()
{
    //TODO #ak interrupting blocking calls
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

template<typename Future>
bool waitFutureMs(Future& future, const boost::optional<unsigned int>& timeoutMillis)
{
    if (!timeoutMillis)
        return true;

    const auto timeout = std::chrono::milliseconds(*timeoutMillis);
    return future.wait_for(timeout) == std::future_status::ready;
}

bool CloudStreamSocket::connect(
    const SocketAddress& remoteAddress,
    unsigned int timeoutMillis)
{
    std::promise<SystemError::ErrorCode> promise;
    connectAsync(remoteAddress, [&](SystemError::ErrorCode code)
    {
        promise.set_value(code);
    });

    auto future = promise.get_future();
    if (!waitFutureMs(future, timeoutMillis))
    {
        SystemError::setLastErrorCode(SystemError::timedOut);
        return false;
    }

    auto result = future.get();
    if (result != SystemError::noError)
    {
        SystemError::setLastErrorCode(result);
        return false;
    }

    return true;
}

int CloudStreamSocket::recv(void* buffer, unsigned int bufferLen, int flags)
{
    Buffer tmpBuffer;
    tmpBuffer.reserve(bufferLen);

    int totallyRead = 0;
    do
    {
        const auto lastRead = recvImpl(&tmpBuffer);
        if (lastRead <= 0)
            return totallyRead;

        memcpy(static_cast<char*>(buffer) + totallyRead, tmpBuffer.data(), lastRead);
        totallyRead += lastRead;
    }
    while ((flags & MSG_WAITALL) && (totallyRead < bufferLen));

    return totallyRead;
}

int CloudStreamSocket::send(const void* buffer, unsigned int bufferLen)
{
    std::promise<std::pair<SystemError::ErrorCode, size_t>> promise;
    sendAsync(
        nx::Buffer(static_cast<const char*>(buffer), bufferLen),
        [&](SystemError::ErrorCode code, size_t size)
        {
            promise.set_value(std::make_pair(code, size));
        });

    auto future = promise.get_future();
    unsigned int sendTimeout = 0;
    if (!getSendTimeout(&sendTimeout))
        return -1;
    if (waitFutureMs(future, sendTimeout))
    {
        SystemError::setLastErrorCode(SystemError::timedOut);
        return -1;
    }

    auto result = future.get();
    if (result.first != SystemError::noError)
    {
        SystemError::setLastErrorCode(result.second);
        return -1;
    }

    return result.second;
}

SocketAddress CloudStreamSocket::getForeignAddress() const
{
    if (m_socketDelegate)
        return m_socketDelegate->getForeignAddress();

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
    std::function<void()> handler)
{
    if (eventType == aio::etWrite || eventType == aio::etNone)
        m_asyncGuard->terminate(); // breaks outgoing connects

    if (m_socketDelegate)
        m_socketDelegate->cancelIOAsync(eventType, std::move(handler));
}

void CloudStreamSocket::post( std::function<void()> handler )
{
    m_aioThreadBinder->post(std::move(handler));
}

void CloudStreamSocket::dispatch( std::function<void()> handler )
{
    m_aioThreadBinder->dispatch(std::move(handler));
}

void CloudStreamSocket::connectAsync(
    const SocketAddress& address,
    std::function<void(SystemError::ErrorCode)> handler)
{
    m_connectHandler = std::move(handler);

    auto sharedOperationGuard = m_asyncGuard.sharedGuard();
    const auto remotePort = address.port;
    nx::network::SocketGlobals::addressResolver().resolveAsync(
        address.address,
        [this, remotePort, sharedOperationGuard](
            SystemError::ErrorCode osErrorCode,
            std::vector<AddressEntry> dnsEntries)
        {
            auto operationLock = sharedOperationGuard->lock();
            if (!operationLock)
                return; //operation has been cancelled

            if (osErrorCode != SystemError::noError)
            {
                auto connectHandlerBak = std::move(m_connectHandler);
                connectHandlerBak(osErrorCode);
                return;
            }

            if (!startAsyncConnect(std::move(dnsEntries), remotePort))
            {
                auto connectHandlerBak = std::move(m_connectHandler);
                connectHandlerBak(SystemError::getLastOSErrorCode());
            }
        },
        true,
        this);   //TODO #ak resolve cancellation
}

void CloudStreamSocket::readSomeAsync(
    nx::Buffer* const buf,
    std::function<void(SystemError::ErrorCode, size_t)> handler)
{
    if (m_socketDelegate)
        m_socketDelegate->readSomeAsync(buf, std::move(handler));
    else
        m_aioThreadBinder->post(std::bind(handler, SystemError::notConnected, 0));
}

void CloudStreamSocket::sendAsync(
    const nx::Buffer& buf,
    std::function<void(SystemError::ErrorCode, size_t)> handler)
{
    if (m_socketDelegate)
        m_socketDelegate->sendAsync(buf, std::move(handler));
    else
        m_aioThreadBinder->post(std::bind(handler, SystemError::notConnected, 0));
}

void CloudStreamSocket::registerTimer(
    unsigned int timeoutMs,
    std::function<void()> handler)
{
    m_aioThreadBinder->registerTimer(timeoutMs, std::move(handler));
}

aio::AbstractAioThread* CloudStreamSocket::getAioThread()
{
    return m_aioThreadBinder->getAioThread();
}

void CloudStreamSocket::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    m_aioThreadBinder->bindToAioThread(aioThread);
}

bool CloudStreamSocket::startAsyncConnect(
    std::vector<AddressEntry> dnsEntries,
    int port)
{
    if (dnsEntries.empty())
    {
        SystemError::setLastErrorCode(SystemError::hostUnreach);
        return false;
    }

    //TODO #ak try every resolved address? Also, should prefer regular address to a cloud one
    const AddressEntry& dnsEntry = dnsEntries[0];
    switch (dnsEntry.type)
    {
        case AddressType::regular:
            //using tcp connection
            m_socketDelegate.reset(new TCPSocket(true));
            setDelegate(m_socketDelegate.get());
            m_socketDelegate->connectAsync(
                SocketAddress(std::move(dnsEntry.host), port),
                m_connectHandler);
            return true;

        case AddressType::cloud:
        case AddressType::unknown:  //if peer is unknown, trying to establish cloud connect
        {
            unsigned int sockSendTimeout = 0;
            if (!getSendTimeout(&sockSendTimeout))
                return false;

            //establishing cloud connect
            auto tunnel = SocketGlobals::tunnelPool().getTunnel(
                dnsEntry.host.toString().toLatin1());
            assert(tunnel);

            unsigned int sendTimeoutMillis = 0;
            if (!getSendTimeout(&sendTimeoutMillis))
                return false;
            auto sharedOperationGuard = m_asyncGuard.sharedGuard();
            tunnel->connect(
                std::chrono::milliseconds(sendTimeoutMillis),
                getSocketAttributes(),
                [this, sharedOperationGuard](
                    SystemError::ErrorCode errorCode,
                    std::unique_ptr<AbstractStreamSocket> cloudConnection)
                {
                    auto operationLock = sharedOperationGuard->lock();
                    if (!operationLock)
                        return; //operation has been cancelled
                    cloudConnectDone(
                        errorCode,
                        std::move(cloudConnection));
                });
        }

        default:
            assert(false);
            SystemError::setLastErrorCode(SystemError::hostUnreach);
            return false;
    }
}

int CloudStreamSocket::recvImpl(nx::Buffer* const buf)
{
    std::promise<std::pair<SystemError::ErrorCode, size_t>> promise;
    sendAsync(
        *buf,
        [&](SystemError::ErrorCode code, size_t size)
        {
            promise.set_value(std::make_pair(code, size));
        });

    auto future = promise.get_future();
    unsigned int recvTimeout = 0;
    if (!getRecvTimeout(&recvTimeout))
        return -1;
    if (waitFutureMs(future, recvTimeout))
    {
        SystemError::setLastErrorCode(SystemError::timedOut);
        return -1;
    }

    auto result = future.get();
    if (result.first != SystemError::noError)
    {
        SystemError::setLastErrorCode(result.first);
        return -1;
    }

    return result.second;
}

void CloudStreamSocket::cloudConnectDone(
    SystemError::ErrorCode errorCode,
    std::unique_ptr<AbstractStreamSocket> cloudConnection)
{
    if (errorCode == SystemError::noError)
    {
        m_socketDelegate = std::move(cloudConnection);
        assert(cloudConnection->getAioThread() == m_aioThreadBinder->getAioThread());
        setDelegate(m_socketDelegate.get());
    }
    else
    {
        assert(!cloudConnection);
    }
    auto userHandler = std::move(m_connectHandler);
    userHandler(errorCode);  //this object can be freed in handler, so using local variable for handler
}

} // namespace cloud
} // namespace network
} // namespace nx
