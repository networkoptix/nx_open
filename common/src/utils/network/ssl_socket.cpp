#include "ssl_socket.h"
#include <openssl/opensslconf.h>
#ifdef ENABLE_SSL
#include <mutex>
#include <errno.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <utils/common/log.h>
#include <list>
#include <memory>
#include <atomic>
#include <thread>

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

static const int BUFFER_SIZE = 1024;
const unsigned char sid[] = "Network Optix SSL socket";

    // TODO: public methods are visible to all, quite bad
int sock_read(BIO *b, char *out, int outl)
{
    QnSSLSocket* sslSock = (QnSSLSocket*) BIO_get_app_data(b);
    if( sslSock->mode() == QnSSLSocket::ASYNC ) {
        int ret = sslSock->asyncRecvInternal(out,outl);
        if( ret == -1 ) {
            BIO_clear_retry_flags(b);
            BIO_set_retry_read(b);
        }
        return ret;
    }
    
    int ret=0;
    if (out != NULL)
    {
        //clear_socket_error();
        ret = sslSock->recvInternal(out, outl, 0);
        BIO_clear_retry_flags(b);
        if (ret <= 0)
        {
            const int sysErrorCode = SystemError::getLastOSErrorCode();
            if( sysErrorCode == SystemError::wouldBlock ||
                sysErrorCode == SystemError::again ||
                BIO_sock_should_retry(sysErrorCode) )
            {
                BIO_set_retry_read(b);
            }

        }
    }
    return ret;
}

int sock_write(BIO *b, const char *in, int inl)
{
    QnSSLSocket* sslSock = (QnSSLSocket*) BIO_get_app_data(b);
    if( sslSock->mode() == QnSSLSocket::ASYNC ) {
        int ret = sslSock->asyncSendInternal(in,inl);
        if( ret == -1 ) {
            BIO_clear_retry_flags(b);
            BIO_set_retry_write(b);
        }
        return ret;
    }

    //clear_socket_error();
    int ret = sslSock->sendInternal(in, inl);
    BIO_clear_retry_flags(b);
    if (ret <= 0)
    {
        const int sysErrorCode = SystemError::getLastOSErrorCode();
        if( sysErrorCode == SystemError::wouldBlock ||
            sysErrorCode == SystemError::again ||
            BIO_sock_should_retry(sysErrorCode) )
        {
            BIO_set_retry_write(b);
        }
    }
    return ret;
}

namespace {
static int sock_puts(BIO *bp, const char *str)
{
    return sock_write(bp, str, strlen(str));
}

static long sock_ctrl(BIO *b, int cmd, long num, void* /*ptr*/)
{
    long ret=1;

    switch (cmd)
    {
    case BIO_C_SET_FD:
        Q_ASSERT("Invalid proxy socket use!");
        break;
    case BIO_C_GET_FD:
        Q_ASSERT("Invalid proxy socket use!");
        break;
    case BIO_CTRL_GET_CLOSE:
        ret=b->shutdown;
        break;
    case BIO_CTRL_SET_CLOSE:
        b->shutdown=(int)num;
        break;
    case BIO_CTRL_DUP:
    case BIO_CTRL_FLUSH:
        ret=1;
        break;
    default:
        ret=0;
        break;
    }
    return(ret);
}

static int sock_new(BIO *bi)
{
    bi->init=1;
    bi->num=0;
    bi->ptr=NULL;
    bi->flags=0;
    return(1);
}

static int sock_free(BIO *a)
{
    if (a == NULL) return(0);
    if (a->shutdown)
    {
        if (a->init)
        {
            QnSSLSocket* sslSock = (QnSSLSocket*) BIO_get_app_data(a);
            if (sslSock)
                sslSock->close();
        }
        a->init=0;
        a->flags=0;
    }
    return(1);
}

static BIO_METHOD Proxy_server_socket =
{
    BIO_TYPE_SOCKET,
    "proxy server socket",
    sock_write,
    sock_read,
    sock_puts,
    NULL, // sock_gets, 
    sock_ctrl,
    sock_new,
    sock_free,
    NULL,
};

}

