#include "udt_socket.h"
#include <utils./network/system_socket.h>

#include <set>
#include <udt/udt.h>
#include <utils/common/log.h>
#include <utils/common/checked_cast.h>
#include <boost/optional.hpp>
#include <mutex>

#ifdef Q_OS_WIN
#  include <ws2tcpip.h>
#  include <iphlpapi.h>
#  include "win32_socket_tools.h"
#else
#  include <netinet/tcp.h>
#endif

#include "aio/async_socket_helper.h"

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
class UdtConnector;

UdtSocketImplPtr::UdtSocketImplPtr( UdtSocketImpl* imp ) : 
    std::unique_ptr<UdtSocketImpl>(imp){}

UdtSocketImplPtr::~UdtSocketImplPtr(){}

// Implementator to keep the layout of class clean and achieve binary compatible
class UdtSocketImpl {
public:
    enum {
        CLOSED,
        OPEN,
        ESTABLISHING,
        ESTABLISHED
    };
    UdtSocketImpl(): handler_( UDT::INVALID_SOCK ) , state_(CLOSED) {
        UdtLibrary::Initialize();
    }
    UdtSocketImpl( UDTSOCKET socket, int state ) : handler_(socket),state_(state){}
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

    UDTSOCKET udt_handler() const {
        return handler_;
    }
    int state() const {
        return state_;
    }
private:
    UDTSOCKET handler_;
    int state_;
    // Modifier/Observer
    friend class UdtAcceptor;
    friend class UdtConnector;
};

bool UdtSocketImpl::Open() {
    Q_ASSERT(IsClosed());
    handler_ = UDT::socket(AF_INET,SOCK_STREAM,0);
    VERIFY_(handler_ != UDT::INVALID_SOCK,"UDT::socket",0);
    if( handler_ != UDT::INVALID_SOCK ) {
        state_ = OPEN;
        return true;
    } else {
        SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
        return false;
    }
}

bool UdtSocketImpl::Bind( const SocketAddress& localAddress ) {
    sockaddr_in addr;
    AddressFrom(localAddress,&addr);
    int ret = UDT::bind(handler_,ADDR_(&addr),sizeof(addr));
    DEBUG_(if(ret !=0) TRACE_("UDT::bind",handler_));
    if( ret != 0 )
        SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
    return ret == 0;
}

SocketAddress UdtSocketImpl::GetLocalAddress() const {
    sockaddr_in addr;
    int len = sizeof(addr);
    if(UDT::getsockname(handler_,ADDR_(&addr),&len) != 0) {
        DEBUG_(TRACE_("UDT::getsockname",handler_));
        SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
        return SocketAddress();
    } else {
        return SocketAddress(
            HostAddress(addr.sin_addr),ntohs(addr.sin_port));
    }
}


SocketAddress UdtSocketImpl::GetPeerAddress() const {
    sockaddr_in addr;
    int len = sizeof(addr);
    if(UDT::getpeername(handler_,ADDR_(&addr),&len) !=0) {
        DEBUG_(TRACE_("UDT::getsockname",handler_));
        SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
        return SocketAddress();
    } else {
        return SocketAddress(
            HostAddress(addr.sin_addr),ntohs(addr.sin_port));
    }
}

bool UdtSocketImpl::Close() {
    //Q_ASSERT(!IsClosed());
    int ret = UDT::close(handler_);
    const char*message = UDT::getlasterror().getErrorMessage();
    //VERIFY_(OK_(ret),"UDT::Close",handler_);
    handler_ = UDT::INVALID_SOCK;
    state_ = CLOSED;
    return ret == 0;
}

bool UdtSocketImpl::IsClosed() const {
    return state_ == CLOSED;
}

bool UdtSocketImpl::SetReuseAddrFlag( bool reuseAddr ) {
    Q_ASSERT(!IsClosed());
    int ret = UDT::setsockopt(handler_,0,UDT_REUSEADDR,&reuseAddr,sizeof(reuseAddr));
    VERIFY_(OK_(ret),"UDT::setsockopt",handler_);
    if( ret != 0 )
        SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
    return ret == 0;
}

bool UdtSocketImpl::GetReuseAddrFlag( bool* val ) const {
    Q_ASSERT(!IsClosed());
    int len = sizeof(*val);
    int ret = UDT::getsockopt(handler_,0,UDT_REUSEADDR,val,&len);
    VERIFY_(OK_(ret),"UDT::getsockopt",handler_);
    if( ret != 0 )
        SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
    return ret == 0;
}

