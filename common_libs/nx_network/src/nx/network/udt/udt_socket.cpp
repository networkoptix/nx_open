
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
    enum class SocketState
    {
        closed,
        open,
        connected
    };

    SocketState m_state;

    UdtSocketImpl(): m_state(SocketState::closed)
    {
        UdtLibrary::Initialize();
    }

    UdtSocketImpl( UDTSOCKET socket, SocketState state )
    :
        UDTSocketImpl(socket),
        m_state(state)
    {
    }
    
    ~UdtSocketImpl()
    {
        if(!IsClosed())
            Close();
    }
    // This creation will not put the socket into the Open status
    // the user Needs To Call Open function before doing anything
    // of this socket !
    static UdtSocketImpl* CreateSocket();
    bool Open();
    bool Bind( const SocketAddress& localAddress );
    SocketAddress GetLocalAddress() const;
    SocketAddress GetPeerAddress() const;
    bool Close();
    bool Shutdown();
    bool IsClosed() const;
    bool IsConnected() const {
        return m_state == SocketState::connected;
    }
    bool SetReuseAddrFlag( bool reuseAddr );
    bool GetReuseAddrFlag( bool* val ) const;
    bool SetNonBlockingMode( bool val );
    bool GetNonBlockingMode( bool* val ) const;
    bool GetMtu( unsigned int* mtuValue ) const;
    bool SetSendBufferSize( unsigned int buffSize );
    bool GetSendBufferSize( unsigned int* buffSize ) const;
    bool SetRecvBufferSize( unsigned int buffSize );
    bool GetRecvBufferSize( unsigned int* buffSize ) const;
    bool SetRecvTimeout( unsigned int millis );
    bool GetRecvTimeout( unsigned int* millis ) const;
    bool SetSendTimeout( unsigned int ms ) ;
    bool GetSendTimeout( unsigned int* millis ) const ;
    bool GetLastError( SystemError::ErrorCode* errorCode ) const;
    AbstractSocket::SOCKET_HANDLE handle() const;
    bool Reopen();

private:
    UdtSocketImpl(const UdtSocketImpl&);
    UdtSocketImpl& operator=(const UdtSocketImpl&);
};

bool UdtSocketImpl::Open()
{
    NX_ASSERT(IsClosed());
    udtHandle = UDT::socket(AF_INET,SOCK_STREAM,0);
    if( udtHandle == UDT::INVALID_SOCK )
    {
        SystemError::setLastErrorCode(
            convertToSystemError(UDT::getlasterror().getErrorCode()));
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
            udtHandle, 0, UDT_MSS,
            &kMtuSize, sizeof(kMtuSize)) != 0
        ||
        UDT::setsockopt(
            udtHandle, 0, UDT_FC,
            &kMaximumUdtWindowSizePackets, sizeof(kMaximumUdtWindowSizePackets)) != 0
        ||
        UDT::setsockopt(
            udtHandle, 0, UDT_SNDBUF,
            &kUdtSendBufSize, sizeof(kUdtSendBufSize)) != 0
        ||
        UDT::setsockopt(
            udtHandle, 0, UDT_RCVBUF,
            &kUdtRecvBufSize, sizeof(kUdtRecvBufSize)) != 0
        ||
        UDT::setsockopt(
            udtHandle, 0, UDP_SNDBUF,
            &kUdpSendBufSize, sizeof(kUdpSendBufSize)) != 0
        ||
        UDT::setsockopt(
            udtHandle, 0, UDP_RCVBUF,
            &kUdpRecvBufSize, sizeof(kUdpRecvBufSize)) != 0
        )
    {
        SystemError::setLastErrorCode(
            convertToSystemError(UDT::getlasterror().getErrorCode()));
        UDT::close(udtHandle);
        return false;
    }

    m_state = SocketState::open;
    return true;
}