namespace {

// SSL global lock. This is a must even if the compilation has configured with THREAD for OpenSSL.
// Based on the documentation of OpenSSL, it internally uses lots of global data structure. Apart
// from this, I have suffered the wired access violation when not give OpenSSL lock callback. The
// documentation says 2 types of callback is needed, however the other one, thread id , is not a 
// must since OpenSSL configured with thread support will give default version. Additionally, the
// dynamic lock interface is not used in current OpenSSL version. So we don't use it.

static std::mutex* kOpenSSLGlobalLock;

void OpenSSLGlobalLock( int mode , int type , const char* file , int line ) {
    Q_UNUSED(file);
    Q_UNUSED(line);
    Q_ASSERT( kOpenSSLGlobalLock != NULL );
    if( mode & CRYPTO_LOCK ) {
        kOpenSSLGlobalLock[type].lock();
    } else {
        kOpenSSLGlobalLock[type].unlock();
    }
}

static std::once_flag kOpenSSLGlobalLockFlag;

void OpenSSLInitGlobalLock() {
    Q_ASSERT(kOpenSSLGlobalLock == NULL);
    // not safe here, new can throw exception 
    kOpenSSLGlobalLock = new std::mutex[CRYPTO_num_locks()];
    CRYPTO_set_locking_callback( OpenSSLGlobalLock );
}

void InitOpenSSLGlobalLock() {
    std::call_once(kOpenSSLGlobalLockFlag,OpenSSLInitGlobalLock);
}

SystemError::ErrorCode kSSLInternalError = 100;
static std::size_t kAsyncSSLRecvBufferSize = 1024*100;

class AsyncSSLOperation {
public:
    enum {
        PENDING,
        EXCEPTION,
        SUCCESS,
        END_OF_STREAM
    };
    virtual void Perform( int* ssl_return , int* ssl_error ) = 0;
    void IncreasePendingIOCount() {
        ++pending_io_count_;
    }
    void DecreasePendingIOCount() {
        Q_ASSERT(pending_io_count_ !=0);
        --pending_io_count_;
        if( pending_io_count_ == 0 ) {
            // Check if we can invoke the IO operations , if the IO
            // status is not PENDING, then we can invoke such async
            // SSL operation's user callback function here
            if( exit_status_ != PENDING )
                InvokeUserCallback();
        }
    }
    void SetExitStatus( int exit_status , SystemError::ErrorCode error_code ) {
        if( exit_status_ == PENDING ) {
            exit_status_ = exit_status;
            error_code_ = error_code;
            if( pending_io_count_ == 0 ) {
                InvokeUserCallback();
            }
        }
    }
    AsyncSSLOperation( SSL* ssl ) :
        pending_io_count_(0),
        exit_status_(PENDING),
        error_code_(SystemError::noError),
        ssl_(ssl)
    {}
    ~AsyncSSLOperation() {
        Q_ASSERT( pending_io_count_ == 0 );
    }
protected:
    virtual void InvokeUserCallback() = 0;
    void Reset() {
        pending_io_count_ = 0;
        exit_status_ = PENDING;
        error_code_ = SystemError::noError;
    }
    int pending_io_count_;
    int exit_status_;
    SystemError::ErrorCode error_code_;
    SSL* ssl_;
};

class AsyncRead : public AsyncSSLOperation {
public:
    virtual void Perform( int* ssl_return , int* ssl_error ) {
        int old_size = read_buffer_->size();
        int read_size = read_buffer_->capacity() - old_size;
        read_buffer_->resize( read_buffer_->capacity() );
        *ssl_return = SSL_read(ssl_,read_buffer_->data()+old_size,read_size);
        *ssl_error = SSL_get_error(ssl_,*ssl_return);
        if( *ssl_return<=0 ) {
            read_buffer_->resize(old_size);
        } else {
            read_buffer_->resize(old_size+*ssl_return);
            read_bytes_ += *ssl_return;
        }
    }
    void Reset( nx::Buffer* buffer , std::function<void(SystemError::ErrorCode,std::size_t)>&& handler ) {
        read_buffer_ = buffer;
        handler_ = std::move(handler);
        read_bytes_ = 0;
        AsyncSSLOperation::Reset();
    }
    AsyncRead( SSL* ssl ) : AsyncSSLOperation(ssl) {}
protected:
    virtual void InvokeUserCallback() {
        switch( exit_status_ ) {
        case AsyncSSLOperation::EXCEPTION:
            handler_(error_code_, (std::numeric_limits<std::size_t>::max)() );
            return;
        case AsyncSSLOperation::SUCCESS:
            handler_(SystemError::noError,read_bytes_);
            return;
        case AsyncSSLOperation::END_OF_STREAM:
            handler_(SystemError::noError,0);
            return;
        default: Q_ASSERT(false); return;
        }
    }
private:
    nx::Buffer* read_buffer_;
    std::function<void(SystemError::ErrorCode,std::size_t)> handler_;
    std::size_t read_bytes_;
};

class AsyncWrite : public AsyncSSLOperation {
public:
    virtual void Perform( int* ssl_return , int* ssl_error ) {
        Q_ASSERT( !write_buffer_->isEmpty() );
        *ssl_return = SSL_write(
            ssl_,write_buffer_->constData(),write_buffer_->size());
        *ssl_error = SSL_get_error(ssl_,*ssl_return);
    }
    void Reset( const nx::Buffer* buffer , std::function<void(SystemError::ErrorCode,std::size_t)>&& handler ) {
        write_buffer_ = buffer;
        handler_ = std::move(handler);
        AsyncSSLOperation::Reset();
    }
    AsyncWrite( SSL* ssl ) : AsyncSSLOperation(ssl) {}
protected:
    virtual void InvokeUserCallback() {
        switch( exit_status_ ) {
        case AsyncSSLOperation::EXCEPTION:
            handler_(kSSLInternalError, std::numeric_limits<std::size_t>::max() );
            return;
        case AsyncSSLOperation::END_OF_STREAM:
            handler_(error_code_, std::numeric_limits<std::size_t>::max() );
            return;
        case AsyncSSLOperation::SUCCESS:
            handler_(SystemError::noError,write_buffer_->size());
            return;
        default: Q_ASSERT(false); return;
        }
    }
private:
    const nx::Buffer* write_buffer_;
    std::function<void(SystemError::ErrorCode,std::size_t)> handler_;
};

class AsyncHandshake : public AsyncSSLOperation {
public:
    virtual void Perform( int* ssl_return , int* ssl_error ) {
        *ssl_return = SSL_do_handshake(ssl_);
        *ssl_error = SSL_get_error(ssl_,*ssl_return);
    }
    void Reset( std::function<void(SystemError::ErrorCode)>&& handler ) {
        handler_ = std::move(handler);
        AsyncSSLOperation::Reset();
    }
    AsyncHandshake( SSL* ssl ) : AsyncSSLOperation(ssl) {}
protected:
    virtual void InvokeUserCallback() {
        switch( exit_status_ ) {
        case AsyncSSLOperation::EXCEPTION:
            handler_(error_code_);
            return;
        case AsyncSSLOperation::END_OF_STREAM:
            handler_(kSSLInternalError);
            return;
        case AsyncSSLOperation::SUCCESS:
            handler_(SystemError::noError);
            return;
        default: Q_ASSERT(false); return;
        }
    }
private:
    std::function<void(SystemError::ErrorCode)> handler_;
};

class AsyncSSL {
public:
    AsyncSSL( SSL* ssl , AbstractStreamSocket* underly_socket , bool is_server );
    ~AsyncSSL();
public:
    // BIO operation function. These 2 BIO operation function will simulate BIO memory
    // type. For read, it will simply checks the input read buffer, and for write operations
    // it will write the data into the write buffer and then the upper will detect what
    // needs to send out. This style makes the code easier and simpler and also since we
    // have PendingIOCount, all the user callback function will be invoked in proper order
    std::size_t BIOWrite( const void* data , std::size_t size );
    std::size_t BIORead ( void* data , std::size_t size );
    bool eof() const {
        return eof_;
    }
    // Asynchronous operation
    bool AsyncSend( const nx::Buffer& buffer , std::function<void(SystemError::ErrorCode,std::size_t)>&& handler );
    bool AsyncRecv( nx::Buffer* buffer , std::function<void(SystemError::ErrorCode,std::size_t)>&& handler );
    void WaitForAllPendingIOFinish() {
        underly_socket_->cancelAsyncIO();
    }
private:
    void AsyncPerform( AsyncSSLOperation* operation );
    // Perform the corresponding SSL operations that is outstanding here.
    // Since all the operation will be serialized inside of a single thread.
    // We can simply pass all the parameter all around as a function parameter.
    void Perform( AsyncSSLOperation* operation );
    // Check whether SSL has been shutdown or not for this situations. 
    void CheckShutdown( int ssl_return , int ssl_error );
    // Handle SSL internal error 
    void HandleSSLError( int ssl_return , int ssl_error );
    // These 2 functions will issue the read/write operations directly to the underlying
    // socket. It will use context variable outstanding_read_/outstanding_write_ operations
    void EnqueueRead( AsyncSSLOperation* operation );
    void DoRead();
    void DoWrite();
    void EnqueueWrite();
    void DoHandshake( AsyncSSLOperation* next_op );

    void OnUnderlySocketRecv( SystemError::ErrorCode error_code , std::size_t bytes_transferred );
    void OnUnderlySocketSend( SystemError::ErrorCode error_code , std::size_t bytes_transferred );
    void OnUnderlySocketConn( SystemError::ErrorCode error_code );

