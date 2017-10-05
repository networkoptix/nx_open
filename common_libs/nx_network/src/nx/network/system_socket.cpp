#include "system_socket.h"

#include <atomic>
#include <memory>

#include <boost/type_traits/is_same.hpp>

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

#include "aio/async_socket_helper.h"
#include "compat_poll.h"

#ifdef _WIN32
/* Check that the typedef in AbstractSocket is correct. */
static_assert(
    boost::is_same<AbstractSocket::SOCKET_HANDLE, SOCKET>::value,
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
#include "ssl_socket.h"
typedef void raw_type;       // Type used for raw data on this platform
#endif

#ifndef SOCKET_ERROR
#define SOCKET_ERROR (-1)
#endif

namespace nx {
namespace network {

#if defined(__arm__)
    qint64 totalSocketBytesSent() { return 0; }
#else
    static std::atomic<qint64> m_totalSocketBytesSent;
    qint64 totalSocketBytesSent() { return m_totalSocketBytesSent; }
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
    if (!addr.ptr)
        return false;

    return ::bind(m_fd, addr.ptr.get(), addr.size) == 0;
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

        return SocketAddress(addr.sin_addr, ntohs(addr.sin_port));
    }
    else if (m_ipVersion == AF_INET6)
    {
        sockaddr_in6 addr;
        socklen_t addrLen = sizeof(addr);
        if (::getsockname(m_fd, (sockaddr*)&addr, &addrLen) < 0)
            return SocketAddress();

        return SocketAddress(addr.sin6_addr, ntohs(addr.sin6_port));
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
    shutdown();

    if (m_fd == -1)
        return true;

    NX_ASSERT(
        !nx::network::SocketGlobals::isInitialized() ||
        !nx::network::SocketGlobals::aioService().isSocketBeingMonitored(static_cast<Pollable*>(this)));

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
    int reuseAddrVal = reuseAddr;

    if (::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseAddrVal, sizeof(reuseAddrVal)))
        return false;
    return true;
}

template<typename SocketInterfaceToImplement>
bool Socket<SocketInterfaceToImplement>::getReuseAddrFlag(bool* val) const
{
    int reuseAddrVal = 0;
    socklen_t optLen = 0;

    if (::getsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseAddrVal, &optLen))
        return false;

    *val = reuseAddrVal > 0;
    return true;
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
Pollable* Socket<SocketInterfaceToImplement>::pollable()
{
    return this;
}

template<typename SocketInterfaceToImplement>
void Socket<SocketInterfaceToImplement>::post(nx::utils::MoveOnlyFunc<void()> handler)
{
    if (impl()->terminated.load(std::memory_order_relaxed) > 0)
        return;

    nx::network::SocketGlobals::aioService().post(this, std::move(handler));
}

template<typename SocketInterfaceToImplement>
void Socket<SocketInterfaceToImplement>::dispatch(nx::utils::MoveOnlyFunc<void()> handler)
{
    if (impl()->terminated.load(std::memory_order_relaxed) > 0)
        return;

    nx::network::SocketGlobals::aioService().dispatch(this, std::move(handler));
}

template<typename SocketInterfaceToImplement>
Socket<SocketInterfaceToImplement>::Socket(
    int type,
    int protocol,
    int ipVersion,
    CommonSocketImpl* impl)
    :
    Pollable(
        INVALID_SOCKET,
        std::unique_ptr<CommonSocketImpl>(impl)),
    m_ipVersion(ipVersion),
    m_nonBlockingMode(false)
{
    createSocket(type, protocol);
}

template<typename SocketInterfaceToImplement>
Socket<SocketInterfaceToImplement>::Socket(
    int _sockDesc,
    int ipVersion,
    CommonSocketImpl* impl)
    :
    Pollable(
        _sockDesc,
        std::unique_ptr<CommonSocketImpl>(impl)),
    m_ipVersion(ipVersion),
    m_nonBlockingMode(false)
{
}

#ifdef _WIN32
static bool win32SocketsInitialized = false;
#endif

template<typename SocketInterfaceToImplement>
bool Socket<SocketInterfaceToImplement>::createSocket(int type, int protocol)
{
#ifdef _WIN32
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

    qWarning() << "created socket type =" << type << "proto =" << protocol << "ip version" << m_ipVersion;
    int on = 1;
    if (::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on)))
        return false;

    if (m_ipVersion == AF_INET6
        && ::setsockopt(m_fd, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&on, sizeof(on)))
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
        NX_LOGX(lm("Can not read options by fcntl: %1")
            .arg(SystemError::getLastOSErrorCode()), cl_logWARNING);
    }
    else if (fcntl(m_fd, F_SETFD, flags | FD_CLOEXEC) < 0)
    {
        NX_LOGX(lm("Can not set FD_CLOEXEC by fcntl: %1")
            .arg(SystemError::getLastOSErrorCode()), cl_logWARNING);
    }
