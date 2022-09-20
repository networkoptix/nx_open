// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_socket.h"

#include <atomic>
#include <memory>
#include <type_traits>

#include <nx/utils/system_error.h>
#include <nx/utils/log/log.h>
#include <nx/utils/platform/win32_syscall_resolver.h>

#ifdef _WIN32
#  include <iphlpapi.h>
#  include <Mstcpip.h>
#  include "win32_socket_tools.h"
#else
#  include <netinet/tcp.h>
#endif

#include <QtCore/QElapsedTimer>
#include <QtCore/QCollator>

#include "aio/aio_service.h"
#include "aio/async_socket_helper.h"
#include "compat_poll.h"
#include "common_socket_impl.h"
#include "address_resolver.h"

#ifdef _WIN32
    /* Check that the typedef in AbstractSocket is correct. */
    static_assert(
        std::is_same_v<nx::network::AbstractSocket::SOCKET_HANDLE, SOCKET>,
        "Invalid socket type is used in AbstractSocket.");
    typedef char raw_type;       // Type used for raw data on this platform
#else
    #include <sys/types.h>       // For data types
    #include <sys/socket.h>      // For socket(), connect(), send(), and recv()
    #include <netdb.h>           // For getaddrinfo()
    #include <arpa/inet.h>       // For inet_addr()
    #include <unistd.h>          // For close()
    #include <netinet/in.h>      // For sockaddr_in
    #include <netinet/tcp.h>      // For TCP_NODELAY
    #include <fcntl.h>
    typedef void raw_type;       // Type used for raw data on this platform
#endif

#ifndef SOCKET_ERROR
    #define SOCKET_ERROR (-1)
#endif

// Needed only for bpi build, that use old C Library headers.
#ifdef __linux__
    #include <sys/utsname.h>
    #ifndef IP_MULTICAST_ALL
        #define IP_MULTICAST_ALL 49
    #endif
#endif

namespace nx {
namespace network {

#if defined(__arm__) //< Some 32-bit ARM devices lack the kernel support for the atomic int64.
    qint64 totalSocketBytesSent() { return 0; }
#else
    static std::atomic<qint64> g_totalSocketBytesSent;
    qint64 totalSocketBytesSent() { return g_totalSocketBytesSent; }
#endif

#ifdef SO_REUSEPORT
    static bool isReusePortSupported()
    {
        static const bool value =
        #ifdef __linux__
            []
            {
                utsname kernel;
                if (uname(&kernel) != 0)
                {
                    NX_WARNING(NX_SCOPE_TAG, "Unable to get kernel info");
                    return false;
                }

                QCollator collator;
                collator.setNumericMode(true);

                // https://stackoverflow.com/questions/3261965/so-reuseport-on-linux
                const bool isSupported = collator.compare(QString(kernel.release), "3.9") >= 0;
                NX_INFO(NX_SCOPE_TAG, "Reuse port %1 supported on Linux kernel %2",
                    isSupported ? "IS": "IS NOT ", kernel.release);

                return isSupported;
            }();
        #else
            true;
        #endif
        return value;
    }
#endif

//-------------------------------------------------------------------------------------------------
// Socket implementation

template<typename SocketInterfaceToImplement>
Socket<SocketInterfaceToImplement>::~Socket()
{
    close();
}

template<typename SocketInterfaceToImplement>
bool Socket<SocketInterfaceToImplement>::getLastError(SystemError::ErrorCode* errorCode) const
{
    socklen_t optLen = sizeof(*errorCode);
    return getsockopt(m_fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(errorCode), &optLen) == 0;
}

template<typename SocketInterfaceToImplement>
AbstractSocket::SOCKET_HANDLE Socket<SocketInterfaceToImplement>::handle() const
{
    return Pollable::handle();
}

template<typename SocketInterfaceToImplement>
bool Socket<SocketInterfaceToImplement>::getRecvTimeout(unsigned int* millis) const
{
    return Pollable::getRecvTimeout(millis);
}

template<typename SocketInterfaceToImplement>
bool Socket<SocketInterfaceToImplement>::getSendTimeout(unsigned int* millis) const
{
    return Pollable::getSendTimeout(millis);
}

template<typename SocketInterfaceToImplement>
aio::AbstractAioThread* Socket<SocketInterfaceToImplement>::getAioThread() const
{
    return Pollable::getAioThread();
}

template<typename SocketInterfaceToImplement>
void Socket<SocketInterfaceToImplement>::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    return Pollable::bindToAioThread(aioThread);
}

template<typename SocketInterfaceToImplement>
bool Socket<SocketInterfaceToImplement>::isInSelfAioThread() const
{
    return Pollable::isInSelfAioThread();
}

template<typename SocketInterfaceToImplement>
bool Socket<SocketInterfaceToImplement>::bind(const SocketAddress& localAddress)
{
    const SystemSocketAddress addr(localAddress, m_ipVersion);
    if (!addr.get())
        return false;

    return ::bind(m_fd, addr.get(), addr.length()) == 0;
}

template<typename SocketInterfaceToImplement>
SocketAddress Socket<SocketInterfaceToImplement>::getLocalAddress() const
{
    if (m_ipVersion == AF_INET)
    {
        sockaddr_in addr;
        socklen_t addrLen = sizeof(addr);
        if (::getsockname(m_fd, (sockaddr*)&addr, &addrLen) < 0)
            return SocketAddress();
        return SocketAddress(addr);
    }
    else if (m_ipVersion == AF_INET6)
    {
        sockaddr_in6 addr;
        socklen_t addrLen = sizeof(addr);
        if (::getsockname(m_fd, (sockaddr*)&addr, &addrLen) < 0)
            return SocketAddress();
        return SocketAddress(addr);
    }

    return SocketAddress();
}

template<typename SocketInterfaceToImplement>
bool Socket<SocketInterfaceToImplement>::shutdown()
{
    if (m_fd == -1)
        return true;

#ifdef _WIN32
    return ::shutdown(m_fd, SD_BOTH) == 0;
#else
    return ::shutdown(m_fd, SHUT_RDWR) == 0;
#endif
}

template<typename SocketInterfaceToImplement>
bool Socket<SocketInterfaceToImplement>::close()
{
    if (m_fd == -1)
        return true;

    if (this->impl()->aioThread.wasAccessed() && this->impl()->aioThread->load())
    {
        NX_ASSERT(!this->impl()->aioThread->load()->isSocketBeingMonitored(this));
    }

    auto fd = m_fd;
    m_fd = -1;

#ifdef _WIN32
    return ::closesocket(fd) == 0;
#else
    return ::close(fd) == 0;
#endif
}

template<typename SocketInterfaceToImplement>
bool Socket<SocketInterfaceToImplement>::isClosed() const
{
    return m_fd == -1;
}

template<typename SocketInterfaceToImplement>
bool Socket<SocketInterfaceToImplement>::setReuseAddrFlag(bool reuseAddr)
{
    const int on = reuseAddr ? 1 : 0;
    if (::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on)))
        return false;

    return true;
}

template<typename SocketInterfaceToImplement>
bool Socket<SocketInterfaceToImplement>::getReuseAddrFlag(bool* val) const
{
    int reuseAddrVal = 0;
    socklen_t optLen = sizeof(reuseAddrVal);

    if (::getsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseAddrVal, &optLen))
        return false;

    *val = reuseAddrVal > 0;
    return true;
}

template<typename SocketInterfaceToImplement>
bool Socket<SocketInterfaceToImplement>::setReusePortFlag(
    bool value)
{
#if defined(_WIN32)
    return setReuseAddrFlag(value);
#else
    #if defined(SO_REUSEPORT)
        if (isReusePortSupported())
        {
            const int on = value ? 1 : 0;
            return ::setsockopt(m_fd, SOL_SOCKET, SO_REUSEPORT, (const char*) &on, sizeof(on)) == 0;
        }
    #endif
    SystemError::setLastErrorCode(SystemError::unknownProtocolOption);
    return false;
#endif
}