bool UdtSocketImpl::Bind( const SocketAddress& localAddress ) {
    sockaddr_in addr;
    AddressFrom(localAddress,&addr);
    int ret = UDT::bind(udtHandle,ADDR_(&addr),sizeof(addr));
    if( ret != 0 )
        SystemError::setLastErrorCode(
            convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

SocketAddress UdtSocketImpl::GetLocalAddress() const {
    sockaddr_in addr;
    int len = sizeof(addr);
    if(UDT::getsockname(udtHandle,ADDR_(&addr),&len) != 0) {
        SystemError::setLastErrorCode(
            convertToSystemError(UDT::getlasterror().getErrorCode()));
        return SocketAddress();
    } else {
        return SocketAddress(
            HostAddress(addr.sin_addr),ntohs(addr.sin_port));
    }
}


SocketAddress UdtSocketImpl::GetPeerAddress() const {
    sockaddr_in addr;
    int len = sizeof(addr);
    if(UDT::getpeername(udtHandle,ADDR_(&addr),&len) !=0) {
        SystemError::setLastErrorCode(
            convertToSystemError(UDT::getlasterror().getErrorCode()));
        return SocketAddress();
    } else {
        return SocketAddress(
            HostAddress(addr.sin_addr),ntohs(addr.sin_port));
    }
}

bool UdtSocketImpl::Close() {
    //NX_ASSERT(!IsClosed());

    if (udtHandle == UDT::INVALID_SOCK)
        return true;    //already closed

    //TODO #ak linger MUST be optional
        //set UDT_LINGER to 1 if socket is in blocking mode?
    int val = 0;
    UDT::setsockopt(udtHandle, 0, UDT_LINGER, &val, sizeof(val));

#ifdef TRACE_UDT_SOCKET
    NX_LOG(lit("closing UDT socket %1").arg(udtHandle), cl_logDEBUG2);
#endif

    int ret = UDT::close(udtHandle);
    udtHandle = UDT::INVALID_SOCK;
    m_state = SocketState::closed;
    return ret == 0;
}

bool UdtSocketImpl::Shutdown() {
    //TODO #ak implementing shutdown via close may lead to undefined behavior in case of handle reuse

#if 0   //no LINGER
    const int ret = UDT::close(udtHandle);
    udtHandle = UDT::INVALID_SOCK;
    m_state = SocketState::closed;
    return ret == 0;

    return false;
#else
    return Close();
#endif
}

bool UdtSocketImpl::IsClosed() const {
    return m_state == SocketState::closed;
}

bool UdtSocketImpl::SetReuseAddrFlag( bool reuseAddr ) {
    NX_ASSERT(!IsClosed());
    int reuseAddrInt = reuseAddr ? 1 : 0;
    int ret = UDT::setsockopt(udtHandle, 0, UDT_REUSEADDR, &reuseAddrInt, sizeof(reuseAddrInt));
    if( ret != 0 )
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

bool UdtSocketImpl::GetReuseAddrFlag( bool* val ) const {
    NX_ASSERT(!IsClosed());
    int len = sizeof(*val);
    int ret = UDT::getsockopt(udtHandle,0,UDT_REUSEADDR,val,&len);
    if( ret != 0 )
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

bool UdtSocketImpl::SetNonBlockingMode( bool val ) {
    NX_ASSERT(!IsClosed());
    bool value = !val;
    int ret = UDT::setsockopt(udtHandle,0,UDT_SNDSYN,&value,sizeof(value));
    if( ret != 0 )
    {
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
        return false;
    }
    ret = UDT::setsockopt(udtHandle,0,UDT_RCVSYN,&value,sizeof(value));
    if( ret != 0 )
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

bool UdtSocketImpl::GetNonBlockingMode( bool* val ) const {
    NX_ASSERT(!IsClosed());
    int len = sizeof(*val);
    int ret = UDT::getsockopt(udtHandle,0,UDT_SNDSYN,val,&len);

    if( ret != 0 )
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
    *val = !*val;
    return ret == 0;
}

bool UdtSocketImpl::GetMtu( unsigned int* mtuValue ) const
{
    int valueLength = sizeof(*mtuValue);
    int ret = UDT::getsockopt(udtHandle, 0, UDT_MSS, mtuValue, &valueLength);
    if (ret != 0)
        SystemError::setLastErrorCode(
            convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

// UDT will round the buffer size internally, so don't expect
// what you give will definitely become the real buffer size .

bool UdtSocketImpl::SetSendBufferSize( unsigned int buffSize )
{
    NX_ASSERT(!IsClosed());
    NX_ASSERT( buffSize < static_cast<unsigned int>(std::numeric_limits<int>::max()) );
    int ret = UDT::setsockopt(udtHandle, 0, UDT_SNDBUF, &buffSize, sizeof(buffSize));
    if( ret != 0 )
        SystemError::setLastErrorCode(
            convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

bool UdtSocketImpl::GetSendBufferSize( unsigned int* buffSize ) const
{
    NX_ASSERT(!IsClosed());
    int len = sizeof(*buffSize);
    int ret = UDT::getsockopt(udtHandle, 0, UDT_SNDBUF, buffSize, &len);
    if( ret != 0 )
        SystemError::setLastErrorCode(
            convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

bool UdtSocketImpl::SetRecvBufferSize( unsigned int buffSize )
{
    NX_ASSERT(!IsClosed());
    NX_ASSERT(buffSize < static_cast<unsigned int>(std::numeric_limits<int>::max()));
    int ret = UDT::setsockopt(udtHandle, 0, UDT_RCVBUF, &buffSize, sizeof(buffSize));
    if( ret != 0 )
        SystemError::setLastErrorCode(
            convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

bool UdtSocketImpl::GetRecvBufferSize( unsigned int* buffSize ) const
{
    NX_ASSERT(!IsClosed());
    int len = sizeof(*buffSize);
    int ret = UDT::getsockopt(udtHandle, 0, UDT_RCVBUF, buffSize, &len);
    if( ret != 0 )
        SystemError::setLastErrorCode(
            convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

bool UdtSocketImpl::SetRecvTimeout( unsigned int millis ) {
    NX_ASSERT(!IsClosed());
    NX_ASSERT( millis < static_cast<unsigned int>(std::numeric_limits<int>::max()) );
    int time = millis ? static_cast<int>(millis) : -1;
    int ret = UDT::setsockopt(
        udtHandle,0,UDT_RCVTIMEO,&time,sizeof(time));
    if( ret != 0 )
        SystemError::setLastErrorCode(
            convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

bool UdtSocketImpl::GetRecvTimeout( unsigned int* millis ) const {
    NX_ASSERT(!IsClosed());
    int time;
    int len = sizeof(time);
    int ret = UDT::getsockopt(
        udtHandle,0,UDT_RCVTIMEO,&time,&len);
    *millis = (time == -1) ? 0 : static_cast<unsigned int>(time);
    if( ret != 0 )
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

bool UdtSocketImpl::SetSendTimeout( unsigned int ms ) {
    NX_ASSERT(!IsClosed());
    NX_ASSERT( ms < static_cast<unsigned int>(std::numeric_limits<int>::max()) );
    int time = ms ? static_cast<int>(ms) : -1;
    int ret = UDT::setsockopt(
        udtHandle,0,UDT_SNDTIMEO,&time,sizeof(time));
    if( ret != 0 )
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

bool UdtSocketImpl::GetSendTimeout(unsigned int* millis) const
{
    NX_ASSERT(!IsClosed());
    int time;
    int len = sizeof(time);
    int ret = UDT::getsockopt(
        udtHandle,0,UDT_SNDTIMEO,&time,&len);
    *millis = (time == -1) ? 0 : static_cast<unsigned int>(time);
    if( ret != 0 )
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

bool UdtSocketImpl::GetLastError(SystemError::ErrorCode* /*errorCode*/) const
{
    NX_ASSERT(!IsClosed());
    //*errorCode = static_cast<SystemError::ErrorCode>(UDT::getlasterror().getErrno());
    //TODO #ak
    SystemError::setLastErrorCode(SystemError::notImplemented);
    return false;
}

AbstractSocket::SOCKET_HANDLE UdtSocketImpl::handle() const
{
    NX_ASSERT(!IsClosed());
    NX_ASSERT(sizeof(UDTSOCKET) == sizeof(AbstractSocket::SOCKET_HANDLE));
    return *reinterpret_cast<const AbstractSocket::SOCKET_HANDLE*>(&udtHandle);
}

// This is a meaningless function. You could call Close and then create another
// one if you bundle the states not_initialized into the original Socket design.
bool UdtSocketImpl::Reopen() {
    if(IsClosed()) {
        return Open();
    }
    return true;
}

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

UdtSocket::UdtSocket()
:
    UdtSocket(new detail::UdtSocketImpl())
{
}

UdtSocket::~UdtSocket()
{
    //TODO #ak if socket is destroyed in its aio thread, it can cleanup here

    NX_ASSERT(!nx::network::SocketGlobals::aioService()
        .isSocketBeingWatched(static_cast<Pollable*>(this)));
}

UdtSocket::UdtSocket(detail::UdtSocketImpl* impl)
:
    Pollable(-1, std::unique_ptr<detail::UdtSocketImpl>(impl))
{
    this->m_impl = static_cast<detail::UdtSocketImpl*>(this->Pollable::m_impl.get());
}

bool UdtSocket::bindToUdpSocket(UDPSocket&& udpSocket)
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

bool UdtSocket::getLastError(SystemError::ErrorCode* errorCode)
{
    return m_impl->GetLastError(errorCode);
}

bool UdtSocket::getRecvTimeout(unsigned int* millis)
{
    return m_impl->GetRecvTimeout(millis);
}

bool UdtSocket::getSendTimeout(unsigned int* millis)
{
    return m_impl->GetSendTimeout(millis);
}

// =====================================================================
// UdtStreamSocket implementation
// =====================================================================
UdtStreamSocket::UdtStreamSocket()
:
    m_aioHelper(
        new aio::AsyncSocketImplHelper<Pollable>(this, this, false /*natTraversal*/)),
    m_noDelay(false)
{
    m_impl->Open();
}

UdtStreamSocket::UdtStreamSocket(detail::UdtSocketImpl* impl)
:
    UdtSocket(impl),
    m_aioHelper(new aio::AsyncSocketImplHelper<Pollable>(this, this, false)),
    m_noDelay(false)
{
}

UdtStreamSocket::~UdtStreamSocket()
{
    m_aioHelper->terminate();
}

bool UdtStreamSocket::bind( const SocketAddress& localAddress ) {
    return m_impl->Bind(localAddress);
}

SocketAddress UdtStreamSocket::getLocalAddress() const {
    return m_impl->GetLocalAddress();
}

void UdtStreamSocket::close() {
    m_impl->Close();
}

void UdtStreamSocket::shutdown() {
    m_impl->Shutdown();
}

bool UdtStreamSocket::isClosed() const {
    return m_impl->IsClosed();
}

bool UdtStreamSocket::setReuseAddrFlag( bool reuseAddr ) {
    return m_impl->SetReuseAddrFlag(reuseAddr);
}

bool UdtStreamSocket::getReuseAddrFlag( bool* val ) const {
    return m_impl->GetReuseAddrFlag(val);
}

bool UdtStreamSocket::setNonBlockingMode( bool val ) {
    return m_impl->SetNonBlockingMode(val);
}

bool UdtStreamSocket::getNonBlockingMode( bool* val ) const {
    return m_impl->GetNonBlockingMode(val);
}

bool UdtStreamSocket::getMtu( unsigned int* mtuValue ) const {
    return m_impl->GetMtu(mtuValue);
}

bool UdtStreamSocket::setSendBufferSize( unsigned int buffSize ) {
    return m_impl->SetSendBufferSize(buffSize);
}

bool UdtStreamSocket::getSendBufferSize( unsigned int* buffSize ) const {
    return m_impl->GetSendBufferSize(buffSize);
}

bool UdtStreamSocket::setRecvBufferSize( unsigned int buffSize ) {
    return m_impl->SetRecvBufferSize(buffSize);
}

bool UdtStreamSocket::getRecvBufferSize( unsigned int* buffSize ) const {
    return m_impl->GetRecvBufferSize( buffSize );
}

bool UdtStreamSocket::setRecvTimeout( unsigned int millis ) {
    if (m_impl->SetRecvTimeout(millis))
    {
        m_readTimeoutMS = millis;
        return true;
    }
    return false;
}

bool UdtStreamSocket::getRecvTimeout( unsigned int* millis ) const {
    return m_impl->GetRecvTimeout(millis);
}

bool UdtStreamSocket::setSendTimeout( unsigned int ms )  {
    if (m_impl->SetSendTimeout(ms))
    {
        m_writeTimeoutMS = ms;
        return true;
    }
    return false;
}

bool UdtStreamSocket::getSendTimeout( unsigned int* millis ) const {
    return m_impl->GetSendTimeout(millis);
}

bool UdtStreamSocket::getLastError( SystemError::ErrorCode* errorCode ) const {
    return m_impl->GetLastError(errorCode);
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

    NX_ASSERT(m_impl->m_state == detail::UdtSocketImpl::SocketState::open);
    sockaddr_in addr;
    detail::AddressFrom(remoteAddress, &addr);
    // The official documentation doesn't advice using select but here we just need
    // to wait on a single socket fd, select is way more faster than epoll on linux
    // since epoll needs to create the internal structure inside of kernel.
    bool nbk_sock = false;
    if (!m_impl->GetNonBlockingMode(&nbk_sock))
        return false;
    if (!nbk_sock)
    {
        if (!m_impl->SetNonBlockingMode(nbk_sock))
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
    m_impl->m_state = detail::UdtSocketImpl::SocketState::connected;
    return true;
}

int UdtStreamSocket::recv( void* buffer, unsigned int bufferLen, int flags )
{
    int sz = UDT::recv(m_impl->udtHandle, reinterpret_cast<char*>(buffer), bufferLen, flags);
    if (sz == UDT::ERROR)
    {
        const auto sysErrorCode =
            detail::convertToSystemError(UDT::getlasterror().getErrorCode());

        if (sysErrorCode != SystemError::wouldBlock)
            m_impl->m_state = detail::UdtSocketImpl::SocketState::open;

        // UDT doesn't translate the EOF into a recv with zero return, but instead
        // it returns error with 2001 error code. We need to detect this and translate
        // back with a zero return here .
        int error_code = UDT::getlasterror().getErrorCode();
        if (error_code == CUDTException::ECONNLOST)
        {
            return 0;
        }
        else if (error_code == CUDTException::EINVSOCK)
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
        m_impl->m_state = detail::UdtSocketImpl::SocketState::open;
    }
    return sz;
}

int UdtStreamSocket::send( const void* buffer, unsigned int bufferLen )
{
    NX_ASSERT(!m_impl->IsClosed());
    int sz = UDT::send(m_impl->udtHandle, reinterpret_cast<const char*>(buffer), bufferLen, 0);
    if (sz == UDT::ERROR)
    {
        const auto sysErrorCode =
            detail::convertToSystemError(UDT::getlasterror().getErrorCode());

        if (sysErrorCode != SystemError::wouldBlock)
            m_impl->m_state = detail::UdtSocketImpl::SocketState::open;

        SystemError::setLastErrorCode(sysErrorCode);
    }
    else if (sz == 0)
    {
        //connection has been closed
        m_impl->m_state = detail::UdtSocketImpl::SocketState::open;
    }
    return sz;
}

SocketAddress UdtStreamSocket::getForeignAddress() const {
    return m_impl->GetPeerAddress();
}

bool UdtStreamSocket::isConnected() const {
    return m_impl->IsConnected();
}

bool UdtStreamSocket::reopen() {
    return m_impl->Reopen();
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

void UdtStreamSocket::post( nx::utils::MoveOnlyFunc<void()> handler )
{
    nx::network::SocketGlobals::aioService().post(
        static_cast<Pollable*>(this),
        std::move(handler) );
}

void UdtStreamSocket::dispatch( nx::utils::MoveOnlyFunc<void()> handler )
{
    nx::network::SocketGlobals::aioService().dispatch(
        static_cast<Pollable*>(this),
        std::move(handler) );
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
                m_impl->m_state = detail::UdtSocketImpl::SocketState::open;
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

AbstractSocket::SOCKET_HANDLE UdtStreamSocket::handle() const {
    return m_impl->handle();
}

aio::AbstractAioThread* UdtStreamSocket::getAioThread()
{
    return UdtSocket::getAioThread();
}

void UdtStreamSocket::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    UdtSocket::bindToAioThread(aioThread);
}

// =====================================================================
// UdtStreamServerSocket implementation
// =====================================================================
UdtStreamServerSocket::UdtStreamServerSocket()
:
    m_aioHelper(new aio::AsyncServerSocketHelper<UdtStreamServerSocket>(this))
{
    m_impl->Open();
}

UdtStreamServerSocket::~UdtStreamServerSocket()
{
    m_aioHelper->cancelIOSync();
}

bool UdtStreamServerSocket::listen( int backlog )
{
    NX_ASSERT(m_impl->m_state == detail::UdtSocketImpl::SocketState::open);
    int ret = UDT::listen(m_impl->udtHandle, backlog);
    if (ret != 0)
    {
        const auto udtError = UDT::getlasterror();
        SystemError::setLastErrorCode(detail::convertToSystemError(udtError.getErrorCode()));
        return false;
    }
    else
    {
        m_impl->m_state = detail::UdtSocketImpl::SocketState::connected;
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

bool UdtStreamServerSocket::bind( const SocketAddress& localAddress ) {
    return m_impl->Bind(localAddress);
}

SocketAddress UdtStreamServerSocket::getLocalAddress() const {
    return m_impl->GetLocalAddress();
}

void UdtStreamServerSocket::close() {
    m_impl->Close();
}

void UdtStreamServerSocket::shutdown() {
    m_impl->Shutdown();
}

bool UdtStreamServerSocket::isClosed() const {
    return m_impl->IsClosed();
}

bool UdtStreamServerSocket::setReuseAddrFlag( bool reuseAddr ) {
    return m_impl->SetReuseAddrFlag(reuseAddr);
}

bool UdtStreamServerSocket::getReuseAddrFlag( bool* val ) const {
    return m_impl->GetReuseAddrFlag(val);
}

bool UdtStreamServerSocket::setNonBlockingMode( bool val ) {
    return m_impl->SetNonBlockingMode(val);
}

bool UdtStreamServerSocket::getNonBlockingMode( bool* val ) const {
    return m_impl->GetNonBlockingMode(val);
}

bool UdtStreamServerSocket::getMtu( unsigned int* mtuValue ) const {
    return m_impl->GetMtu(mtuValue);
}

bool UdtStreamServerSocket::setSendBufferSize( unsigned int buffSize ) {
    return m_impl->SetSendBufferSize(buffSize);
}

bool UdtStreamServerSocket::getSendBufferSize( unsigned int* buffSize ) const {
    return m_impl->GetSendBufferSize(buffSize);
}

bool UdtStreamServerSocket::setRecvBufferSize( unsigned int buffSize ) {
    return m_impl->SetRecvBufferSize(buffSize);
}

bool UdtStreamServerSocket::getRecvBufferSize( unsigned int* buffSize ) const {
    return m_impl->GetRecvBufferSize(buffSize);
}

bool UdtStreamServerSocket::setRecvTimeout( unsigned int millis ) {
    if (m_impl->SetRecvTimeout(millis))
    {
        m_readTimeoutMS = millis;
        return true;
    }
    return false;
}

bool UdtStreamServerSocket::getRecvTimeout( unsigned int* millis ) const {
    return m_impl->GetRecvTimeout(millis);
}

bool UdtStreamServerSocket::setSendTimeout( unsigned int ms ) {
    if (m_impl->SetSendTimeout(ms))
    {
        m_writeTimeoutMS = ms;
        return true;
    }
    return false;
}

bool UdtStreamServerSocket::getSendTimeout( unsigned int* millis ) const {
    return m_impl->GetSendTimeout(millis);
}

bool UdtStreamServerSocket::getLastError( SystemError::ErrorCode* errorCode ) const {
    return m_impl->GetLastError(errorCode);
}

void UdtStreamServerSocket::post( nx::utils::MoveOnlyFunc<void()> handler )
{
    nx::network::SocketGlobals::aioService().post(
        static_cast<Pollable*>(this),
        std::move(handler) );
}

void UdtStreamServerSocket::dispatch( nx::utils::MoveOnlyFunc<void()> handler )
{
    nx::network::SocketGlobals::aioService().dispatch(
        static_cast<Pollable*>(this),
        std::move(handler) );
}

AbstractStreamSocket* UdtStreamServerSocket::systemAccept()
{
    NX_ASSERT(m_impl->m_state == detail::UdtSocketImpl::SocketState::connected);
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
        auto* impl =
            new detail::UdtSocketImpl(
                ret,
                detail::UdtSocketImpl::SocketState::connected);
        return new UdtStreamSocket(impl);
    }
}

AbstractSocket::SOCKET_HANDLE UdtStreamServerSocket::handle() const {
    return m_impl->handle();
}

aio::AbstractAioThread* UdtStreamServerSocket::getAioThread()
{
    return UdtSocket::getAioThread();
}

void UdtStreamServerSocket::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    UdtSocket::bindToAioThread(aioThread);
}

}   //network
}   //nx