#endif
    return true;
}

//-------------------------------------------------------------------------------------------------
// class CommunicatingSocket

#ifndef _WIN32
namespace {

template<class Func>
int doInterruptableSystemCallWithTimeout(const Func& func, unsigned int timeout)
{
    QElapsedTimer et;
    et.start();

    bool waitStartTimeActual = false;
    if (timeout > 0)
        waitStartTimeActual = true;
    for (;; )
    {
        int result = func();
        if (result == -1 && errno == EINTR)
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
        return result;
    }
}
} // namespace
#endif

template<typename SocketInterfaceToImplement>
CommunicatingSocket<SocketInterfaceToImplement>::CommunicatingSocket(
    int type,
    int protocol,
    int ipVersion,
    CommonSocketImpl* sockImpl)
    :
    Socket<SocketInterfaceToImplement>(
        type,
        protocol,
        ipVersion,
        sockImpl),
    m_aioHelper(new aio::AsyncSocketImplHelper<SelfType>(this, ipVersion)),
    m_connected(false)
#ifdef WIN32
    , m_eventObject(::CreateEvent(0, false, false, nullptr))
#endif
{
}

template<typename SocketInterfaceToImplement>
CommunicatingSocket<SocketInterfaceToImplement>::CommunicatingSocket(
    int newConnSD,
    int ipVersion,
    CommonSocketImpl* sockImpl)
    :
    Socket<SocketInterfaceToImplement>(
        newConnSD,
        ipVersion,
        sockImpl),
    m_aioHelper(new aio::AsyncSocketImplHelper<SelfType>(this, ipVersion)),
    m_connected(true)   // This constructor is used by server socket.
#ifdef WIN32
    , m_eventObject(::CreateEvent(0, false, false, nullptr))
#endif
{
}

template<typename SocketInterfaceToImplement>
CommunicatingSocket<SocketInterfaceToImplement>::~CommunicatingSocket()
{
    if (m_aioHelper)
        m_aioHelper->terminate();
#ifdef WIN32
    ::CloseHandle(m_eventObject);
#endif
}

template<typename SocketInterfaceToImplement>
bool CommunicatingSocket<SocketInterfaceToImplement>::connect(
    const SocketAddress& remoteAddress, unsigned int timeoutMs)
{
    if (remoteAddress.address.isIpAddress())
        return connectToIp(remoteAddress, timeoutMs);

    std::deque<HostAddress> resolvedAddresses;
    const SystemError::ErrorCode resultCode =
        SocketGlobals::addressResolver().dnsResolver().resolveSync(
            remoteAddress.address.toString(), this->m_ipVersion, &resolvedAddresses);
    if (resultCode != SystemError::noError)
    {
        SystemError::setLastErrorCode(resultCode);
        return false;
    }

    while (!resolvedAddresses.empty())
    {
        auto ip = std::move(resolvedAddresses.front());
        resolvedAddresses.pop_front();
        if (connectToIp(SocketAddress(std::move(ip), remoteAddress.port), timeoutMs))
            return true;
    }

    return false; //< Could not connect by any of addresses.
}