template<typename SocketInterfaceToImplement>
bool Socket<SocketInterfaceToImplement>::getReusePortFlag(
    bool* value) const
{
#if defined(_WIN32)
    return getReuseAddrFlag(value);
#else
    #if defined(SO_REUSEPORT)
        if (isReusePortSupported())
        {
            int reuseAddrVal = 0;
            socklen_t optLen = sizeof(reuseAddrVal);
            if (::getsockopt(m_fd, SOL_SOCKET, SO_REUSEPORT, (char*) &reuseAddrVal, &optLen))
                return false;

            *value = reuseAddrVal > 0;
            return true;
        }
    #endif
    SystemError::setLastErrorCode(SystemError::unknownProtocolOption);
    return false;
#endif
}

template<typename SocketInterfaceToImplement>
bool Socket<SocketInterfaceToImplement>::setNonBlockingMode(bool val)
{
#ifdef _WIN32
    u_long _val = val ? 1 : 0;
    if (ioctlsocket(m_fd, FIONBIO, &_val) == 0)
    {
        m_nonBlockingMode = val;
        return true;
    }
    else
    {
        return false;
    }
#else
    long currentFlags = fcntl(m_fd, F_GETFL, 0);
    if (currentFlags == -1)
        return false;
    if (val)
        currentFlags |= O_NONBLOCK;
    else
        currentFlags &= ~O_NONBLOCK;
    if (fcntl(m_fd, F_SETFL, currentFlags) == 0)
    {
        m_nonBlockingMode = val;
        return true;
    }
    else
    {
        return false;
    }
#endif
}

template<typename SocketInterfaceToImplement>
bool Socket<SocketInterfaceToImplement>::getNonBlockingMode(bool* val) const
{
    *val = m_nonBlockingMode;
    return true;
}

template<typename SocketInterfaceToImplement>
bool Socket<SocketInterfaceToImplement>::getMtu(unsigned int* mtuValue) const
{
#ifdef IP_MTU
    socklen_t optLen = 0;
    return ::getsockopt(m_fd, IPPROTO_IP, IP_MTU, (char*)mtuValue, &optLen) == 0;
#else
    *mtuValue = 1500;   //in winsock there is no IP_MTU, returning 1500 as most common value
    return true;
#endif
}

template<typename SocketInterfaceToImplement>
bool Socket<SocketInterfaceToImplement>::setSendBufferSize(unsigned int buff_size)
{
    return ::setsockopt(m_fd, SOL_SOCKET, SO_SNDBUF, (const char*)&buff_size, sizeof(buff_size)) == 0;
}

template<typename SocketInterfaceToImplement>
bool Socket<SocketInterfaceToImplement>::getSendBufferSize(unsigned int* buffSize) const
{
    socklen_t optLen = sizeof(*buffSize);
    return ::getsockopt(m_fd, SOL_SOCKET, SO_SNDBUF, (char*)buffSize, &optLen) == 0;
}

template<typename SocketInterfaceToImplement>
bool Socket<SocketInterfaceToImplement>::setRecvBufferSize(unsigned int buff_size)
{
    return ::setsockopt(m_fd, SOL_SOCKET, SO_RCVBUF, (const char*)&buff_size, sizeof(buff_size)) == 0;
}

template<typename SocketInterfaceToImplement>
bool Socket<SocketInterfaceToImplement>::getRecvBufferSize(unsigned int* buffSize) const
{
    socklen_t optLen = sizeof(*buffSize);
    return ::getsockopt(m_fd, SOL_SOCKET, SO_RCVBUF, (char*)buffSize, &optLen) == 0;
}

template<typename SocketInterfaceToImplement>
bool Socket<SocketInterfaceToImplement>::setRecvTimeout(unsigned int ms)
{
    timeval tv;

    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;   //1 Secs Timeout
#ifdef Q_OS_WIN32
    if (setsockopt(m_fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&ms, sizeof(ms)) != 0)
#else
    if (::setsockopt(m_fd, SOL_SOCKET, SO_RCVTIMEO, (const void *)&tv, sizeof(struct timeval)) < 0)
#endif
    {
        return false;
    }
    m_readTimeoutMS = ms;
    return true;
}

template<typename SocketInterfaceToImplement>
bool Socket<SocketInterfaceToImplement>::setSendTimeout(unsigned int ms)
{
    timeval tv;

    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;   //1 Secs Timeout
#ifdef Q_OS_WIN32
    if (setsockopt(m_fd, SOL_SOCKET, SO_SNDTIMEO, (char*)&ms, sizeof(ms)) != 0)
#else
    if (::setsockopt(m_fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(struct timeval)) < 0)
#endif
    {
        return false;
    }
    m_writeTimeoutMS = ms;
    return true;
}

template<typename SocketInterfaceToImplement>
bool Socket<SocketInterfaceToImplement>::setIpv6Only(bool val)
{
    NX_ASSERT(this->m_ipVersion == AF_INET6);

    const int on = val ? 1 : 0;
    return ::setsockopt(this->m_fd, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&on, sizeof(on)) == 0;
}

template<typename SocketInterfaceToImplement>
Pollable* Socket<SocketInterfaceToImplement>::pollable()
{
    return this;
}

template<typename SocketInterfaceToImplement>
void Socket<SocketInterfaceToImplement>::post(nx::utils::MoveOnlyFunc<void()> handler)
{
    if (impl()->terminated.load(std::memory_order_relaxed) > 0)
        return;

    impl()->aioThread->load()->post(this, std::move(handler));
}

template<typename SocketInterfaceToImplement>
void Socket<SocketInterfaceToImplement>::dispatch(nx::utils::MoveOnlyFunc<void()> handler)
{
    if (impl()->terminated.load(std::memory_order_relaxed) > 0)
        return;

    impl()->aioThread->load()->dispatch(this, std::move(handler));
}

template<typename SocketInterfaceToImplement>
Socket<SocketInterfaceToImplement>::Socket(
    aio::AbstractAioThread* aioThread,
    int type,
    int protocol,
    int ipVersion,
    std::unique_ptr<CommonSocketImpl> impl)
    :
    Pollable(aioThread, INVALID_SOCKET, std::move(impl)),
    m_ipVersion(ipVersion)
{
    createSocket(type, protocol);
}

template<typename SocketInterfaceToImplement>
Socket<SocketInterfaceToImplement>::Socket(
    aio::AbstractAioThread* aioThread,
    int _sockDesc,
    int ipVersion,
    std::unique_ptr<CommonSocketImpl> impl)
    :
    Pollable(aioThread, _sockDesc, std::move(impl)),
    m_ipVersion(ipVersion)
{
}

#ifdef _WIN32
static bool win32SocketsInitialized = false;
#endif