bool UdtSocketImpl::SetNonBlockingMode( bool val ) {
    Q_ASSERT(!IsClosed());
    bool value = !val;
    int ret = UDT::setsockopt(handler_,0,UDT_SNDSYN,&value,sizeof(value));
    VERIFY_(OK_(ret),"UDT::setsockopt",handler_);
    if( ret != 0 )
    {
        SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
        return false;
    }
    ret = UDT::setsockopt(handler_,0,UDT_RCVSYN,&value,sizeof(value));
    VERIFY_(OK_(ret),"UDT::setsockopt",handler_);
    if( ret != 0 )
        SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
    return ret == 0;
}

bool UdtSocketImpl::GetNonBlockingMode( bool* val ) const {
    Q_ASSERT(!IsClosed());
    int len = sizeof(*val);
    int ret = UDT::getsockopt(handler_,0,UDT_SNDSYN,val,&len);
    VERIFY_(OK_(ret),"UDT::getsockopt",handler_);
    DEBUG_(
        bool bval;
        ret = UDT::getsockopt(handler_,0,UDT_RCVSYN,&bval,&len); 
        VERIFY_(OK_(ret) && *val == bval,"UDT::getsockopt",handler_));

    if( ret != 0 )
        SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
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
        handler_,0,UDT_SNDBUF,&buff_size,sizeof(buff_size));
    VERIFY_(OK_(ret),"UDT::getsockopt",handler_);
    if( ret != 0 )
        SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
    return ret == 0;
}

bool UdtSocketImpl::GetSendBufferSize( unsigned int* buffSize ) const{
    Q_ASSERT(!IsClosed());
    int buff_size;
    int len = sizeof(buff_size);
    int ret = UDT::getsockopt(
        handler_,0,UDT_SNDBUF,&buff_size,&len);
    VERIFY_(OK_(ret),"UDT::getsockopt",handler_);
    *buffSize = static_cast<unsigned int>(buff_size);
    if( ret != 0 )
        SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
    return ret == 0;
}

bool UdtSocketImpl::SetRecvBufferSize( unsigned int buffSize ){
    Q_ASSERT(!IsClosed());
    Q_ASSERT( buffSize < static_cast<unsigned int>(std::numeric_limits<int>::max()) );
    int buff_size = static_cast<int>(buffSize);
    int ret = UDT::setsockopt(
        handler_,0,UDT_RCVBUF,&buff_size,sizeof(buff_size));
    VERIFY_(OK_(ret),"UDT::setsockopt",handler_);
    if( ret != 0 )
        SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
    return ret == 0;
}

bool UdtSocketImpl::GetRecvBufferSize( unsigned int* buffSize ) const{
    Q_ASSERT(!IsClosed());
    int buff_size;
    int len = sizeof(buff_size);
    int ret = UDT::getsockopt(
        handler_,0,UDT_RCVBUF,&buff_size,&len);
    VERIFY_(OK_(ret),"UDT::getsockopt",handler_);
    *buffSize = static_cast<unsigned int>(buff_size);
    if( ret != 0 )
        SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
    return ret == 0;
}

bool UdtSocketImpl::SetRecvTimeout( unsigned int millis ) {
    Q_ASSERT(!IsClosed());
    Q_ASSERT( millis < static_cast<unsigned int>(std::numeric_limits<int>::max()) );
    int time = static_cast<int>(millis);
    int ret = UDT::setsockopt(
        handler_,0,UDT_RCVTIMEO,&time,sizeof(time));
    VERIFY_(OK_(ret),"UDT::setsockopt",handler_);
    if( ret != 0 )
        SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
    return ret == 0;
}

bool UdtSocketImpl::GetRecvTimeout( unsigned int* millis ) const {
    Q_ASSERT(!IsClosed());
    int time;
    int len = sizeof(time);
    int ret = UDT::getsockopt(
        handler_,0,UDT_RCVTIMEO,&time,&len);
    VERIFY_(OK_(ret),"UDT::getsockopt",handler_);
    *millis = static_cast<int>(time);
    if( ret != 0 )
        SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
    return ret == 0;
}

bool UdtSocketImpl::SetSendTimeout( unsigned int ms ) {
    Q_ASSERT(!IsClosed());
    Q_ASSERT( ms < static_cast<unsigned int>(std::numeric_limits<int>::max()) );
    int time = static_cast<int>(ms);
    int ret = UDT::setsockopt(
        handler_,0,UDT_SNDTIMEO,&time,sizeof(time));
    VERIFY_(OK_(ret),"UDT::setsockopt",handler_);
    if( ret != 0 )
        SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
    return ret == 0;
}

