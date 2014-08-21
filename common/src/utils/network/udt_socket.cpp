#include "udt_socket.h"
#include <udt/udt.h>
#include <utils/common/log.h>

#ifdef Q_OS_WIN
#  include <ws2tcpip.h>
#  include <iphlpapi.h>
#  include "win32_socket_tools.h"
#else
#  include <netinet/tcp.h>
#endif

#define ADDR_(x) reinterpret_cast<sockaddr*>(x)

// Tracking operation. This operation should be put inside of the DEBUG_
// macro and also using it will log the error that has happened at last 
// time using general log macro. This helpful for us to do debugging and
// event tracing.

#define TRACE_(func,handler) \
    do { \
    NX_LOG( \
    QString(QLatin1String("Udt function[%1]:%2 has error!\nUdt Error Message:%3.\nSystem Error Message:%4\n" \
    "File:%5 Line:%6")). \
    arg(static_cast<int>(handler)).arg(QLatin1String(func)). \
    arg(QLatin1String(UDT::getlasterror().getErrorMessage())). \
    arg(SystemError::getLastOSErrorText()). \
    arg(QLatin1String(__FILE__)).arg(__LINE__),cl_logERROR); } while(0)

// Verify macro is useful for us to push up the error detection in DEBUG mode
// It will trigger a assertion failed in DEBUG version or LOG ERROR once in 
// the RELEASE version. This is useful for those ALWAYS RIGHT error, like 
// create a thread, or close a socket .

#ifndef NDEBUG
#define VERIFY_(cond,func,handler) \
    do { \
    bool eval = (cond); \
    if(!eval) TRACE_(func,handler); Q_ASSERT(cond);}while(0)
#else
#define VERIFY_(cond,func,handler) \
    do { \
    bool eval = (cond); \
    if(!eval) TRACE_(func,handler); }while(0)
#endif // NDEBUG

#ifndef NDEBUG
#define DEBUG_(...) __VA_ARGS__
#else
#define DEBUG_(...) (void*)(NULL)
#endif // NDEBUG

#define OK_(r) ((r) == 0)

#ifndef NDEBUG
#define STUB_(c) Q_ASSERT(!"Unimplemented!"); c
#endif

// =========================================================
// Udt library initialization and tear down routine.
// =========================================================
#ifndef NDEBUG
namespace {
class UdtLibraryStatus {
public:
    static bool IsUdtLibraryInitialized() {
        return kFlag;
    }
    UdtLibraryStatus() {
        kFlag = true;
    }
private:
    static bool kFlag;
};
bool UdtLibraryStatus::kFlag = false;
}// namespace
#endif NDEBUG

bool InitializeUdtLibrary() {
#ifndef NDEBUG
    static UdtLibraryStatus status;
#endif
    Q_ASSERT(status.IsUdtLibraryInitialized());
    return UDT::startup() == 0; // Always success, offical documentation says.
}

bool DestroyUdtLibrary() {
    Q_ASSERT(UdtLibraryStatus::IsUdtLibraryInitialized());
    return UDT::cleanup() == 0;
}

#ifndef NDEBUG
#define CHECK_UDT_() Q_ASSERT(UdtLibraryStatus::IsUdtLibraryInitialized())
#else
#define CHECK_UDT_() (void*)(NULL)
#endif 