template<typename SocketInterfaceToImplement>
bool Socket<SocketInterfaceToImplement>::createSocket(int type, int protocol)
{
#ifdef _WIN32
    // TODO: #akolesnikov Remove it from here.
    if (!win32SocketsInitialized)
    {
        WORD wVersionRequested;
        WSADATA wsaData;

        wVersionRequested = MAKEWORD(2, 0);              // Request WinSock v2.0
        if (WSAStartup(wVersionRequested, &wsaData) != 0)  // Load WinSock DLL
            return false;
        win32SocketsInitialized = true;
    }
#endif

    m_fd = ::socket(m_ipVersion, type, protocol);
    if (m_fd < 0)
    {
        qWarning() << strerror(errno);
        return false;
    }

    const int off = 0;
    if (m_ipVersion == AF_INET6
        && ::setsockopt(m_fd, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&off, sizeof(off)))
    {
        return false;
    }

#ifdef SO_NOSIGPIPE
    int set = 1;
    setsockopt(m_fd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
#endif

    // We do not want child processes created by exec() to inherit socket descriptors.
#ifdef FD_CLOEXEC
    int flags = fcntl(m_fd, F_GETFD, 0);
    if (flags < 0)
    {
        NX_WARNING(this, nx::format("Can not read options by fcntl: %1")
            .arg(SystemError::getLastOSErrorCode()));
    }
    else if (fcntl(m_fd, F_SETFD, flags | FD_CLOEXEC) < 0)
    {
        NX_WARNING(this, nx::format("Can not set FD_CLOEXEC by fcntl: %1")
            .arg(SystemError::getLastOSErrorCode()));
    }
#endif
    return true;
}

//-------------------------------------------------------------------------------------------------
// class CommunicatingSocket

#ifndef _WIN32
namespace {

template<class Func>
int doInterruptableSystemCallWithTimeout(
    AbstractCommunicatingSocket* socket,
    const Func& func,
    unsigned int timeout,
    int flags)
{
    bool isNonBlocking = false;
    if (flags & MSG_DONTWAIT)
        isNonBlocking = true;
    else if (flags & MSG_WAITALL)
        isNonBlocking = false;
    else if (!socket->getNonBlockingMode(&isNonBlocking))
        return -1;

    QElapsedTimer et;
    et.start();

    bool waitStartTimeActual = false;
    if (timeout > 0)
        waitStartTimeActual = true;
    for (;; )
    {
        int result = func();
        if (result == -1)
        {
            if (errno == EINTR)
            {
                if (timeout == 0 ||  //< No timeout
                    !waitStartTimeActual)  //< Cannot check timeout expiration
                {
                    continue;
                }
                if (et.elapsed() < timeout)
                    continue;
                errno = ETIMEDOUT;
            }
            else if (!isNonBlocking && (errno == EWOULDBLOCK || errno == EAGAIN))
            {
                // Returning ETIMEDOUT on timeout to make error codes consistent across platforms.
                errno = ETIMEDOUT;
            }
        }
        return result;
    }
}
} // namespace
#endif

template<typename SocketInterfaceToImplement>
CommunicatingSocket<SocketInterfaceToImplement>::CommunicatingSocket(
    aio::AbstractAioThread* aioThread,
    int type,
    int protocol,
    int ipVersion,
    std::unique_ptr<CommonSocketImpl> sockImpl)
    :
    Socket<SocketInterfaceToImplement>(
        aioThread,
        type,
        protocol,
        ipVersion,
        std::move(sockImpl)),
    m_aioHelper(std::make_unique<aio::AsyncSocketImplHelper<self_type>>(
        this,
        ipVersion)),
    m_connected(false)
{
}

template<typename SocketInterfaceToImplement>
CommunicatingSocket<SocketInterfaceToImplement>::CommunicatingSocket(
    aio::AbstractAioThread* aioThread,
    int newConnSD,
    int ipVersion,
    std::unique_ptr<CommonSocketImpl> sockImpl)
    :
    Socket<SocketInterfaceToImplement>(
        aioThread,
        newConnSD,
        ipVersion,
        std::move(sockImpl)),
    m_aioHelper(std::make_unique<aio::AsyncSocketImplHelper<self_type>>(
        this,
        ipVersion)),
    m_connected(true)   // This constructor is used by server socket.
{
}

template<typename SocketInterfaceToImplement>
CommunicatingSocket<SocketInterfaceToImplement>::~CommunicatingSocket()
{
    if (m_aioHelper)
        m_aioHelper->terminate();
}

template<typename SocketInterfaceToImplement>
bool CommunicatingSocket<SocketInterfaceToImplement>::connect(
    const SocketAddress& remoteAddress,
    std::chrono::milliseconds timeout)
{
    if (remoteAddress.address.isIpAddress())
        return connectToIp(remoteAddress, timeout);

    auto resolvedEntries = SocketGlobals::addressResolver().resolveSync(
        remoteAddress.address.toString(), NatTraversalSupport::disabled, this->m_ipVersion);
    if (resolvedEntries.empty())
        return false;

    std::deque<HostAddress> resolvedAddresses;
    for (auto& entry: resolvedEntries)
        resolvedAddresses.push_back(std::move(entry.host));

    while (!resolvedAddresses.empty())
    {
        auto ip = std::move(resolvedAddresses.front());
        resolvedAddresses.pop_front();
        if (connectToIp(SocketAddress(std::move(ip), remoteAddress.port), timeout))
            return true;
    }

    return false; //< Could not connect by any of addresses.
}

template<typename SocketInterfaceToImplement>
void CommunicatingSocket<SocketInterfaceToImplement>::connectAsync(
    const SocketAddress& addr,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    return m_aioHelper->connectAsync(
        addr,
        [this, addr, handler = std::move(handler)](
            SystemError::ErrorCode code)
        {
            NX_VERBOSE(this, "Connect to %1 completed with result %2",
                addr, SystemError::toString(code));
            m_connected = (code == SystemError::noError);
            handler(code);
        });
}

template<typename SocketInterfaceToImplement>
bool CommunicatingSocket<SocketInterfaceToImplement>::isConnected() const
{
    return m_connected;
}

template<typename SocketInterfaceToImplement>
void CommunicatingSocket<SocketInterfaceToImplement>::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    // Calling m_aioHelper->bindToAioThread first so that it is able to detect aio thread change.
    m_aioHelper->bindToAioThread(aioThread);

    base_type::bindToAioThread(aioThread);
}

template<typename SocketInterfaceToImplement>
int CommunicatingSocket<SocketInterfaceToImplement>::recv(
    void* buffer, std::size_t bufferLen, int flags)
{
    #if defined(Q_OS_WIN)
        bool isNonBlockingMode;
        if (!this->getNonBlockingMode(&isNonBlockingMode))
            return -1;

        const bool needNonBlockingMode =
            !isNonBlockingMode && (flags & MSG_DONTWAIT) == MSG_DONTWAIT;
        if (needNonBlockingMode)
        {
            if (!this->setNonBlockingMode(true))
                return -1;
        }

        const int bytesRead = ::recv(this->m_fd, (raw_type *)buffer, bufferLen, flags & ~MSG_DONTWAIT);

        if (needNonBlockingMode)
        {
            // Save error code as changing mode will drop it.
            auto sysErrorCodeBak = SystemError::getLastOSErrorCode();
            if (!this->setNonBlockingMode(false))
                return -1;
            // SystemError::interrupted is a result of the WSACancelBlockingCall() that is
            // deprecated and seems to be used by the recv() implementation internally. So we
            // replace it with more neutral timeout code.
            if (sysErrorCodeBak == SystemError::interrupted)
                sysErrorCodeBak = SystemError::timedOut;
            SystemError::setLastErrorCode(sysErrorCodeBak);
        }
        else
        {
            // This may be a driver bug that may occur on some win10 systems. So we replace
            // WSA_IO_PENDING error with SystemError::timedOut.
            // See https://stackoverflow.com/questions/52419993/unexpected-wsa-io-pending-from-blocking-with-overlapped-i-o-attribute-winsock2
            auto sysErrorCode = SystemError::getLastOSErrorCode();
            if (sysErrorCode == WSA_IO_PENDING)
                sysErrorCode = SystemError::timedOut;
            SystemError::setLastErrorCode(sysErrorCode);
        }
    #else
        unsigned int recvTimeout = 0;
        if (!this->getRecvTimeout(&recvTimeout))
            return -1;

        const int bytesRead = doInterruptableSystemCallWithTimeout<>(
            this,
            std::bind(&::recv, this->m_fd, (void*)buffer, (size_t)bufferLen, flags),
            recvTimeout,
            flags);
    #endif
    if (bytesRead < 0)
    {
        const SystemError::ErrorCode errCode = SystemError::getLastOSErrorCode();
        if (socketCannotRecoverFromError(errCode))
            m_connected = false;
    }
    else if (bytesRead == 0)
    {
        m_connected = false; //connection closed by remote host
    }

    return bytesRead;
}

template<typename SocketInterfaceToImplement>
int CommunicatingSocket<SocketInterfaceToImplement>::send(
    const void* buffer, std::size_t bufferLen)
{
    #if defined(Q_OS_WIN)
        const int sent = ::send(this->m_fd, (raw_type*)buffer, bufferLen, 0);
    #else
        unsigned int sendTimeout = 0;
        if (!this->getSendTimeout(&sendTimeout))
            return -1;

        const int sent = doInterruptableSystemCallWithTimeout<>(
            this,
            std::bind(
                &::send,
                this->m_fd,
                buffer,
                (size_t)bufferLen,
                #if defined(Q_OS_LINUX)
                    MSG_NOSIGNAL
                #else
                    0
                #endif
            ),
            sendTimeout,
            0);
    #endif

    if (sent < 0)
    {
        const SystemError::ErrorCode errCode = SystemError::getLastOSErrorCode();
        if (socketCannotRecoverFromError(errCode))
            m_connected = false;
    }
    else if (sent == 0)
    {
        m_connected = false;
    }
    else
    {
        #if !defined(__arm__)
            g_totalSocketBytesSent += sent;
        #endif
    }

    return sent;
}