    void ContinueRead();
    void ContinueWrite();
protected:
    AbstractStreamSocket* socket() {
        return underly_socket_;
    }
    void InjectSnifferData( const nx::Buffer& buffer ) {
        underly_socket_recv_buffer_.append(buffer);
    }
    enum {
        HANDSHAKE_NOT_YET,
        HANDSHAKE_PENDING,
        HANDSHAKE_DONE
    };
    void SetHandshakeStage( int handshake_stage ) {
        handshake_stage_ = handshake_stage;
    }
private:
    // Operations
    std::unique_ptr<AsyncRead> read_;
    std::unique_ptr<AsyncWrite> write_;
    std::unique_ptr<AsyncHandshake> handshake_;
    // Handshake
    std::list<AsyncSSLOperation*> handshake_queue_;
    // Read
    bool allow_bio_read_;
    nx::Buffer underly_socket_recv_buffer_;
    std::size_t recv_buffer_read_pos_;
    std::list<AsyncSSLOperation*> read_queue_;
    AsyncSSLOperation* outstanding_read_;
    // Write
    struct PendingWrite {
        nx::Buffer write_buffer;
        AsyncSSLOperation* operation;
        PendingWrite( PendingWrite&& write ) {
            write_buffer = std::move(write.write_buffer);
            operation = write.operation;
        }
        PendingWrite():operation(NULL){}
        bool NeedWrite() const {
            return !write_buffer.isEmpty();
        }
        void Reset( AsyncSSLOperation* op ) {
            operation = op;
            write_buffer.clear();
        }
    };
    std::list<PendingWrite> write_queue_;
    PendingWrite* outstanding_write_;
    PendingWrite current_write_;
    // SSL related
    SSL* ssl_;
    bool eof_;
    bool is_server_;
    int handshake_stage_;
    AbstractStreamSocket* underly_socket_;
    // A deletion flag must be used to protect the read/write
    class DeletionFlag {
    public:
        DeletionFlag( AsyncSSL* ssl ) : flag_( false ) , ssl_(ssl) {
            ssl->deletion_flag_ = &flag_;
        }
        ~DeletionFlag() {
            if( flag_ != true ) {
                ssl_->deletion_flag_ = NULL;
            }
        }
        operator bool () const {
            return flag_;
        }
        void Reset() {
            flag_ = false;
        }
    private:
        bool flag_;
        AsyncSSL* ssl_;
    };
    bool* deletion_flag_;
    friend class DeletionFlag;
};

AsyncSSL::AsyncSSL( SSL* ssl , AbstractStreamSocket* underly_socket , bool is_server ) :
    read_( new AsyncRead(ssl) ),
    write_( new AsyncWrite(ssl) ),
    handshake_( new AsyncHandshake(ssl) ),
    allow_bio_read_(false),
    recv_buffer_read_pos_(0),
    ssl_(ssl),
    eof_(false),
    is_server_(is_server),
    handshake_stage_(HANDSHAKE_NOT_YET),
    underly_socket_(underly_socket),
    deletion_flag_(NULL){
        underly_socket_recv_buffer_.reserve(kAsyncSSLRecvBufferSize);
}

AsyncSSL::~AsyncSSL() {
    if( deletion_flag_ != NULL ) {
        *deletion_flag_ = true;
    }
}

void AsyncSSL::Perform( AsyncSSLOperation* operation ) {
    int ssl_return , ssl_error ;
    // Setting up the write buffer here
    current_write_.Reset(operation);
    // Perform the underlying SSL operations
    operation->Perform(&ssl_return,&ssl_error);
    // Check the EOF/SHUTDOWN status of the ssl 
    CheckShutdown(ssl_return,ssl_error);
    // Handle each ssl error accordingly here
    if( eof() ) {
        operation->SetExitStatus(AsyncSSLOperation::END_OF_STREAM,SystemError::noError);
        return;
    }
    // Handle existed write here
    if( current_write_.NeedWrite() ) {
        EnqueueWrite();
    }
    switch( ssl_error ) {
    case SSL_ERROR_NONE: 
        // The SSL operation has been finished here
        operation->SetExitStatus(AsyncSSLOperation::SUCCESS,SystemError::noError);
        break;
    case SSL_ERROR_WANT_READ:
        EnqueueRead( operation );
        break;
    case SSL_ERROR_WANT_WRITE:
        break;
    default:
        HandleSSLError(ssl_return,ssl_error);
        operation->SetExitStatus(AsyncSSLOperation::EXCEPTION,kSSLInternalError);
        break;
    }
}

void AsyncSSL::CheckShutdown( int ssl_return , int ssl_error ) {
    Q_UNUSED(ssl_return);
    if( SSL_get_shutdown(ssl_) == SSL_RECEIVED_SHUTDOWN || 
        ssl_error == SSL_ERROR_ZERO_RETURN ) {
            // This should be the normal shutdown which means the
            // peer at least call SSL_shutdown once
            eof_ = true;
    } else if( ssl_error == SSL_ERROR_SYSCALL ) {
        // Brute shutdown for SSL connection
        if( ERR_get_error() == 0 ) {
            eof_ = true;
        }
    }
}

const char* SSLErrorStr( int ssl_error ) {
    switch(ssl_error) {
    case SSL_ERROR_SSL: 
        return "SSL_ERROR_SSL";
    case SSL_ERROR_SYSCALL:
        return "SSL_ERROR_SYSCALL";
    default:
        return "<ssl not a real error>";
    }
}

void AsyncSSL::HandleSSLError( int ssl_return , int ssl_error ) {
    Q_UNUSED(ssl_return);
    NX_LOG(
        lit("SSL error code:%1\n").arg(QLatin1String(SSLErrorStr(ssl_error))),
        cl_logDEBUG1 );
    qDebug()<<QLatin1String(SSLErrorStr(ssl_error));
    int err;
    while( (err = ERR_get_error()) != 0 ) {
        char err_str[1024];
        ERR_error_string_n(err,err_str,1024);
        NX_LOG(
            lit("SSL error stack:%1\n").arg(QLatin1String(err_str)),cl_logDEBUG1);
        qDebug()<<QLatin1String(err_str);
    }
    NX_LOG(
        lit("System error code:%1\n").arg(SystemError::getLastOSErrorCode()),cl_logDEBUG1);

}

void AsyncSSL::OnUnderlySocketRecv( SystemError::ErrorCode error_code , std::size_t bytes_transferred ) {
    if( error_code != SystemError::noError ) {
        outstanding_read_->SetExitStatus(AsyncSSLOperation::EXCEPTION,error_code);
        outstanding_read_->DecreasePendingIOCount();
        ContinueRead();
        return;
    } else {
        if( bytes_transferred == 0 ) {
            eof_ = true;
        }
        // Since we are gonna invoke user's callback here, so a deletion flag will
        // help us to avoid reuse member object once the user deleted such object 
        DeletionFlag deleted(this);
        // Set up the flag to let the user runs into the read operation's returned buffer
        allow_bio_read_ = true;
        Perform(outstanding_read_);
        if( !deleted ) {
            outstanding_read_->DecreasePendingIOCount();
            if( !deleted ) {
                allow_bio_read_ = false;
                ContinueRead();
            }
        }
    }
}

void AsyncSSL::EnqueueRead( AsyncSSLOperation* operation ) {
    read_queue_.push_back(operation);
    if( read_queue_.size() ==1 ) {
        outstanding_read_ = read_queue_.front();
        DoRead();
    }
}

void AsyncSSL::ContinueRead() {
    read_queue_.pop_front();
    if( !read_queue_.empty() ) {
        outstanding_read_ = read_queue_.front();
        DoRead();
    }
}

void AsyncSSL::DoRead() {
    // Setting up the outstanding read here
    outstanding_read_ = read_queue_.front();
    // Checking if we have some data lefts inside of the read buffer, if so
    // we could just call SSL operation right here. And since this function
    // is executed inside of AIO thread, no recursive lock will happened in
    // user's callback function. 
    if( static_cast<int>(recv_buffer_read_pos_) < underly_socket_recv_buffer_.size() ) {
        outstanding_read_->IncreasePendingIOCount();
        OnUnderlySocketRecv(SystemError::noError,
            underly_socket_recv_buffer_.size() - recv_buffer_read_pos_ );
    } else {
        // We have to issue our operations right just here since we have no
        // left data inside of the buffer and our user needs to read all the
        // data out in the buffer here. 
        bool ret = underly_socket_->readSomeAsync(
            &underly_socket_recv_buffer_,
            std::bind(
            &AsyncSSL::OnUnderlySocketRecv,
            this,
            std::placeholders::_1,
            std::placeholders::_2));
        if(!ret) {
            outstanding_read_->SetExitStatus( AsyncSSLOperation::EXCEPTION , SystemError::getLastOSErrorCode() );
        } else {
            outstanding_read_->IncreasePendingIOCount();
        }
    }
}

void AsyncSSL::OnUnderlySocketSend( SystemError::ErrorCode error_code , std::size_t bytes_transferred ) {
    Q_UNUSED(bytes_transferred);
    if( error_code != SystemError::noError ) {
        outstanding_write_->operation->SetExitStatus(AsyncSSLOperation::EXCEPTION,error_code);
        outstanding_write_->operation->DecreasePendingIOCount();
        ContinueWrite();
        return;
    } else {
        // For read operations, if it succeeded, we don't need to call Perform
        // since this operation has been go ahead for that SSL_related operations.
        // What we need to do is just decrease user's pending IO count value.
        DeletionFlag deleted(this);
        outstanding_write_->operation->DecreasePendingIOCount();
        if( !deleted ) {
            ContinueWrite();
        }
    }
}

void AsyncSSL::DoWrite() {
    bool ret = underly_socket_->sendAsync(
        outstanding_write_->write_buffer,
        std::bind(
        &AsyncSSL::OnUnderlySocketSend,
        this,
        std::placeholders::_1,
        std::placeholders::_2));
    if(!ret) {
        outstanding_write_->operation->SetExitStatus( 
            AsyncSSLOperation::EXCEPTION, SystemError::getLastOSErrorCode() );
    } else {
        outstanding_write_->operation->IncreasePendingIOCount();
    }
}

void AsyncSSL::EnqueueWrite() {
    write_queue_.push_back(std::move(current_write_));
    if( write_queue_.size() == 1 ) {
        outstanding_write_ = &(write_queue_.front());
        DoWrite();
    }
}

void AsyncSSL::ContinueWrite() {
    write_queue_.pop_front();
    if( !write_queue_.empty() ) {
        outstanding_write_ = &(write_queue_.front());
        DoWrite();
    }
}

std::size_t AsyncSSL::BIORead( void* data , std::size_t size ) {
    if( allow_bio_read_ ) {
        if( underly_socket_recv_buffer_.size() == static_cast<int>(recv_buffer_read_pos_) ) {
            return 0;
        } else {
            std::size_t digest_size = 
                std::min(size, 
                    static_cast<int>(underly_socket_recv_buffer_.size()) - recv_buffer_read_pos_ );
            memcpy(data,
                   underly_socket_recv_buffer_.constData() + recv_buffer_read_pos_ ,
                   digest_size);
            recv_buffer_read_pos_ += digest_size;
            if( static_cast<int>(recv_buffer_read_pos_) == underly_socket_recv_buffer_.size() ) {
                recv_buffer_read_pos_ = 0;
                underly_socket_recv_buffer_.clear();
                underly_socket_recv_buffer_.reserve( static_cast<int>(kAsyncSSLRecvBufferSize) );
            }
            return digest_size;
        }
    }
    return 0;
}

std::size_t AsyncSSL::BIOWrite( const void* data , std::size_t size ) {
    current_write_.write_buffer.append(static_cast<const char*>(data),size);
    return size;
}

void AsyncSSL::OnUnderlySocketConn( SystemError::ErrorCode error_code ) {
    if( error_code != SystemError::noError ) {
        // No, we cannot connect to the peer sides, so we just tell our user
        // that we have expired whatever we've got currently 
        for( auto op : handshake_queue_ ) {
            DeletionFlag deleted(this);
            op->SetExitStatus( AsyncSSLOperation::EXCEPTION , error_code );
            if( deleted )
                return;
        }
        handshake_queue_.clear();
        handshake_stage_ = HANDSHAKE_NOT_YET;
    } else {
        for( auto op : handshake_queue_ ) {
            DeletionFlag deleted(this);
            Perform(op);
            if( deleted )
                return;
        }
        handshake_queue_.clear();
        handshake_stage_ = HANDSHAKE_DONE;
    }
}

void AsyncSSL::DoHandshake( AsyncSSLOperation* next_op ) {
    handshake_queue_.push_back(next_op);
    if( is_server_ ) {
        SSL_set_accept_state(ssl_);
    } else {
        SSL_set_connect_state(ssl_);
    }
    handshake_->Reset(
        std::bind(
        &AsyncSSL::OnUnderlySocketConn,
        this,
        std::placeholders::_1));
    Perform(handshake_.get());
}

void AsyncSSL::AsyncPerform( AsyncSSLOperation* operation ) {
    switch( handshake_stage_ ) {
    case HANDSHAKE_NOT_YET:
        handshake_stage_ = HANDSHAKE_PENDING;
        DoHandshake(operation);
        break;
    case HANDSHAKE_PENDING:
        handshake_queue_.push_back(operation);
        break;
    case HANDSHAKE_DONE:
        Perform(operation);
        break;
    default: Q_ASSERT(0); return;
    }
}

bool AsyncSSL::AsyncSend( const nx::Buffer& buffer , std::function<void(SystemError::ErrorCode,std::size_t)>&& handler ) {
    write_->Reset(&buffer,std::move(handler));
    AsyncPerform(write_.get());
    return true;
}

bool AsyncSSL::AsyncRecv( nx::Buffer* buffer , std::function<void(SystemError::ErrorCode,std::size_t)>&& handler ) {
    read_->Reset(buffer,std::move(handler));
    AsyncPerform(read_.get());
    return true;
}

class MixedAsyncSSL : public AsyncSSL {
public:

    struct SnifferData {
        std::function<void(SystemError::ErrorCode,std::size_t)> ssl_cb;
        std::function<void(SystemError::ErrorCode,std::size_t)> other_cb;
        nx::Buffer* buffer;
        SnifferData( const std::function<void(SystemError::ErrorCode,std::size_t)>& ssl , 
            const std::function<void(SystemError::ErrorCode,std::size_t)>& other,
            nx::Buffer* buf ) :
            ssl_cb(ssl),
            other_cb(other),
            buffer(buf){}
    };

public:
    static const int kSnifferDataHeaderLength = 2;

    MixedAsyncSSL( SSL* ssl , AbstractStreamSocket* socket )
        :AsyncSSL(ssl,socket,true),
        is_initialized_(false),
        is_ssl_(false){}

    // User could not issue a async_send before a async_recv . Otherwise
    // we are in trouble since we don't know the type of the underly socking
    void AsyncSend(
        const nx::Buffer& buffer, std::function<void(SystemError::ErrorCode,std::size_t)>&& op ) {
            // When you see this, it means you screw up since the very first call should 
            // be a async_recv instead of async_send here . 
            Q_ASSERT(is_initialized_);
            Q_ASSERT(is_ssl_);
            AsyncSSL::AsyncSend(buffer,std::move(op));
    }