template<typename SocketInterfaceToImplement>
int CommunicatingSocket<SocketInterfaceToImplement>::recv(
    void* buffer, unsigned int bufferLen, int flags)
{
#ifdef _WIN32
    int bytesRead = -1;

    bool isNonBlockingMode;
    if (!getNonBlockingMode(&isNonBlockingMode))
        return -1;

    if (isNonBlockingMode || (flags & MSG_DONTWAIT))
    {
        if (!isNonBlockingMode)
        {
            if (!setNonBlockingMode(true))
                return -1;
        }

        bytesRead = ::recv(m_fd, (raw_type *)buffer, bufferLen, flags & ~MSG_DONTWAIT);

        if (!isNonBlockingMode)
        {
            // Save error code as changing mode will drop it.
            const auto sysErrorCodeBak = SystemError::getLastOSErrorCode();
            if (!setNonBlockingMode(false))
                return -1;
            SystemError::setLastErrorCode(sysErrorCodeBak);
        }
    }
    else
    {
        WSAOVERLAPPED overlapped;
        memset(&overlapped, 0, sizeof(overlapped));
        overlapped.hEvent = m_eventObject;

        WSABUF wsaBuffer;
        wsaBuffer.len = bufferLen;
        wsaBuffer.buf = (char*)buffer;
        DWORD wsaFlags = flags;
        DWORD* wsaBytesRead = (DWORD*)&bytesRead;

        auto wsaResult = WSARecv(m_fd, &wsaBuffer, /* buffer count*/ 1, wsaBytesRead,
            &wsaFlags, &overlapped, nullptr);
        if (wsaResult == SOCKET_ERROR && SystemError::getLastOSErrorCode() == WSA_IO_PENDING)
        {
            auto timeout = m_readTimeoutMS ? m_readTimeoutMS : INFINITE;

            WaitForSingleObject(m_eventObject, timeout);
            if (!WSAGetOverlappedResult(m_fd, &overlapped, wsaBytesRead, FALSE, &wsaFlags))
            {
                auto errCode = SystemError::getLastOSErrorCode();
                if (errCode == WSA_IO_INCOMPLETE)
                {
                    ::CancelIo((HANDLE)m_fd);
                    // Wait for:
                    // 1. CancelIo have been finished.
                    // 2. WSARecv have been finished.
                    // 3. Shutdown called.
                    WaitForSingleObject(m_eventObject, INFINITE);
                    // Check status again in case of race condition between CancelIo and other conditions
                    while (!WSAGetOverlappedResult(m_fd, &overlapped, wsaBytesRead, FALSE, &wsaFlags))
                    {
                        errCode = SystemError::getLastOSErrorCode();
                        if (errCode != WSA_IO_INCOMPLETE)
                        {
                            if (errCode == WSA_OPERATION_ABORTED)
                                SystemError::setLastErrorCode(SystemError::timedOut); //< emulate timeout code
                            return -1;
                        }
                        ::Sleep(0);
                    }
                }
            }
        }
    }
#else
    unsigned int recvTimeout = 0;
    if (!this->getRecvTimeout(&recvTimeout))
        return -1;

    int bytesRead = doInterruptableSystemCallWithTimeout<>(
        std::bind(&::recv, this->m_fd, (void*)buffer, (size_t)bufferLen, flags),
        recvTimeout);
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
    const void* buffer, unsigned int bufferLen)
{
#ifdef _WIN32
    int sended = ::send(m_fd, (raw_type*)buffer, bufferLen, 0);
#else
    unsigned int sendTimeout = 0;
    if (!this->getSendTimeout(&sendTimeout))
        return -1;

    int sended = doInterruptableSystemCallWithTimeout<>(
        std::bind(&::send, this->m_fd, (const void*)buffer, (size_t)bufferLen,
#ifdef __linux
            MSG_NOSIGNAL
#else
            0
#endif
        ),
        sendTimeout);
#endif

    if (sended < 0)
    {
        const SystemError::ErrorCode errCode = SystemError::getLastOSErrorCode();
        if (socketCannotRecoverFromError(errCode))
            m_connected = false;
    }
    else if (sended == 0)
    {
        m_connected = false;
    }
#if !defined(__arm__)
    m_totalSocketBytesSent += sended;
#endif
    return sended;
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

        return SocketAddress(addr.sin_addr, ntohs(addr.sin_port));
    }
    else if (this->m_ipVersion == AF_INET6)
    {
        sockaddr_in6 addr;
        socklen_t addrLen = sizeof(addr);
        if (::getpeername(this->m_fd, (sockaddr*)&addr, &addrLen) < 0)
            return SocketAddress();

        return SocketAddress(addr.sin6_addr, ntohs(addr.sin6_port));
    }

    return SocketAddress();
}

template<typename SocketInterfaceToImplement>
bool CommunicatingSocket<SocketInterfaceToImplement>::isConnected() const
{
    return m_connected;
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
#ifdef WIN32
    ::SetEvent(m_eventObject); //< Terminate current wait call.
#endif
    return result;
}