template<typename SocketInterfaceToImplement>
SocketAddress CommunicatingSocket<SocketInterfaceToImplement>::getForeignAddress() const
{
    if (this->m_ipVersion == AF_INET)
    {
        sockaddr_in addr;
        socklen_t addrLen = sizeof(addr);
        if (::getpeername(this->m_fd, (sockaddr*)&addr, &addrLen) < 0)
            return SocketAddress();
        return SocketAddress(addr);
    }
    else if (this->m_ipVersion == AF_INET6)
    {
        sockaddr_in6 addr;
        socklen_t addrLen = sizeof(addr);
        if (::getpeername(this->m_fd, (sockaddr*)&addr, &addrLen) < 0)
            return SocketAddress();
        return SocketAddress(addr);
    }

    return SocketAddress();
}

template<typename SocketInterfaceToImplement>
bool CommunicatingSocket<SocketInterfaceToImplement>::close()
{
    m_connected = false;
    return Socket<SocketInterfaceToImplement>::close();
}

template<typename SocketInterfaceToImplement>
bool CommunicatingSocket<SocketInterfaceToImplement>::shutdown()
{
    m_connected = false;
    bool result = Socket<SocketInterfaceToImplement>::shutdown();
    #if defined(Q_OS_WIN)
        if (this->m_fd != -1)
            ::CancelIoEx((HANDLE)this->m_fd, NULL);
    #endif
    return result;
}

template<typename SocketInterfaceToImplement>
void CommunicatingSocket<SocketInterfaceToImplement>::readSomeAsync(
    nx::Buffer* const buf,
    IoCompletionHandler handler)
{
    return m_aioHelper->readSomeAsync(buf, std::move(handler));
}

template<typename SocketInterfaceToImplement>
void CommunicatingSocket<SocketInterfaceToImplement>::sendAsync(
    const nx::Buffer* buf,
    IoCompletionHandler handler)
{
    return m_aioHelper->sendAsync(buf, std::move(handler));
}

template<typename SocketInterfaceToImplement>
void CommunicatingSocket<SocketInterfaceToImplement>::registerTimer(
    std::chrono::milliseconds timeout,
    nx::utils::MoveOnlyFunc<void()> handler)
{
    //currently, aio considers 0 timeout as no timeout and will NOT call handler
    //NX_ASSERT(timeoutMs > std::chrono::milliseconds(0));
    if (timeout == std::chrono::milliseconds::zero())
        timeout = std::chrono::milliseconds(1);  //handler of zero timer will NOT be called
    return m_aioHelper->registerTimer(timeout, std::move(handler));
}

template<typename SocketInterfaceToImplement>
void CommunicatingSocket<SocketInterfaceToImplement>::cancelIoInAioThread(
    aio::EventType eventType)
{
    m_aioHelper->cancelIOSync(eventType);
}

template<typename SocketInterfaceToImplement>
bool CommunicatingSocket<SocketInterfaceToImplement>::connectToIp(
    const SocketAddress& remoteAddress,
    std::chrono::milliseconds timeout)
{
    int timeoutMs = timeout == nx::network::kNoTimeout ? -1 : timeout.count();
    // Get the address of the requested host.
    m_connected = false;

    const SystemSocketAddress addr(remoteAddress, this->m_ipVersion);
    if (!addr.get())
        return false;

    //switching to non-blocking mode to connect with timeout
    bool isNonBlockingModeBak = false;
    if (!this->getNonBlockingMode(&isNonBlockingModeBak))
        return false;
    if (!isNonBlockingModeBak && !this->setNonBlockingMode(true))
        return false;

    NX_ASSERT(addr.get()->sa_family == this->m_ipVersion);

    int connectResult = ::connect(this->m_fd, addr.get(), addr.length());
    if (connectResult != 0)
    {
        auto errorCode = SystemError::getLastOSErrorCode();
        if (errorCode != SystemError::inProgress)
            return false;
        if (isNonBlockingModeBak)
            return true;        //async connect started
    }

    SystemError::ErrorCode connectErrorCode = SystemError::noError;

#ifdef _WIN32
    timeval timeVal;
    fd_set wrtFDS;
    FD_ZERO(&wrtFDS);
    FD_SET(this->m_fd, &wrtFDS);

    fd_set exceptFDS;
    FD_ZERO(&exceptFDS);
    FD_SET(this->m_fd, &exceptFDS);

    /* set timeout values */
    timeVal.tv_sec = timeoutMs / 1000;
    timeVal.tv_usec = (timeoutMs % 1000) * 1000;
    const int selectResult = ::select(
        this->m_fd + 1,
        NULL,
        &wrtFDS,
        &exceptFDS,
        timeoutMs >= 0 ? &timeVal : NULL);

    if (selectResult < 0)
    {
        connectErrorCode = SystemError::getLastOSErrorCode();
    }
    else if (selectResult == 0)
    {
        connectErrorCode = SystemError::timedOut;
    }
    else
    {
        if (FD_ISSET(this->m_fd, &wrtFDS))
        {
            m_connected = true;
        }
        else if (FD_ISSET(this->m_fd, &exceptFDS))
        {
            if (!this->getLastError(&connectErrorCode) || connectErrorCode == SystemError::noError)
                connectErrorCode = SystemError::connectionRefused;
        }
    }
#else
    QElapsedTimer et;
    et.start();
    bool waitStartTimeActual = false;
    if (timeoutMs > 0)
        waitStartTimeActual = true;
    for (;;)
    {
        struct pollfd sockPollfd;
        memset(&sockPollfd, 0, sizeof(sockPollfd));
        sockPollfd.fd = this->m_fd;
        sockPollfd.events = POLLOUT;
#ifdef _GNU_SOURCE
        sockPollfd.events |= POLLRDHUP;
#endif
        const int pollResult = ::poll(&sockPollfd, 1, timeoutMs);
        if (pollResult < 0)
        {
            if (errno == EINTR)
            {
                //modifying timeout for time we've already spent in select
                if (timeoutMs < 0 ||  //no timeout
                    !waitStartTimeActual)
                {
                    //not updating timeout value. This can lead to spending "tcp connect timeout" in select (if signals arrive frequently and no monotonic clock on system)
                    continue;
                }
                const int millisAlreadySlept = et.elapsed();
                if (millisAlreadySlept >= (int)timeoutMs)
                {
                    connectErrorCode = SystemError::timedOut;
                    break;
                }
                timeoutMs -= millisAlreadySlept;
                continue;
            }

            connectErrorCode = SystemError::getLastOSErrorCode();
            break;
        }

        if (pollResult == 0)
        {
            connectErrorCode = SystemError::timedOut;
            break;
        }

        if (sockPollfd.revents & (POLLERR | POLLHUP))
        {
            if (!this->getLastError(&connectErrorCode) || connectErrorCode == SystemError::noError)
                connectErrorCode = SystemError::connectionRefused;
            break;
        }

        // Success.
        break;
    }
#endif

    m_connected = connectErrorCode == SystemError::noError;

    //restoring original mode
    this->setNonBlockingMode(isNonBlockingModeBak);

    SystemError::setLastErrorCode(connectErrorCode);

    NX_VERBOSE(this, "Connect to %1 completed with result %2",
        remoteAddress, SystemError::toString(connectErrorCode));

    return m_connected;
}

//-------------------------------------------------------------------------------------------------
// class TCPSocket

#ifdef _WIN32
class Win32TcpSocketImpl:
    public CommonSocketImpl
{
public:
    MIB_TCPROW win32TcpTableRow;

    Win32TcpSocketImpl()
    {
        memset(&win32TcpTableRow, 0, sizeof(win32TcpTableRow));
    }
};
#endif