    void AsyncRecv( nx::Buffer* buffer, 
         std::function<void(SystemError::ErrorCode,std::size_t)>&& ssl ,
         std::function<void(SystemError::ErrorCode,std::size_t)>&& other ) {
            if( !is_initialized_ ) {
                // We need to sniffer the buffer here
                sniffer_buffer_.reserve(kSnifferDataHeaderLength);
                socket()->
                    readSomeAsync(
                    &sniffer_buffer_,
                    std::bind(
                    &MixedAsyncSSL::Sniffer,
                    this,
                    std::placeholders::_1,
                    std::placeholders::_2,
                    SnifferData(std::move(ssl),std::move(other),buffer)));
                return;
            } else {
                Q_ASSERT(is_ssl_);
                AsyncSSL::AsyncRecv(buffer,std::move(ssl));
            }
    }

    // When blocking version detects it is an SSL, it has to notify me
    void set_ssl( bool ssl ) {
        is_initialized_ = true;
        is_ssl_ = ssl;
        AsyncSSL::SetHandshakeStage( AsyncSSL::HANDSHAKE_DONE );
    }


    bool is_initialized() const {
        return is_initialized_;
    }

    bool is_ssl() const {
        return is_ssl_;
    }

private:
    void Sniffer( SystemError::ErrorCode ec , std::size_t bytes_transferred , SnifferData data ) {
        // We have the data in our buffer right now
        if(ec) {
            data.ssl_cb(ec,0);
            return;
        } else {
            if( bytes_transferred == 0 ) {
                data.ssl_cb(ec,0);
                return;
            } else if( sniffer_buffer_.size() < 2 ) {
                socket()->readSomeAsync(
                    &sniffer_buffer_,
                    std::bind(
                    &MixedAsyncSSL::Sniffer,
                    this,
                    std::placeholders::_1,
                    std::placeholders::_2,
                    data));
                return;
            }
            // Fix for the bug that always false in terms of comparison of 0x80
            const unsigned char* buf = reinterpret_cast<unsigned char*>(sniffer_buffer_.data());
            if( buf[0] == 0x80 || (buf[0] == 0x16 && buf[1] == 0x03) ) {
                is_ssl_ = true;
                is_initialized_ = true;
            } else {
                is_ssl_ = false;
                is_initialized_ = true;
            }
            // If we are SSL , we need to push the data into the buffer
            AsyncSSL::InjectSnifferData( sniffer_buffer_ );
            // If it is an SSL we still need to continue our async operation
            // otherwise we call the failed callback for the upper usage class
            // it should be QnMixedSSLSocket class
            if( is_ssl_ ) {
                // request a SSL async recv
                AsyncSSL::AsyncRecv(data.buffer,std::move(data.ssl_cb));
            } else {
                // request a common async recv
                socket()->readSomeAsync(data.buffer,data.other_cb);
            }
        }
    }

private:
    bool is_initialized_;
    bool is_ssl_;
    nx::Buffer sniffer_buffer_; 

};

}// namespace


// ---------------------------- QnSSLSocket -----------------------------------

class SSLStaticData
{
public:
    EVP_PKEY* pkey;
    SSL_CTX* serverCTX;
    SSL_CTX* clientCTX;

    SSLStaticData()
    :
        pkey( nullptr ),
        serverCTX( nullptr ),
        clientCTX( nullptr )
    {
        SSL_library_init();
        OpenSSL_add_all_algorithms();
        SSL_load_error_strings();
        serverCTX = SSL_CTX_new(SSLv23_server_method());
        clientCTX = SSL_CTX_new(SSLv23_client_method());

        SSL_CTX_set_options(serverCTX, SSL_OP_SINGLE_DH_USE);
        SSL_CTX_set_session_id_context(serverCTX, sid, 4);
    }

    ~SSLStaticData()
    {
        release();
    }

    void release()
    {
        if( serverCTX )
        {
            SSL_CTX_free(serverCTX);
            serverCTX = nullptr;
        }

        if( clientCTX )
        {
            SSL_CTX_free(clientCTX);
            clientCTX = nullptr;
        }

        if( pkey )
        {
            EVP_PKEY_free(pkey);
            pkey = nullptr;
        }
    }
    
    static SSLStaticData* instance();
};

Q_GLOBAL_STATIC(SSLStaticData, SSLStaticData_instance);

SSLStaticData* SSLStaticData::instance()
{
    return SSLStaticData_instance();
}


void QnSSLSocket::initSSLEngine(const QByteArray& certData)
{
    BIO *bufio = BIO_new_mem_buf((void*) certData.data(), certData.size());
    X509 *x = PEM_read_bio_X509_AUX(
        bufio,
        NULL,
        SSLStaticData::instance()->serverCTX->default_passwd_callback,
        SSLStaticData::instance()->serverCTX->default_passwd_callback_userdata);
    SSL_CTX_use_certificate(SSLStaticData::instance()->serverCTX, x);
    SSL_CTX_use_certificate(SSLStaticData::instance()->clientCTX, x);
    BIO_free(bufio);

    bufio = BIO_new_mem_buf((void*) certData.data(), certData.size());
    SSLStaticData::instance()->pkey = PEM_read_bio_PrivateKey(
        bufio,
        NULL,
        SSLStaticData::instance()->serverCTX->default_passwd_callback,
        SSLStaticData::instance()->serverCTX->default_passwd_callback_userdata);
    SSL_CTX_use_PrivateKey(SSLStaticData::instance()->serverCTX, SSLStaticData::instance()->pkey);
    BIO_free(bufio);
    // Initialize OpenSSL global lock, so server side will initialize it right here
    InitOpenSSLGlobalLock();
}

void QnSSLSocket::releaseSSLEngine()
{
    SSLStaticData::instance()->release();
}

class QnSSLSocketPrivate
{
public:
    AbstractStreamSocket* wrappedSocket;
    SSL* ssl;
    BIO* read;
    BIO* write;
    bool isServerSide;
    quint8 extraBuffer[32];
    int extraBufferLen;
    //!Socket works as regular socket (without encryption until this flag is set to \a true)
    bool ecnryptionEnabled;

    // This model flag will be set up by the interface internally. It just tells
    // the user what our socket will be. An async version or a sync version. We
    // keep the sync mode for historic reason , but during the support for async,
    // the call for sync is undefined. This is for purpose since it heavily reduce
    // the pain of 
    std::atomic<int> mode;
    std::shared_ptr<AsyncSSL> async_ssl_ptr;

