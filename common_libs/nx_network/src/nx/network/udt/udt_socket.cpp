#include "udt_socket.h"

#ifdef _WIN32
#  include <ws2tcpip.h>
#  include <iphlpapi.h>
#else
#  include <netinet/tcp.h>
#endif

#include <mutex>
#include <set>

#include <boost/optional.hpp>

#include <udt/udt.h>

#include <nx/network/aio/aio_service.h>
#include <nx/network/system_socket.h>
#include <nx/utils/std/future.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>

#include "udt_common.h"
#include "udt_socket_impl.h"
#include "../aio/async_socket_helper.h"
#include "../address_resolver.h"
#ifdef _WIN32
#include "../win32_socket_tools.h"
#endif
#include "../system_socket_address.h"

namespace nx {
namespace network {
namespace detail {

// TODO: #ak we have UdtSocketImpl and UDTSocketImpl! refactor!
class UdtSocketImpl:
    public UDTSocketImpl
{
public:
    UdtSocketImpl() = default;

    UdtSocketImpl(UDTSOCKET socket):
        UDTSocketImpl(socket)
    {
    }

private:
    UdtSocketImpl(const UdtSocketImpl&);
    UdtSocketImpl& operator=(const UdtSocketImpl&);
};

struct UdtEpollHandlerHelper
{
    UdtEpollHandlerHelper(int fd, UDTSOCKET udt_handler):
        epoll_fd(fd)
    {
#ifndef TRACE_UDT_SOCKET
        Q_UNUSED(udt_handler);
#endif
    }
    ~UdtEpollHandlerHelper()
    {
        if (epoll_fd >= 0)
        {
            int ret = UDT::epoll_release(epoll_fd);
#ifndef TRACE_UDT_SOCKET
            Q_UNUSED(ret);
#endif
        }
    }

    int epoll_fd;
    UDTSOCKET udt_handler;
};


static void setLastSystemErrorCodeAppropriately()
{
    const auto systemErrorCode =
        detail::convertToSystemError(UDT::getlasterror().getErrorCode());
    NX_ASSERT(systemErrorCode != SystemError::noError);
    SystemError::setLastErrorCode(
        systemErrorCode == SystemError::noError
        ? SystemError::invalidData
        : systemErrorCode);
}

}// namespace detail