TCPSocket::TCPSocket(int ipVersion):
    base_type(
        SocketGlobals::instance().aioService().findLeastUsedAioThread(),
        SOCK_STREAM,
        IPPROTO_TCP,
        ipVersion
#ifdef _WIN32
        , std::make_unique<Win32TcpSocketImpl>()
#endif
    )
{
    SocketGlobals::instance().allocationAnalyzer().recordObjectCreation(this);
    ++SocketGlobals::instance().debugCounters().tcpSocketCount;
}

TCPSocket::TCPSocket(
    aio::AbstractAioThread* aioThread,
    int newConnSD,
    int ipVersion)
    :
    base_type(
        aioThread,
        newConnSD,
        ipVersion
#ifdef _WIN32
        , std::make_unique<Win32TcpSocketImpl>()
#endif
    )
{
    SocketGlobals::instance().allocationAnalyzer().recordObjectCreation(this);
    ++SocketGlobals::instance().debugCounters().tcpSocketCount;
}

TCPSocket::~TCPSocket()
{
    --SocketGlobals::instance().debugCounters().tcpSocketCount;
    SocketGlobals::instance().allocationAnalyzer().recordObjectDestruction(this);
}

bool TCPSocket::getProtocol(int* protocol) const
{
    *protocol = Protocol::tcp;
    return true;
}

bool TCPSocket::reopen()
{
    close();
    return createSocket(SOCK_STREAM, IPPROTO_TCP);
}

bool TCPSocket::setNoDelay(bool value)
{
    int flag = value ? 1 : 0;
    return setsockopt(
        handle(),            // socket affected
        IPPROTO_TCP,     // set option at TCP level
        TCP_NODELAY,     // name of option
        (char *)&flag,  // the cast is historical cruft
        sizeof(int)) == 0;    // length of option value
}

bool TCPSocket::getNoDelay(bool* value) const
{
    int flag = 0;
    socklen_t optLen = 0;
    if (getsockopt(
            handle(),            // socket affected
            IPPROTO_TCP,      // set option at TCP level
            TCP_NODELAY,      // name of option
            (char*)&flag,     // the cast is historical cruft
            &optLen) != 0)  // length of option value
    {
        return false;
    }

    *value = flag > 0;
    return true;
}

bool TCPSocket::toggleStatisticsCollection([[maybe_unused]] bool val)
{
#ifdef _WIN32
    //dynamically resolving functions that require win >= vista we want to use here
    typedef decltype(&SetPerTcpConnectionEStats) SetPerTcpConnectionEStatsType;
    static SetPerTcpConnectionEStatsType SetPerTcpConnectionEStatsAddr =
        Win32FuncResolver::instance()->resolveFunction<SetPerTcpConnectionEStatsType>
        (L"Iphlpapi.dll", "SetPerTcpConnectionEStats");

    if (SetPerTcpConnectionEStatsAddr == NULL)
        return false;

    Win32TcpSocketImpl* d = static_cast<Win32TcpSocketImpl*>(impl());

    if (GetTcpRow(
            getLocalAddress().port,
            getForeignAddress().port,
            MIB_TCP_STATE_ESTAB,
            &d->win32TcpTableRow) != ERROR_SUCCESS)
    {
        memset(&d->win32TcpTableRow, 0, sizeof(d->win32TcpTableRow));
        return false;
    }

    auto freeLambda = [](void* ptr) { ::free(ptr); };
    std::unique_ptr<TCP_ESTATS_PATH_RW_v0, decltype(freeLambda)> pathRW((TCP_ESTATS_PATH_RW_v0*)malloc(sizeof(TCP_ESTATS_PATH_RW_v0)), freeLambda);
    if (!pathRW.get())
    {
        memset(&d->win32TcpTableRow, 0, sizeof(d->win32TcpTableRow));
        return false;
    }

    memset(pathRW.get(), 0, sizeof(*pathRW)); // zero the buffer
    pathRW->EnableCollection = val ? TRUE : FALSE;
    //enabling statistics collection
    if (SetPerTcpConnectionEStatsAddr(
            &d->win32TcpTableRow,
            TcpConnectionEstatsPath,
            (UCHAR*)pathRW.get(), 0, sizeof(*pathRW),
            0) != NO_ERROR)
    {
        memset(&d->win32TcpTableRow, 0, sizeof(d->win32TcpTableRow));
        return false;
    }
    return true;
#elif defined(__linux__)
    return true;
#else
    return false;
#endif
}

bool TCPSocket::getConnectionStatistics([[maybe_unused]] StreamSocketInfo* info)
{
#ifdef _WIN32
    Win32TcpSocketImpl* d = static_cast<Win32TcpSocketImpl*>(impl());

    if (!d->win32TcpTableRow.dwLocalAddr &&
        !d->win32TcpTableRow.dwLocalPort)
    {
        return false;
    }
    return readTcpStat(&d->win32TcpTableRow, info) == ERROR_SUCCESS;
#elif defined(__linux__)
    static const size_t USEC_PER_MSEC = 1000;

    struct tcp_info tcpinfo;
    memset(&tcpinfo, 0, sizeof(tcpinfo));
    socklen_t tcp_info_length = sizeof(tcpinfo);
    if (getsockopt(handle(), SOL_TCP, TCP_INFO, (void *)&tcpinfo, &tcp_info_length) != 0)
        return false;
    info->rttVar = tcpinfo.tcpi_rttvar / USEC_PER_MSEC;
    return true;
#else
    return false;
#endif
}

template<typename TargetType, typename SourceType>
int intDuration(SourceType duration)
{
    const auto repr = std::chrono::duration_cast<TargetType>(duration).count();
    NX_ASSERT(repr >= std::numeric_limits<int>::min() && repr <= std::numeric_limits<int>::max());
    return (int)repr;
}

bool TCPSocket::setKeepAlive(std::optional< KeepAliveOptions > info)
{
    using namespace std::chrono;

#if defined( _WIN32 )
    struct tcp_keepalive ka = { FALSE, 0, 0 };
    if (info)
    {
        ka.onoff = TRUE;
        ka.keepalivetime = intDuration<milliseconds>(info->inactivityPeriodBeforeFirstProbe);
        ka.keepaliveinterval = intDuration<milliseconds>(info->probeSendPeriod);

        // The value can not be changed, 0 means default.
        info->probeCount = 10; //< Value cannot be changed on mswin.
    }

    DWORD length = sizeof(ka);
    if (WSAIoctl(handle(), SIO_KEEPALIVE_VALS,
            &ka, sizeof(ka), NULL, 0, &length, NULL, NULL))
    {
        return false;
    }

    if (info)
        m_keepAlive = std::move(*info);
#else
    int isEnabled = info ? 1 : 0;
    if (setsockopt(handle(), SOL_SOCKET, SO_KEEPALIVE, &isEnabled, sizeof(isEnabled)) != 0)
        return false;

    if (!info)
        return true;

#if defined( Q_OS_LINUX )
    const int inactivityPeriodBeforeFirstProbe =
        intDuration<seconds>(info->inactivityPeriodBeforeFirstProbe);
    if (setsockopt(handle(), SOL_TCP, TCP_KEEPIDLE,
            &inactivityPeriodBeforeFirstProbe, sizeof(inactivityPeriodBeforeFirstProbe)) < 0)
    {
        return false;
    }

    const int probeSendPeriod = intDuration<seconds>(info->probeSendPeriod);
    if (setsockopt(handle(), SOL_TCP, TCP_KEEPINTVL,
            &probeSendPeriod, sizeof(probeSendPeriod)) < 0)
    {
        return false;
    }

    const int count = (int)info->probeCount;
    if (setsockopt(handle(), SOL_TCP, TCP_KEEPCNT, &count, sizeof(count)) < 0)
        return false;
#elif defined( Q_OS_MACX )
    const int inactivityPeriodBeforeFirstProbe =
        intDuration<seconds>(info->inactivityPeriodBeforeFirstProbe);
    if (setsockopt(handle(), IPPROTO_TCP, TCP_KEEPALIVE,
            &inactivityPeriodBeforeFirstProbe, sizeof(inactivityPeriodBeforeFirstProbe)) < 0)
    {
        return false;
    }
#endif
#endif

    return true;
}