    QnSSLSocketPrivate()
    :
        wrappedSocket( nullptr ),
        ssl(nullptr),
        read(nullptr),
        write(nullptr),
        isServerSide( false ),
        extraBufferLen( 0 ),
        ecnryptionEnabled(false),
        mode(QnSSLSocket::SYNC)
    {
    }
};

QnSSLSocket::QnSSLSocket(AbstractStreamSocket* wrappedSocket, bool isServerSide):
    d_ptr(new QnSSLSocketPrivate())
{
    Q_D(QnSSLSocket);
    d->wrappedSocket = wrappedSocket;
    d->isServerSide = isServerSide;
    d->extraBufferLen = 0;
    init();
    InitOpenSSLGlobalLock();
    d->async_ssl_ptr.reset( new AsyncSSL(d->ssl,d->wrappedSocket,isServerSide) );
}

QnSSLSocket::QnSSLSocket(QnSSLSocketPrivate* priv, AbstractStreamSocket* wrappedSocket, bool isServerSide):
    d_ptr(priv)
{
    Q_D(QnSSLSocket);
    d->wrappedSocket = wrappedSocket;
    d->isServerSide = isServerSide;
    d->extraBufferLen = 0;
    init();
    InitOpenSSLGlobalLock();
}

void QnSSLSocket::init()
{
    Q_D(QnSSLSocket);

    d->read = BIO_new(&Proxy_server_socket);
    BIO_set_nbio(d->read, 1);
    BIO_set_app_data(d->read, this);

    d->write = BIO_new(&Proxy_server_socket);
    BIO_set_app_data(d->write, this);
    BIO_set_nbio(d->write, 1);

    assert(d->isServerSide ? SSLStaticData::instance()->serverCTX : SSLStaticData::instance()->clientCTX);

    d->ssl = SSL_new(d->isServerSide ? SSLStaticData::instance()->serverCTX : SSLStaticData::instance()->clientCTX);  // get new SSL state with context 
    SSL_set_verify(d->ssl, SSL_VERIFY_NONE, NULL);
    SSL_set_session_id_context(d->ssl, sid, 4);
    SSL_set_bio(d->ssl, d->read, d->write);

}

QnSSLSocket::~QnSSLSocket()
{
    Q_D(QnSSLSocket);
    if(d->mode == ASYNC ) {
        if(d->async_ssl_ptr)
            d->async_ssl_ptr->WaitForAllPendingIOFinish();
    } else {
        if (d->ssl)
            SSL_free(d->ssl);
    }
    delete d->wrappedSocket;
    delete d_ptr;
}


bool QnSSLSocket::doServerHandshake()
{
    Q_D(QnSSLSocket);
    SSL_set_accept_state(d->ssl);

    return SSL_do_handshake(d->ssl) == 1;
}

bool QnSSLSocket::doClientHandshake()
{
    Q_D(QnSSLSocket);
    SSL_set_connect_state(d->ssl);

    int ret = SSL_do_handshake(d->ssl);
    //int err2 = SSL_get_error(d->ssl, ret);
    //const char* err = ERR_reason_error_string(ERR_get_error());
    return ret == 1;
}

int QnSSLSocket::recvInternal(void* buffer, unsigned int bufferLen, int /*flags*/)
{
    Q_D(QnSSLSocket);
    if (d->extraBufferLen > 0)
    {
        int toReadLen = qMin((int)bufferLen, d->extraBufferLen);
        memcpy(buffer, d->extraBuffer, toReadLen);
        memmove(d->extraBuffer, d->extraBuffer + toReadLen, d->extraBufferLen - toReadLen);
        d->extraBufferLen -= toReadLen;
        int readRest = bufferLen - toReadLen;
        // Does this should be readReset ? -- DPENG
        if (toReadLen > 0) {
            int readed = d->wrappedSocket->recv((char*) buffer + toReadLen, readRest);
            if (readed > 0)
                toReadLen += readed;
        }
        return toReadLen;
    }
    return d->wrappedSocket->recv(buffer, bufferLen);
}

int QnSSLSocket::recv( void* buffer, unsigned int bufferLen, int flags)
{
    Q_D(QnSSLSocket);
    if( !d->ecnryptionEnabled )
        return d->wrappedSocket->recv( buffer, bufferLen, flags );

    d->mode.store(QnSSLSocket::SYNC,std::memory_order_release);
    if (!SSL_is_init_finished(d->ssl)) {
        if (d->isServerSide)
            doServerHandshake();
        else
            doClientHandshake();
    }
    return SSL_read(d->ssl, (char*) buffer, bufferLen);
}

int QnSSLSocket::sendInternal( const void* buffer, unsigned int bufferLen )
{
    Q_D(QnSSLSocket);
    return d->wrappedSocket->send(buffer,bufferLen);
}

int QnSSLSocket::send( const void* buffer, unsigned int bufferLen )
{
    Q_D(QnSSLSocket);

    if( !d->ecnryptionEnabled )
        return d->wrappedSocket->send( buffer, bufferLen );

    d->mode.store(QnSSLSocket::SYNC,std::memory_order_release);
    if (!SSL_is_init_finished(d->ssl)) {
        if (d->isServerSide)
            doServerHandshake();
        else
            doClientHandshake();
    }

    return SSL_write(d->ssl, buffer, bufferLen);
}

bool QnSSLSocket::reopen()
{
    Q_D(QnSSLSocket);
    d->ecnryptionEnabled = false;
    return d->wrappedSocket->reopen();
}

bool QnSSLSocket::setNoDelay( bool value )
{
    Q_D(QnSSLSocket);
    return d->wrappedSocket->setNoDelay(value);
}

bool QnSSLSocket::getNoDelay( bool* value )
{
    Q_D(QnSSLSocket);
    return d->wrappedSocket->getNoDelay(value);
}

bool QnSSLSocket::toggleStatisticsCollection( bool val )
{
    Q_D(QnSSLSocket);
    return d->wrappedSocket->toggleStatisticsCollection(val);
}

bool QnSSLSocket::getConnectionStatistics( StreamSocketInfo* info )
{
    Q_D(QnSSLSocket);
    return d->wrappedSocket->getConnectionStatistics(info);
}

bool QnSSLSocket::connect(
                     const QString& foreignAddress,
                     unsigned short foreignPort,
                     unsigned int timeoutMillis)
{
    Q_D(QnSSLSocket);
    d->ecnryptionEnabled = true;
    return d->wrappedSocket->connect(foreignAddress, foreignPort, timeoutMillis);
}

SocketAddress QnSSLSocket::getForeignAddress() const
{
    Q_D(const QnSSLSocket);
    return d->wrappedSocket->getForeignAddress();
}

bool QnSSLSocket::isConnected() const
{
    Q_D(const QnSSLSocket);
    return d->wrappedSocket->isConnected();
}

bool QnSSLSocket::bind( const SocketAddress& localAddress )
{
    Q_D(const QnSSLSocket);
    return d->wrappedSocket->bind(localAddress);
}

