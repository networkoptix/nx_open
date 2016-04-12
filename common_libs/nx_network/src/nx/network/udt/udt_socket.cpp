
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
#include <nx/utils/log/log.h>
#include <nx/network/system_socket.h>
#include <utils/common/checked_cast.h>

#include "udt_common.h"
#include "udt_socket_impl.h"
#include "../aio/async_socket_helper.h"
#ifdef _WIN32
#include "../win32_socket_tools.h"
#endif

#define ADDR_(x) reinterpret_cast<sockaddr*>(x)


namespace nx {
namespace network {
namespace detail {

// =========================================================
// Udt library initialization and tear down routine.
// =========================================================

class UdtLibrary {
public:
    static void Initialize() {
        std::call_once(kOnceFlag,&UdtLibrary::InitializeUdt);
    }

private:
    static void InitializeUdt() {
        UDT::startup();
    }
private:
    static std::once_flag kOnceFlag;
};

std::once_flag UdtLibrary::kOnceFlag;

void AddressFrom( const SocketAddress& local_addr , sockaddr_in* out ) {
    memset(out, 0, sizeof(*out));    // Zero out address structure
    out->sin_family = AF_INET;       // Internet address
    out->sin_addr = local_addr.address.inAddr();
    out->sin_port = htons(local_addr.port);
}

//TODO #ak we have UdtSocketImpl and UDTSocketImpl! refactor!
// Implementator to keep the layout of class clean and achieve binary compatible
class UdtSocketImpl
:
    public UDTSocketImpl
{
public:
    UdtSocketImpl()
    {
    }

    UdtSocketImpl(UDTSOCKET socket)
    :
        UDTSocketImpl(socket)
    {
    }
    
    ~UdtSocketImpl()
    {
    }

private:
    UdtSocketImpl(const UdtSocketImpl&);
    UdtSocketImpl& operator=(const UdtSocketImpl&);
};

struct UdtEpollHandlerHelper {
    UdtEpollHandlerHelper(int fd,UDTSOCKET udt_handler):
        epoll_fd(fd){
#ifndef TRACE_UDT_SOCKET
            Q_UNUSED(udt_handler);
#endif
    }
    ~UdtEpollHandlerHelper() {
        if(epoll_fd >=0) {
            int ret = UDT::epoll_release(epoll_fd);
#ifndef TRACE_UDT_SOCKET
            Q_UNUSED(ret);
#endif
        }
    }
    int epoll_fd;
    UDTSOCKET udt_handler;
};


}// namespace detail

// =====================================================================
// UdtSocket implementation
// =====================================================================

template<typename InterfaceToImplement>
UdtSocket<InterfaceToImplement>::UdtSocket()
:
    UdtSocket(new detail::UdtSocketImpl(), detail::SocketState::closed)
{
    detail::UdtLibrary::Initialize();
}

template<typename InterfaceToImplement>
UdtSocket<InterfaceToImplement>::~UdtSocket()
{
    //TODO #ak if socket is destroyed in its aio thread, it can cleanup here

    NX_ASSERT(!nx::network::SocketGlobals::aioService()
        .isSocketBeingWatched(static_cast<Pollable*>(this)));

    if (!isClosed())
        close();
}

template<typename InterfaceToImplement>
UdtSocket<InterfaceToImplement>::UdtSocket(
    detail::UdtSocketImpl* impl,
    detail::SocketState state)
:
    Pollable(-1, std::unique_ptr<detail::UdtSocketImpl>(impl)),
    m_state(state)
{
    this->m_impl = static_cast<detail::UdtSocketImpl*>(this->Pollable::m_impl.get());
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::bindToUdpSocket(UDPSocket&& udpSocket)
{
    //switching socket to blocking mode
    if (!udpSocket.setNonBlockingMode(false))
        return false;

    //taking system socket out of udpSocket
    if (UDT::bind2(m_impl->udtHandle, udpSocket.handle()) != 0)
    {
        SystemError::setLastErrorCode(
            detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
        return false;
    }
    udpSocket.takeHandle();
    return true;
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::bind(const SocketAddress& localAddress)
{
    sockaddr_in addr;
    detail::AddressFrom(localAddress, &addr);
    int ret = UDT::bind(m_impl->udtHandle, ADDR_(&addr), sizeof(addr));
    if (ret != 0)
        SystemError::setLastErrorCode(
            detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

template<typename InterfaceToImplement>
SocketAddress UdtSocket<InterfaceToImplement>::getLocalAddress() const
{
    sockaddr_in addr;
    int len = sizeof(addr);
    if (UDT::getsockname(m_impl->udtHandle, ADDR_(&addr), &len) != 0)
    {
        SystemError::setLastErrorCode(
            detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
        return SocketAddress();
    }
    else
    {
        return SocketAddress(
            HostAddress(addr.sin_addr), ntohs(addr.sin_port));
    }
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::close()
{
    //NX_ASSERT(!isClosed());

    if (m_impl->udtHandle == UDT::INVALID_SOCK)
        return true;    //already closed

                        //TODO #ak linger MUST be optional
                        //set UDT_LINGER to 1 if socket is in blocking mode?
    int val = 0;
    UDT::setsockopt(m_impl->udtHandle, 0, UDT_LINGER, &val, sizeof(val));

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
    //TODO #ak implementing shutdown via close may lead to undefined behavior in case of handle reuse

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
        SystemError::setLastErrorCode(detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::getReuseAddrFlag(bool* val) const
{
    NX_ASSERT(!isClosed());
    int len = sizeof(*val);
    int ret = UDT::getsockopt(m_impl->udtHandle, 0, UDT_REUSEADDR, val, &len);
    if (ret != 0)
        SystemError::setLastErrorCode(detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::setNonBlockingMode(bool val)
{
    NX_ASSERT(!isClosed());
    bool value = !val;
    int ret = UDT::setsockopt(m_impl->udtHandle, 0, UDT_SNDSYN, &value, sizeof(value));
    if (ret != 0)
    {
        SystemError::setLastErrorCode(detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
        return false;
    }
    ret = UDT::setsockopt(m_impl->udtHandle, 0, UDT_RCVSYN, &value, sizeof(value));
    if (ret != 0)
        SystemError::setLastErrorCode(detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::getNonBlockingMode(bool* val) const
{
    NX_ASSERT(!isClosed());
    int len = sizeof(*val);
    int ret = UDT::getsockopt(m_impl->udtHandle, 0, UDT_SNDSYN, val, &len);

    if (ret != 0)
        SystemError::setLastErrorCode(detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
    *val = !*val;
    return ret == 0;
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::getMtu(unsigned int* mtuValue) const
{
    int valueLength = sizeof(*mtuValue);
    int ret = UDT::getsockopt(m_impl->udtHandle, 0, UDT_MSS, mtuValue, &valueLength);
    if (ret != 0)
        SystemError::setLastErrorCode(
            detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::setSendBufferSize(unsigned int buffSize)
{
    NX_ASSERT(!isClosed());
    NX_ASSERT(buffSize < static_cast<unsigned int>(std::numeric_limits<int>::max()));
    int ret = UDT::setsockopt(m_impl->udtHandle, 0, UDT_SNDBUF, &buffSize, sizeof(buffSize));
    if (ret != 0)
        SystemError::setLastErrorCode(
            detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::getSendBufferSize(unsigned int* buffSize) const
{
    NX_ASSERT(!isClosed());
    int len = sizeof(*buffSize);
    int ret = UDT::getsockopt(m_impl->udtHandle, 0, UDT_SNDBUF, buffSize, &len);
    if (ret != 0)
        SystemError::setLastErrorCode(
            detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::setRecvBufferSize(unsigned int buffSize)
{
    NX_ASSERT(!isClosed());
    NX_ASSERT(buffSize < static_cast<unsigned int>(std::numeric_limits<int>::max()));
    int ret = UDT::setsockopt(m_impl->udtHandle, 0, UDT_RCVBUF, &buffSize, sizeof(buffSize));
    if (ret != 0)
        SystemError::setLastErrorCode(
            detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::getRecvBufferSize(unsigned int* buffSize) const
{
    NX_ASSERT(!isClosed());
    int len = sizeof(*buffSize);
    int ret = UDT::getsockopt(m_impl->udtHandle, 0, UDT_RCVBUF, buffSize, &len);
    if (ret != 0)
        SystemError::setLastErrorCode(
            detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
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
        SystemError::setLastErrorCode(
            detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
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
        SystemError::setLastErrorCode(detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
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
        SystemError::setLastErrorCode(detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
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
        SystemError::setLastErrorCode(detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

template<typename InterfaceToImplement>
bool UdtSocket<InterfaceToImplement>::getLastError(SystemError::ErrorCode* /*errorCode*/) const
{
    NX_ASSERT(!isClosed());
    //*errorCode = static_cast<SystemError::ErrorCode>(UDT::getlasterror().getErrno());
    //TODO #ak
    SystemError::setLastErrorCode(SystemError::notImplemented);
    return false;
}

template<typename InterfaceToImplement>
AbstractSocket::SOCKET_HANDLE UdtSocket<InterfaceToImplement>::handle() const
{
    NX_ASSERT(!isClosed());
    NX_ASSERT(sizeof(UDTSOCKET) == sizeof(AbstractSocket::SOCKET_HANDLE));
    return *reinterpret_cast<const AbstractSocket::SOCKET_HANDLE*>(&m_impl->udtHandle);
}

template<typename InterfaceToImplement>
aio::AbstractAioThread* UdtSocket<InterfaceToImplement>::getAioThread()
{
    return Pollable::getAioThread();
}

template<typename InterfaceToImplement>
void UdtSocket<InterfaceToImplement>::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    Pollable::bindToAioThread(aioThread);
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
    m_impl->udtHandle = UDT::socket(AF_INET, SOCK_STREAM, 0);
    if (m_impl->udtHandle == UDT::INVALID_SOCK)
    {
        SystemError::setLastErrorCode(
            detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
        return false;
    }

    //tuning udt socket
    constexpr const int kMtuSize = 1400;
    constexpr const int kMaximumUdtWindowSizePackets = 64;
    constexpr const int kUdtSendBufSize = 48 * kMtuSize;
    constexpr const int kUdtRecvBufSize = 48 * kMtuSize;
    constexpr const int kUdpSendBufSize = 48 * kMtuSize;
    constexpr const int kUdpRecvBufSize = 48 * kMtuSize;

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
        SystemError::setLastErrorCode(
            detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
        UDT::close(m_impl->udtHandle);
        return false;
    }

    m_state = detail::SocketState::open;
    return true;
}

template class UdtSocket<AbstractStreamSocket>;
template class UdtSocket<AbstractStreamServerSocket>;


// =====================================================================
// UdtStreamSocket implementation
// =====================================================================
UdtStreamSocket::UdtStreamSocket()
:
    m_aioHelper(
        new aio::AsyncSocketImplHelper<Pollable>(this, this, false /*natTraversal*/)),
    m_noDelay(false)
{
    open();
}

UdtStreamSocket::UdtStreamSocket(detail::UdtSocketImpl* impl, detail::SocketState state)
:
    UdtSocket(impl, state),
    m_aioHelper(new aio::AsyncSocketImplHelper<Pollable>(this, this, false)),
    m_noDelay(false)
{
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
    unsigned int timeoutMillis )
{
    //TODO #ak use timeoutMillis

    NX_ASSERT(m_state == detail::SocketState::open);
    sockaddr_in addr;
    detail::AddressFrom(remoteAddress, &addr);
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
    int ret = UDT::connect(m_impl->udtHandle, ADDR_(&addr), sizeof(addr));
    // The UDT connect will always return zero even if such operation is async which is 
    // different with the existed Posix/Win32 socket design. So if we meet an non-zero
    // value, the only explanation is an error happened which cannot be solved.
    if (ret != 0)
    {
        // Error happened
        SystemError::setLastErrorCode(detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
        return false;
    }
    m_state = detail::SocketState::connected;
    return true;
}

int UdtStreamSocket::recv(void* buffer, unsigned int bufferLen, int flags)
{
    if (m_impl->udtHandle == UDT::INVALID_SOCK)
    {
        SystemError::setLastErrorCode(SystemError::badDescriptor);
        return -1;
    }

    int sz = UDT::recv(m_impl->udtHandle, reinterpret_cast<char*>(buffer), bufferLen, flags);
    if (sz == UDT::ERROR)
    {
        const int udtErrorCode = UDT::getlasterror().getErrorCode();
        const auto sysErrorCode = detail::convertToSystemError(udtErrorCode);

        if (sysErrorCode != SystemError::wouldBlock)
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
    else if (sz == 0)
    {
        //connection has been closed
        m_state = detail::SocketState::open;
    }
    return sz;
}

int UdtStreamSocket::send( const void* buffer, unsigned int bufferLen )
{
    int sz = UDT::send(m_impl->udtHandle, reinterpret_cast<const char*>(buffer), bufferLen, 0);
    if (sz == UDT::ERROR)
    {
        const auto sysErrorCode =
            detail::convertToSystemError(UDT::getlasterror().getErrorCode());

        if (sysErrorCode != SystemError::wouldBlock)
            m_state = detail::SocketState::open;

        SystemError::setLastErrorCode(sysErrorCode);
    }
    else if (sz == 0)
    {
        //connection has been closed
        m_state = detail::SocketState::open;
    }
    return sz;
}

SocketAddress UdtStreamSocket::getForeignAddress() const {
    sockaddr_in addr;
    int len = sizeof(addr);
    if (UDT::getpeername(m_impl->udtHandle, ADDR_(&addr), &len) != 0)
    {
        SystemError::setLastErrorCode(
            detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
        return SocketAddress();
    }
    else
    {
        return SocketAddress(
            HostAddress(addr.sin_addr), ntohs(addr.sin_port));
    }
}

bool UdtStreamSocket::isConnected() const {
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
    //udt does not support it. Returned true so that caller code works
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
    //udt has keep-alives but provides no way to modify it...
    KeepAliveOptions options;
    options.probeCount = 10;    //TODO #ak find real value in udt
    options.timeSec = 5;
    options.intervalSec = 5;
    *result = options;
    return true;
}

void UdtStreamSocket::connectAsync(
    const SocketAddress& addr,
    nx::utils::MoveOnlyFunc<void( SystemError::ErrorCode )> handler )
{
    return m_aioHelper->connectAsync(
        addr,
        [this, handler = std::move(handler)](SystemError::ErrorCode errorCode) mutable
        {
            if (errorCode != SystemError::noError && errorCode != SystemError::wouldBlock)
                m_state = detail::SocketState::open;
            handler(errorCode);
        });
}

void UdtStreamSocket::readSomeAsync(
    nx::Buffer* const buf,
    std::function<void( SystemError::ErrorCode, size_t )> handler )
{
    return m_aioHelper->readSomeAsync(buf, std::move(handler));
}

void UdtStreamSocket::sendAsync(
    const nx::Buffer& buf,
    std::function<void( SystemError::ErrorCode, size_t )> handler )
{
    return m_aioHelper->sendAsync(buf, std::move(handler));
}

void UdtStreamSocket::registerTimer(
    std::chrono::milliseconds timeoutMillis,
    nx::utils::MoveOnlyFunc<void()> handler )
{
    return m_aioHelper->registerTimer(timeoutMillis, std::move(handler));
}

// =====================================================================
// UdtStreamServerSocket implementation
// =====================================================================
UdtStreamServerSocket::UdtStreamServerSocket()
:
    m_aioHelper(new aio::AsyncServerSocketHelper<UdtStreamServerSocket>(this))
{
    open();
}

UdtStreamServerSocket::~UdtStreamServerSocket()
{
    m_aioHelper->cancelIOSync();
}

bool UdtStreamServerSocket::listen( int backlog )
{
    NX_ASSERT(m_state == detail::SocketState::open);
    int ret = UDT::listen(m_impl->udtHandle, backlog);
    if (ret != 0)
    {
        const auto udtError = UDT::getlasterror();
        SystemError::setLastErrorCode(detail::convertToSystemError(udtError.getErrorCode()));
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

    //this method always blocks
    std::promise<
        std::pair<SystemError::ErrorCode, AbstractStreamSocket*>
    > acceptedSocketPromise;

    acceptAsync(
        [this, &acceptedSocketPromise](
            SystemError::ErrorCode errorCode, AbstractStreamSocket* socket)
        {
            //need post here to be sure that aio subsystem does not use socket anymore
            post(
                [&acceptedSocketPromise, errorCode, socket]
                {
                    acceptedSocketPromise.set_value(
                        std::make_pair(errorCode, socket));
                });
        });

    auto acceptedSocketPair = acceptedSocketPromise.get_future().get();
    if (acceptedSocketPair.first != SystemError::noError)
    {
        SystemError::setLastErrorCode(acceptedSocketPair.first);
        return nullptr;
    }
    return acceptedSocketPair.second;
}

void UdtStreamServerSocket::acceptAsync(
    nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode,
        AbstractStreamSocket*)> handler)
{
    return m_aioHelper->acceptAsync(
        [handler = std::move(handler)](
            SystemError::ErrorCode errorCode,
            AbstractStreamSocket* socket)
        {
            //every accepted socket MUST be in blocking mode!
            if (socket)
            {
                if (!socket->setNonBlockingMode(false))
                {
                    delete socket;
                    socket = nullptr;
                    errorCode = SystemError::getLastOSErrorCode();
                }
            }
            handler(errorCode, socket);
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
    nx::utils::MoveOnlyFunc< void() > handler )
{
    m_aioHelper->cancelIOAsync( std::move( handler ) );
}

AbstractStreamSocket* UdtStreamServerSocket::systemAccept()
{
    NX_ASSERT(m_state == detail::SocketState::connected);
    UDTSOCKET ret = UDT::accept(m_impl->udtHandle, NULL, NULL);
    if (ret == UDT::INVALID_SOCK)
    {
        SystemError::setLastErrorCode(
            detail::convertToSystemError(
                UDT::getlasterror().getErrorCode()));
        return nullptr;
    }
    else
    {
#ifdef TRACE_UDT_SOCKET
        NX_LOG(lit("accepted UDT socket %1").arg(ret), cl_logDEBUG2);
#endif
        return new UdtStreamSocket(
            new detail::UdtSocketImpl(ret),
            detail::SocketState::connected);
    }
}

}   //network
}   //nx