bool TCPSocket::getKeepAlive(std::optional< KeepAliveOptions >* result) const
{
    using namespace std::chrono;

    int isEnabled = 0;
    socklen_t length = sizeof(isEnabled);
    if (getsockopt(handle(), SOL_SOCKET, SO_KEEPALIVE,
        reinterpret_cast<char*>(&isEnabled), &length) != 0)
        return false;

    if (!isEnabled)
    {
        *result = std::nullopt;
        return true;
    }

#if defined(_WIN32)
    *result = m_keepAlive;
#else
    *result = KeepAliveOptions();
#if defined(Q_OS_LINUX)
    int inactivityPeriodBeforeFirstProbe = 0;
    if (getsockopt(handle(), SOL_TCP, TCP_KEEPIDLE,
            &inactivityPeriodBeforeFirstProbe, &length) < 0)
    {
        return false;
    }

    int probeSendPeriod = 0;
    if (getsockopt(handle(), SOL_TCP, TCP_KEEPINTVL, &probeSendPeriod, &length) < 0)
        return false;

    int probeCount = 0;
    if (getsockopt(handle(), SOL_TCP, TCP_KEEPCNT, &probeCount, &length) < 0)
        return false;

    (*result)->inactivityPeriodBeforeFirstProbe = seconds(inactivityPeriodBeforeFirstProbe);
    (*result)->probeSendPeriod = seconds(probeSendPeriod);
    (*result)->probeCount = (size_t)probeCount;
#elif defined( Q_OS_MACX )
    int inactivityPeriodBeforeFirstProbe = 0;
    if (getsockopt(handle(), IPPROTO_TCP, TCP_KEEPALIVE,
            &inactivityPeriodBeforeFirstProbe, &length) < 0)
    {
        return false;
    }

    (*result)->inactivityPeriodBeforeFirstProbe = seconds(inactivityPeriodBeforeFirstProbe);
#endif
#endif

    return true;
}

//-------------------------------------------------------------------------------------------------
// class TCPServerSocket

static const int DEFAULT_ACCEPT_TIMEOUT_MSEC = 250;
/**
 * @return fd (>=0) on success, <0 on error (-2 if timed out)
 */
static int acceptWithTimeout(
    int fd,
    int timeoutMillis = DEFAULT_ACCEPT_TIMEOUT_MSEC,
    bool nonBlockingMode = false)
{
    if (nonBlockingMode)
        return ::accept(fd, NULL, NULL);

    int result = 0;
    if (timeoutMillis == 0)
        timeoutMillis = -1; // poll expects -1 as an infinity

#ifdef _WIN32
    struct pollfd fds[1];
    memset(fds, 0, sizeof(fds));
    fds[0].fd = fd;
    fds[0].events |= POLLIN;
    result = poll(fds, sizeof(fds) / sizeof(*fds), timeoutMillis);
    if (result < 0)
        return result;
    if (result == 0)   //timeout
    {
        ::SetLastError(SystemError::timedOut);
        return -1;
    }
    if (fds[0].revents & POLLERR)
    {
        int errorCode = 0;
        int errorCodeLen = sizeof(errorCode);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&errorCode), &errorCodeLen) != 0)
            return -1;
        ::SetLastError(errorCode);
        return -1;
    }
    return ::accept(fd, NULL, NULL);
#else
    struct pollfd sockPollfd;
    memset(&sockPollfd, 0, sizeof(sockPollfd));
    sockPollfd.fd = fd;
    sockPollfd.events = POLLIN;
#ifdef _GNU_SOURCE
    sockPollfd.events |= POLLRDHUP;
#endif
    result = ::poll(&sockPollfd, 1, timeoutMillis);
    if (result < 0)
        return result;
    if (result == 0)   //timeout
    {
        errno = SystemError::timedOut;
        return -1;
    }
    if (sockPollfd.revents & POLLIN)
        return ::accept(fd, NULL, NULL);

    if ((sockPollfd.revents & POLLHUP)
#ifdef _GNU_SOURCE
        || (sockPollfd.revents & POLLRDHUP)
#endif
        )
    {
        errno = ENOTCONN;
        return -1;
    }
    if (sockPollfd.revents & POLLERR)
    {
        int errorCode = 0;
        socklen_t errorCodeLen = sizeof(errorCode);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &errorCode, &errorCodeLen) != 0)
            return -1;
        errno = errorCode;
        return -1;
    }
    return -1;
#endif
}

class TCPServerSocketPrivate:
    public CommonSocketImpl
{
public:
    aio::AIOService* aioService = nullptr;
    int socketHandle;
    const int ipVersion;
    aio::AsyncServerSocketHelper<TCPServerSocket> asyncServerSocketHelper;

    TCPServerSocketPrivate(
        aio::AIOService* aioService,
        TCPServerSocket* _sock,
        int _ipVersion)
        :
        aioService(aioService),
        socketHandle(-1),
        ipVersion(_ipVersion),
        asyncServerSocketHelper(_sock)
    {
    }

    std::unique_ptr<AbstractStreamSocket> accept(unsigned int recvTimeoutMs, bool nonBlockingMode)
    {
        int newConnSD = acceptWithTimeout(socketHandle, recvTimeoutMs, nonBlockingMode);
        if (newConnSD >= 0)
        {
            auto tcpSocket = std::unique_ptr<TCPSocket>(
                new TCPSocket(aioService->getRandomAioThread(), newConnSD, ipVersion));
            return tcpSocket;
        }
        else if (newConnSD == -2)
        {
            //setting system error code
#ifdef _WIN32
            ::SetLastError(SystemError::timedOut);
#else
            errno = SystemError::timedOut;
#endif
            return nullptr;    //timeout
        }
        else
        {
            return nullptr;
        }
    }
};

TCPServerSocket::TCPServerSocket(int ipVersion):
    base_type(
        SocketGlobals::instance().aioService().findLeastUsedAioThread(),
        SOCK_STREAM,
        IPPROTO_TCP,
        ipVersion,
        std::make_unique<TCPServerSocketPrivate>(
            &SocketGlobals::instance().aioService(),
            this,
            ipVersion))
{
    static_cast<TCPServerSocketPrivate*>(impl())->socketHandle = handle();
}

TCPServerSocket::~TCPServerSocket()
{
    if (!impl()->aioThread.wasAccessed() || !impl()->aioThread->load())
        return; //< AIO was not used by this socket. No need to cancel anything.

    if (isInSelfAioThread())
    {
        stopWhileInAioThread();
        return;
    }

    NX_CRITICAL(
        !impl()->aioThread->load()->isSocketBeingMonitored(this),
        "You MUST cancel running async socket operation before "
        "deleting socket if you delete socket from non-aio thread");
}

void TCPServerSocket::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    // Calling asyncServerSocketHelper.bindToAioThread first so that it is able to detect
    // aio thread change.
    static_cast<TCPServerSocketPrivate*>(impl())->
        asyncServerSocketHelper.bindToAioThread(aioThread);

    base_type::bindToAioThread(aioThread);
}

int TCPServerSocket::accept(int sockDesc)
{
    return acceptWithTimeout(sockDesc);
}

void TCPServerSocket::acceptAsync(AcceptCompletionHandler handler)
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

    TCPServerSocketPrivate* d = static_cast<TCPServerSocketPrivate*>(impl());
    return d->asyncServerSocketHelper.acceptAsync(std::move(handler));
}

void TCPServerSocket::cancelIoInAioThread()
{
    TCPServerSocketPrivate* d = static_cast<TCPServerSocketPrivate*>(impl());
    return d->asyncServerSocketHelper.cancelIOSync();
}

bool TCPServerSocket::getProtocol(int* protocol) const
{
    *protocol = Protocol::tcp;
    return true;
}

bool TCPServerSocket::listen(int queueLen)
{
    if (::listen(handle(), queueLen) == 0)
    {
        NX_VERBOSE(this, "Listening on local address %1", getLocalAddress());
        return true;
    }

    return false;
}

void TCPServerSocket::pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    // TODO #akolesnikov: Add general implementation to Socket class and remove this method.
    dispatch(
        [this, completionHandler = std::move(completionHandler)]()
        {
            stopWhileInAioThread();
            completionHandler();
        });
}