//bool QnSSLSocket::bindToInterface( const QnInterfaceAndAddr& iface )
//{
//    Q_D(const QnSSLSocket);
//    return d->wrappedSocket->bindToInterface(iface);
//}

SocketAddress QnSSLSocket::getLocalAddress() const
{
    Q_D(const QnSSLSocket);
    return d->wrappedSocket->getLocalAddress();
}

SocketAddress QnSSLSocket::getPeerAddress() const
{
    Q_D(const QnSSLSocket);
    return d->wrappedSocket->getPeerAddress();
}

void QnSSLSocket::close()
{
    Q_D(const QnSSLSocket);
    d->wrappedSocket->close();
}

bool QnSSLSocket::isClosed() const
{
    Q_D(const QnSSLSocket);
    return d->wrappedSocket->isClosed();
}

bool QnSSLSocket::setReuseAddrFlag( bool reuseAddr )
{
    Q_D(const QnSSLSocket);
    return d->wrappedSocket->setReuseAddrFlag(reuseAddr);
}

bool QnSSLSocket::getReuseAddrFlag( bool* val )
{
    Q_D(const QnSSLSocket);
    return d->wrappedSocket->getReuseAddrFlag(val);
}

bool QnSSLSocket::setNonBlockingMode( bool val )
{
    Q_D(const QnSSLSocket);
    return d->wrappedSocket->setNonBlockingMode(val);
}

bool QnSSLSocket::getNonBlockingMode( bool* val ) const
{
    Q_D(const QnSSLSocket);
    return d->wrappedSocket->getNonBlockingMode(val);
}

bool QnSSLSocket::getMtu( unsigned int* mtuValue )
{
    Q_D(const QnSSLSocket);
    return d->wrappedSocket->getMtu(mtuValue);
}

bool QnSSLSocket::setSendBufferSize( unsigned int buffSize )
{
    Q_D(const QnSSLSocket);
    return d->wrappedSocket->setSendBufferSize(buffSize);
}

bool QnSSLSocket::getSendBufferSize( unsigned int* buffSize )
{
    Q_D(const QnSSLSocket);
    return d->wrappedSocket->getSendBufferSize(buffSize);
}

bool QnSSLSocket::setRecvBufferSize( unsigned int buffSize )
{
    Q_D(const QnSSLSocket);
    return d->wrappedSocket->setRecvBufferSize(buffSize);
}

bool QnSSLSocket::getRecvBufferSize( unsigned int* buffSize )
{
    Q_D(const QnSSLSocket);
    return d->wrappedSocket->getRecvBufferSize(buffSize);
}

bool QnSSLSocket::setRecvTimeout( unsigned int millis )
{
    Q_D(const QnSSLSocket);
    return d->wrappedSocket->setRecvTimeout(millis);
}

bool QnSSLSocket::getRecvTimeout( unsigned int* millis )
{
    Q_D(const QnSSLSocket);
    return d->wrappedSocket->getRecvTimeout(millis);
}

bool QnSSLSocket::setSendTimeout( unsigned int ms )
{
    Q_D(const QnSSLSocket);
    return d->wrappedSocket->setSendTimeout(ms);
}

bool QnSSLSocket::getSendTimeout( unsigned int* millis )
{
    Q_D(const QnSSLSocket);
    return d->wrappedSocket->getSendTimeout(millis);
}

//!Implementation of AbstractSocket::getLastError
bool QnSSLSocket::getLastError( SystemError::ErrorCode* errorCode )
{
    Q_D(const QnSSLSocket);
    return d->wrappedSocket->getLastError(errorCode);
}

AbstractSocket::SOCKET_HANDLE QnSSLSocket::handle() const
{
    Q_D(const QnSSLSocket);
    return d->wrappedSocket->handle();
}

bool QnSSLSocket::postImpl( std::function<void()>&& handler )
{
    Q_D(const QnSSLSocket);
    return d->wrappedSocket->post( std::move(handler) );
}

bool QnSSLSocket::dispatchImpl( std::function<void()>&& handler )
{
    Q_D(const QnSSLSocket);
    return d->wrappedSocket->dispatch( std::move(handler) );
}

bool QnSSLSocket::connectWithoutEncryption(
    const QString& foreignAddress,
    unsigned short foreignPort,
    unsigned int timeoutMillis )
{
    Q_D( const QnSSLSocket );
    return d->wrappedSocket->connect( foreignAddress, foreignPort, timeoutMillis );
}

bool QnSSLSocket::enableClientEncryption()
{
    Q_D( QnSSLSocket );
    d->ecnryptionEnabled = true;
    return doClientHandshake();
}

void QnSSLSocket::cancelAsyncIO( aio::EventType eventType, bool waitForRunningHandlerCompletion )
{
    Q_D( const QnSSLSocket );
    return d->wrappedSocket->cancelAsyncIO( eventType, waitForRunningHandlerCompletion );
}

bool QnSSLSocket::connectAsyncImpl( const SocketAddress& addr, std::function<void( SystemError::ErrorCode )>&& handler )
{
    Q_D( const QnSSLSocket );
    return d->wrappedSocket->connectAsync( addr, std::move(handler) );
}

bool QnSSLSocket::recvAsyncImpl( nx::Buffer* const buffer , std::function<void( SystemError::ErrorCode, std::size_t )>&& handler )
{
    Q_D(QnSSLSocket);
    return d->wrappedSocket->post(
        [this,buffer,handler]() {
            Q_D(QnSSLSocket);
            d->mode.store(QnSSLSocket::ASYNC,std::memory_order_release);
            d->async_ssl_ptr->AsyncRecv( buffer, std::function<void( SystemError::ErrorCode, std::size_t )>(handler) );
        });
}

bool QnSSLSocket::sendAsyncImpl( const nx::Buffer& buffer , std::function<void( SystemError::ErrorCode, std::size_t )>&& handler )
{
    Q_D(QnSSLSocket);
    return d->wrappedSocket->post(
        [this,&buffer,handler]() {
            Q_D(QnSSLSocket);
            d->mode.store(QnSSLSocket::ASYNC,std::memory_order_release);
            d->async_ssl_ptr->AsyncSend( buffer, std::function<void( SystemError::ErrorCode, std::size_t )>(handler) );
    });
}

int QnSSLSocket::asyncRecvInternal( void* buffer , unsigned int bufferLen ) {
    // For async operation here
    Q_D(QnSSLSocket);
    Q_ASSERT(mode() == ASYNC);
    Q_ASSERT(d->async_ssl_ptr != NULL);
    if(d->async_ssl_ptr->eof())
        return 0;
    int ret = static_cast<int>(d->async_ssl_ptr->BIORead( buffer , bufferLen ));
    return ret == 0 ? -1 : ret;
}

int QnSSLSocket::asyncSendInternal( const void* buffer , unsigned int bufferLen ) {
    Q_D(QnSSLSocket);
    Q_ASSERT(mode() == ASYNC);
    Q_ASSERT(d->async_ssl_ptr != NULL);
    return static_cast<int>(d->async_ssl_ptr->BIOWrite(buffer,bufferLen));
}