namespace detail{

void AddressFrom( const SocketAddress& local_addr , sockaddr_in* out ) {
    memset(out, 0, sizeof(*out));  // Zero out address structure
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
    UdtSocketImpl(): handler_( UDT::INVALID_SOCK ) , state_(CLOSED) {}
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

    // Async implementation , N/A
    void CancelAsyncIO( aio::EventType eventType, bool waitForRunningHandlerCompletion ) {
        Q_UNUSED(waitForRunningHandlerCompletion);
        Q_UNUSED(eventType);
        STUB_(return); 
    }

    bool RecvAsyncImpl( nx::Buffer* const buf, std::function<void( SystemError::ErrorCode, size_t )>&& handler ) { 
        Q_UNUSED(buf);
        Q_UNUSED(handler);
        STUB_(return false); 
    }

    bool SendAsyncImpl( const nx::Buffer& buf, std::function<void( SystemError::ErrorCode, size_t )>&& handler ) { 
        Q_UNUSED(buf);
        Q_UNUSED(handler);
        STUB_(return false); 
    }

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
    CHECK_UDT_();
    Q_ASSERT(IsClosed());
    handler_ = UDT::socket(AF_INET,SOCK_STREAM,0);
    VERIFY_(handler_ != UDT::INVALID_SOCK,"UDT::socket",0);
    if( handler_ != UDT::INVALID_SOCK ) {
        state_ = OPEN;
        return true;
    } else {
        return false;
    }
}

bool UdtSocketImpl::Bind( const SocketAddress& localAddress ) {
    CHECK_UDT_();
    sockaddr_in addr;
    AddressFrom(localAddress,&addr);
    int ret = UDT::bind(handler_,ADDR_(&addr),sizeof(addr));
    DEBUG_(if(ret !=0) TRACE_("UDT::bind",handler_));
    return ret == 0;
}

SocketAddress UdtSocketImpl::GetLocalAddress() const {
    CHECK_UDT_();
    sockaddr_in addr;
    int len = sizeof(addr);
    if(UDT::getsockname(handler_,ADDR_(&addr),&len) != 0) {
        DEBUG_(TRACE_("UDT::getsockname",handler_));
        return SocketAddress();
    } else {
        return SocketAddress(
            HostAddress(addr.sin_addr),ntohs(addr.sin_port));
    }
}


SocketAddress UdtSocketImpl::GetPeerAddress() const {
    CHECK_UDT_();
    sockaddr_in addr;
    int len = sizeof(addr);
    if(UDT::getpeername(handler_,ADDR_(&addr),&len) !=0) {
        DEBUG_(TRACE_("UDT::getsockname",handler_));
        return SocketAddress();
    } else {
        return SocketAddress(
            HostAddress(addr.sin_addr),ntohs(addr.sin_port));
    }
}

bool UdtSocketImpl::Close() {
    CHECK_UDT_();
    Q_ASSERT(!IsClosed());
    int ret = UDT::close(handler_);
    VERIFY_(OK_(ret),"UDT::Close",handler_);
    state_ = CLOSED;
    return ret == 0;
}

bool UdtSocketImpl::IsClosed() const {
    CHECK_UDT_();
    return state_ == CLOSED;
}

bool UdtSocketImpl::SetReuseAddrFlag( bool reuseAddr ) {
    CHECK_UDT_();
    Q_ASSERT(!IsClosed());
    int ret = UDT::setsockopt(handler_,0,UDT_REUSEADDR,&reuseAddr,sizeof(reuseAddr));
    VERIFY_(OK_(ret),"UDT::setsockopt",handler_);
    return ret == 0;
}

bool UdtSocketImpl::GetReuseAddrFlag( bool* val ) const {
    CHECK_UDT_();
    Q_ASSERT(!IsClosed());
    int len = sizeof(*val);
    int ret = UDT::getsockopt(handler_,0,UDT_REUSEADDR,val,&len);
    VERIFY_(OK_(ret),"UDT::getsockopt",handler_);
    return ret == 0;
}

bool UdtSocketImpl::SetNonBlockingMode( bool val ) {
    CHECK_UDT_();
    Q_ASSERT(!IsClosed());
    bool value = !val;
    int ret = UDT::setsockopt(handler_,0,UDT_SNDSYN,&value,sizeof(value));
    VERIFY_(OK_(ret),"UDT::setsockopt",handler_);
    ret = UDT::setsockopt(handler_,0,UDT_RCVSYN,&value,sizeof(value));
    VERIFY_(OK_(ret),"UDT::setsockopt",handler_);
    return ret == 0;
}

bool UdtSocketImpl::GetNonBlockingMode( bool* val ) const {
    CHECK_UDT_();
    Q_ASSERT(!IsClosed());
    int len = sizeof(*val);
    int ret = UDT::getsockopt(handler_,0,UDT_SNDSYN,val,&len);
    VERIFY_(OK_(ret),"UDT::getsockopt",handler_);
    DEBUG_(
        ret = UDT::getsockopt(handler_,0,UDT_RCVSYN,val,&len); 
        VERIFY_(OK_(ret) && *val == true,"UDT::getsockopt",handler_));

    *val = !*val;
    return ret == 0;
}

// No way. I don't understand why we need MTU for each socket ?
// No one knows the MTU, they use ICMP/UDP to try to detect it,
// and in modern switch, typically will change every half hour !!!
// Better keep it opaque when you make decision what to send .
bool UdtSocketImpl::GetMtu( unsigned int* mtuValue ) const {
    CHECK_UDT_();
    Q_ASSERT(!IsClosed());
    *mtuValue = 1500; // Ethernet MTU 
    return true;
}

// UDT will round the buffer size internally, so don't expect
// what you give will definitly become the real buffer size .

bool UdtSocketImpl::SetSendBufferSize( unsigned int buffSize ) {
    CHECK_UDT_();
    Q_ASSERT(!IsClosed());
    Q_ASSERT( buffSize < static_cast<unsigned int>(std::numeric_limits<int>::max()) );
    int buff_size = static_cast<int>(buffSize);
    int ret = UDT::setsockopt(
        handler_,0,UDT_SNDBUF,&buff_size,sizeof(buff_size));
    VERIFY_(OK_(ret),"UDT::getsockopt",handler_);
    return ret == 0;
}

bool UdtSocketImpl::GetSendBufferSize( unsigned int* buffSize ) const{
    CHECK_UDT_();
    Q_ASSERT(!IsClosed());
    int buff_size;
    int len = sizeof(buff_size);
    int ret = UDT::getsockopt(
        handler_,0,UDT_SNDBUF,&buff_size,&len);
    VERIFY_(OK_(ret),"UDT::getsockopt",handler_);
    *buffSize = static_cast<unsigned int>(buff_size);
    return ret == 0;
}

bool UdtSocketImpl::SetRecvBufferSize( unsigned int buffSize ){
    CHECK_UDT_();
    Q_ASSERT(!IsClosed());
    Q_ASSERT( buffSize < static_cast<unsigned int>(std::numeric_limits<int>::max()) );
    int buff_size = static_cast<int>(buffSize);
    int ret = UDT::setsockopt(
        handler_,0,UDT_RCVBUF,&buff_size,sizeof(buff_size));
    VERIFY_(OK_(ret),"UDT::setsockopt",handler_);
    return ret == 0;
}

bool UdtSocketImpl::GetRecvBufferSize( unsigned int* buffSize ) const{
    CHECK_UDT_();
    Q_ASSERT(!IsClosed());
    int buff_size;
    int len = sizeof(buff_size);
    int ret = UDT::getsockopt(
        handler_,0,UDT_RCVBUF,&buff_size,&len);
    VERIFY_(OK_(ret),"UDT::getsockopt",handler_);
    *buffSize = static_cast<unsigned int>(buff_size);
    return ret == 0;
}

bool UdtSocketImpl::SetRecvTimeout( unsigned int millis ) {
    CHECK_UDT_();
    Q_ASSERT(!IsClosed());
    Q_ASSERT( millis < static_cast<unsigned int>(std::numeric_limits<int>::max()) );
    int time = static_cast<int>(millis);
    int ret = UDT::setsockopt(
        handler_,0,UDT_RCVTIMEO,&time,sizeof(time));
    VERIFY_(OK_(ret),"UDT::setsockopt",handler_);
    return ret == 0;
}

bool UdtSocketImpl::GetRecvTimeout( unsigned int* millis ) const {
    CHECK_UDT_();
    Q_ASSERT(!IsClosed());
    int time;
    int len = sizeof(time);
    int ret = UDT::getsockopt(
        handler_,0,UDT_RCVTIMEO,&time,&len);
    VERIFY_(OK_(ret),"UDT::getsockopt",handler_);
    *millis = static_cast<int>(time);
    return ret == 0;
}

bool UdtSocketImpl::SetSendTimeout( unsigned int ms ) {
    CHECK_UDT_();
    Q_ASSERT(!IsClosed());
    Q_ASSERT( ms < static_cast<unsigned int>(std::numeric_limits<int>::max()) );
    int time = static_cast<int>(ms);
    int ret = UDT::setsockopt(
        handler_,0,UDT_SNDTIMEO,&time,sizeof(time));
    VERIFY_(OK_(ret),"UDT::setsockopt",handler_);
    return ret == 0;
}

bool UdtSocketImpl::GetSendTimeout( unsigned int* millis ) const {
    CHECK_UDT_();
    Q_ASSERT(!IsClosed());
    int time;
    int len = sizeof(time);
    int ret = UDT::getsockopt(
        handler_,0,UDT_SNDTIMEO,&time,&len);
    VERIFY_(OK_(ret),"UDT::getsockopt",handler_);
    *millis = static_cast<unsigned int>(time);
    return ret == 0;
}

bool UdtSocketImpl::GetLastError( SystemError::ErrorCode* errorCode ) const {
    CHECK_UDT_();
    Q_ASSERT(!IsClosed());
    *errorCode = static_cast<SystemError::ErrorCode>(UDT::getlasterror().getErrorCode());
    return true;
}

AbstractSocket::SOCKET_HANDLE UdtSocketImpl::handle() const {
    CHECK_UDT_();
    Q_ASSERT(!IsClosed());
    Q_ASSERT(sizeof(UDTSOCKET) == sizeof(AbstractSocket::SOCKET_HANDLE));
    return *reinterpret_cast<const AbstractSocket::SOCKET_HANDLE*>(&handler_);
}


int UdtSocketImpl::Recv( void* buffer, unsigned int bufferLen, int flags ) {
    CHECK_UDT_();
    Q_ASSERT(!IsClosed());
    int sz = UDT::recv(handler_,reinterpret_cast<char*>(buffer),bufferLen,flags);
    if(sz <0) {
        // UDT doesn't translate the EOF into a recv with zero return, but instead
        // it returns error with 2001 error code. We need to detect this and translate
        // back with a zero return here .
        int error_code = UDT::getlasterror().getErrorCode();
        if( error_code == CUDTException::ECONNLOST ) {
            return 0;
        } else {
            DEBUG_(TRACE_("UDT::recv",handler_));
        }
    }
    return sz;
}

int UdtSocketImpl::Send( const void* buffer, unsigned int bufferLen ) {
    CHECK_UDT_();
    Q_ASSERT(!IsClosed());
    int sz = UDT::send(handler_,reinterpret_cast<const char*>(buffer),bufferLen,0);
    DEBUG_(
        if(sz<0) TRACE_("UDT::send",handler_));
    return sz;
}

// This is a meaningless function. You could call Close and then create another
// one if you bundle the states not_initialized into the original Socket design.
bool UdtSocketImpl::Reopen() {
    CHECK_UDT_();
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
    bool AsyncConnect( const SocketAddress& addr,
        std::function<void(SystemError::ErrorCode)>&& handler ) {
            Q_UNUSED(handler);
            Q_UNUSED(addr);
            STUB_(return false);
    }

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
    CHECK_UDT_();
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
        return false;
    } else {
        if( ret == 0 )  {
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
    bool AsyncAccept( std::function<void( SystemError::ErrorCode, AbstractStreamSocket* )> && handler ) {
        Q_UNUSED(handler);
        STUB_(return false);
    }
private:
    UdtSocketImpl* impl_;
};

UdtSocketImpl* UdtAcceptor::Accept() {
    CHECK_UDT_();
    Q_ASSERT(impl_->state() == UdtSocketImpl::ESTABLISHED);
    UDTSOCKET ret = UDT::accept(impl_->udt_handler(),NULL,NULL);
    if( ret == UDT::INVALID_SOCK ) {
        DEBUG_(TRACE_("UDT::accept",impl_->udt_handler()));
        return NULL;
    } else {
        return new UdtSocketImpl( ret , UdtSocketImpl::ESTABLISHED );
    }
}


bool UdtAcceptor::Listen( int backlog ) {
    CHECK_UDT_();
    Q_ASSERT(impl_->state() == UdtSocketImpl::OPEN);
    int ret = UDT::listen(impl_->udt_handler(),backlog);
    if( ret != 0 ) {
        DEBUG_(TRACE_("UDT::listen",impl_->udt_handler()));
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

bool UdtSocket::bind( const SocketAddress& localAddress ) {
    return impl_->Bind(localAddress);
}

SocketAddress UdtSocket::getLocalAddress() const {
    return impl_->GetLocalAddress();
}

SocketAddress UdtSocket::getPeerAddress() const {
    return impl_->GetPeerAddress();
}

void UdtSocket::close() {
    impl_->Close();
}

bool UdtSocket::isClosed() const {
    return impl_->IsClosed();
}

bool UdtSocket::setReuseAddrFlag( bool reuseAddr ) {
    return impl_->SetReuseAddrFlag(reuseAddr);
}

bool UdtSocket::getReuseAddrFlag( bool* val ) {
    return impl_->GetReuseAddrFlag(val);
}

bool UdtSocket::setNonBlockingMode( bool val ) {
    return impl_->SetNonBlockingMode(val);
}

bool UdtSocket::getNonBlockingMode( bool* val ) const {
    return impl_->GetNonBlockingMode(val);
}

bool UdtSocket::getMtu( unsigned int* mtuValue ) {
    return impl_->GetMtu(mtuValue);
}

bool UdtSocket::setSendBufferSize( unsigned int buffSize ) {
    return impl_->SetSendBufferSize(buffSize);
}

bool UdtSocket::getSendBufferSize( unsigned int* buffSize ) {
    return impl_->GetSendBufferSize(buffSize);
}

bool UdtSocket::setRecvBufferSize( unsigned int buffSize ) {
    return impl_->SetRecvBufferSize(buffSize);
}

bool UdtSocket::getRecvBufferSize( unsigned int* buffSize ) {
    return impl_->GetRecvBufferSize( buffSize );
}

bool UdtSocket::setRecvTimeout( unsigned int millis ) {
    return impl_->SetRecvTimeout(millis);
}

bool UdtSocket::getRecvTimeout( unsigned int* millis ) {
    return impl_->GetRecvTimeout(millis);
}

bool UdtSocket::setSendTimeout( unsigned int ms )  {
    return impl_->SetSendTimeout(ms);
}

bool UdtSocket::getSendTimeout( unsigned int* millis ) {
    return impl_->GetSendTimeout(millis);
}

bool UdtSocket::getLastError( SystemError::ErrorCode* errorCode ) {
    return impl_->GetLastError(errorCode);
}

bool UdtSocket::connect(
    const QString& foreignAddress,
    unsigned short foreignPort,
    unsigned int timeoutMillis ) {
        return detail::UdtConnector(impl_.get()).
            Connect(foreignAddress,foreignPort,timeoutMillis);
}

int UdtSocket::recv( void* buffer, unsigned int bufferLen, int flags ) {
    return impl_->Recv(buffer,bufferLen,flags);
}

int UdtSocket::send( const void* buffer, unsigned int bufferLen ) {
    return impl_->Send(buffer,bufferLen);
}

bool UdtSocket::isConnected() const {
    return impl_->IsConnected();
}

bool UdtSocket::reopen() {
    return impl_->Reopen();
}

void UdtSocket::cancelAsyncIO( aio::EventType eventType, bool waitForRunningHandlerCompletion )  {
    return impl_->CancelAsyncIO(eventType,waitForRunningHandlerCompletion);
}

bool UdtSocket::connectAsyncImpl( const SocketAddress& addr, std::function<void( SystemError::ErrorCode )>&& handler ) {
    return detail::UdtConnector(impl_.get()).AsyncConnect(addr,std::move(handler));
}

bool UdtSocket::recvAsyncImpl( nx::Buffer* const buf, std::function<void( SystemError::ErrorCode, size_t )>&& handler ) {
    return impl_->RecvAsyncImpl(buf,std::move(handler));
}

bool UdtSocket::sendAsyncImpl( const nx::Buffer& buf, std::function<void( SystemError::ErrorCode, size_t )>&& handler ) {
    return impl_->SendAsyncImpl(buf,std::move(handler));
}

AbstractSocket::SOCKET_HANDLE UdtSocket::handle() const {
    return impl_->handle();
}

UdtSocket::UdtSocket():impl_(new detail::UdtSocketImpl()) {
    impl_->Open();
}

UdtSocket::~UdtSocket(){}

// =====================================================================
// UdtServerSocket implementation
// =====================================================================

bool UdtServerSocket::listen( int backlog )  {
    return detail::UdtAcceptor(impl_.get()).Listen(backlog);
}

AbstractStreamSocket* UdtServerSocket::accept()  {
    detail::UdtSocketImpl* impl = detail::UdtAcceptor(impl_.get()).Accept();
    if( impl == NULL )
        return NULL;
    else
        return new UdtSocket(impl);
}

bool UdtServerSocket::bind( const SocketAddress& localAddress ) {
    return impl_->Bind(localAddress);
}

SocketAddress UdtServerSocket::getLocalAddress() const {
    return impl_->GetLocalAddress();
}

SocketAddress UdtServerSocket::getPeerAddress() const {
    return impl_->GetPeerAddress();
}

void UdtServerSocket::close() {
    impl_->Close();
}

bool UdtServerSocket::isClosed() const {
    return impl_->IsClosed();
}

bool UdtServerSocket::setReuseAddrFlag( bool reuseAddr ) {
    return impl_->SetReuseAddrFlag(reuseAddr);
}

bool UdtServerSocket::getReuseAddrFlag( bool* val ) {
    return impl_->GetReuseAddrFlag(val);
}

bool UdtServerSocket::setNonBlockingMode( bool val ) {
    return impl_->SetNonBlockingMode(val);
}

bool UdtServerSocket::getNonBlockingMode( bool* val ) const {
    return impl_->GetNonBlockingMode(val);
}

bool UdtServerSocket::getMtu( unsigned int* mtuValue ) {
    return impl_->GetMtu(mtuValue);
}

bool UdtServerSocket::setSendBufferSize( unsigned int buffSize ) {
    return impl_->SetSendBufferSize(buffSize);
}

bool UdtServerSocket::getSendBufferSize( unsigned int* buffSize ) {
    return impl_->GetSendBufferSize(buffSize);
}

bool UdtServerSocket::setRecvBufferSize( unsigned int buffSize ) {
    return impl_->SetRecvBufferSize(buffSize);
}

bool UdtServerSocket::getRecvBufferSize( unsigned int* buffSize ) {
    return impl_->GetRecvBufferSize(buffSize);
}

bool UdtServerSocket::setRecvTimeout( unsigned int millis ) {
    return impl_->SetRecvTimeout(millis);
}

bool UdtServerSocket::getRecvTimeout( unsigned int* millis ) {
    return impl_->GetRecvTimeout(millis);
}

bool UdtServerSocket::setSendTimeout( unsigned int ms ) {
    return impl_->SetSendTimeout(ms);
}

bool UdtServerSocket::getSendTimeout( unsigned int* millis ) {
    return impl_->GetSendTimeout(millis);
}

bool UdtServerSocket::getLastError( SystemError::ErrorCode* errorCode ) {
    return impl_->GetLastError(errorCode);
}

bool UdtServerSocket::acceptAsyncImpl( std::function<void( SystemError::ErrorCode, AbstractStreamSocket* )> handler ) {
    Q_UNUSED(handler);
    STUB_(return false);
}

AbstractSocket::SOCKET_HANDLE UdtServerSocket::handle() const {
    return impl_->handle();
}

UdtServerSocket::UdtServerSocket() : impl_( new detail::UdtSocketImpl() ) {
    impl_->Open();
}

UdtServerSocket::~UdtServerSocket(){}


// =========================================================
// Async Implementation 
// =========================================================