void TCPServerSocket::pleaseStopSync()
{
    if (isInSelfAioThread())
        stopWhileInAioThread();
    else
        QnStoppableAsync::pleaseStopSync();
}

std::unique_ptr<AbstractStreamSocket> TCPServerSocket::accept()
{
    return systemAccept();
}

std::unique_ptr<AbstractStreamSocket> TCPServerSocket::systemAccept()
{
    TCPServerSocketPrivate* d = static_cast<TCPServerSocketPrivate*>(impl());

    unsigned int recvTimeoutMs = 0;
    if (!getRecvTimeout(&recvTimeoutMs))
        return nullptr;

    bool nonBlockingMode = false;
    if (!getNonBlockingMode(&nonBlockingMode))
        return nullptr;

    auto acceptedSocket = d->accept(recvTimeoutMs, nonBlockingMode);
    if (!acceptedSocket)
        return nullptr;

#if defined(_WIN32) || defined(Q_OS_MACX)
    if (nonBlockingMode)
    {
        // Make all platforms behave like Linux, so all new sockets are in blocking mode
        // regardless of their origin.
        if (!acceptedSocket->setNonBlockingMode(false))
            return nullptr;
    }
#endif

    // Needed since our socket API is somehow expected not to inherit timeouts.
    if (!acceptedSocket->setRecvTimeout(0) ||
        !acceptedSocket->setSendTimeout(0))
    {
        return nullptr;
    }

    NX_VERBOSE(this, "Accepted new connection from %1", acceptedSocket->getForeignAddress());

    return acceptedSocket;
}

bool TCPServerSocket::setListen(int queueLen)
{
    return ::listen(handle(), queueLen) == 0;
}

void TCPServerSocket::stopWhileInAioThread()
{
    TCPServerSocketPrivate* d = static_cast<TCPServerSocketPrivate*>(impl());
    d->asyncServerSocketHelper.stopPolling();
}

//-------------------------------------------------------------------------------------------------
// class UDPSocket

UDPSocket::UDPSocket(int ipVersion):
    base_type(
        SocketGlobals::instance().aioService().findLeastUsedAioThread(),
        SOCK_DGRAM,
        IPPROTO_UDP,
        ipVersion)
{
    setBroadcast();

    int buff_size = 1024 * 512;
    if (::setsockopt(handle(), SOL_SOCKET, SO_RCVBUF, (const char*)&buff_size, sizeof(buff_size)) < 0)
    {
        //error
    }

    // Made with an assumption that SO_LINGER may cause ::close system call to block
    // on win32 with some network drivers when network inteface fails.
    struct linger lingerOptions;
    memset(&lingerOptions, 0, sizeof(lingerOptions));
    if (setsockopt(
            handle(), SOL_SOCKET, SO_LINGER,
            (const char*) &lingerOptions, sizeof(lingerOptions)) < 0)
    {
        // Ignoring for now.
    }

    SocketGlobals::instance().allocationAnalyzer().recordObjectCreation(this);
    ++SocketGlobals::instance().debugCounters().udpSocketCount;
}

UDPSocket::~UDPSocket()
{
    NX_ASSERT(SocketGlobals::isInitialized());
    --SocketGlobals::instance().debugCounters().udpSocketCount;
    SocketGlobals::instance().allocationAnalyzer().recordObjectDestruction(this);
}

bool UDPSocket::getProtocol(int* protocol) const
{
    *protocol = Protocol::udp;
    return true;
}

SocketAddress UDPSocket::getForeignAddress() const
{
    return m_destAddr.toSocketAddress();
}

void UDPSocket::setBroadcast()
{
    // If this fails, we'll hear about it when we try to send.  This will allow
    // system that cannot broadcast to continue if they don't plan to broadcast
    int broadcastPermission = 1;
    setsockopt(
        handle(), SOL_SOCKET, SO_BROADCAST,
        (raw_type *)&broadcastPermission, sizeof(broadcastPermission));
}

bool UDPSocket::sendTo(const void *buffer, int bufferLen)
{
    // Write out the whole buffer as a single message.
#ifdef _WIN32
    return sendto(
        handle(), (raw_type *)buffer, bufferLen, 0,
        m_destAddr.get(), m_destAddr.length()) == bufferLen;
#else
    unsigned int sendTimeout = 0;
    if (!getSendTimeout(&sendTimeout))
        return -1;

#ifdef __linux__
    int flags = MSG_NOSIGNAL;
#else
    int flags = 0;
#endif

    return doInterruptableSystemCallWithTimeout<>(
        this,
        std::bind(
            &::sendto, handle(), (const void*)buffer, (size_t)bufferLen, flags,
            m_destAddr.get(), m_destAddr.length()),
        sendTimeout,
        flags) == bufferLen;
#endif
}

bool UDPSocket::setMulticastTTL(unsigned char multicastTTL)
{
    if (setsockopt(
            handle(), IPPROTO_IP, IP_MULTICAST_TTL,
            (raw_type *)&multicastTTL, sizeof(multicastTTL)) < 0)
    {
        return false;
    }
    return true;
}

bool UDPSocket::setMulticastIF(const std::string& multicastIF)
{
    struct in_addr localInterface;
    localInterface.s_addr = inet_addr(multicastIF.c_str());
    if (setsockopt(
            handle(), IPPROTO_IP, IP_MULTICAST_IF,
            (raw_type *)&localInterface, sizeof(localInterface)) < 0)
    {
        return false;
    }
    return true;
}

bool UDPSocket::joinGroup(const HostAddress& multicastGroup)
{
#ifdef __linux__
    int mcAll = 0;
    if (setsockopt(handle(), IPPROTO_IP, IP_MULTICAST_ALL, (raw_type *)&mcAll, sizeof(mcAll)) < 0)
    {
        NX_WARNING(this, "Failed to disable IP_MULTICAST_ALL socket option for group %1. %2",
            multicastGroup, SystemError::getLastOSErrorText());
        return false;
    }
#endif
    struct ip_mreq multicastRequest;
    memset(&multicastRequest, 0, sizeof(multicastRequest));

    multicastRequest.imr_multiaddr.s_addr = inet_addr(multicastGroup.toString().c_str());
    multicastRequest.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(
            handle(), IPPROTO_IP, IP_ADD_MEMBERSHIP,
            (raw_type *)&multicastRequest, sizeof(multicastRequest)) < 0)
    {
        NX_WARNING(this, "failed to join multicast group %1", multicastGroup);
        return false;
    }
    return true;
}

bool UDPSocket::joinGroup(const HostAddress& multicastGroup, const HostAddress& multicastIF)
{
#ifdef __linux__
    int mcAll = 0;
    if (setsockopt(handle(), IPPROTO_IP, IP_MULTICAST_ALL, (raw_type *)&mcAll, sizeof(mcAll)) < 0)
    {
        NX_WARNING(this, "Failed to disable IP_MULTICAST_ALL socket option for group %1. %2",
            multicastGroup, SystemError::getLastOSErrorText());
        return false;
    }
#endif
    struct ip_mreq multicastRequest;
    memset(&multicastRequest, 0, sizeof(multicastRequest));
    multicastRequest.imr_multiaddr.s_addr = inet_addr(multicastGroup.toString().c_str());
    multicastRequest.imr_interface.s_addr = inet_addr(multicastIF.toString().c_str());
    if (setsockopt(
            handle(), IPPROTO_IP, IP_ADD_MEMBERSHIP,
            (raw_type *)&multicastRequest, sizeof(multicastRequest)) < 0)
    {
        NX_WARNING(this, "Failed to join multicast group %1 from interface with IP %2. %3",
                   multicastGroup, multicastIF, SystemError::getLastOSErrorText());
        return false;
    }
    return true;
}

bool UDPSocket::leaveGroup(const HostAddress& multicastGroup)
{
    struct ip_mreq multicastRequest;
    memset(&multicastRequest, 0, sizeof(multicastRequest));

    multicastRequest.imr_multiaddr.s_addr = inet_addr(multicastGroup.toString().c_str());
    multicastRequest.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(handle(), IPPROTO_IP, IP_DROP_MEMBERSHIP,
        (raw_type *)&multicastRequest,
        sizeof(multicastRequest)) < 0)
    {
        return false;
    }
    return true;
}