bool UdtSocketImpl::GetSendTimeout( unsigned int* millis ) const {
    Q_ASSERT(!IsClosed());
    int time;
    int len = sizeof(time);
    int ret = UDT::getsockopt(
        handler_,0,UDT_SNDTIMEO,&time,&len);
    VERIFY_(OK_(ret),"UDT::getsockopt",handler_);
    *millis = static_cast<unsigned int>(time);
    if( ret != 0 )
        SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
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
    return *reinterpret_cast<const AbstractSocket::SOCKET_HANDLE*>(&handler_);
}


int UdtSocketImpl::Recv( void* buffer, unsigned int bufferLen, int flags ) {
    int sz = UDT::recv(handler_,reinterpret_cast<char*>(buffer),bufferLen,flags);
    if( sz == UDT::ERROR ) {
        // UDT doesn't translate the EOF into a recv with zero return, but instead
        // it returns error with 2001 error code. We need to detect this and translate
        // back with a zero return here .
        int error_code = UDT::getlasterror().getErrorCode();
        if( error_code == CUDTException::ECONNLOST ) {
            return 0;
        } else {
            DEBUG_(TRACE_("UDT::recv",handler_));
        }
        SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
    }
    return sz;
}

int UdtSocketImpl::Send( const void* buffer, unsigned int bufferLen ) {
    Q_ASSERT(!IsClosed());
    int sz = UDT::send(handler_,reinterpret_cast<const char*>(buffer),bufferLen,0);
    DEBUG_(
        if(sz == UDT::ERROR ) TRACE_("UDT::send",handler_));
    if( sz == UDT::ERROR )
        SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
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


// Connector / Acceptor 
// We use a connector and acceptor to decide what kind of socket we want.

class UdtConnector {
public:
    UdtConnector( UdtSocketImpl* impl ) : impl_(impl) {}
    bool Connect( const QString& address , unsigned short port , int timeouts );
private:
    UdtSocketImpl* impl_;
};

struct UdtEpollHandlerHelper {
    UdtEpollHandlerHelper(int fd,UDTSOCKET udt_handler):
        epoll_fd(fd){
            VERIFY_(fd>=0,"UDT::epoll_create",udt_handler);
    }
    ~UdtEpollHandlerHelper() {
        if(epoll_fd >=0) {
            int ret = UDT::epoll_release(epoll_fd);
            VERIFY_(ret ==0,"UDT::epoll_release",udt_handler);
        }
    }
    int epoll_fd;
    UDTSOCKET udt_handler;
};

bool UdtConnector::Connect( const QString& address , unsigned short port , int timeouts ) {
    Q_ASSERT(impl_->state() == UdtSocketImpl::OPEN);
    sockaddr_in addr;
    AddressFrom(
        SocketAddress(HostAddress(address),port),&addr);
    // The official documentation doesn't advice using select but here we just need
    // to wait on a single socket fd, select is way more faster than epoll on linux
    // since epoll needs to create the internal structure inside of kernel.
    bool nbk_sock;
    if(!impl_->GetNonBlockingMode(&nbk_sock))
        return false;
    if( !nbk_sock ) {
        if(!impl_->SetNonBlockingMode(nbk_sock))
            return false;
    }
    int ret = UDT::connect(impl_->udt_handler(),ADDR_(&addr),sizeof(addr));
    // The UDT connect will always return zero even if such operation is async which is 
    // different with the existed Posix/Win32 socket design. So if we meet an non-zero
    // value, the only explanation is an error happened which cannot be solved.
    if( ret != 0 ) {
        // Error happened
        DEBUG_(TRACE_("UDT::connect",impl_->udt_handler()));
        SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
        return false;
    } else {
        if( nbk_sock ) {
            impl_->state_ = UdtSocketImpl::ESTABLISHING;
            return true;
        }
    }
    // Using epoll will ensure that it is correct with any value for the file descriptor. 
    UdtEpollHandlerHelper epoll_fd(UDT::epoll_create(),impl_->udt_handler());
    int write_ev = UDT_EPOLL_OUT;
    ret = UDT::epoll_add_usock(epoll_fd.epoll_fd,impl_->udt_handler(),&write_ev);
    VERIFY_(ret ==0,"UDT::epoll_add_ssock",impl_->udt_handler());

    std::set<UDTSOCKET> write_fd;
    // Waiting on epoll set
    ret = UDT::epoll_wait(epoll_fd.epoll_fd,NULL,&write_fd,timeouts,NULL,NULL);
    
    if( ret < 0 ) {
        DEBUG_(TRACE_("UDT::epoll_wait",impl_->udt_handler()));
        SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
        return false;
    } else {
        if( ret == 0 )  {
            SystemError::setLastErrorCode( SystemError::timedOut );
            return false; // Timeout
        } else {
            Q_ASSERT(ret == 1);
            impl_->state_ = UdtSocketImpl::ESTABLISHED;
            return true;
        }
    }
}

class UdtAcceptor {
public:
    UdtAcceptor( UdtSocketImpl* imp ) : impl_(imp){}
    UdtSocketImpl* Accept();
    bool Listen( int backlog );
private:
    UdtSocketImpl* impl_;
};

UdtSocketImpl* UdtAcceptor::Accept() {
    Q_ASSERT(impl_->state() == UdtSocketImpl::ESTABLISHED);
    UDTSOCKET ret = UDT::accept(impl_->udt_handler(),NULL,NULL);
    if( ret == UDT::INVALID_SOCK ) {
        DEBUG_(TRACE_("UDT::accept",impl_->udt_handler()));
        SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
        return NULL;
    } else {
        return new UdtSocketImpl( ret , UdtSocketImpl::ESTABLISHED );
    }
}


bool UdtAcceptor::Listen( int backlog ) {
    Q_ASSERT(impl_->state() == UdtSocketImpl::OPEN);
    int ret = UDT::listen(impl_->udt_handler(),backlog);
    if( ret != 0 ) {
        DEBUG_(TRACE_("UDT::listen",impl_->udt_handler()));
        SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
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


// =====================================================================
// UdtStreamSocket implementation
// =====================================================================
bool UdtStreamSocket::bind( const SocketAddress& localAddress ) {
    return impl_->Bind(localAddress);
}

SocketAddress UdtStreamSocket::getLocalAddress() const {
    return impl_->GetLocalAddress();
}

SocketAddress UdtStreamSocket::getPeerAddress() const {
    return impl_->GetPeerAddress();
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
    const QString& foreignAddress,
    unsigned short foreignPort,
    unsigned int timeoutMillis ) {
        return detail::UdtConnector(impl_.get()).
            Connect(foreignAddress,foreignPort,timeoutMillis);
}

int UdtStreamSocket::recv( void* buffer, unsigned int bufferLen, int flags ) {
    return impl_->Recv(buffer,bufferLen,flags);
}

int UdtStreamSocket::send( const void* buffer, unsigned int bufferLen ) {
    return impl_->Send(buffer,bufferLen);
}

bool UdtStreamSocket::isConnected() const {
    return impl_->IsConnected();
}

bool UdtStreamSocket::reopen() {
    return impl_->Reopen();
}

void UdtStreamSocket::cancelAsyncIO( aio::EventType eventType, bool waitForRunningHandlerCompletion )  {
    return m_aioHelper->cancelAsyncIO(eventType, waitForRunningHandlerCompletion);
}

bool UdtStreamSocket::connectAsyncImpl( const SocketAddress& addr, std::function<void( SystemError::ErrorCode )>&& handler ) {
    //return detail::UdtConnector(impl_.get()).AsyncConnect(addr,std::move(handler));
    return m_aioHelper->connectAsyncImpl( addr, std::move(handler) );
}

bool UdtStreamSocket::recvAsyncImpl( nx::Buffer* const buf, std::function<void( SystemError::ErrorCode, size_t )>&& handler ) {
    return m_aioHelper->recvAsyncImpl(buf, std::move(handler));
}

bool UdtStreamSocket::sendAsyncImpl( const nx::Buffer& buf, std::function<void( SystemError::ErrorCode, size_t )>&& handler ) {
    return m_aioHelper->sendAsyncImpl(buf, std::move(handler));
}

AbstractSocket::SOCKET_HANDLE UdtStreamSocket::handle() const {
    return impl_->handle();
}

UdtStreamSocket::UdtStreamSocket()
:
    m_aioHelper(new AsyncSocketImplHelper<UdtSocket>(this, this))
{
    impl_->Open();
}

UdtStreamSocket::UdtStreamSocket( detail::UdtSocketImpl* impl )
:
    UdtSocket(impl),
    m_aioHelper(new AsyncSocketImplHelper<UdtSocket>(this, this))
{
}

UdtStreamSocket::~UdtStreamSocket()
{
    m_aioHelper->terminate();
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

bool UdtStreamServerSocket::bind( const SocketAddress& localAddress ) {
    return impl_->Bind(localAddress);
}

SocketAddress UdtStreamServerSocket::getLocalAddress() const {
    return impl_->GetLocalAddress();
}

SocketAddress UdtStreamServerSocket::getPeerAddress() const {
    return impl_->GetPeerAddress();
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

bool UdtStreamServerSocket::acceptAsyncImpl( std::function<void( SystemError::ErrorCode, AbstractStreamSocket* )>&& handler ) {
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

UdtStreamServerSocket::~UdtStreamServerSocket(){}


// ========================================================
// PollSet implementation
// ========================================================
namespace detail{
namespace {
struct SocketUserData {
    boost::optional<void*> read_data;
    boost::optional<void*> write_data;
    UdtSocket* socket;
    bool deleted;
    SocketUserData():socket(NULL),deleted(false){}
};
}// namespace 

UdtPollSetImplPtr::UdtPollSetImplPtr( UdtPollSetImpl* imp ) :
    std::unique_ptr<UdtPollSetImpl>(imp){}

UdtPollSetImplPtr::~UdtPollSetImplPtr(){}

UdtPollSetConstIteratorImplPtr::UdtPollSetConstIteratorImplPtr( UdtPollSetConstIteratorImpl* imp ) :
    std::unique_ptr<UdtPollSetConstIteratorImpl>(imp){}

UdtPollSetConstIteratorImplPtr::UdtPollSetConstIteratorImplPtr(){}

UdtPollSetConstIteratorImplPtr::~UdtPollSetConstIteratorImplPtr(){}

class UdtPollSetImpl {
public:
    UdtPollSetImpl():epoll_fd_(-1),size_(0){}
    int Poll( int milliseconds );
    bool Add( UdtSocket* socket , UdtSocketImpl* imp , aio::EventType event , void* data );
    void* GetUserData( UdtSocketImpl* imp , aio::EventType eventType ) const ;
    void* Remove( UdtSocketImpl* imp , aio::EventType eventType );
    bool Initialize();
    static UdtPollSetImpl* Create();
    bool Interrupt();
    std::size_t size() const {
        return size_;
    }
private:
    bool InitializeInterruptSocket();
    void RemoveFromEpoll( UDTSOCKET udt_handler , int event_type );
    static const int kInterruptionBufferLength = 128;
    int MapAioEventToUdtEvent( aio::EventType et ) {
        switch(et) {
        case aio::etRead: return UDT_EPOLL_IN;
        case aio::etWrite:return UDT_EPOLL_OUT;
        default: Q_ASSERT(0); return 0;
        }
    }
    void ReclaimSocket();
    int RemovePhantomSockets( std::set<UDTSOCKET>* sockets_set );
    bool IsPhantomSocket( UDTSOCKET fd ) {
        std::map<UDTSOCKET,SocketUserData>::iterator it = 
            socket_user_data_.find(fd);
        if( it == socket_user_data_.end() ) {
            return true;
        } else {
            return it->second.deleted;
        }
    }
private:
    std::set<UDTSOCKET> udt_read_set_;
    std::set<UDTSOCKET> udt_write_set_;
    std::size_t size_;
    std::map<UDTSOCKET,SocketUserData> socket_user_data_;
    int epoll_fd_;
    UDPSocket interrupt_socket_;
    std::set<SYSSOCKET> udt_sys_socket_read_set_;
    typedef std::map<UDTSOCKET,SocketUserData>::iterator socket_iterator;
    std::list<socket_iterator> reclaim_list_;
    friend class UdtPollSetConstIteratorImpl;
};

// I have observed that in multi-thread environment, the epoll_wait can give
// notification for those sockets that has been closed/removed from epoll_set.
// This workaround ensure that only the socket has been added to the epoll set
// will get the notification. But this assume that the user needs to _remove_
// all the waiting fd from the pollset before calling the close operation .

int UdtPollSetImpl::RemovePhantomSockets( std::set<UDTSOCKET>* sockets_set ) {
    int count = 0;
    for( std::set<UDTSOCKET>::iterator ib = sockets_set->begin() ; ib != sockets_set->end() ; ) {
        if( IsPhantomSocket(*ib) ) {
            ib = sockets_set->erase(ib);
            ++count;
        } else {
            ++ib;
        }
    }
    return count;
}

void UdtPollSetImpl::RemoveFromEpoll( UDTSOCKET udt_handler , int event_type ) {
    // UDT will remove all the related event type related to a certain UDT handler, so if a udt
    // handler is watching read/write, then after a remove, all the event type will be removed.
    int ret = UDT::epoll_remove_usock( epoll_fd_ , udt_handler );
    VERIFY_(ret ==0,"UDT::epoll_remove_usock",udt_handler);
    if( event_type != UDT_EPOLL_ERR ) {
        Q_ASSERT( event_type == UDT_EPOLL_IN || event_type == UDT_EPOLL_OUT );
        ret = UDT::epoll_add_usock(epoll_fd_,udt_handler,&event_type);
        VERIFY_(ret ==0,"UDT::epoll_remove_usock",udt_handler);
         if( ret != 0 )
            SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
    }
}

void* UdtPollSetImpl::Remove( UdtSocketImpl* imp , aio::EventType eventType ) {
    Q_ASSERT(imp != NULL);
    switch(eventType) {
    case aio::etRead:
        {
            std::map<UDTSOCKET,SocketUserData>::iterator
                ib = socket_user_data_.find(imp->udt_handler());
            if( ib == socket_user_data_.end() || ib->second.read_data == boost::none )
                return NULL;
            void* rudata = ib->second.read_data.get();
            int remain_event_type = UDT_EPOLL_ERR;
            ib->second.read_data = boost::none;
            if( ib->second.write_data == boost::none ) {
                ib->second.deleted = true;
                reclaim_list_.push_back(ib);
            } else {
                // We have write operation for this objects
                remain_event_type = UDT_EPOLL_OUT;
            }
            RemoveFromEpoll(imp->udt_handler(),remain_event_type);
            --size_;
            return rudata;
        }
    case aio::etWrite:
        {
            std::map<UDTSOCKET,SocketUserData>::iterator
                ib = socket_user_data_.find(imp->udt_handler());
            if( ib == socket_user_data_.end() || ib->second.write_data == boost::none )
                return false;
            void* rudata = ib->second.write_data.get();
            int remain_event_type = UDT_EPOLL_ERR;
            ib->second.write_data = boost::none;
            if( ib->second.read_data == boost::none ) {
                ib->second.deleted = true;
                reclaim_list_.push_back(ib);
            } else {
                remain_event_type = UDT_EPOLL_IN;
            }
            RemoveFromEpoll(imp->udt_handler(),remain_event_type);
            --size_;
            return rudata;
        }
    default: Q_ASSERT(0); return NULL;
    }
}

void* UdtPollSetImpl::GetUserData( UdtSocketImpl* imp , aio::EventType eventType ) const {
    Q_ASSERT(imp != NULL);
    std::map<UDTSOCKET,SocketUserData>::const_iterator
        ib = socket_user_data_.find(imp->udt_handler());
    if(ib == socket_user_data_.end())
        return NULL;
    else {
        switch(eventType) {
        case aio::etRead:
            return ib->second.read_data == boost::none ? NULL : ib->second.read_data.get();
        case aio::etWrite:
            return ib->second.write_data == boost::none ? NULL : ib->second.write_data.get();
        default: return NULL;
        }
    }
}

bool UdtPollSetImpl::Add( UdtSocket* socket , UdtSocketImpl* imp , aio::EventType event , void* data ) {
    Q_ASSERT(socket != NULL);
    int ev = MapAioEventToUdtEvent(event);
    int ret = UDT::epoll_add_usock(epoll_fd_,imp->udt_handler(),&ev);
    DEBUG_(if(ret <0) TRACE_("UDT::epoll_add_usock",imp->udt_handler()));
    if( ret <0 )
    {
        SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
        return false;
    }
    SocketUserData& ref = socket_user_data_[imp->udt_handler()];
    if( ev == UDT_EPOLL_IN )
        ref.read_data = data;
    else
        ref.write_data = data;
    ref.socket = socket;
    ref.deleted = false;
    ++size_;
    return true;
}

void UdtPollSetImpl::ReclaimSocket() {
    for( std::list<socket_iterator>::iterator ib = reclaim_list_.begin() ; ib != reclaim_list_.end() ; ++ib ) {
        if((*ib)->second.deleted)
            socket_user_data_.erase(*ib);
    }
    reclaim_list_.clear();
}

int UdtPollSetImpl::Poll( int milliseconds ) {
    ReclaimSocket();
    udt_read_set_.clear();
    udt_write_set_.clear();
    udt_sys_socket_read_set_.clear();
    if( milliseconds == aio::INFINITE_TIMEOUT )
        milliseconds = -1;
    int ret = UDT::epoll_wait(
        epoll_fd_,&udt_read_set_,&udt_write_set_,milliseconds,&udt_sys_socket_read_set_,NULL);
    if( ret <0 ) {
        // UDT documentation is wrong, after "carefully" inspect its source code, the epoll_wait
        // for UDT will _NEVER_ return zero when time out, it will 1) return a positive number 
        // 2) -1 as time out or other error. The documentation says it will return zero . :(
        // And the ETIMEOUT's related error string is not even _IMPLEMENTED_ in its getErrorMessage
        // function !
        if( UDT::getlasterror_code() == CUDTException::ETIMEOUT ) {
            return 0; // Translate this error code
        } 
        TRACE_("UDT::epoll_wait",UDT::INVALID_SOCK);
        SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
        return -1;
    } else {
        // Remove those phantom sockets
        ret -= RemovePhantomSockets(&udt_read_set_);
        ret -= RemovePhantomSockets(&udt_write_set_);
        Q_ASSERT( ret >= 0 );
        // Remove control sockets
        if( !udt_sys_socket_read_set_.empty() ) {
            char buffer[kInterruptionBufferLength];
            interrupt_socket_.recv(buffer,kInterruptionBufferLength,0);
            --ret;
        }
        return ret;
    }
}

bool UdtPollSetImpl::Initialize() {
    Q_ASSERT(epoll_fd_ <0);
    epoll_fd_ = UDT::epoll_create();
    if( epoll_fd_ <0 ) {
        DEBUG_(TRACE_("UDT::epoll_create",UDT::INVALID_SOCK));
        SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
        return false;
    }
    return InitializeInterruptSocket();
}

bool UdtPollSetImpl::Interrupt() {
    char buffer[kInterruptionBufferLength];
    bool ret = interrupt_socket_.sendTo(buffer,kInterruptionBufferLength,interrupt_socket_.getLocalAddress());
    return ret;
}

bool UdtPollSetImpl::InitializeInterruptSocket() {
    interrupt_socket_.setNonBlockingMode(true);
    interrupt_socket_.bind( SocketAddress(HostAddress::localhost,0) );
    // adding this iterrupt_socket_ to the epoll set
    int ret = UDT::epoll_add_ssock(epoll_fd_,interrupt_socket_.handle());
    if( ret <0 ) {
        DEBUG_(TRACE_("UDT::epoll_add_ssock",UDT::INVALID_SOCK));
        SystemError::setLastErrorCode(UDT::getlasterror().getErrno());
        return false;
    } 
    return true;
}

UdtPollSetImpl* UdtPollSetImpl::Create() {
    std::unique_ptr<UdtPollSetImpl> ret(new UdtPollSetImpl());
    if( ret->Initialize() )
        return ret.release();
    else
        return NULL;
}

class UdtPollSetConstIteratorImpl {
public:

    UdtPollSetConstIteratorImpl( UdtPollSetImpl* impl , bool end ) : 
        impl_(impl),
        in_read_set_(!end) {
            if(end) {
               iterator_ = impl->udt_write_set_.end();
            } else {
                iterator_ = impl->udt_read_set_.begin();
                if( iterator_ == impl_->udt_read_set_.end() ) {
                    iterator_ = impl_->udt_write_set_.begin();
                    in_read_set_ = false;
                }
            }
    }

    UdtPollSetConstIteratorImpl( const UdtPollSetConstIteratorImpl& impl ) :
        impl_(impl.impl_),
        in_read_set_(impl.in_read_set_),
        iterator_(impl.iterator_){}

    UdtPollSetConstIteratorImpl& operator = ( const UdtPollSetConstIteratorImpl& impl ) {
        if( this == &impl ) return *this;
        impl_ = impl.impl_;
        in_read_set_ = impl.in_read_set_;
        iterator_ = impl.iterator_;
        return *this;
    }

    void Next() {
        Q_ASSERT(!Done());
        if( in_read_set_ ) {
            ++iterator_;
            if( iterator_ == impl_->udt_read_set_.end() ) {
                in_read_set_ = false;
                iterator_ = impl_->udt_write_set_.begin();
            }
        } else {
            ++iterator_;
        }
    }

    bool Done() const {
        if( in_read_set_ )
            return false;
        else {
            return iterator_ == impl_->udt_write_set_.end();
        }
    }

    UdtSocket* GetSocket() const {
        return GetSocketUserData()->socket;
    }

    void* GetUserData() const {
        const SocketUserData* ref = GetSocketUserData();
        if( in_read_set_ ) {
            return ref->read_data == boost::none ? NULL : ref->read_data.get();
        } else {
            return ref->write_data == boost::none ? NULL : ref->write_data.get();
        }
    }

    aio::EventType GetEventType() const {
        Q_ASSERT(!Done());
        return in_read_set_ ? aio::etRead : aio::etWrite;
    }

    bool Equal( const UdtPollSetConstIteratorImpl& impl ) const {
        // iterator from different set/map cannot compare 
        bool result = in_read_set_ ^ impl.in_read_set_;
        if( result ) {
            return false;
        } else {
            return iterator_ == impl.iterator_;
        }
    }
private:

    const SocketUserData* GetSocketUserData() const {
        Q_ASSERT(!Done());
        std::map<UDTSOCKET,SocketUserData>::
            iterator ib = impl_->socket_user_data_.find(*iterator_);
        Q_ASSERT(ib != impl_->socket_user_data_.end());
        return &(ib->second);
    }

private:
    UdtPollSetImpl* impl_;
    bool in_read_set_;
    std::set<UDTSOCKET>::iterator iterator_;
};

}// namespace detail


// =========================================
// Const Iterator
// =========================================

UdtPollSet::const_iterator::const_iterator(){}

UdtPollSet::const_iterator::const_iterator( const const_iterator& it ) {
    if(it.impl_) 
        impl_.reset( new detail::UdtPollSetConstIteratorImpl(*it.impl_));
}

UdtPollSet::const_iterator::const_iterator( detail::UdtPollSetImpl* impl , bool end ):
    impl_( new detail::UdtPollSetConstIteratorImpl(impl,end) ) {}

UdtPollSet::const_iterator& UdtPollSet::const_iterator::operator =( const UdtPollSet::const_iterator& ib ) {
    if( this == &ib ) return *this;
    if( !impl_ ) {
        if( ib.impl_ )
            impl_.reset( new detail::UdtPollSetConstIteratorImpl(*ib.impl_));
        else
            impl_.reset();
    } else {
        if( ib.impl_ ) {
            *impl_ = *ib.impl_;
        } else {
            impl_.reset();
        }
    }
    return *this;
}

UdtPollSet::const_iterator& UdtPollSet::const_iterator::operator ++() {
    Q_ASSERT(impl_);
    impl_->Next();
    return *this;
}

UdtPollSet::const_iterator UdtPollSet::const_iterator::operator++(int) {
    UdtPollSet::const_iterator self(*this);
    impl_->Next();
    return self;
}

bool UdtPollSet::const_iterator::operator==( const UdtPollSet::const_iterator& right ) const {
    if( this == &right ) return true;
    if( impl_ ) {
        if( right.impl_ ) 
            return impl_->Equal(*right.impl_);
        else 
            return false;
    } else {
        if( right.impl_ )
            return false;
        else 
            return true;
    }
}

bool UdtPollSet::const_iterator::operator!=( const UdtPollSet::const_iterator& right ) const {
    return !(*this == right);
}

UdtSocket* UdtPollSet::const_iterator::socket() {
    Q_ASSERT(impl_);
    return impl_->GetSocket();
}

const UdtSocket* UdtPollSet::const_iterator::socket() const {
    Q_ASSERT(impl_);
    return impl_->GetSocket();
}

aio::EventType UdtPollSet::const_iterator::eventType() const {
    Q_ASSERT(impl_);
    return impl_->GetEventType();
}

void* UdtPollSet::const_iterator::userData() {
    Q_ASSERT(impl_);
    return impl_->GetUserData();
}

UdtPollSet::const_iterator::~const_iterator(){}

// ============================================
// UdtPollSet
// ============================================

UdtPollSet::UdtPollSet():
    impl_( detail::UdtPollSetImpl::Create() ) {
}

UdtPollSet::~UdtPollSet(){}

void UdtPollSet::interrupt() {
    Q_ASSERT(impl_);
    impl_->Interrupt();
}

bool UdtPollSet::add( UdtSocket* socket , aio::EventType eventType, void* userData ) {
    Q_ASSERT(impl_);
    return impl_->Add(socket,socket->impl_.get(),eventType,userData);
}

void* UdtPollSet::remove( UdtSocket* socket , aio::EventType eventType ) {
    Q_ASSERT(impl_);
    return impl_->Remove(socket->impl_.get(),eventType);
}

size_t UdtPollSet::size() const {
    Q_ASSERT(impl_);
    return impl_->size();
}

void* UdtPollSet::getUserData( UdtSocket* socket, aio::EventType eventType ) const {
    Q_ASSERT(impl_);
    return impl_->GetUserData(socket->impl_.get(),eventType);
}

int UdtPollSet::poll( int millisToWait ) {
    Q_ASSERT(impl_);
    return impl_->Poll(millisToWait);
}

UdtPollSet::const_iterator UdtPollSet::begin() const {
    Q_ASSERT(impl_);
    return const_iterator(impl_.get(),false);
}

UdtPollSet::const_iterator UdtPollSet::end() const {
    Q_ASSERT(impl_);
    return const_iterator(impl_.get(),true);
}