template<typename SocketInterfaceToImplement>
void CommunicatingSocket<SocketInterfaceToImplement>::cancelIOAsync(
    aio::EventType eventType,
    nx::utils::MoveOnlyFunc<void()> cancellationDoneHandler)
{
    m_aioHelper->cancelIOAsync(eventType, std::move(cancellationDoneHandler));
}

template<typename SocketInterfaceToImplement>
void CommunicatingSocket<SocketInterfaceToImplement>::cancelIOSync(aio::EventType eventType)
{
    m_aioHelper->cancelIOSync(eventType);
}

template<typename SocketInterfaceToImplement>
void CommunicatingSocket<SocketInterfaceToImplement>::connectAsync(
    const SocketAddress& addr,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    return m_aioHelper->connectAsync(
        addr,
        [handler = std::move(handler), this](
            SystemError::ErrorCode code)
        {
            m_connected = (code == SystemError::noError);
            handler(code);
        });
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
    const nx::Buffer& buf,
    IoCompletionHandler handler)
{
    return m_aioHelper->sendAsync(buf, std::move(handler));
}

template<typename SocketInterfaceToImplement>
void CommunicatingSocket<SocketInterfaceToImplement>::registerTimer(
    std::chrono::milliseconds timeoutMs,
    nx::utils::MoveOnlyFunc<void()> handler)
{
    //currently, aio considers 0 timeout as no timeout and will NOT call handler
    //NX_ASSERT(timeoutMs > std::chrono::milliseconds(0));
    if (timeoutMs == std::chrono::milliseconds(0))
        timeoutMs = std::chrono::milliseconds(1);  //handler of zero timer will NOT be called
    return m_aioHelper->registerTimer(timeoutMs, std::move(handler));
}