bool QnSSLSocket::registerTimerImpl( unsigned int timeoutMs, std::function<void()>&& handler ) {
    Q_D(QnSSLSocket);
    return d->wrappedSocket->registerTimer( timeoutMs, std::move(handler) );
}

int QnSSLSocket::mode() const {
    Q_D(const QnSSLSocket);
    return d->mode.load(std::memory_order_acquire);
}

// ------------------------------ QnMixedSSLSocket -------------------------------------------------------
static const int TEST_DATA_LEN = 3;
class QnMixedSSLSocketPrivate: public QnSSLSocketPrivate
{
public:
    bool initState;
    bool useSSL;

    QnMixedSSLSocketPrivate()
    :
        initState( true ),
        useSSL( false )
    {
    }
};

QnMixedSSLSocket::QnMixedSSLSocket(AbstractStreamSocket* wrappedSocket):
    QnSSLSocket(new QnMixedSSLSocketPrivate, wrappedSocket, true)
{
    Q_D(QnMixedSSLSocket);
    d->initState = true;
    d->useSSL = false;
    d->async_ssl_ptr.reset( new MixedAsyncSSL(d->ssl,d->wrappedSocket));
}

int QnMixedSSLSocket::recv( void* buffer, unsigned int bufferLen, int flags)
{
    Q_D(QnMixedSSLSocket);
    d->mode = QnSSLSocket::SYNC;
    // check for SSL pattern 0x80 (v2) or 0x16 03 (v3)
    if (d->initState) 
    {
        if (d->extraBufferLen == 0) {
            int readed = d->wrappedSocket->recv(d->extraBuffer, 1);
            if (readed < 1)
                return readed;
            d->extraBufferLen += readed;
        }

        if (d->extraBuffer[0] == 0x80)
        {
            d->useSSL = true;
            d->ecnryptionEnabled = true;
            d->initState = false;
        }
        else if (d->extraBuffer[0] == 0x16)
        {
            int readed = d->wrappedSocket->recv(d->extraBuffer+1, 1);
            if (readed < 1)
                return readed;
            d->extraBufferLen += readed;

            if( d->extraBuffer[1] == 0x03 )
            {
                d->useSSL = true;
                d->ecnryptionEnabled = true;
            }
        }
        d->initState = false;
    }

    if (d->useSSL)
        return QnSSLSocket::recv((char*) buffer, bufferLen, flags);
    else 
        return recvInternal(buffer, bufferLen, flags);
}

int QnMixedSSLSocket::send( const void* buffer, unsigned int bufferLen )
{
    Q_D(QnMixedSSLSocket);
    d->mode = QnSSLSocket::SYNC;
    if (d->useSSL)
        return QnSSLSocket::send((char*) buffer, bufferLen);
    else 
        return d->wrappedSocket->send(buffer, bufferLen);
}

//!Implementation of AbstractCommunicatingSocket::cancelAsyncIO
void QnMixedSSLSocket::cancelAsyncIO( aio::EventType eventType, bool waitForRunningHandlerCompletion )
{
    Q_D( QnMixedSSLSocket );
    if( d->useSSL )
        QnSSLSocket::cancelAsyncIO( eventType, waitForRunningHandlerCompletion );
    else
        d->wrappedSocket->cancelAsyncIO( eventType, waitForRunningHandlerCompletion );
}

bool QnMixedSSLSocket::connectAsyncImpl( const SocketAddress& addr, std::function<void( SystemError::ErrorCode )>&& handler )
{
    Q_D( QnMixedSSLSocket );
    if( d->useSSL )
        return QnSSLSocket::connectAsyncImpl( addr, std::move(handler) );
    else
        return d->wrappedSocket->connectAsync( addr, std::move( handler) );
}

//!Implementation of AbstractCommunicatingSocket::recvAsyncImpl
bool QnMixedSSLSocket::recvAsyncImpl( nx::Buffer* const buffer, std::function<void( SystemError::ErrorCode , std::size_t )>&&  handler )
{
    Q_D(QnMixedSSLSocket);
    return d->wrappedSocket->post(
        [this,buffer,handler]() {
            Q_D(QnMixedSSLSocket);
            d->mode.store(QnSSLSocket::ASYNC,std::memory_order_release);
            MixedAsyncSSL* ssl_ptr = 
                static_cast< MixedAsyncSSL* >( d->async_ssl_ptr.get() );
            if( !d->initState )
                ssl_ptr->set_ssl(d->useSSL);
            if( ssl_ptr->is_initialized() && !ssl_ptr->is_ssl() ) {
                d->wrappedSocket->readSomeAsync( buffer, handler );
            } else {
                ssl_ptr->AsyncRecv(buffer,
                    std::function<void( SystemError::ErrorCode , std::size_t )>(handler),
                    std::function<void( SystemError::ErrorCode , std::size_t )>(handler));
            }
    });
}

//!Implementation of AbstractCommunicatingSocket::sendAsyncImpl
bool QnMixedSSLSocket::sendAsyncImpl( const nx::Buffer& buffer, std::function<void( SystemError::ErrorCode , std::size_t )>&&  handler )
{
    Q_D(QnMixedSSLSocket);
    return d->wrappedSocket->post(
        [this,&buffer,handler]() {
            Q_D(QnMixedSSLSocket);
            d->mode.store(QnSSLSocket::ASYNC,std::memory_order_release);
            MixedAsyncSSL* ssl_ptr = 
                static_cast< MixedAsyncSSL* >( d->async_ssl_ptr.get() );
            if( !d->initState )
                ssl_ptr->set_ssl(d->useSSL);
            if( ssl_ptr->is_initialized() && !ssl_ptr->is_ssl() ) 
                d->wrappedSocket->sendAsync(buffer, handler );
            else {
                ssl_ptr->AsyncSend( buffer, std::function<void( SystemError::ErrorCode , std::size_t )>(handler) );
            }
    });
}

bool QnMixedSSLSocket::registerTimerImpl( unsigned int timeoutMs, std::function<void()>&& handler ) {
    Q_D(QnMixedSSLSocket);
    return d->wrappedSocket->registerTimer( timeoutMs, std::move(handler) );
}


//////////////////////////////////////////////////////////
////////////// class TCPSslServerSocket
//////////////////////////////////////////////////////////

TCPSslServerSocket::TCPSslServerSocket(bool allowNonSecureConnect)
:
    TCPServerSocket(),
    m_allowNonSecureConnect(allowNonSecureConnect)
{
    InitOpenSSLGlobalLock();
}

AbstractStreamSocket* TCPSslServerSocket::accept()
{
    AbstractStreamSocket* sock = TCPServerSocket::accept();
    if (!sock)
        return 0;

    if (m_allowNonSecureConnect)
        return new QnMixedSSLSocket(sock);

    else
        return new QnSSLSocket(sock, true);

#if 0
    // transparent accept required state machine here. doesn't implemented. Handshake implemented on first IO operations

    QnSSLSocket* sslSock = new QnSSLSocket(sock);
    if (sslSock->doServerHandshake())
        return sslSock;
    
    delete sslSock;
    return 0;
#endif
}

#endif // ENABLE_SSL