bool UDPSocket::leaveGroup(const HostAddress& multicastGroup, const HostAddress& multicastIF)
{
    struct ip_mreq multicastRequest;
    memset(&multicastRequest, 0, sizeof(multicastRequest));

    multicastRequest.imr_multiaddr.s_addr = inet_addr(multicastGroup.toString().c_str());
    multicastRequest.imr_interface.s_addr = inet_addr(multicastIF.toString().c_str());

    return setsockopt(
        handle(), IPPROTO_IP, IP_DROP_MEMBERSHIP,
        (raw_type*) &multicastRequest, sizeof(multicastRequest)) == 0;
}

int UDPSocket::send(const void* buffer, std::size_t bufferLen)
{
#ifdef _WIN32
    return sendto(
        handle(), (raw_type*) buffer, bufferLen, 0,
        m_destAddr.get(), m_destAddr.length());
#else
    unsigned int sendTimeout = 0;
    if (!getSendTimeout(&sendTimeout))
        return -1;

    return doInterruptableSystemCallWithTimeout<>(
        this,
        std::bind(
            &::sendto, handle(),
            (const void*)buffer, (size_t)bufferLen, 0,
            m_destAddr.get(), m_destAddr.length()),
        sendTimeout,
        0);
#endif
}

bool UDPSocket::setDestAddr(const SocketAddress& endpoint)
{
    if (endpoint.address.isIpAddress())
    {
        m_destAddr = SystemSocketAddress(endpoint, m_ipVersion);
    }
    else
    {
        auto resolvedEntries = SocketGlobals::addressResolver().resolveSync(
            endpoint.address.toString(), NatTraversalSupport::disabled, m_ipVersion);
        if (resolvedEntries.empty())
            return false;

        std::deque<HostAddress> resolvedAddresses;
        for (auto& entry: resolvedEntries)
            resolvedAddresses.push_back(std::move(entry.host));

        // TODO: Here we select first address with hope it is correct one. This will never work
        // for NAT64, so we have to fix it somehow.
        m_destAddr = SystemSocketAddress(
            SocketAddress(resolvedAddresses.front(), endpoint.port), m_ipVersion);
    }

    return m_destAddr.get() != nullptr;
}

bool UDPSocket::sendTo(
    const void* buffer,
    std::size_t bufferLen,
    const SocketAddress& foreignEndpoint)
{
    setDestAddr(foreignEndpoint);
    return sendTo(buffer, bufferLen);
}

void UDPSocket::sendToAsync(
    const nx::Buffer* buffer,
    const SocketAddress& endpoint,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, SocketAddress, size_t)> handler)
{
    if (endpoint.address.isIpAddress())
    {
        SocketAddress resolvedEndpoint;
        resolvedEndpoint.address = endpoint.address.toPureIpAddress(this->m_ipVersion);
        resolvedEndpoint.port = endpoint.port;

        // TODO: #akolesnikov In case of ipv6 we have to determine scope_id.

        // It is possible that address still has to be resolved.
        // E.g., if ipv6 address has been passed to ipv4 socket.
        // In this case - returning an error.
        if (!resolvedEndpoint.address.isIpAddress())
        {
            return post(std::bind(std::move(handler),
                SystemError::addressNotAvailable,
                SocketAddress(),
                (size_t)-1));
        }

        setDestAddr(resolvedEndpoint);
        return sendAsync(
            buffer,
            [resolvedEndpoint, handler = std::move(handler)](
                SystemError::ErrorCode code, size_t size)
            {
                handler(code, resolvedEndpoint, size);
            });
    }

    m_aioHelper->resolve(
        endpoint.address,
        [this, buffer, port = endpoint.port, handler = std::move(handler)](
            SystemError::ErrorCode code, std::deque<HostAddress> ips) mutable
        {
            if (code != SystemError::noError)
            {
                // Making sure that handler is never invoked within sendToAsync thread.
                post(
                    [handler = std::move(handler), code]()
                    {
                        handler(code, SocketAddress(), 0);
                    });
                return;
            }

            auto addressIter = std::find_if(
                ips.begin(), ips.end(),
                [this](const HostAddress& addr)
                {
                    if (m_ipVersion == AF_INET && addr.ipV4())
                        return true;
                    if (m_ipVersion == AF_INET6 && addr.isPureIpV6())
                        return true;
                    return false;
                });
            if (addressIter == ips.end())
                addressIter = ips.begin();

            sendToAsync(
                buffer,
                SocketAddress(*addressIter, port),
                std::move(handler));
        });
}

int UDPSocket::recv(void* buffer, std::size_t bufferLen, int /*flags*/)
{
    //TODO #akolesnikov use flags
    return recvFrom(buffer, bufferLen, &m_prevDatagramAddress.address, &m_prevDatagramAddress.port);
}

int UDPSocket::recvFrom(
    void *buffer,
    std::size_t bufferLen,
    SocketAddress* const sourceAddress)
{
    int rtn = recvFrom(
        buffer,
        bufferLen,
        &m_prevDatagramAddress.address,
        &m_prevDatagramAddress.port);

    if (rtn >= 0 && sourceAddress)
        *sourceAddress = m_prevDatagramAddress;
    return rtn;
}

void UDPSocket::recvFromAsync(
    nx::Buffer* const buf,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, SocketAddress, size_t)> handler)
{
    readSomeAsync(
        buf,
        [handler = std::move(handler), this](
            SystemError::ErrorCode errCode, size_t bytesRead)
        {
            handler(errCode, std::move(m_prevDatagramAddress), bytesRead);
        });
}

SocketAddress UDPSocket::lastDatagramSourceAddress() const
{
    return m_prevDatagramAddress;
}

bool UDPSocket::hasData() const
{
#ifdef _WIN32
    fd_set read_set;
    struct timeval timeout;
    FD_ZERO(&read_set);
    FD_SET(handle(), &read_set);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    switch (::select(FD_SETSIZE, &read_set, NULL, NULL, &timeout))
    {
        case 0:             // timeout expired
            return false;
        case SOCKET_ERROR:  // error occured
            return false;
    }
    return true;
#else
    struct pollfd sockPollfd;
    memset(&sockPollfd, 0, sizeof(sockPollfd));
    sockPollfd.fd = handle();
    sockPollfd.events = POLLIN;
#ifdef _GNU_SOURCE
    sockPollfd.events |= POLLRDHUP;
#endif
    return (::poll(&sockPollfd, 1, 0) == 1) && ((sockPollfd.revents & POLLIN) != 0);
#endif
}

int UDPSocket::recvFrom(
    void* buffer,
    std::size_t bufferLen,
    HostAddress* const sourceAddress,
    quint16* const sourcePort)
{
    SystemSocketAddress address(m_ipVersion);

#ifdef _WIN32
    const auto h = handle();
    int rtn = recvfrom(h, (raw_type*)buffer, bufferLen, 0, address.get(), &address.length());
    if ((rtn == SOCKET_ERROR) &&
        (SystemError::getLastOSErrorCode() == SystemError::connectionReset))
    {
        // Win32 can return connectionReset for UDP(!) socket
        //   sometimes on receiving ICMP error response.
        SystemError::setLastErrorCode(SystemError::again);
        return -1;
    }
#else
    unsigned int recvTimeout = 0;
    if (!getRecvTimeout(&recvTimeout))
        return -1;

    int rtn = doInterruptableSystemCallWithTimeout<>(
        this,
        std::bind(&::recvfrom, handle(), (void*)buffer, (size_t)bufferLen, 0, address.get(), &address.length()),
        recvTimeout,
        0);
#endif

    if (rtn >= 0)
    {
        SocketAddress socketAddress = address.toSocketAddress();
        *sourceAddress = std::move(socketAddress.address);
        *sourcePort = socketAddress.port;
    }

    return rtn;
}

template class Socket<AbstractStreamServerSocket>;
template class Socket<AbstractStreamSocket>;
template class Socket<AbstractDatagramSocket>;
template class CommunicatingSocket<AbstractStreamSocket>;
template class CommunicatingSocket<AbstractDatagramSocket>;

} // namespace network
} // namespace nx