template<typename SocketInterfaceToImplement>
bool CommunicatingSocket<SocketInterfaceToImplement>::connectToIp(
    const SocketAddress& remoteAddress, unsigned int timeoutMs)
{
    // Get the address of the requested host.
    m_connected = false;

    const SystemSocketAddress addr(remoteAddress, this->m_ipVersion);
    if (!addr.ptr)
        return false;

    //switching to non-blocking mode to connect with timeout
    bool isNonBlockingModeBak = false;
    if (!this->getNonBlockingMode(&isNonBlockingModeBak))
        return false;
    if (!isNonBlockingModeBak && !this->setNonBlockingMode(true))
        return false;

    int connectResult = ::connect(this->m_fd, addr.ptr.get(), addr.size);
    if (connectResult != 0)
    {
        if (SystemError::getLastOSErrorCode() != SystemError::inProgress)
            return false;
        if (isNonBlockingModeBak)
            return true;        //async connect started
    }

    SystemError::ErrorCode connectErrorCode = SystemError::noError;

#ifdef _WIN32
    timeval timeVal;
    fd_set wrtFDS;
    FD_ZERO(&wrtFDS);
    FD_SET(m_fd, &wrtFDS);

    fd_set exceptFDS;
    FD_ZERO(&exceptFDS);
    FD_SET(m_fd, &exceptFDS);

    /* set timeout values */
    timeVal.tv_sec = timeoutMs / 1000;
    timeVal.tv_usec = (timeoutMs % 1000) * 1000;
    const int selectResult = ::select(
        m_fd + 1,
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
        if (FD_ISSET(m_fd, &wrtFDS))
        {
            m_connected = true;
        }
        else if (FD_ISSET(m_fd, &exceptFDS))
        {
            if (!getLastError(&connectErrorCode) || connectErrorCode == SystemError::noError)
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
                if (timeoutMs == 0 ||  //no timeout
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
        SOCK_STREAM,
        IPPROTO_TCP,
        ipVersion
#ifdef _WIN32
        , new Win32TcpSocketImpl()
#endif
    )
{
}

TCPSocket::TCPSocket(int newConnSD, int ipVersion):
    base_type(
        newConnSD,
        ipVersion
#ifdef _WIN32
        , new Win32TcpSocketImpl()
#endif
    )
{
}

TCPSocket::~TCPSocket()
{
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

bool TCPSocket::toggleStatisticsCollection(bool val)
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
    Q_UNUSED(val);
    return true;
#else
    Q_UNUSED(val);
    return false;
#endif
}

bool TCPSocket::getConnectionStatistics(StreamSocketInfo* info)
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
    Q_UNUSED(info);
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

bool TCPSocket::setKeepAlive(boost::optional< KeepAliveOptions > info)
{
    using namespace std::chrono;

#if defined( _WIN32 )
    struct tcp_keepalive ka = { FALSE, 0, 0 };
    if (info)
    {
        ka.onoff = TRUE;
        ka.keepalivetime = intDuration<milliseconds>(info->inactivityPeriodBeforeFirstProbe);
        ka.keepaliveinterval = intDuration<milliseconds>(info->probeSendPeriod);

        // the value can not be changed, 0 means default
        info->probeCount = 0;
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

bool TCPSocket::getKeepAlive(boost::optional< KeepAliveOptions >* result) const
{
    using namespace std::chrono;

    int isEnabled = 0;
    socklen_t length = sizeof(isEnabled);
    if (getsockopt(handle(), SOL_SOCKET, SO_KEEPALIVE,
        reinterpret_cast<char*>(&isEnabled), &length) != 0)
        return false;

    if (!isEnabled)
    {
        *result = boost::none;
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
    int m_fd,
    int timeoutMillis = DEFAULT_ACCEPT_TIMEOUT_MSEC,
    bool nonBlockingMode = false)
{
    if (nonBlockingMode)
        return ::accept(m_fd, NULL, NULL);

    int result = 0;
    if (timeoutMillis == 0)
        timeoutMillis = -1; // poll expects -1 as an infinity

#ifdef _WIN32
    struct pollfd fds[1];
    memset(fds, 0, sizeof(fds));
    fds[0].fd = m_fd;
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
        if (getsockopt(m_fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&errorCode), &errorCodeLen) != 0)
            return -1;
        ::SetLastError(errorCode);
        return -1;
    }
    return ::accept(m_fd, NULL, NULL);
#else
    struct pollfd sockPollfd;
    memset(&sockPollfd, 0, sizeof(sockPollfd));
    sockPollfd.fd = m_fd;
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
    {
        auto fd = ::accept(m_fd, NULL, NULL);
        return fd;
    }
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
        if (getsockopt(m_fd, SOL_SOCKET, SO_ERROR, &errorCode, &errorCodeLen) != 0)
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
    int socketHandle;
    const int ipVersion;
    aio::AsyncServerSocketHelper<TCPServerSocket> asyncServerSocketHelper;

    TCPServerSocketPrivate(TCPServerSocket* _sock, int _ipVersion):
        socketHandle(-1),
        ipVersion(_ipVersion),
        asyncServerSocketHelper(_sock)
    {
    }

    AbstractStreamSocket* accept(unsigned int recvTimeoutMs, bool nonBlockingMode)
    {
        int newConnSD = acceptWithTimeout(socketHandle, recvTimeoutMs, nonBlockingMode);
        if (newConnSD >= 0)
        {
            auto tcpSocket = new TCPSocket(newConnSD, ipVersion);
            tcpSocket->bindToAioThread(SocketGlobals::aioService().getRandomAioThread());
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
        SOCK_STREAM,
        IPPROTO_TCP,
        ipVersion,
        new TCPServerSocketPrivate(this, ipVersion))
{
    static_cast<TCPServerSocketPrivate*>(impl())->socketHandle = handle();
}

TCPServerSocket::~TCPServerSocket()
{
    if (isInSelfAioThread())
    {
        stopWhileInAioThread();
        return;
    }

    NX_CRITICAL(
        !nx::network::SocketGlobals::isInitialized() ||
        !nx::network::SocketGlobals::aioService()
        .isSocketBeingMonitored(static_cast<Pollable*>(this)),
        "You MUST cancel running async socket operation before "
        "deleting socket if you delete socket from non-aio thread");
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

void TCPServerSocket::cancelIOAsync(nx::utils::MoveOnlyFunc<void()> handler)
{
    TCPServerSocketPrivate* d = static_cast<TCPServerSocketPrivate*>(impl());
    return d->asyncServerSocketHelper.cancelIOAsync(std::move(handler));
}

void TCPServerSocket::cancelIOSync()
{
    TCPServerSocketPrivate* d = static_cast<TCPServerSocketPrivate*>(impl());
    return d->asyncServerSocketHelper.cancelIOSync();
}

bool TCPServerSocket::listen(int queueLen)
{
    return ::listen(handle(), queueLen) == 0;
}

void TCPServerSocket::pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    // TODO #ak: Add general implementation to Socket class and remove this method.
    dispatch(
        [this, completionHandler = std::move(completionHandler)]()
        {
            stopWhileInAioThread();
            completionHandler();
        });
}

void TCPServerSocket::pleaseStopSync(bool assertIfCalledUnderLock)
{
    if (isInSelfAioThread())
        stopWhileInAioThread();
    else
        QnStoppableAsync::pleaseStopSync(assertIfCalledUnderLock);
}

AbstractStreamSocket* TCPServerSocket::accept()
{
    return systemAccept();
}

AbstractStreamSocket* TCPServerSocket::systemAccept()
{
    TCPServerSocketPrivate* d = static_cast<TCPServerSocketPrivate*>(impl());

    unsigned int recvTimeoutMs = 0;
    if (!getRecvTimeout(&recvTimeoutMs))
        return nullptr;

    bool nonBlockingMode = false;
    if (!getNonBlockingMode(&nonBlockingMode))
        return nullptr;

    std::unique_ptr<AbstractStreamSocket> acceptedSocket(
        d->accept(recvTimeoutMs, nonBlockingMode));
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

    return acceptedSocket.release();
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
    base_type(SOCK_DGRAM, IPPROTO_UDP, ipVersion),
    m_destAddr()
{
    setBroadcast();
    int buff_size = 1024 * 512;
    if (::setsockopt(handle(), SOL_SOCKET, SO_RCVBUF, (const char*)&buff_size, sizeof(buff_size)) < 0)
    {
        //error
    }

    struct linger lingerOptions;
    memset(&lingerOptions, 0, sizeof(lingerOptions));
    if (setsockopt(
            handle(), SOL_SOCKET, SO_LINGER,
            (const char*)&lingerOptions, sizeof(lingerOptions)) < 0) 
    {
        // Ignoring for now.
    }
}

UDPSocket::~UDPSocket()
{
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
        m_destAddr.ptr.get(), m_destAddr.size) == bufferLen;
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
        std::bind(
            &::sendto, handle(), (const void*)buffer, (size_t)bufferLen, flags,
            m_destAddr.ptr.get(), m_destAddr.size),
        sendTimeout) == bufferLen;
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

bool UDPSocket::setMulticastIF(const QString& multicastIF)
{
    struct in_addr localInterface;
    localInterface.s_addr = inet_addr(multicastIF.toLatin1().data());
    if (setsockopt(
            handle(), IPPROTO_IP, IP_MULTICAST_IF,
            (raw_type *)&localInterface, sizeof(localInterface)) < 0)
    {
        return false;
    }
    return true;
}

bool UDPSocket::joinGroup(const QString &multicastGroup)
{
    struct ip_mreq multicastRequest;
    memset(&multicastRequest, 0, sizeof(multicastRequest));

    multicastRequest.imr_multiaddr.s_addr = inet_addr(multicastGroup.toLatin1());
    multicastRequest.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(
            handle(), IPPROTO_IP, IP_ADD_MEMBERSHIP,
            (raw_type *)&multicastRequest, sizeof(multicastRequest)) < 0)
    {
        qWarning() << "failed to join multicast group" << multicastGroup;
        return false;
    }
    return true;
}

bool UDPSocket::joinGroup(const QString &multicastGroup, const QString& multicastIF)
{
    struct ip_mreq multicastRequest;
    memset(&multicastRequest, 0, sizeof(multicastRequest));

    multicastRequest.imr_multiaddr.s_addr = inet_addr(multicastGroup.toLatin1());
    multicastRequest.imr_interface.s_addr = inet_addr(multicastIF.toLatin1());
    if (setsockopt(
            handle(), IPPROTO_IP, IP_ADD_MEMBERSHIP,
            (raw_type *)&multicastRequest, sizeof(multicastRequest)) < 0)
    {
        qWarning() << "failed to join multicast group" << multicastGroup
            << "from IF" << multicastIF << ". " << SystemError::getLastOSErrorText();
        return false;
    }
    return true;
}

bool UDPSocket::leaveGroup(const QString &multicastGroup)
{
    struct ip_mreq multicastRequest;
    memset(&multicastRequest, 0, sizeof(multicastRequest));

    multicastRequest.imr_multiaddr.s_addr = inet_addr(multicastGroup.toLatin1());
    multicastRequest.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(handle(), IPPROTO_IP, IP_DROP_MEMBERSHIP,
        (raw_type *)&multicastRequest,
        sizeof(multicastRequest)) < 0)
    {
        return false;
    }
    return true;
}

bool UDPSocket::leaveGroup(const QString &multicastGroup, const QString& multicastIF)
{
    struct ip_mreq multicastRequest;
    memset(&multicastRequest, 0, sizeof(multicastRequest));

    multicastRequest.imr_multiaddr.s_addr = inet_addr(multicastGroup.toLatin1());
    multicastRequest.imr_interface.s_addr = inet_addr(multicastIF.toLatin1());
    if (setsockopt(handle(), IPPROTO_IP, IP_DROP_MEMBERSHIP,
        (raw_type *)&multicastRequest,
        sizeof(multicastRequest)) < 0)
    {
        qWarning() << "Multicast group leave failed at IF" << multicastIF << "error:" <<
            SystemError::getLastOSErrorText();
        return false;
    }
    return true;
}

int UDPSocket::send(const void* buffer, unsigned int bufferLen)
{
#ifdef _WIN32
    return sendto(
        handle(), (raw_type *)buffer, bufferLen, 0,
        m_destAddr.ptr.get(), m_destAddr.size);
#else
    unsigned int sendTimeout = 0;
    if (!getSendTimeout(&sendTimeout))
        return -1;

    return doInterruptableSystemCallWithTimeout<>(
        std::bind(
            &::sendto, handle(),
            (const void*)buffer, (size_t)bufferLen, 0,
            m_destAddr.ptr.get(), m_destAddr.size),
        sendTimeout);
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
        std::deque<HostAddress> resolvedAddresses;
        const SystemError::ErrorCode resultCode =
            SocketGlobals::addressResolver().dnsResolver().resolveSync(
                endpoint.address.toString(), m_ipVersion, &resolvedAddresses);
        if (resultCode != SystemError::noError)
        {
            SystemError::setLastErrorCode(resultCode);
            return false;
        }

        // TODO: Here we select first address with hope it is correct one. This will never work
        // for NAT64, so we have to fix it somehow.
        m_destAddr = SystemSocketAddress(
            SocketAddress(resolvedAddresses.front(), endpoint.port), m_ipVersion);
    }

    return (bool)m_destAddr.ptr;
}

bool UDPSocket::sendTo(
    const void* buffer,
    unsigned int bufferLen,
    const SocketAddress& foreignEndpoint)
{
    setDestAddr(foreignEndpoint);
    return sendTo(buffer, bufferLen);
}

void UDPSocket::sendToAsync(
    const nx::Buffer& buffer,
    const SocketAddress& endpoint,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, SocketAddress, size_t)> handler)
{
    if (endpoint.address.isIpAddress())
    {
        setDestAddr(endpoint);
        return sendAsync(
            buffer,
            [endpoint, handler = std::move(handler)](
                SystemError::ErrorCode code, size_t size)
            {
                handler(code, endpoint, size);
            });
    }

    m_aioHelper->resolve(
        endpoint.address,
        [this, &buffer, port = endpoint.port, handler = std::move(handler)](
            SystemError::ErrorCode code, std::deque<HostAddress> ips) mutable
        {
            if (code != SystemError::noError)
                return handler(code, SocketAddress(), 0);

            // TODO: Here we select first address with hope it is correct one. This will never work
            // for NAT64, so we have to fix it somehow.
            sendToAsync(buffer, SocketAddress(ips.front(), port), std::move(handler));
        });
}

int UDPSocket::recv(void* buffer, unsigned int bufferLen, int /*flags*/)
{
    //TODO #ak use flags
    return recvFrom(buffer, bufferLen, &m_prevDatagramAddress.address, &m_prevDatagramAddress.port);
}

int UDPSocket::recvFrom(
    void *buffer,
    unsigned int bufferLen,
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
    unsigned int bufferLen,
    HostAddress* const sourceAddress,
    quint16* const sourcePort)
{
    SystemSocketAddress address(m_ipVersion);

    const auto sockAddrPtr = address.ptr.get();

#ifdef _WIN32
    const auto h = handle();
    int rtn = recvfrom(h, (raw_type *)buffer, bufferLen, 0, sockAddrPtr, &address.size);
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
        std::bind(&::recvfrom, handle(), (void*)buffer, (size_t)bufferLen, 0, sockAddrPtr, &address.size),
        recvTimeout);
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
template class CommunicatingSocket<AbstractStreamSocket>;
template class CommunicatingSocket<AbstractDatagramSocket>;

} // namespace network
} // namespace nx
