#include "cloud_stream_socket.h"

#include <future>

#include <utils/common/systemerror.h>

#include "cloud_tunnel.h"
#include "../socket_global.h"
#include "../system_socket.h"

namespace nx {
namespace network {
namespace cloud {

CloudStreamSocket::CloudStreamSocket(bool natTraversal)
    : m_nonBlockingMode(false)
    , m_socketOptions(new StreamSocketOptions)
{
    // TODO: mux probably should initialize m_socketOptions with default values

    // TODO: what to do with natTraversal?
    static_cast<void>(natTraversal);
}

SocketAddress CloudStreamSocket::getLocalAddress() const
{
    if (m_socketDelegate)
        return m_socketDelegate->getLocalAddress();

    if (m_socketOptions->boundAddress)
        return *m_socketOptions->boundAddress;

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

#ifdef CloudStreamSocket_setSocketOption
    #error CloudStreamSocket_setSocketOption macro is already defined.
#endif

#define CloudStreamSocket_setSocketOption(SETTER, TYPE, NAME)   \
    bool CloudStreamSocket::SETTER(TYPE NAME)                   \
    {                                                           \
        if (m_socketDelegate)                                   \
            return m_socketDelegate->SETTER(NAME);              \
                                                                \
        m_socketOptions->NAME = NAME;                           \
        return true;                                            \
    }

#ifdef CloudStreamSocket_getSocketOption
    #error CloudStreamSocket_getSocketOption macro is already defined.
#endif

#define CloudStreamSocket_getSocketOption(GETTER, TYPE, NAME)   \
    bool CloudStreamSocket::GETTER(TYPE* NAME) const            \
    {                                                           \
        if (m_socketDelegate)                                   \
            return m_socketDelegate->GETTER(NAME);              \
                                                                \
        if (!m_socketOptions->NAME)                             \
            return false;                                       \
                                                                \
        *NAME = *m_socketOptions->NAME;                         \
        return true;                                            \
    }

CloudStreamSocket_setSocketOption(bind, const SocketAddress&, boundAddress)

CloudStreamSocket_setSocketOption(setReuseAddrFlag, bool, reuseAddrFlag)
CloudStreamSocket_getSocketOption(getReuseAddrFlag, bool, reuseAddrFlag)

CloudStreamSocket_setSocketOption(setSendBufferSize, unsigned int, sendBufferSize)
CloudStreamSocket_getSocketOption(getSendBufferSize, unsigned int, sendBufferSize)

CloudStreamSocket_setSocketOption(setRecvBufferSize, unsigned int, recvBufferSize)
CloudStreamSocket_getSocketOption(getRecvBufferSize, unsigned int, recvBufferSize)

CloudStreamSocket_setSocketOption(setRecvTimeout, unsigned int, recvTimeout)
CloudStreamSocket_getSocketOption(getRecvTimeout, unsigned int, recvTimeout)

CloudStreamSocket_setSocketOption(setSendTimeout, unsigned int, sendTimeout)
CloudStreamSocket_getSocketOption(getSendTimeout, unsigned int, sendTimeout)

CloudStreamSocket_setSocketOption(setNoDelay, bool, noDelay)
CloudStreamSocket_getSocketOption(getNoDelay, bool, noDelay)

CloudStreamSocket_setSocketOption(toggleStatisticsCollection, bool, statCollect)

CloudStreamSocket_setSocketOption(setKeepAlive, boost::optional<KeepAliveOptions>,
                                  keepAliveOptions)
CloudStreamSocket_getSocketOption(getKeepAlive, boost::optional<KeepAliveOptions>,
                                  keepAliveOptions)

#undef CloudStreamSocket_setSocketOption
#undef CloudStreamSocket_getSocketOption

bool CloudStreamSocket::setNonBlockingMode(bool val)
{
    m_nonBlockingMode = val;
    return true;
}

bool CloudStreamSocket::getNonBlockingMode(bool* val) const
{
    *val = m_nonBlockingMode;
    return true;
}

bool CloudStreamSocket::getMtu(unsigned int* mtuValue) const
{
    if (m_socketDelegate)
        return m_socketDelegate->getMtu(mtuValue);

    return false;
}

bool CloudStreamSocket::getLastError(SystemError::ErrorCode* errorCode) const
{
    if (m_socketDelegate)
        return m_socketDelegate->getLastError(errorCode);

    // TODO: #mux Provide more realistic error codes
    *errorCode = SystemError::notImplemented;
    return true;
}

AbstractSocket::SOCKET_HANDLE CloudStreamSocket::handle() const
{
    if (m_socketDelegate)
        return m_socketDelegate->handle();

    // TODO: #mux Figure out what to do in such case
    return -1;
}

bool CloudStreamSocket::reopen()
{
    if (m_socketDelegate)
        return m_socketDelegate->reopen();

    return false;
}

bool CloudStreamSocket::getConnectionStatistics(StreamSocketInfo* info)
{
    if (m_socketDelegate)
        return m_socketDelegate->getConnectionStatistics(info);

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
    if (waitFutureMs(future, timeoutMillis))
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
    unsigned int bufferSize;
    if (!getRecvBufferSize(&bufferSize))
        return -1;

    Buffer tmpBuffer;
    tmpBuffer.reserve(bufferLen);

    int totalyRead(0);
    do
    {
        const auto lastRead = recvImpl(&tmpBuffer);
        if (lastRead <= 0)
            return lastRead;

        memcpy(buffer + totalyRead, tmpBuffer.data(), lastRead);
        totalyRead += lastRead;
    }
    while ((flags & MSG_WAITALL) && (totalyRead < bufferLen));

    return totalyRead;
}

int CloudStreamSocket::send(const void* buffer, unsigned int bufferLen)
{
    std::promise<std::pair<SystemError::ErrorCode, size_t>> promise;
    sendAsync(nx::Buffer(static_cast<const char*>(buffer), bufferLen),
                  [&](SystemError::ErrorCode code, size_t size)
    {
        promise.set_value(std::make_pair(code, size));
    });

    auto future = promise.get_future();
    if (waitFutureMs(future, m_socketOptions->sendTimeout))
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

void CloudStreamSocket::cancelIOAsync(aio::EventType eventType,
                                      std::function<void()> handler)
{
    if (eventType == aio::etWrite || eventType == aio::etNone)
        m_asyncGuard->terminate(); // breaks outgoing connects

    if (m_socketDelegate)
        m_socketDelegate->cancelIOAsync(eventType, std::move(handler));
}

void CloudStreamSocket::post( std::function<void()> handler )
{
    auto lock = m_asyncGuard->lock();
    if (m_socketDelegate)
    {
        lock.unlock();
        m_socketDelegate->post(std::move(handler));
    }

    lock.unlock();
    handler();
}

void CloudStreamSocket::dispatch( std::function<void()> handler )
{
    auto lock = m_asyncGuard->lock();
    if (m_socketDelegate)
    {
        lock.unlock();
        m_socketDelegate->dispatch(std::move(handler));
    }

    lock.unlock();
    handler();
}

void CloudStreamSocket::connectAsync(
    const SocketAddress& address,
    std::function<void(SystemError::ErrorCode)> handler)
{
    m_connectHandler = std::move(handler);

    auto sharedGuard = m_asyncGuard.sharedGuard();
    SocketGlobals::addressResolver().resolveAsync(
        address.address,
        [this, address, sharedGuard](SystemError::ErrorCode code,
                                     std::vector<AddressEntry> entries)
        {
            if (auto lk = sharedGuard->lock())
            {
                if (code == SystemError::noError)
                    if (startAsyncConnect(std::move(address), std::move(entries)))
                        return;
                    else
                        code = SystemError::hostUnreach;

                auto connectHandlerBak = std::move(m_connectHandler);
                lk.unlock();
                connectHandlerBak(code);
                return;
            }
        },
        true, this);
}

void CloudStreamSocket::readSomeAsync(
    nx::Buffer* const buf,
    std::function<void(SystemError::ErrorCode, size_t)> handler)
{
    if (m_socketDelegate)
        m_socketDelegate->readSomeAsync(buf, std::move(handler));
    else
        handler(SystemError::notConnected, 0);
}

void CloudStreamSocket::sendAsync(
    const nx::Buffer& buf,
    std::function<void(SystemError::ErrorCode, size_t)> handler)
{
    if (m_socketDelegate)
        m_socketDelegate->sendAsync(buf, std::move(handler));
    else
        handler(SystemError::notConnected, 0);
}

void CloudStreamSocket::registerTimer(
    unsigned int timeoutMs,
    std::function<void()> handler)
{
    // TODO: #mux shell we create extra pollable and use directly with AIOService?
}

bool CloudStreamSocket::startAsyncConnect(const SocketAddress& originalAddress,
                                          std::vector<AddressEntry> dnsEntries)
{
    std::vector<CloudConnectType> cloudConnectTypes;
    for (const auto entry : dnsEntries)
    {
        switch( entry.type )
        {
            case AddressType::regular:
            {
                SocketAddress target(entry.host, originalAddress.port);
                for(const auto& attr : entry.attributes)
                    if(attr.type == AddressAttributeType::nxApiPort)
                        target.port = static_cast<quint16>(attr.value);

                m_socketDelegate.reset(new TCPSocket(true));
                if (!m_socketOptions->apply(m_socketDelegate.get()))
                    return false;
                if (!m_socketDelegate->setNonBlockingMode(true))
                    return false;
                m_socketDelegate->connectAsync(
                    std::move(target), std::move(m_connectHandler));

                return true;
            }
            case AddressType::cloud:
            {
                auto ccType = CloudConnectType::unknown;
                for(const auto& attr : entry.attributes)
                    if(attr.type == AddressAttributeType::cloudConnect)
                        ccType = static_cast<CloudConnectType>(attr.value);

                cloudConnectTypes.push_back(ccType);
                break;
            }
            default:
                Q_ASSERT_X(false, Q_FUNC_INFO, "Unexpected AddressType value!");

        };
    }

    return tunnelConnect(originalAddress.address.toString().toUtf8(),
                         std::move(cloudConnectTypes));
}

int CloudStreamSocket::recvImpl(nx::Buffer* const buf)
{
    std::promise<std::pair<SystemError::ErrorCode, size_t>> promise;
    sendAsync(*buf, [&](SystemError::ErrorCode code, size_t size)
    {
        promise.set_value(std::make_pair(code, size));
    });

    auto future = promise.get_future();
    if (waitFutureMs(future, m_socketOptions->recvTimeout))
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

bool CloudStreamSocket::tunnelConnect(String peerId,
                                      std::vector<CloudConnectType> ccTypes)
{
    if (ccTypes.empty())
        return false;

    auto cloudTunnel = SocketGlobals::tunnelPool().get(
        peerId, std::move(ccTypes));

    auto sharedGuard = m_asyncGuard.sharedGuard();
    cloudTunnel->connect(m_socketOptions,
                         [this, sharedGuard](
        SystemError::ErrorCode errorCode,
        std::unique_ptr<AbstractStreamSocket> socket)
    {
        if (auto lk = sharedGuard->lock())
        {
            m_socketDelegate = std::move(socket);
            const auto handler = std::move(m_connectHandler);

            lk.unlock();
            handler(errorCode);
        }
    });

    return true;
}

} // namespace cloud
} // namespace network
} // namespace nx