 //=================================================================================================
 // UdtSocket implementation.

template<typename InterfaceToImplement>
UdtSocket<InterfaceToImplement>::UdtSocket(int ipVersion):
    UdtSocket(
        ipVersion,
        new detail::UdtSocketImpl(),
        detail::SocketState::closed)
{
    SocketGlobals::customInit(
        reinterpret_cast<SocketGlobals::CustomInit>(&UDT::startup),
        reinterpret_cast<SocketGlobals::CustomDeinit>(&UDT::cleanup));
}

template<typename InterfaceToImplement>
UdtSocket<InterfaceToImplement>::~UdtSocket()
{
    // TODO: #ak If socket is destroyed in its aio thread, it can cleanup here.

    NX_CRITICAL(
        !nx::network::SocketGlobals::isInitialized() ||
        !nx::network::SocketGlobals::aioService()
        .isSocketBeingMonitored(static_cast<Pollable*>(this)),
        "You MUST cancel running async socket operation before "
        "deleting socket if you delete socket from non-aio thread");

    if (!isClosed())
        close();
}

template<typename InterfaceToImplement>
UdtSocket<InterfaceToImplement>::UdtSocket(
    int ipVersion,
    detail::UdtSocketImpl* impl,
    detail::SocketState state)
    :
    Pollable(-1, std::unique_ptr<detail::UdtSocketImpl>(impl)),
    m_state(state),
    m_ipVersion(ipVersion)
{
    NX_CRITICAL((SocketGlobals::initializationFlags() & InitializationFlags::disableUdt) == 0);

    this->m_impl = static_cast<detail::UdtSocketImpl*>(this->Pollable::m_impl.get());
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::bindToUdpSocket(UDPSocket&& udpSocket)
{
    if (!udpSocket.setNonBlockingMode(false))
        return false;

    // Taking system socket out of udpSocket.
    if (UDT::bind2(m_impl->udtHandle, udpSocket.handle()) != 0)
    {
        detail::setLastSystemErrorCodeAppropriately();
        return false;
    }
    udpSocket.takeHandle();
    return true;
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::bind(const SocketAddress& localAddress)
{
    SystemSocketAddress addr(localAddress, m_ipVersion);

    int ret = UDT::bind(m_impl->udtHandle, addr.ptr.get(), addr.size);
    if (ret != 0)
        detail::setLastSystemErrorCodeAppropriately();
    return ret == 0;
}

template<typename InterfaceToImplement>
SocketAddress UdtSocket<InterfaceToImplement>::getLocalAddress() const
{
    SystemSocketAddress addr(m_ipVersion);
    if (UDT::getsockname(m_impl->udtHandle, addr.ptr.get(), reinterpret_cast<int*>(&addr.size)) != 0)
    {
        detail::setLastSystemErrorCodeAppropriately();
        return SocketAddress();
    }
    else
    {
        return addr.toSocketAddress();
    }
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::close()
{
    if (m_impl->udtHandle == UDT::INVALID_SOCK)
        return true; //< Already closed.

                     // TODO: #ak linger MUST be optional.
                     //  set UDT_LINGER to 1 if socket is in blocking mode?
    struct linger lingerVal;
    memset(&lingerVal, 0, sizeof(lingerVal));
    lingerVal.l_onoff = 1;
    lingerVal.l_linger = 7; //TODO #ak why 7?
    UDT::setsockopt(m_impl->udtHandle, 0, UDT_LINGER, &lingerVal, sizeof(lingerVal));

#ifdef TRACE_UDT_SOCKET
    NX_LOG(lit("closing UDT socket %1").arg(udtHandle), cl_logDEBUG2);
#endif

    const int ret = UDT::close(m_impl->udtHandle);
    m_impl->udtHandle = UDT::INVALID_SOCK;
    m_state = detail::SocketState::closed;
    return ret == 0;
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::shutdown()
{
    // TODO #ak: Implementing shutdown via close may lead to undefined behavior in case of handle reuse.
    // But, UDT allocates socket handles as atomic increment, so handle reuse is
    // hard to imagine in real life.

#if 0   //no LINGER
    const int ret = UDT::close(m_impl->udtHandle);
    m_impl->udtHandle = UDT::INVALID_SOCK;
    m_state = detail::SocketState::closed;
    return ret == 0;

    return false;
#else
    return close();
#endif
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::isClosed() const
{
    return m_state == detail::SocketState::closed;
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::setReuseAddrFlag(bool reuseAddr)
{
    NX_ASSERT(!isClosed());
    int reuseAddrInt = reuseAddr ? 1 : 0;
    int ret = UDT::setsockopt(m_impl->udtHandle, 0, UDT_REUSEADDR, &reuseAddrInt, sizeof(reuseAddrInt));
    if (ret != 0)
        detail::setLastSystemErrorCodeAppropriately();
    return ret == 0;
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::getReuseAddrFlag(bool* val) const
{
    NX_ASSERT(!isClosed());
    int len = sizeof(*val);
    int ret = UDT::getsockopt(m_impl->udtHandle, 0, UDT_REUSEADDR, val, &len);
    if (ret != 0)
        detail::setLastSystemErrorCodeAppropriately();
    return ret == 0;
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::setReusePortFlag(bool /*value*/)
{
    return true;
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::getReusePortFlag(bool* value) const
{
    return getReuseAddrFlag(value);
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::setNonBlockingMode(bool val)
{
    NX_ASSERT(!isClosed());
    bool value = !val;
    int ret = UDT::setsockopt(m_impl->udtHandle, 0, UDT_SNDSYN, &value, sizeof(value));
    if (ret != 0)
    {
        detail::setLastSystemErrorCodeAppropriately();
        return false;
    }
    ret = UDT::setsockopt(m_impl->udtHandle, 0, UDT_RCVSYN, &value, sizeof(value));
    if (ret != 0)
        detail::setLastSystemErrorCodeAppropriately();
    return ret == 0;
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::getNonBlockingMode(bool* val) const
{
    NX_ASSERT(!isClosed());
    int len = sizeof(*val);
    int ret = UDT::getsockopt(m_impl->udtHandle, 0, UDT_SNDSYN, val, &len);

    if (ret != 0)
        detail::setLastSystemErrorCodeAppropriately();

    *val = !*val;
    return ret == 0;
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::getMtu(unsigned int* mtuValue) const
{
    int valueLength = sizeof(*mtuValue);
    int ret = UDT::getsockopt(m_impl->udtHandle, 0, UDT_MSS, mtuValue, &valueLength);
    if (ret != 0)
        detail::setLastSystemErrorCodeAppropriately();
    return ret == 0;
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::setSendBufferSize(unsigned int buffSize)
{
    NX_ASSERT(!isClosed());
    NX_ASSERT(buffSize < static_cast<unsigned int>(std::numeric_limits<int>::max()));
    int ret = UDT::setsockopt(m_impl->udtHandle, 0, UDT_SNDBUF, &buffSize, sizeof(buffSize));
    if (ret != 0)
        detail::setLastSystemErrorCodeAppropriately();
    return ret == 0;
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::getSendBufferSize(unsigned int* buffSize) const
{
    NX_ASSERT(!isClosed());
    int len = sizeof(*buffSize);
    int ret = UDT::getsockopt(m_impl->udtHandle, 0, UDT_SNDBUF, buffSize, &len);
    if (ret != 0)
        detail::setLastSystemErrorCodeAppropriately();
    return ret == 0;
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::setRecvBufferSize(unsigned int buffSize)
{
    NX_ASSERT(!isClosed());
    NX_ASSERT(buffSize < static_cast<unsigned int>(std::numeric_limits<int>::max()));
    int ret = UDT::setsockopt(m_impl->udtHandle, 0, UDT_RCVBUF, &buffSize, sizeof(buffSize));
    if (ret != 0)
        detail::setLastSystemErrorCodeAppropriately();
    return ret == 0;
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::getRecvBufferSize(unsigned int* buffSize) const
{
    NX_ASSERT(!isClosed());
    int len = sizeof(*buffSize);
    int ret = UDT::getsockopt(m_impl->udtHandle, 0, UDT_RCVBUF, buffSize, &len);
    if (ret != 0)
        detail::setLastSystemErrorCodeAppropriately();
    return ret == 0;
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::setRecvTimeout(unsigned int millis)
{
    NX_ASSERT(!isClosed());
    NX_ASSERT(millis < static_cast<unsigned int>(std::numeric_limits<int>::max()));
    int time = millis ? static_cast<int>(millis) : -1;
    int ret = UDT::setsockopt(
        m_impl->udtHandle, 0, UDT_RCVTIMEO, &time, sizeof(time));
    if (ret == 0)
        m_readTimeoutMS = millis;
    else
        detail::setLastSystemErrorCodeAppropriately();
    return ret == 0;
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::getRecvTimeout(unsigned int* millis) const
{
    NX_ASSERT(!isClosed());
    int time;
    int len = sizeof(time);
    int ret = UDT::getsockopt(
        m_impl->udtHandle, 0, UDT_RCVTIMEO, &time, &len);
    *millis = (time == -1) ? 0 : static_cast<unsigned int>(time);
    if (ret != 0)
        detail::setLastSystemErrorCodeAppropriately();
    return ret == 0;
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::setSendTimeout(unsigned int ms)
{
    NX_ASSERT(!isClosed());
    NX_ASSERT(ms < static_cast<unsigned int>(std::numeric_limits<int>::max()));
    int time = ms ? static_cast<int>(ms) : -1;
    int ret = UDT::setsockopt(
        m_impl->udtHandle, 0, UDT_SNDTIMEO, &time, sizeof(time));
    if (ret == 0)
        m_writeTimeoutMS = ms;
    else
        detail::setLastSystemErrorCodeAppropriately();
    return ret == 0;
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::getSendTimeout(unsigned int* millis) const
{
    NX_ASSERT(!isClosed());
    int time;
    int len = sizeof(time);
    int ret = UDT::getsockopt(
        m_impl->udtHandle, 0, UDT_SNDTIMEO, &time, &len);
    *millis = (time == -1) ? 0 : static_cast<unsigned int>(time);
    if (ret != 0)
        detail::setLastSystemErrorCodeAppropriately();
    return ret == 0;
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::getLastError(SystemError::ErrorCode* /*errorCode*/) const
{
    SystemError::setLastErrorCode(SystemError::notImplemented);
    return false;
}

template<typename InterfaceToImplement>
AbstractSocket::SOCKET_HANDLE UdtSocket<InterfaceToImplement>::handle() const
{
    NX_ASSERT(!isClosed());
    static_assert(
        sizeof(UDTSOCKET) <= sizeof(AbstractSocket::SOCKET_HANDLE),
        "One have to fix UdtSocket<InterfaceToImplement>::handle() implementation");
    return static_cast<AbstractSocket::SOCKET_HANDLE>(m_impl->udtHandle);
}

template<typename InterfaceToImplement>
aio::AbstractAioThread* UdtSocket<InterfaceToImplement>::getAioThread() const
{
    return Pollable::getAioThread();
}

template<typename InterfaceToImplement>
void UdtSocket<InterfaceToImplement>::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    Pollable::bindToAioThread(aioThread);
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::isInSelfAioThread() const
{
    return Pollable::isInSelfAioThread();
}

template<typename InterfaceToImplement>
Pollable* UdtSocket<InterfaceToImplement>::pollable()
{
    return this;
}

template<typename InterfaceToImplement>
void UdtSocket<InterfaceToImplement>::post(nx::utils::MoveOnlyFunc<void()> handler)
{
    nx::network::SocketGlobals::aioService().post(
        static_cast<Pollable*>(this),
        std::move(handler));
}

template<typename InterfaceToImplement>
void UdtSocket<InterfaceToImplement>::dispatch(nx::utils::MoveOnlyFunc<void()> handler)
{
    nx::network::SocketGlobals::aioService().dispatch(
        static_cast<Pollable*>(this),
        std::move(handler));
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::open()
{
    NX_ASSERT(isClosed());
    m_impl->udtHandle = UDT::socket(m_ipVersion, SOCK_STREAM, 0);
    if (m_impl->udtHandle == UDT::INVALID_SOCK)
    {
        detail::setLastSystemErrorCodeAppropriately();
        return false;
    }

    // Tuning udt socket.
    constexpr const int kPacketsCoeff = 2;
    constexpr const int kBytesCoeff = 6;

    constexpr const int kMtuSize = 1400;
    constexpr const int kMaximumUdtWindowSizePacketsBase = 64;
    constexpr const int kMaximumUdtWindowSizePackets =
        kMaximumUdtWindowSizePacketsBase * kPacketsCoeff;

    constexpr const int kUdtBufferMultiplier = 48;
    constexpr const int kUdtSendBufSize = kUdtBufferMultiplier * kMtuSize * kBytesCoeff;
    constexpr const int kUdtRecvBufSize = kUdtBufferMultiplier * kMtuSize * kBytesCoeff;

    constexpr const int kUdpBufferMultiplier = 64;
    constexpr const int kUdpSendBufSize = kUdpBufferMultiplier * kMtuSize * kBytesCoeff;
    constexpr const int kUdpRecvBufSize = kUdpBufferMultiplier * kMtuSize * kBytesCoeff;

    if (UDT::setsockopt(
        m_impl->udtHandle, 0, UDT_MSS,
        &kMtuSize, sizeof(kMtuSize)) != 0
        ||
        UDT::setsockopt(
            m_impl->udtHandle, 0, UDT_FC,
            &kMaximumUdtWindowSizePackets, sizeof(kMaximumUdtWindowSizePackets)) != 0
        ||
        UDT::setsockopt(
            m_impl->udtHandle, 0, UDT_SNDBUF,
            &kUdtSendBufSize, sizeof(kUdtSendBufSize)) != 0
        ||
        UDT::setsockopt(
            m_impl->udtHandle, 0, UDT_RCVBUF,
            &kUdtRecvBufSize, sizeof(kUdtRecvBufSize)) != 0
        ||
        UDT::setsockopt(
            m_impl->udtHandle, 0, UDP_SNDBUF,
            &kUdpSendBufSize, sizeof(kUdpSendBufSize)) != 0
        ||
        UDT::setsockopt(
            m_impl->udtHandle, 0, UDP_RCVBUF,
            &kUdpRecvBufSize, sizeof(kUdpRecvBufSize)) != 0
        )
    {
        detail::setLastSystemErrorCodeAppropriately();
        UDT::close(m_impl->udtHandle);
        return false;
    }

    m_state = detail::SocketState::open;
    return true;
}

template class UdtSocket<AbstractStreamSocket>;
template class UdtSocket<AbstractStreamServerSocket>;


//=================================================================================================
// UdtStreamSocket implementation.

UdtStreamSocket::UdtStreamSocket(int ipVersion):
    base_type(ipVersion),
    m_aioHelper(new aio::AsyncSocketImplHelper<UdtStreamSocket>(this, ipVersion))
{
    open();
}

UdtStreamSocket::UdtStreamSocket(
    int ipVersion,
    detail::UdtSocketImpl* impl,
    detail::SocketState state)
    :
    base_type(ipVersion, impl, state),
    m_aioHelper(new aio::AsyncSocketImplHelper<UdtStreamSocket>(this, ipVersion))
{
    if (state == detail::SocketState::connected)
        m_isInternetConnection = !getForeignAddress().address.isLocal();
}

UdtStreamSocket::~UdtStreamSocket()
{
    m_aioHelper->terminate();
}

bool UdtStreamSocket::setRendezvous(bool val)
{
    return UDT::setsockopt(
        m_impl->udtHandle, 0, UDT_RENDEZVOUS, &val, sizeof(bool)) == 0;
}

bool UdtStreamSocket::connect(
    const SocketAddress& remoteAddress,
    unsigned int timeoutMs)
{
    if (remoteAddress.address.isIpAddress())
        return connectToIp(remoteAddress, timeoutMs);

    std::deque<HostAddress> ips;
    const SystemError::ErrorCode resultCode =
        SocketGlobals::addressResolver().dnsResolver().resolveSync(
            remoteAddress.address.toString(), m_ipVersion, &ips);
    if (resultCode != SystemError::noError)
    {
        SystemError::setLastErrorCode(resultCode);
        return false;
    }

    while (!ips.empty())
    {
        auto ip = std::move(ips.front());
        ips.pop_front();
        if (connectToIp(SocketAddress(std::move(ip), remoteAddress.port), timeoutMs))
            return true;
    }

    return false; //< Could not connect by any of addresses.
}

int UdtStreamSocket::recv(void* buffer, unsigned int bufferLen, int flags)
{
    if (m_impl->udtHandle == UDT::INVALID_SOCK)
    {
        SystemError::setLastErrorCode(SystemError::badDescriptor);
        return -1;
    }

    ScopeGuard<std::function<void()>> socketModeGuard;

    boost::optional<bool> newRecvMode;
    if (!checkIfRecvModeSwitchIsRequired(flags, &newRecvMode))
        return -1;

    if (newRecvMode)
    {
        if (!setRecvMode(*newRecvMode))
            return -1;
        socketModeGuard = ScopeGuard<std::function<void()>>(
            [newRecvMode = *newRecvMode, this]() { setRecvMode(!newRecvMode); });
    }

    const int bytesRead = UDT::recv(
        m_impl->udtHandle, reinterpret_cast<char*>(buffer), bufferLen, 0);

    return handleRecvResult(bytesRead);
}

int UdtStreamSocket::send(const void* buffer, unsigned int bufferLen)
{
    int sendResult = UDT::send(m_impl->udtHandle, reinterpret_cast<const char*>(buffer), bufferLen, 0);
    if (sendResult == UDT::ERROR)
    {
        const auto sysErrorCode =
            detail::convertToSystemError(UDT::getlasterror().getErrorCode());

        if (socketCannotRecoverFromError(sysErrorCode))
            m_state = detail::SocketState::open;

        SystemError::setLastErrorCode(sysErrorCode);
    }
    else if (sendResult == 0)
    {
        // Connection has been closed.
        m_state = detail::SocketState::open;
    }
    else if (sendResult > 0)
    {
        if (m_isInternetConnection)
            UdtStatistics::global.internetBytesTransfered += (size_t)sendResult;
    }

    return sendResult;
}

SocketAddress UdtStreamSocket::getForeignAddress() const
{
    SystemSocketAddress addr(m_ipVersion);
    if (UDT::getpeername(m_impl->udtHandle, addr.ptr.get(), reinterpret_cast<int*>(&addr.size)) != 0)
    {
        detail::setLastSystemErrorCodeAppropriately();
        return SocketAddress();
    }
    else
    {
        return addr.toSocketAddress();
    }
}

bool UdtStreamSocket::isConnected() const
{
    return m_state == detail::SocketState::connected;
}

bool UdtStreamSocket::reopen()
{
    if (isClosed())
        return open();
    return true;
}

void UdtStreamSocket::cancelIOAsync(
    aio::EventType eventType,
    nx::utils::MoveOnlyFunc<void()> cancellationDoneHandler)
{
    return m_aioHelper->cancelIOAsync(
        eventType,
        std::move(cancellationDoneHandler));
}

void UdtStreamSocket::cancelIOSync(aio::EventType eventType)
{
    m_aioHelper->cancelIOSync(eventType);
}

bool UdtStreamSocket::setNoDelay(bool value)
{
    // Udt does not support it. Returned true so that caller code works.
    m_noDelay = value;
    return true;
}

bool UdtStreamSocket::getNoDelay(bool* value) const
{
    *value = m_noDelay;
    return true;
}

bool UdtStreamSocket::toggleStatisticsCollection(bool /*val*/)
{
    SystemError::setLastErrorCode(SystemError::notImplemented);
    return false;
}

bool UdtStreamSocket::getConnectionStatistics(StreamSocketInfo* /*info*/)
{
    SystemError::setLastErrorCode(SystemError::notImplemented);
    return false;
}

bool UdtStreamSocket::setKeepAlive(boost::optional< KeepAliveOptions > /*info*/)
{
    SystemError::setLastErrorCode(SystemError::notImplemented);
    return false; // not implemented yet
}

bool UdtStreamSocket::getKeepAlive(boost::optional< KeepAliveOptions >* result) const
{
    // UDT has keep-alives but provides no way to modify it...
    (*result)->probeCount = 10; // TODO: #ak find real value in udt.
    (*result)->inactivityPeriodBeforeFirstProbe = std::chrono::seconds(5);
    (*result)->probeSendPeriod = std::chrono::seconds(5);
    return true;
}

void UdtStreamSocket::connectAsync(
    const SocketAddress& addr,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    return m_aioHelper->connectAsync(
        addr,
        [this, handler = std::move(handler)](SystemError::ErrorCode errorCode) mutable
        {
            if (socketCannotRecoverFromError(errorCode))
                m_state = detail::SocketState::open;
            handler(errorCode);
        });
}

void UdtStreamSocket::readSomeAsync(
    nx::Buffer* const buf,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, size_t)> handler)
{
    return m_aioHelper->readSomeAsync(buf, std::move(handler));
}

void UdtStreamSocket::sendAsync(
    const nx::Buffer& buf,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, size_t)> handler)
{
    return m_aioHelper->sendAsync(buf, std::move(handler));
}

void UdtStreamSocket::registerTimer(
    std::chrono::milliseconds timeoutMillis,
    nx::utils::MoveOnlyFunc<void()> handler)
{
    return m_aioHelper->registerTimer(timeoutMillis, std::move(handler));
}

bool UdtStreamSocket::connectToIp(
    const SocketAddress& remoteAddress,
    unsigned int /*timeoutMs*/)
{
    // TODO: #ak use timeoutMs.

    NX_ASSERT(m_state == detail::SocketState::open);

    SystemSocketAddress addr(remoteAddress, m_ipVersion);

    // The official documentation doesn't advice using select but here we just need
    // to wait on a single socket fd, select is way more faster than epoll on linux
    // since epoll needs to create the internal structure inside of kernel.
    bool nbk_sock = false;
    if (!getNonBlockingMode(&nbk_sock))
        return false;
    if (!nbk_sock)
    {
        if (!setNonBlockingMode(nbk_sock))
            return false;
    }
    int ret = UDT::connect(m_impl->udtHandle, addr.ptr.get(), addr.size);
    // The UDT connect will always return zero even if such operation is async which is
    // different with the existed Posix/Win32 socket design. So if we meet an non-zero
    // value, the only explanation is an error happened which cannot be solved.
    if (ret != 0)
    {
        // Error happened
        detail::setLastSystemErrorCodeAppropriately();
        return false;
    }
    m_state = detail::SocketState::connected;
    m_isInternetConnection = !getForeignAddress().address.isLocal();
    return true;
}

bool UdtStreamSocket::checkIfRecvModeSwitchIsRequired(
    int flags,
    boost::optional<bool>* requiredRecvMode)
{
    *requiredRecvMode = boost::none;

    if ((flags & (MSG_DONTWAIT | MSG_WAITALL)) == 0)
        return true;

    bool currentMode = false;
    int len = sizeof(currentMode);
    int ret = UDT::getsockopt(m_impl->udtHandle, 0, UDT_RCVSYN, &currentMode, &len);
    if (ret != 0)
    {
        detail::setLastSystemErrorCodeAppropriately();
        return false;
    }

    bool requiredRecvModeTmp = false;
    if (flags & MSG_DONTWAIT)
        requiredRecvModeTmp = false;
    else if (flags & MSG_WAITALL)
        requiredRecvModeTmp = true;

    if (currentMode != requiredRecvModeTmp)
        *requiredRecvMode = requiredRecvModeTmp;

    return true;
}

bool UdtStreamSocket::setRecvMode(bool isRecvSync)
{
    auto ret = UDT::setsockopt(m_impl->udtHandle, 0, UDT_RCVSYN, &isRecvSync, sizeof(isRecvSync));
    if (ret != 0)
        detail::setLastSystemErrorCodeAppropriately();

    return ret == 0;
}

int UdtStreamSocket::handleRecvResult(int recvResult)
{
    if (recvResult == UDT::ERROR)
    {
        const int udtErrorCode = UDT::getlasterror().getErrorCode();
        const auto sysErrorCode = detail::convertToSystemError(udtErrorCode);

        if (socketCannotRecoverFromError(sysErrorCode))
            m_state = detail::SocketState::open;

        // UDT doesn't translate the EOF into a recv with zero return, but instead
        // it returns error with 2001 error code. We need to detect this and translate
        // back with a zero return here .
        if (udtErrorCode == CUDTException::ECONNLOST)
        {
            return 0;
        }
        else if (udtErrorCode == CUDTException::EINVSOCK)
        {
            // This is another very ugly hack since after our patch for UDT.
            // UDT cannot distinguish a clean close or a crash. And I cannot
            // come up with perfect way to patch the code and make it work.
            // So we just hack here. When we encounter an error Invalid socket,
            // it should be a clean close when we use a epoll.
            return 0;
        }
        SystemError::setLastErrorCode(sysErrorCode);
    }
    else if (recvResult == 0)
    {
        //connection has been closed
        m_state = detail::SocketState::open;
    }
    else if (recvResult > 0)
    {
        if (m_isInternetConnection)
            UdtStatistics::global.internetBytesTransfered += (size_t)recvResult;
    }

    return recvResult;
}

//=================================================================================================
// UdtStreamServerSocket implementation.

UdtStreamServerSocket::UdtStreamServerSocket(int ipVersion):
    base_type(ipVersion),
    m_aioHelper(new aio::AsyncServerSocketHelper<UdtStreamServerSocket>(this))
{
    open();
}

UdtStreamServerSocket::~UdtStreamServerSocket()
{
    if (isInSelfAioThread())
        stopWhileInAioThread();
}

bool UdtStreamServerSocket::listen(int backlog)
{
    NX_ASSERT(m_state == detail::SocketState::open);
    int ret = UDT::listen(m_impl->udtHandle, backlog);
    if (ret != 0)
    {
        detail::setLastSystemErrorCodeAppropriately();
        return false;
    }
    else
    {
        m_state = detail::SocketState::connected;
        return true;
    }
}

AbstractStreamSocket* UdtStreamServerSocket::accept()
{
    bool isNonBlocking = false;
    if (!getNonBlockingMode(&isNonBlocking))
        return nullptr;

    if (isNonBlocking)
        return systemAccept();

    // This method always blocks.
    nx::utils::promise<
        std::pair<SystemError::ErrorCode, std::unique_ptr<AbstractStreamSocket>>
    > acceptedSocketPromise;

    if (!setNonBlockingMode(true))
        return nullptr;

    acceptAsync(
        [this, &acceptedSocketPromise](
            SystemError::ErrorCode errorCode, std::unique_ptr<AbstractStreamSocket> socket)
        {
            // Need post here to be sure that aio subsystem does not use socket anymore.
            post(
                [&acceptedSocketPromise, errorCode, socket = std::move(socket)]() mutable
                {
                    acceptedSocketPromise.set_value(
                        std::make_pair(errorCode, std::move(socket)));
                });
        });

    auto acceptedSocketPair = acceptedSocketPromise.get_future().get();

    if (!setNonBlockingMode(false))
        return nullptr;

    if (acceptedSocketPair.first != SystemError::noError)
    {
        SystemError::setLastErrorCode(acceptedSocketPair.first);
        return nullptr;
    }
    return acceptedSocketPair.second.release();
}

void UdtStreamServerSocket::acceptAsync(AcceptCompletionHandler handler)
{
    bool nonBlockingMode = false;
    if (!getNonBlockingMode(&nonBlockingMode))
    {
        const auto sysErrorCode = SystemError::getLastOSErrorCode();
        return post(
            [handler = std::move(handler), sysErrorCode]() { handler(sysErrorCode, nullptr); });
    }
    if (!nonBlockingMode)
    {
        return post(
            [handler = std::move(handler)]() { handler(SystemError::notSupported, nullptr); });
    }

    return m_aioHelper->acceptAsync(
        [handler = std::move(handler)](
            SystemError::ErrorCode errorCode,
            std::unique_ptr<AbstractStreamSocket> socket)
        {
            // Every accepted socket MUST be in blocking mode!
            if (socket)
            {
                if (!socket->setNonBlockingMode(false))
                {
                    socket.reset();
                    errorCode = SystemError::getLastOSErrorCode();
                }
            }
            handler(errorCode, std::move(socket));
        });
}

void UdtStreamServerSocket::cancelIOAsync(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_aioHelper->cancelIOAsync(std::move(handler));
}

void UdtStreamServerSocket::cancelIOSync()
{
    m_aioHelper->cancelIOSync();
}

void UdtStreamServerSocket::pleaseStop(
    nx::utils::MoveOnlyFunc< void() > completionHandler)
{
    // TODO #ak: Add general implementation to Socket class and remove this method.
    dispatch(
        [this, completionHandler = std::move(completionHandler)]()
        {
            stopWhileInAioThread();
            completionHandler();
        });
}

void UdtStreamServerSocket::pleaseStopSync(bool assertIfCalledUnderLock)
{
    if (isInSelfAioThread())
        stopWhileInAioThread();
    else
        QnStoppableAsync::pleaseStopSync(assertIfCalledUnderLock);
}

AbstractStreamSocket* UdtStreamServerSocket::systemAccept()
{
    NX_ASSERT(m_state == detail::SocketState::connected);
    UDTSOCKET ret = UDT::accept(m_impl->udtHandle, NULL, NULL);
    if (ret == UDT::INVALID_SOCK)
    {
        detail::setLastSystemErrorCodeAppropriately();
        return nullptr;
    }

#ifdef TRACE_UDT_SOCKET
    NX_LOGX(lit("accepted UDT socket %1").arg(ret), cl_logDEBUG2);
#endif
    auto acceptedSocket = new UdtStreamSocket(
        m_ipVersion,
        new detail::UdtSocketImpl(ret),
        detail::SocketState::connected);
    acceptedSocket->bindToAioThread(SocketGlobals::aioService().getRandomAioThread());

    if (!acceptedSocket->setSendTimeout(0) || !acceptedSocket->setRecvTimeout(0))
    {
        delete acceptedSocket;
        return nullptr;
    }

    return acceptedSocket;
}

void UdtStreamServerSocket::stopWhileInAioThread()
{
    m_aioHelper->stopPolling();
}

UdtStatistics UdtStatistics::global;

} // namespace network
} // namespace nx
