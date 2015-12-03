
#include "udt_socket.h"

#ifdef _WIN32
#  include <ws2tcpip.h>
#  include <iphlpapi.h>
#else
#  include <netinet/tcp.h>
#endif

#include <set>
#include <mutex>

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

// Tracking operation. This operation should be put inside of the DEBUG_
// macro and also using it will log the error that has happened at last 
// time using general log macro. This helpful for us to do debugging and
// event tracing.

#ifdef TRACE_UDT_SOCKET
#define TRACE_(func,handler)                                                                                            \
    do {                                                                                                                \
        NX_LOG(                                                                                                         \
            QString(QLatin1String("Udt function[%1]:%2 has error!\nUdt Error Message:%3.\nSystem Error Message:%4\n"    \
            "Udt Error Code:%5\n"                                                                                       \
            "File:%6 Line:%7")).                                                                                        \
            arg(static_cast<int>(handler)).arg(QLatin1String(func)).                                                    \
            arg(QLatin1String(UDT::getlasterror().getErrorMessage())).                                                  \
            arg(SystemError::getLastOSErrorText()).                                                                     \
            arg(UDT::getlasterror_code()).                                                                              \
            arg(QLatin1String(__FILE__)).arg(__LINE__),cl_logERROR);                                                    \
    } while(0)
#else
#define TRACE_(func,handler) (void*)(NULL)
#endif // 

// Verify macro is useful for us to push up the error detection in DEBUG mode
// It will trigger a assertion failed in DEBUG version or LOG ERROR once in 
// the RELEASE version. This is useful for those ALWAYS RIGHT error, like 
// create a thread, or close a socket .

#ifdef VERIFY_UDT_RETURN
#define VERIFY_(cond,func,handler) \
    do { \
    bool eval = (cond); \
    if(!eval) TRACE_(func,handler); Q_ASSERT(cond);}while(0)
#else
#define VERIFY_(cond,func,handler) \
    (void*)(NULL)
#endif // NDEBUG

#ifndef NDEBUG
#define DEBUG_(...) __VA_ARGS__
#else
#define DEBUG_(...) (void*)(NULL)
#endif // NDEBUG

#define OK_(r) ((r) == 0)

#ifndef NDEBUG
#define STUB_(c) Q_ASSERT(!"Unimplemented!"); c
#else
#define STUB_(c) c
#endif

namespace detail{
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

class UdtAcceptor;

UdtSocketImplPtr::UdtSocketImplPtr( UdtSocketImpl* imp ) : 
    std::unique_ptr<UdtSocketImpl>(imp){}

UdtSocketImplPtr::~UdtSocketImplPtr(){}

// Implementator to keep the layout of class clean and achieve binary compatible
class UdtSocketImpl
:
    public UDTSocketImpl
{
public:
    int state_;

    enum {
        CLOSED,
        OPEN,
        ESTABLISHING,
        ESTABLISHED
    };
    UdtSocketImpl(): state_(CLOSED) {
        UdtLibrary::Initialize();
    }
    UdtSocketImpl( UDTSOCKET socket, int state ) : UDTSocketImpl(socket),state_(state){}
    ~UdtSocketImpl() {
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
    bool IsClosed() const;
    bool IsConnected() const {
        return state_ == ESTABLISHED;
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
    int Recv( void* buffer, unsigned int bufferLen, int flags );
    int Send( const void* buffer, unsigned int bufferLen );
    bool Reopen();
};

bool UdtSocketImpl::Open() {
    Q_ASSERT(IsClosed());
    udtHandle = UDT::socket(AF_INET,SOCK_STREAM,0);
#ifdef TRACE_UDT_SOCKET
    NX_LOG(lit("created UDT socket %1").arg(udtHandle), cl_logDEBUG2);
#endif
    VERIFY_(udtHandle != UDT::INVALID_SOCK,"UDT::socket",0);
    if( udtHandle != UDT::INVALID_SOCK ) {
        state_ = OPEN;
        return true;
    } else {
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
        return false;
    }
}

bool UdtSocketImpl::Bind( const SocketAddress& localAddress ) {
    sockaddr_in addr;
    AddressFrom(localAddress,&addr);
    int ret = UDT::bind(udtHandle,ADDR_(&addr),sizeof(addr));
    DEBUG_(if(ret !=0) TRACE_("UDT::bind",udtHandle));
    if( ret != 0 )
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

SocketAddress UdtSocketImpl::GetLocalAddress() const {
    sockaddr_in addr;
    int len = sizeof(addr);
    if(UDT::getsockname(udtHandle,ADDR_(&addr),&len) != 0) {
        DEBUG_(TRACE_("UDT::getsockname",udtHandle));
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
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
        DEBUG_(TRACE_("UDT::getsockname",udtHandle));
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
        return SocketAddress();
    } else {
        return SocketAddress(
            HostAddress(addr.sin_addr),ntohs(addr.sin_port));
    }
}

bool UdtSocketImpl::Close() {
    //Q_ASSERT(!IsClosed());

    int val = 1;
    UDT::setsockopt(udtHandle, 0, UDT_LINGER, &val, sizeof(val));

#ifdef TRACE_UDT_SOCKET
    NX_LOG(lit("closing UDT %1").arg(udtHandle), cl_logDEBUG2);
#endif

    int ret = UDT::close(udtHandle);
    //VERIFY_(OK_(ret),"UDT::Close",udtHandle);
    udtHandle = UDT::INVALID_SOCK;
    state_ = CLOSED;
    return ret == 0;
}

bool UdtSocketImpl::IsClosed() const {
    return state_ == CLOSED;
}

bool UdtSocketImpl::SetReuseAddrFlag( bool reuseAddr ) {
    Q_ASSERT(!IsClosed());
    int ret = UDT::setsockopt(udtHandle,0,UDT_REUSEADDR,&reuseAddr,sizeof(reuseAddr));
    VERIFY_(OK_(ret),"UDT::setsockopt",udtHandle);
    if( ret != 0 )
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

bool UdtSocketImpl::GetReuseAddrFlag( bool* val ) const {
    Q_ASSERT(!IsClosed());
    int len = sizeof(*val);
    int ret = UDT::getsockopt(udtHandle,0,UDT_REUSEADDR,val,&len);
    VERIFY_(OK_(ret),"UDT::getsockopt",udtHandle);
    if( ret != 0 )
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

bool UdtSocketImpl::SetNonBlockingMode( bool val ) {
    Q_ASSERT(!IsClosed());
    bool value = !val;
    int ret = UDT::setsockopt(udtHandle,0,UDT_SNDSYN,&value,sizeof(value));
    VERIFY_(OK_(ret),"UDT::setsockopt",udtHandle);
    if( ret != 0 )
    {
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
        return false;
    }
    ret = UDT::setsockopt(udtHandle,0,UDT_RCVSYN,&value,sizeof(value));
    VERIFY_(OK_(ret),"UDT::setsockopt",udtHandle);
    if( ret != 0 )
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

bool UdtSocketImpl::GetNonBlockingMode( bool* val ) const {
    Q_ASSERT(!IsClosed());
    int len = sizeof(*val);
    int ret = UDT::getsockopt(udtHandle,0,UDT_SNDSYN,val,&len);
    VERIFY_(OK_(ret),"UDT::getsockopt",udtHandle);
    DEBUG_(
        bool bval;
        ret = UDT::getsockopt(udtHandle,0,UDT_RCVSYN,&bval,&len); 
        VERIFY_(OK_(ret) && *val == bval,"UDT::getsockopt",udtHandle));

    if( ret != 0 )
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
    *val = !*val;
    return ret == 0;
}

// No way. I don't understand why we need MTU for each socket ?
// No one knows the MTU, they use ICMP/UDP to try to detect it,
// and in modern switch, typically will change every half hour !!!
// Better keep it opaque when you make decision what to send .

//mtu is known per path and is available for connected socket only.
//It present in AbstractStreamSocket since it was implemented only by TcpSocket
//BTW, I have never seen it changing once per 30 minutes for a given path

bool UdtSocketImpl::GetMtu( unsigned int* mtuValue ) const {
    Q_ASSERT(!IsClosed());
    *mtuValue = 1500; // Ethernet MTU 
    return true;
}

// UDT will round the buffer size internally, so don't expect
// what you give will definitely become the real buffer size .

bool UdtSocketImpl::SetSendBufferSize( unsigned int buffSize ) {
    Q_ASSERT(!IsClosed());
    Q_ASSERT( buffSize < static_cast<unsigned int>(std::numeric_limits<int>::max()) );
    int buff_size = static_cast<int>(buffSize);
    int ret = UDT::setsockopt(
        udtHandle,0,UDT_SNDBUF,&buff_size,sizeof(buff_size));
    VERIFY_(OK_(ret),"UDT::getsockopt",udtHandle);
    if( ret != 0 )
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

bool UdtSocketImpl::GetSendBufferSize( unsigned int* buffSize ) const{
    Q_ASSERT(!IsClosed());
    int buff_size;
    int len = sizeof(buff_size);
    int ret = UDT::getsockopt(
        udtHandle,0,UDT_SNDBUF,&buff_size,&len);
    VERIFY_(OK_(ret),"UDT::getsockopt",udtHandle);
    *buffSize = static_cast<unsigned int>(buff_size);
    if( ret != 0 )
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

bool UdtSocketImpl::SetRecvBufferSize( unsigned int buffSize ){
    Q_ASSERT(!IsClosed());
    Q_ASSERT( buffSize < static_cast<unsigned int>(std::numeric_limits<int>::max()) );
    int buff_size = static_cast<int>(buffSize);
    int ret = UDT::setsockopt(
        udtHandle,0,UDT_RCVBUF,&buff_size,sizeof(buff_size));
    VERIFY_(OK_(ret),"UDT::setsockopt",udtHandle);
    if( ret != 0 )
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

bool UdtSocketImpl::GetRecvBufferSize( unsigned int* buffSize ) const{
    Q_ASSERT(!IsClosed());
    int buff_size;
    int len = sizeof(buff_size);
    int ret = UDT::getsockopt(
        udtHandle,0,UDT_RCVBUF,&buff_size,&len);
    VERIFY_(OK_(ret),"UDT::getsockopt",udtHandle);
    *buffSize = static_cast<unsigned int>(buff_size);
    if( ret != 0 )
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

bool UdtSocketImpl::SetRecvTimeout( unsigned int millis ) {
    Q_ASSERT(!IsClosed());
    Q_ASSERT( millis < static_cast<unsigned int>(std::numeric_limits<int>::max()) );
    int time = static_cast<int>(millis);
    int ret = UDT::setsockopt(
        udtHandle,0,UDT_RCVTIMEO,&time,sizeof(time));
    VERIFY_(OK_(ret),"UDT::setsockopt",udtHandle);
    if( ret != 0 )
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

bool UdtSocketImpl::GetRecvTimeout( unsigned int* millis ) const {
    Q_ASSERT(!IsClosed());
    int time;
    int len = sizeof(time);
    int ret = UDT::getsockopt(
        udtHandle,0,UDT_RCVTIMEO,&time,&len);
    VERIFY_(OK_(ret),"UDT::getsockopt",udtHandle);
    *millis = static_cast<int>(time);
    if( ret != 0 )
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

bool UdtSocketImpl::SetSendTimeout( unsigned int ms ) {
    Q_ASSERT(!IsClosed());
    Q_ASSERT( ms < static_cast<unsigned int>(std::numeric_limits<int>::max()) );
    int time = static_cast<int>(ms);
    int ret = UDT::setsockopt(
        udtHandle,0,UDT_SNDTIMEO,&time,sizeof(time));
    VERIFY_(OK_(ret),"UDT::setsockopt",udtHandle);
    if( ret != 0 )
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

bool UdtSocketImpl::GetSendTimeout( unsigned int* millis ) const {
    Q_ASSERT(!IsClosed());
    int time;
    int len = sizeof(time);
    int ret = UDT::getsockopt(
        udtHandle,0,UDT_SNDTIMEO,&time,&len);
    VERIFY_(OK_(ret),"UDT::getsockopt",udtHandle);
    *millis = static_cast<unsigned int>(time);
    if( ret != 0 )
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
    return ret == 0;
}

bool UdtSocketImpl::GetLastError( SystemError::ErrorCode* errorCode ) const {
    Q_ASSERT(!IsClosed());
    *errorCode = static_cast<SystemError::ErrorCode>(UDT::getlasterror().getErrno());
    return true;
}

AbstractSocket::SOCKET_HANDLE UdtSocketImpl::handle() const {
    Q_ASSERT(!IsClosed());
    Q_ASSERT(sizeof(UDTSOCKET) == sizeof(AbstractSocket::SOCKET_HANDLE));
    return *reinterpret_cast<const AbstractSocket::SOCKET_HANDLE*>(&udtHandle);
}


int UdtSocketImpl::Recv( void* buffer, unsigned int bufferLen, int flags ) {
    int sz = UDT::recv(udtHandle,reinterpret_cast<char*>(buffer),bufferLen,flags);
    if( sz == UDT::ERROR ) {
        // UDT doesn't translate the EOF into a recv with zero return, but instead
        // it returns error with 2001 error code. We need to detect this and translate
        // back with a zero return here .
        int error_code = UDT::getlasterror().getErrorCode();
        if( error_code == CUDTException::ECONNLOST ) {
            return 0;
        } else if( error_code == CUDTException::EINVSOCK ) {
            // This is another very ugly hack since after our patch for UDT.
            // UDT cannot distinguish a clean close or a crash. And I cannot
            // come up with perfect way to patch the code and make it work.
            // So we just hack here. When we encounter an error Invalid socket,
            // it should be a clean close when we use a epoll.
            return 0;
        } else {
            DEBUG_(TRACE_("UDT::recv",udtHandle));
        }
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
    }
    return sz;
}

int UdtSocketImpl::Send( const void* buffer, unsigned int bufferLen ) {
    Q_ASSERT(!IsClosed());
    int sz = UDT::send(udtHandle,reinterpret_cast<const char*>(buffer),bufferLen,0);
    DEBUG_(
        if(sz == UDT::ERROR ) TRACE_("UDT::send",udtHandle));
    if( sz == UDT::ERROR )
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
    return sz;
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
            VERIFY_(fd>=0,"UDT::epoll_create",udt_handler);
    }
    ~UdtEpollHandlerHelper() {
        if(epoll_fd >=0) {
            int ret = UDT::epoll_release(epoll_fd);
#ifndef TRACE_UDT_SOCKET
            Q_UNUSED(ret);
#endif
            VERIFY_(ret ==0,"UDT::epoll_release",udt_handler);
        }
    }
    int epoll_fd;
    UDTSOCKET udt_handler;
};

class UdtAcceptor {
public:
    UdtAcceptor( UdtSocketImpl* imp ) : impl_(imp){}
    UdtSocketImpl* Accept();
    bool Listen( int backlog );
private:
    UdtSocketImpl* impl_;
};

UdtSocketImpl* UdtAcceptor::Accept() {
    Q_ASSERT(impl_->state_ == UdtSocketImpl::ESTABLISHED);
    UDTSOCKET ret = UDT::accept(impl_->udtHandle,NULL,NULL);
    if( ret == UDT::INVALID_SOCK ) {
        DEBUG_(TRACE_("UDT::accept",impl_->udtHandle));
        SystemError::setLastErrorCode(convertToSystemError(UDT::getlasterror().getErrorCode()));
        return NULL;
    } else {
#ifdef TRACE_UDT_SOCKET
        NX_LOG(lit("accepted UDT socket %1").arg(ret), cl_logDEBUG2);
#endif
        return new UdtSocketImpl( ret , UdtSocketImpl::ESTABLISHED );
    }
}


bool UdtAcceptor::Listen( int backlog ) {
    Q_ASSERT(impl_->state_ == UdtSocketImpl::OPEN);
    int ret = UDT::listen(impl_->udtHandle,backlog);
    if( ret != 0 ) {
        DEBUG_(TRACE_("UDT::listen",impl_->udtHandle));
        const auto udtError = UDT::getlasterror();
        SystemError::setLastErrorCode(convertToSystemError(udtError.getErrorCode()));
        return false;
    } else {
        impl_->state_ = UdtSocketImpl::ESTABLISHED;
        return true;
    }
}

}// namespace detail

// =====================================================================
// UdtSocket implementation
// =====================================================================

UdtSocket::UdtSocket(): impl_( new detail::UdtSocketImpl() ) {}
UdtSocket::~UdtSocket(){}
UdtSocket::UdtSocket( detail::UdtSocketImpl* impl ) : impl_(impl)  {}

bool UdtSocket::getLastError(SystemError::ErrorCode* errorCode)
{
    return impl_->GetLastError(errorCode);
}

bool UdtSocket::getRecvTimeout(unsigned int* millis)
{
    return impl_->GetRecvTimeout(millis);
}

bool UdtSocket::getSendTimeout(unsigned int* millis)
{
    return impl_->GetSendTimeout(millis);
}

CommonSocketImpl<UdtSocket>* UdtSocket::impl()
{
    return impl_.get();
}

const CommonSocketImpl<UdtSocket>* UdtSocket::impl() const
{
    return impl_.get();
}

// =====================================================================
// UdtStreamSocket implementation
// =====================================================================
bool UdtStreamSocket::bind( const SocketAddress& localAddress ) {
    return impl_->Bind(localAddress);
}

SocketAddress UdtStreamSocket::getLocalAddress() const {
    return impl_->GetLocalAddress();
}

void UdtStreamSocket::close() {
    impl_->Close();
}

bool UdtStreamSocket::isClosed() const {
    return impl_->IsClosed();
}

bool UdtStreamSocket::setReuseAddrFlag( bool reuseAddr ) {
    return impl_->SetReuseAddrFlag(reuseAddr);
}

bool UdtStreamSocket::getReuseAddrFlag( bool* val ) const {
    return impl_->GetReuseAddrFlag(val);
}

bool UdtStreamSocket::setNonBlockingMode( bool val ) {
    return impl_->SetNonBlockingMode(val);
}

bool UdtStreamSocket::getNonBlockingMode( bool* val ) const {
    return impl_->GetNonBlockingMode(val);
}

bool UdtStreamSocket::getMtu( unsigned int* mtuValue ) const {
    return impl_->GetMtu(mtuValue);
}

bool UdtStreamSocket::setSendBufferSize( unsigned int buffSize ) {
    return impl_->SetSendBufferSize(buffSize);
}

bool UdtStreamSocket::getSendBufferSize( unsigned int* buffSize ) const {
    return impl_->GetSendBufferSize(buffSize);
}

bool UdtStreamSocket::setRecvBufferSize( unsigned int buffSize ) {
    return impl_->SetRecvBufferSize(buffSize);
}

bool UdtStreamSocket::getRecvBufferSize( unsigned int* buffSize ) const {
    return impl_->GetRecvBufferSize( buffSize );
}

bool UdtStreamSocket::setRecvTimeout( unsigned int millis ) {
    return impl_->SetRecvTimeout(millis);
}

bool UdtStreamSocket::getRecvTimeout( unsigned int* millis ) const {
    return impl_->GetRecvTimeout(millis);
}

bool UdtStreamSocket::setSendTimeout( unsigned int ms )  {
    return impl_->SetSendTimeout(ms);
}

bool UdtStreamSocket::getSendTimeout( unsigned int* millis ) const {
    return impl_->GetSendTimeout(millis);
}

bool UdtStreamSocket::getLastError( SystemError::ErrorCode* errorCode ) const {
    return impl_->GetLastError(errorCode);
}

bool UdtStreamSocket::connect(
    const SocketAddress& remoteAddress,
    unsigned int timeoutMillis )
{
    //TODO #ak use timeoutMillis

    Q_ASSERT(impl_->state_ == detail::UdtSocketImpl::OPEN);
    sockaddr_in addr;
    detail::AddressFrom(remoteAddress, &addr);
    // The official documentation doesn't advice using select but here we just need
    // to wait on a single socket fd, select is way more faster than epoll on linux
    // since epoll needs to create the internal structure inside of kernel.
    bool nbk_sock = false;
    if (!impl_->GetNonBlockingMode(&nbk_sock))
        return false;
    if (!nbk_sock)
    {
        if (!impl_->SetNonBlockingMode(nbk_sock))
            return false;
    }
    int ret = UDT::connect(impl_->udtHandle, ADDR_(&addr), sizeof(addr));
    // The UDT connect will always return zero even if such operation is async which is 
    // different with the existed Posix/Win32 socket design. So if we meet an non-zero
    // value, the only explanation is an error happened which cannot be solved.
    if (ret != 0)
    {
        // Error happened
        DEBUG_(TRACE_("UDT::connect", impl_->udtHandle));
        SystemError::setLastErrorCode(detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
        return false;
    }
    impl_->state_ = nbk_sock
        ? detail::UdtSocketImpl::ESTABLISHING
        : detail::UdtSocketImpl::ESTABLISHED;
    return true;
}

int UdtStreamSocket::recv( void* buffer, unsigned int bufferLen, int flags ) {
    return impl_->Recv(buffer,bufferLen,flags);
}

int UdtStreamSocket::send( const void* buffer, unsigned int bufferLen ) {
    return impl_->Send(buffer,bufferLen);
}

SocketAddress UdtStreamSocket::getForeignAddress() const {
    return impl_->GetPeerAddress();
}

bool UdtStreamSocket::isConnected() const {
    return impl_->IsConnected();
}

bool UdtStreamSocket::reopen() {
    return impl_->Reopen();
}

void UdtStreamSocket::cancelIOAsync(
    aio::EventType eventType,
    std::function<void()> cancellationDoneHandler)
{
    return m_aioHelper->cancelIOAsync(eventType, std::move(cancellationDoneHandler));
}

void UdtStreamSocket::cancelIOSync(aio::EventType eventType)
{
    return m_aioHelper->cancelIOSync(eventType);
}

void UdtStreamSocket::pleaseStop( std::function<void()> completionHandler )
{
    m_aioHelper->terminateAsyncIO();
    m_aioHelper->cancelIOAsync( aio::etNone, std::move( completionHandler ) );
}

void UdtStreamSocket::postImpl( std::function<void()>&& handler )
{
    nx::SocketGlobals::aioService().post( static_cast<UdtSocket*>(this), std::move(handler) );
}

void UdtStreamSocket::dispatchImpl( std::function<void()>&& handler )
{
    nx::SocketGlobals::aioService().dispatch( static_cast<UdtSocket*>(this), std::move(handler) );
}

void UdtStreamSocket::connectAsyncImpl( const SocketAddress& addr, std::function<void( SystemError::ErrorCode )>&& handler ) {
    return m_aioHelper->connectAsyncImpl( addr, std::move(handler) );
}

void UdtStreamSocket::recvAsyncImpl( nx::Buffer* const buf, std::function<void( SystemError::ErrorCode, size_t )>&& handler ) {
    return m_aioHelper->recvAsyncImpl(buf, std::move(handler));
}

void UdtStreamSocket::sendAsyncImpl( const nx::Buffer& buf, std::function<void( SystemError::ErrorCode, size_t )>&& handler ) {
    return m_aioHelper->sendAsyncImpl(buf, std::move(handler));
}

void UdtStreamSocket::registerTimerImpl( unsigned int timeoutMillis, std::function<void()>&& handler ) {
    return m_aioHelper->registerTimerImpl(timeoutMillis, std::move(handler));
}

AbstractSocket::SOCKET_HANDLE UdtStreamSocket::handle() const {
    return impl_->handle();
}

UdtStreamSocket::UdtStreamSocket( bool natTraversal )
:
    m_aioHelper(new AsyncSocketImplHelper<UdtSocket>(this, this, natTraversal))
{
    impl_->Open();
}

UdtStreamSocket::UdtStreamSocket( detail::UdtSocketImpl* impl )
:
    UdtSocket(impl),
    m_aioHelper(new AsyncSocketImplHelper<UdtSocket>(this, this, false))
{
}

UdtStreamSocket::~UdtStreamSocket()
{
    //m_aioHelper->terminate();
}

// =====================================================================
// UdtStreamSocket implementation
// =====================================================================
bool UdtStreamServerSocket::listen( int backlog )  {
    return detail::UdtAcceptor(impl_.get()).Listen(backlog);
}

AbstractStreamSocket* UdtStreamServerSocket::accept()  {
    detail::UdtSocketImpl* impl = detail::UdtAcceptor(impl_.get()).Accept();
    if( impl == NULL )
        return NULL;
    else
        return new UdtStreamSocket(impl);
}

void UdtStreamServerSocket::cancelAsyncIO(bool waitForRunningHandlerCompletion)
{
    m_aioHelper->cancelAsyncIO(waitForRunningHandlerCompletion);
}

void UdtStreamServerSocket::pleaseStop( std::function<void()> completionHandler )
{
    impl()->terminated.store( true, std::memory_order_relaxed );

    // TODO: #mux FIX THIS! the completionHandler should be passed and called!
    m_aioHelper->cancelAsyncIO( false );
    completionHandler();
}

bool UdtStreamServerSocket::bind( const SocketAddress& localAddress ) {
    return impl_->Bind(localAddress);
}

SocketAddress UdtStreamServerSocket::getLocalAddress() const {
    return impl_->GetLocalAddress();
}

void UdtStreamServerSocket::close() {
    impl_->Close();
}

bool UdtStreamServerSocket::isClosed() const {
    return impl_->IsClosed();
}

bool UdtStreamServerSocket::setReuseAddrFlag( bool reuseAddr ) {
    return impl_->SetReuseAddrFlag(reuseAddr);
}

bool UdtStreamServerSocket::getReuseAddrFlag( bool* val ) const {
    return impl_->GetReuseAddrFlag(val);
}

bool UdtStreamServerSocket::setNonBlockingMode( bool val ) {
    return impl_->SetNonBlockingMode(val);
}

bool UdtStreamServerSocket::getNonBlockingMode( bool* val ) const {
    return impl_->GetNonBlockingMode(val);
}

bool UdtStreamServerSocket::getMtu( unsigned int* mtuValue ) const {
    return impl_->GetMtu(mtuValue);
}

bool UdtStreamServerSocket::setSendBufferSize( unsigned int buffSize ) {
    return impl_->SetSendBufferSize(buffSize);
}

bool UdtStreamServerSocket::getSendBufferSize( unsigned int* buffSize ) const {
    return impl_->GetSendBufferSize(buffSize);
}

bool UdtStreamServerSocket::setRecvBufferSize( unsigned int buffSize ) {
    return impl_->SetRecvBufferSize(buffSize);
}

bool UdtStreamServerSocket::getRecvBufferSize( unsigned int* buffSize ) const {
    return impl_->GetRecvBufferSize(buffSize);
}

bool UdtStreamServerSocket::setRecvTimeout( unsigned int millis ) {
    return impl_->SetRecvTimeout(millis);
}

bool UdtStreamServerSocket::getRecvTimeout( unsigned int* millis ) const {
    return impl_->GetRecvTimeout(millis);
}

bool UdtStreamServerSocket::setSendTimeout( unsigned int ms ) {
    return impl_->SetSendTimeout(ms);
}

bool UdtStreamServerSocket::getSendTimeout( unsigned int* millis ) const {
    return impl_->GetSendTimeout(millis);
}

bool UdtStreamServerSocket::getLastError( SystemError::ErrorCode* errorCode ) const {
    return impl_->GetLastError(errorCode);
}

void UdtStreamServerSocket::postImpl( std::function<void()>&& handler )
{
    nx::SocketGlobals::aioService().post( static_cast<UdtSocket*>(this), std::move(handler) );
}

void UdtStreamServerSocket::dispatchImpl( std::function<void()>&& handler )
{
    nx::SocketGlobals::aioService().dispatch( static_cast<UdtSocket*>(this), std::move(handler) );
}

void UdtStreamServerSocket::acceptAsyncImpl( std::function<void( SystemError::ErrorCode, AbstractStreamSocket* )>&& handler ) {
    return m_aioHelper->acceptAsync( std::move(handler) );
}

AbstractSocket::SOCKET_HANDLE UdtStreamServerSocket::handle() const {
    return impl_->handle();
}

UdtStreamServerSocket::UdtStreamServerSocket()
:
    m_aioHelper(new AsyncServerSocketHelper<UdtSocket>( this, this ) )
{
    impl_->Open();
}

UdtStreamServerSocket::~UdtStreamServerSocket()
{
    m_aioHelper->terminate();
}
