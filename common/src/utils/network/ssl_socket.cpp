#include "ssl_socket.h"

#ifdef ENABLE_SSL
#include <mutex>
#include <errno.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <utils/common/log.h>
#include <list>
#include <memory>
#include <atomic>

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

class AsyncSSL;
class AsyncSSLOperation;
class AsyncSSLRead;
class AsyncSSLWrite;
class AsyncSSLHandshake;

static const SystemError::ErrorCode kSSLInternalErrorCode = 100;

class AsyncSSLOperation {
public:
    virtual void Perform( int* return_value , int* ssl_error ) = 0;
    // For each IO operations that issued with underlying socket, called
    // the pair function here.
    void IncreasePendingIOCount() {
        ++pending_io_count_;
    }
    void DecreasePendingIOCount() {
        Q_ASSERT(pending_io_count() != 0);
        --pending_io_count_;
        if( pending_io_count() == 0 ) {
            InvokeUserFinializeHandler();
        }
    }
    void DecreasePendingIOCount( int status , SystemError::ErrorCode error_code ) {
        SetExitStatus(status,error_code);
        DecreasePendingIOCount();
    }
    int pending_io_count() const {
        return pending_io_count_.load(std::memory_order_acquire);
    }
    enum {
        EXECUTING,
        FAIL,
        SUCCESS
    };
    void SetExitStatus( int status , SystemError::ErrorCode error_code ) {
        if( status() == FAIL ) {
            return;
        } else {
            status_.store(status,std::memory_order_release);
            system_error_.store(error_code,std::memory_order_release);
        }
    }
    int status() const {
        return status_.load( std::memory_order_acquire );
    }

    virtual void OnHandshakeFail( SystemError::ErrorCode ) = 0;

    AsyncSSLOperation( SSL* ssl , AsyncSSL* async_ssl ): 
        pending_io_count_(0),
        status_(EXECUTING),
        system_error_(SystemError::noError),
        ssl_(ssl),
        async_ssl_(async_ssl) {}

    ~AsyncSSLOperation() {
        Q_ASSERT(pending_io_count() == 0);
    }

protected:
    // In the DecreasePendingIOCount, once the PendingIOCount becomes zero again
    // it will call this function to invoke the user callback, this is a safe call.
    virtual void InvokeUserFinializeHandler() = 0;
    std::atomic<int> pending_io_count_;
    std::atomic<int> status_;
    std::atomic<SystemError::ErrorCode> system_error_;
    SSL* ssl_;
    AsyncSSL* async_ssl_;
};

class AsyncSSL : public std::enable_shared_from_this<AsyncSSL> {
public:
    // BIO buffer read/write function.
    int BioRead( void* data , std::size_t size ) {
        // The BIO Read operations will have 2 underlying behavior, since we need to
        // allow the certain function runs into the received buffer to get the real data.
        // Since we cannot tell SSL what we want, we can only setup a flag to do so .
        if( allow_bio_in_buffer_ ) {
            std::size_t digest_size = 
                std::min( size , static_cast<std::size_t>( bio_in_buffer_.size() - bio_in_buffer_pos_ ));
            memcpy(data, bio_in_buffer_.constData()+bio_in_buffer_pos_, digest_size);
            bio_in_buffer_pos_ += digest_size;
            if( bio_in_buffer_pos_ == bio_in_buffer_.size() ) {
                bio_in_buffer_pos_ = 0;
                bio_in_buffer_.clear();
            }
            return digest_size;
        } else {
            // This is where we have return zero and then SSL will stuck on SSL_ERROR_WANT_READ, thus
            // leading to the corresponding read operation being appended into the read_queue later on.
            return 0;
        }
    }

    int BioWrite( const void* data , std::size_t size ) {
        EnqueTempWriteQueue(data,size,current_ssl_operation_);
        return static_cast<int>(size);
    }

    AsyncSSL( SSL* ssl , bool server , AbstractStreamSocket* socket );
    ~AsyncSSL();

    bool AsyncSend( const nx::Buffer& data, std::function<void(SystemError::ErrorCode,std::size_t)> && callback );
    bool AsyncRecv( nx::Buffer* data, std::function<void(SystemError::ErrorCode,std::size_t)> && callback );

    void DrainSSLBuffer() {
        std::lock_guard<std::mutex> lock(ssl_mutex_);
        char byte;
        while( SSL_read(ssl(),&byte,1) == 1 ) {
            // We need to push back the lead byte here
            user_buffer_->push_back(byte);
            read_bytes_ += byte;
            for( int i = 0 ; i < 14 ; ++i )
                DrainSSLBufferComponent(i);
        }
        Q_ASSERT(SSL_read(ssl(),&byte,1) <=0);
    }

    void InjectSnifferData( const nx::Buffer& buffer ) {
        std::lock_guard<std::mutex> lock(ssl_mutex_);
        bio_in_buffer_.append(buffer);
    }

private: // Cancel

    void WaitForAllPendingIOFinish() {
        socket_->cancelAsyncIO();
    }

private: // Handshake

    void OnHandshakeDone( SystemError::ErrorCode ec , std::shared_ptr<AsyncSSLOperation> op , 
        std::function<void( std::shared_ptr<AsyncSSLOperation> )> callback ) {
        if( !ec ) {
            op->OnHandshakeFail(ec);
        } else {
            callback(op);
        }
    }

private: // SSL perform

    // Call the following 3 functions to start each different SSL_read/SSL_write/SSL_conn
    void PerformSSLRead( std::shared_ptr<AsyncSSLOperation>& op );
    void PerformSSLWrite( std::shared_ptr<AsyncSSLOperation>& op );
    void PerformSSLConn( std::function<void(SystemError::ErrorCode,std::size_t)>&& callback );

    void PerformSSLForRead( int*return_value , int* ssl_error , std::shared_ptr<AsyncSSLOperation>& op  );

    void PerformSSLForWrite( int*return_value , int* ssl_error , std::shared_ptr<AsyncSSLOperation>& op  );

    void CheckSSLShutdown( int return_value , int ssl_error );

    std::weak_ptr<AsyncSSL> HandlePerformSSLResult( int return_value , int ssl_error , std::shared_ptr<AsyncSSLOperation>& op );

    void HandleSSLError();

private: // Read

    void Read( std::shared_ptr<AsyncSSLOperation>& op );

    void OnRecvBufferNotEmpty( std::shared_ptr<AsyncSSLOperation>& op );

    void OnUnderlySocketReadDone( SystemError::ErrorCode ec , std::size_t transferred_bytes , std::shared_ptr<AsyncSSLOperation> op );

    void ContinueReadOperation();

    bool DoRead( std::shared_ptr<AsyncSSLOperation>& op );

    void RemoveCurrentReadIO();

private: // Write
    // The Enque/Deque needs to be called inside of the ssl_mutex_ lock and then 
    // the user could go out of the scope and call the write with return value returned
    // from DequeTempWriteQueue.

    void EnqueTempWriteQueue( const void* data , std::size_t size , std::shared_ptr<AsyncSSLOperation>& op );

    bool DequeTempWriteQueue( );

    void Write( bool can_write );

    void OnUnderlySocketWriteDone( SystemError::ErrorCode ec, std::size_t transferred_bytes , std::shared_ptr<AsyncSSLOperation> op );

    void RemoveCurrentWriteIO() ;

    void ContinueWriteOperation();

    // Do not call AsyncSSLOperation::DecreasePendingIOCount, since it will result in the deletion
    // of the object, so using this wrapper function to get the weak pointer and then you can check
    // that if such deletion is happened or not.
    std::weak_ptr<AsyncSSL> DecreaseOPPendingIOCount( int status , SystemError::ErrorCode ec , std::shared_ptr<AsyncSSLOperation>& op );
    std::weak_ptr<AsyncSSL> DecreaseOPPendingIOCount( std::shared_ptr<AsyncSSLOperation>& op );

    struct PendingWriteIO;
    bool DoWrite( PendingWriteIO* write_io );


private: // MISC

    bool IsBIOInBufferEmpty() {
        return bio_in_buffer_pos_ != bio_in_buffer_.size();
    }
    void DrainSSLBufferComponent( int bit_pos ) {
        const int size = 1 <<(bit_pos);
        int old_size = user_buffer_->size();
        user_buffer_->resize(size+old_size);
        int ret = SSL_read(ssl(),(user_buffer_->data()+old_size),size);
        if( ret >0 ) {
            user_buffer_->resize(ret+old_size);
            read_bytes_ += ret;
        } else 
            user_buffer_->resize(old_size);
    }

protected:

    AbstractStreamSocket* socket() const {
        return socket_;
    }

private:
    bool allow_bio_in_buffer_;

    nx::Buffer underly_socket_recv_buffer_;
    std::size_t bio_in_buffer_pos_;
    nx::Buffer bio_in_buffer_;

    std::mutex write_mutex_;
    std::mutex read_mutex_;
    std::mutex ssl_mutex_;
    // Pending Read 
    std::list<std::shared_ptr<AsyncSSLOperation> > pending_read_io_list_;

    // Pending Write 
    struct PendingWriteIO {
        std::shared_ptr<AsyncSSLOperation> operation;
        nx::Buffer data;
        PendingWriteIO( const void* d , std::size_t sz , std::shared_ptr<AsyncSSLOperation>& op ) :
            data(static_cast<const char*>(d),static_cast<int>(sz)),
            operation(op){}
    };

    std::list<std::unique_ptr<PendingWriteIO> > pending_write_io_list_;
    // This list is an artifact list to work around the recursive lock problem.
    std::list<std::unique_ptr<PendingWriteIO> > pending_write_io_temp_list_;
    AbstractStreamSocket* socket_;
    // This pointer will always point to the related async ssl operations
    // when the user specify any SSL_read/write operations and this pointer
    // will be wrapped inside of ssl_mutex_ so it should be valid all session.
    std::shared_ptr<AsyncSSLOperation> current_ssl_operation_;
    // EOF and SSL shutdown
    bool eof_;
    bool ssl_shutdown_;
    // SSL
    SSL* ssl_;
    // Socket type
    bool server_;
};

void AsyncSSL::Read( std::shared_ptr<AsyncSSLOperation>& op ) {
    bool can_read = false;
    std::list<std::shared_ptr<AsyncSSLOperation> > element;
    element.push_back(op);
    {
        std::list<std::shared_ptr<AsyncSSLOperation> > element;
        element.push_back(op);
        std::lock_guard<std::mutex> lock(read_mutex_);
        pending_read_io_list_.splice( pending_read_io_list_.end() , std::move(element) );
        if( pending_read_io_list_.size() == 1 )
            can_read = true;
    }
    if(can_read) {
        DoRead(op);
    }
}

void AsyncSSL::OnUnderlySocketReadDone( SystemError::ErrorCode ec , std::size_t transferred_bytes , std::shared_ptr<AsyncSSLOperation> op ) {
    if( ec ) {
        if( DecreaseOPPendingIOCount(AsyncSSLOperation::FAIL,ec,op).lock() ) {
            RemoveCurrentReadIO();
            ContinueReadOperation();
        }
        return;
    }
    if( transferred_bytes == 0 ) {
        eof_ = true;
    }
    // Inject the underly_recv_buffer_ into the bio_in_buffer_. For this operation, no
    // lock is needed. Since this operation is still in pending queue so no other send
    // will be issued until this operation is removed and made visible
    bio_in_buffer_.append( underly_socket_recv_buffer_ );
    underly_socket_recv_buffer_.clear();
    // Now we are same as RecvBufferNotEmpty status.
    OnRecvBufferNotEmpty(op);
}

void AsyncSSL::OnRecvBufferNotEmpty( std::shared_ptr<AsyncSSLOperation>& op ) {
    // This function is invoked when the read operation is pending but there're data left
    // in the read buffer area. 
    int ssl_error, return_value;
    PerformSSLForRead(&return_value,&ssl_error,op);
    std::shared_ptr<AsyncSSL> self = HandlePerformSSLResult(ssl_error,return_value,op).lock();
    if( self ) {
        RemoveCurrentReadIO();
        ContinueReadOperation();
    }
}

void AsyncSSL::ContinueReadOperation() {
    std::shared_ptr<AsyncSSLOperation> op;
    do {
        std::lock_guard<std::mutex> lock(read_mutex_);
        if( !pending_read_io_list_.empty() ) {
            op = pending_read_io_list_.front();
        } else {
            return;
        }
    } while( !DoRead(op) );
}

bool AsyncSSL::DoRead( std::shared_ptr<AsyncSSLOperation>& op ) {
    switch(op->status()) {
    case AsyncSSLOperation::FAIL:
    case AsyncSSLOperation::SUCCESS:
        if( DecreaseOPPendingIOCount(op).lock() )
            return false;
        return true;
    case AsyncSSLOperation::EXECUTING:
        {
            if( eof_ ) {
                std::shared_ptr<AsyncSSL> self = DecreaseOPPendingIOCount(
                    AsyncSSLOperation::FAIL,kSSLInternalErrorCode,op).lock();
                if( !self )
                    return true;
                else {
                    RemoveCurrentWriteIO();
                    return false;
                }
            }
            // If we have left data inside of the bio_in_buffer_ we don't
            // need to issue any write operation but just invoke the read
            if( IsBIOInBufferEmpty() ) {
                // We have extra data here, so we just issue the read locally
                OnRecvBufferNotEmpty(op);
                // Since we have issued at least one read successfully, return true;
                return true;
            }
            bool ret = socket_->readSomeAsync(
                &underly_socket_recv_buffer_,
                std::bind(
                &AsyncSSL::OnUnderlySocketReadDone,
                this,
                std::placeholders::_1,
                std::placeholders::_2,
                op));
            if(!ret) {
                if( DecreaseOPPendingIOCount(AsyncSSLOperation::FAIL,
                    SystemError::getLastOSErrorCode(),op).lock() ) {
                        return false;
                }
                return true;
            } else {
                return true;
            }
        }
    default: Q_ASSERT(0); return false;
    }
}

void AsyncSSL::EnqueTempWriteQueue( const void* data , std::size_t size , std::shared_ptr<AsyncSSLOperation>& op ) {
    // Enqueue the data into the write buffer queue. This operation will not check
    // any pending write IO operations, but instead just insert the data into the
    // queue. This is OK since we return 0 for user operations. And that means the
    // user's operations will be STUCK in state SSL_ERROR_WANT_WRITE. Later the callback
    // invoke we can call the corresponding callback function to recover the SSL states
    std::list<std::unique_ptr<PendingWriteIO> > element;
    element.push_back( std::unique_ptr<PendingWriteIO>( new PendingWriteIO(data,size,op) ) );
    pending_write_io_temp_list_.splice( pending_write_io_list_.end(), std::move(element));
    pending_write_io_temp_list_.front()->operation->IncreasePendingIOCount();
}

bool AsyncSSL::DequeTempWriteQueue() {
    bool can_write = false;
    std::lock_guard<std::mutex> lock(write_mutex_);
    if( pending_write_io_list_.empty() && !pending_write_io_temp_list_.empty() ) {
        can_write = true;
    }
    pending_write_io_list_.splice(
        pending_write_io_list_.end(),std::move(pending_write_io_temp_list_));
    pending_write_io_temp_list_.clear();
    return can_write;
}

void AsyncSSL::Write( bool can_write ) {
    if( can_write ) {
        PendingWriteIO* write_io;
        do {
            std::lock_guard<std::mutex> lock(write_mutex_);
            write_io = pending_write_io_list_.front().get();
        } while(!DoWrite(write_io));
    }
}

void AsyncSSL::OnUnderlySocketWriteDone( SystemError::ErrorCode ec, std::size_t transferred_bytes , std::shared_ptr<AsyncSSLOperation> op ) {
    if( ec ) {
        if( DecreaseOPPendingIOCount(AsyncSSLOperation::FAIL,ec,op).lock() ) {
            RemoveCurrentWriteIO();
            ContinueWriteOperation();
        }
        return;
    }
    int ssl_error;
    int return_value;
    // Perform SSL operations 
    PerformSSLForWrite(&return_value,&ssl_error,op);
    std::shared_ptr<AsyncSSL> self = HandlePerformSSLResult(return_value,ssl_error,op).lock();
    if( self ) {
        // Remove the current IO operations
        RemoveCurrentWriteIO();
        // Now we need to check the next buffered data inside of the write queue
        ContinueWriteOperation();
    }
}

void AsyncSSL::RemoveCurrentWriteIO() {
    std::lock_guard<std::mutex> lock(write_mutex_);
    pending_write_io_list_.pop_front();
}

void AsyncSSL::ContinueWriteOperation() {
    PendingWriteIO* write_io;
    do {
        // Dequeue the pending write IO from the list 
        std::lock_guard<std::mutex> lock(write_mutex_);
        // Pop the current IO operations
        if( !pending_write_io_list_.empty() ) {
            write_io = pending_write_io_list_.front().get();
        } else {
            return;
        }
    } while( !DoWrite(write_io) ) ;
}

// Do not call AsyncSSLOperation::DecreasePendingIOCount, since it will result in the deletion
// of the object, so using this wrapper function to get the weak pointer and then you can check
// that if such deletion is happened or not.

std::weak_ptr<AsyncSSL> AsyncSSL::DecreaseOPPendingIOCount( int status , SystemError::ErrorCode ec , std::shared_ptr<AsyncSSLOperation>& op ) {
    std::weak_ptr<AsyncSSL> ret(shared_from_this());
    op->DecreasePendingIOCount(status,ec);
    return ret;
}

std::weak_ptr<AsyncSSL> AsyncSSL::DecreaseOPPendingIOCount( std::shared_ptr<AsyncSSLOperation>& op ) {
    std::weak_ptr<AsyncSSL> ret(shared_from_this());
    op->DecreasePendingIOCount();
    return ret;
}

bool AsyncSSL::DoWrite( PendingWriteIO* write_io ) {
    switch( write_io->operation->status() ) {
    case AsyncSSLOperation::EXECUTING:
    case AsyncSSLOperation::SUCCESS:
        if( eof_ ) {
            std::shared_ptr<AsyncSSL> self = DecreaseOPPendingIOCount(
                AsyncSSLOperation::FAIL,kSSLInternalErrorCode,write_io->operation).lock();
            if( !self )
                return true;
            else {
                RemoveCurrentWriteIO();
                return false;
            }
        }
        bool ret = socket_->sendAsync( write_io->data , std::bind(
            AsyncSSL::OnUnderlySocketWriteDone,
            this,
            std::placeholders::_1,
            std::placeholders::_2,
            write_io->operation));
        if(!ret) {
            std::shared_ptr<AsyncSSL> self = DecreaseOPPendingIOCount(
                AsyncSSLOperation::FAIL,
                SystemError::getLastOSErrorCode(),write_io->operation).lock();
            if( !self )
                return true;
            else {
                RemoveCurrentWriteIO();
                return false;
            }
        }
        return true;
    case AsyncSSLOperation::FAIL:
        if( DecreaseOPPendingIOCount(write_io->operation).lock() ) {
            RemoveCurrentWriteIO();
            return false;
        } else {
            return true;
        }
        break;
    default: Q_ASSERT(0); return;
    }
}

// SSL perform

void AsyncSSL::PerformSSLForRead( int*return_value , int* ssl_error , std::shared_ptr<AsyncSSLOperation>& op ) {
    bool can_write;
    {
        std::lock_guard<std::mutex> lock(ssl_mutex_);
        current_ssl_operation_ = op;
        allow_bio_in_buffer_ = true;
        op->Perform(return_value,ssl_error);
        allow_bio_in_buffer_ = false;
        CheckSSLShutdown(*return_value,*ssl_error);
        can_write = DequeTempWriteQueue();
    }
    Write(can_write);
}

void AsyncSSL::PerformSSLForWrite( int*return_value , int* ssl_error , std::shared_ptr<AsyncSSLOperation>& op ) {
    bool can_write;
    {
        std::lock_guard<std::mutex> lock(ssl_mutex_);
        current_ssl_operation_ = op;
        op->Perform(return_value,ssl_error);
        CheckSSLShutdown(*return_value,*ssl_error);
        can_write = DequeTempWriteQueue();
    }
    Write(can_write);
}

std::weak_ptr<AsyncSSL> AsyncSSL::HandlePerformSSLResult( int return_value , int ssl_error , std::shared_ptr<AsyncSSLOperation>& op ) {
    std::weak_ptr<AsyncSSL> ret;
    switch(ssl_error) {
    case SSL_ERROR_NONE:
        // This operation is finished to some extent, if we have pending write operations
        // we can detect it based on our PendingIOCount area in the AsyncSSLOperation, but
        // here we are safe the decrease the HandlePerformSSLResult operations and set the
        // status to Success. But if a Fail is already set, the Success set will be ignored
        return DecreaseOPPendingIOCount(AsyncSSLOperation::SUCCESS,0,op);
    case SSL_ERROR_WANT_READ:
        ret = DecreaseOPPendingIOCount(op);
        // Enqueue to the read queue
        return ret;
    case SSL_ERROR_WANT_WRITE:
        // This should never happen since we will always
        // issue the write either in the pending IO queue
        // or issue it directly .
        Q_ASSERT(0);
        return shared_from_this();
    default:
        HandleSSLError();
        // Since it is an exception, it can be a SSL_SHUTDOWN_ however we will always treat it
        // as an error no matter what happened. 
        return DecreaseOPPendingIOCount(AsyncSSLOperation::FAIL,kSSLInternalErrorCode,op);
    }
}

void AsyncSSL::CheckSSLShutdown( int return_value , int ssl_error ) {
    if( SSL_get_shutdown(ssl_) == SSL_RECEIVED_SHUTDOWN || 
        ssl_error == SSL_ERROR_ZERO_RETURN ) {
            // This should be the normal shutdown which means the
            // peer at least call SSL_shutdown once
            ssl_shutdown_ = true;
    } else if( ssl_error == SSL_ERROR_SYSCALL ) {
        // Brute shutdown for SSL connection
        if( ERR_get_error() == 0 ) {
            ssl_shutdown_ = true;
        }
    } else {
        ssl_shutdown_ = eof_ ;
    }
}

void AsyncSSL::HandleSSLError() {
    std::lock_guard<std::mutex> lock(ssl_mutex_);
    NX_LOG(
        lit("SSL error code:%1\n").arg(QLatin1String(ssl_error_str(ssl_error))),
        cl_logDEBUG1 );
    int err;
    while( (err = ERR_get_error()) != 0 ) {
        char err_str[1024];
        ERR_error_string_n(err,err_str,1024);
        NX_LOG(
            lit("SSL error stack:%1\n").arg(QLatin1String(err_str)),cl_logDEBUG1);
    }
    NX_LOG(
        lit("System error code:%1\n").arg(SystemError::getLastOSErrorCode()),cl_logDEBUG1);
}

// Async Operation
class AsyncSSLRead : public AsyncSSLOperation {
public:
    AsyncSSLRead( nx::Buffer* user_buffer , std::function<void(SystemError::ErrorCode,std::size_t)>&& callback ,
        SSL* ssl , AsyncSSL* async_ssl ) :
        user_buffer_(user_buffer),
        user_callback_(std::move(callback)),
        written_size_(0),
        AsyncSSLOperation(ssl,async_ssl) {}

protected:
    virtual void InvokeUserFinializeHandler() {
        switch(status()) {
        case FAIL:
            user_callback_( system_error_ , std::numeric_limits<std::size_t>::max() );
            return;
        case SUCCESS:
            async_ssl_->DrainSSLBuffer();
            user_callback_( 0 , written_size_ );
            return;
        default:
            Q_ASSERT(0); return;
        }
    }

    virtual void Perform( int* return_value , int* ssl_error );
    
    virtual void OnHandshakeFail( SystemError::ErrorCode ec ) {
        user_callback_( ec , std::numeric_limits<std::size_t>::max() );
    }

private:
    std::function<void(SystemError::ErrorCode,std::size_t)> user_callback_;
    nx::Buffer* user_buffer_;
    std::size_t written_size_;
};

void AsyncSSLRead::Perform( int* return_value , int* ssl_error ) {
    Q_ASSERT(user_buffer_->capacity() == 0);
    std::size_t old_size = user_buffer_->size();
    std::size_t empty_size= user_buffer_->capacity() - user_buffer_->size();
    Q_ASSERT( empty_size > 0 );
    user_buffer_->resize(static_cast<int>(empty_size)+user_buffer_->size());
    void* buf = user_buffer_->data() + old_size;
    int ssl_read_sz = SSL_read(ssl_,buf,empty_size);
    if( ssl_read_sz <= 0 ) {
        user_buffer_->resize(static_cast<int>(old_size));
        written_size_ += 0;
    } else {
        user_buffer_->resize(static_cast<int>(old_size)+ssl_read_sz);
        written_size_ += ssl_read_sz;
    }
    *return_value = ssl_read_sz;
    *ssl_error = SSL_get_error(ssl_,ssl_read_sz);
}

class AsyncSSLWrite : public AsyncSSLOperation {
public:
    AsyncSSLWrite( const nx::Buffer& user_buffer , std::function<void(SystemError::ErrorCode,std::size_t)>&& user_callback ,
        SSL* ssl, AsyncSSL* async_ssl ) :
    user_buffer_(user_buffer),
        user_callback_(std::move(user_callback)),
        AsyncSSLOperation(ssl,async_ssl){}

protected:
    virtual void InvokeUserFinializeHandler() {
        switch(status()) {
        case FAIL:
            user_callback_( system_error_ , std::numeric_limits<std::size_t>::max() );
            return;
        case SUCCESS:
            user_callback_( 0 , static_cast<std::size_t>(user_buffer_.size()) );
            return;
        default:
            Q_ASSERT(0); return;
        }
    }

    virtual void OnHandshakeFail( SystemError::ErrorCode ec ) {
        user_callback_( ec , std::numeric_limits<std::size_t>::max() );
    }

    virtual void Perform( int* return_value , int* ssl_error );
private:
    std::function<void(SystemError::ErrorCode,std::size_t)> user_callback_;
    const nx::Buffer& user_buffer_;
};

void AsyncSSLWrite::Perform( int* return_value , int* ssl_error ) {
    // Write the data through SSL_write operations
    Q_ASSERT(!user_buffer_.isEmpty());
    *return_value = SSL_write(ssl_,user_buffer_.constData(),user_buffer_.size());
    *ssl_error = SSL_get_error(ssl_,*return_value);
}

class AsyncSSLHandshake : public AsyncSSLOperation {
public:
    AsyncSSLHandshake( bool server , std::function<void(SystemError::ErrorCode)>&& user_callback ,
        SSL* ssl, AsyncSSL* async_ssl ) :
        user_callback_(std::move(user_callback)),
        AsyncSSLOperation(ssl,async_ssl) {
            if(server)
                SSL_set_accept_state(ssl_);
            else
                SSL_set_connect_state(ssl_);
        }

    virtual void Perform( int* return_value , int* ssl_error ) {
        *return_value = SSL_do_handshake(ssl_);
        *ssl_error = SSL_get_error(ssl_,*return_value);
    }

    virtual void InvokeUserFinializeHandler() {
        switch(status()) {
        case FAIL:
            user_callback_( system_error_ );
            return;
        case SUCCESS:
            user_callback_( 0 );
            return;
        default:
            Q_ASSERT(0); return;
        }
    }

    virtual void OnHandshakeFail( SystemError::ErrorCode ec ) {
        // This function will not be called since no manully handshake is existed
        user_callback_( SystemError::getLastOSErrorCode() );
    }

private:
    std::function<void(SystemError::ErrorCode)> user_callback_;
};

// Perform each specific SSL operations

void AsyncSSL::PerformSSLRead( std::shared_ptr<AsyncSSLOperation>& op   ) {
    int return_value,ssl_error;
    {
        // Do not call this function with SSL not initialized
        std::lock_guard<std::mutex> lock(ssl_mutex_);
        current_ssl_operation_ = op;
        op->Perform(&return_value,&ssl_error); 
        CheckSSLShutdown(return_value,ssl_error);
        DequeTempWriteQueue();
    }
    HandlePerformSSLResult(return_value,ssl_error,op);
}

void AsyncSSL::PerformSSLWrite( std::shared_ptr<AsyncSSLOperation>& op ) {
    int return_value,ssl_error;
    {
        std::lock_guard<std::mutex> lock(ssl_mutex_);
        current_ssl_operation_ = op;
        op->Perform(&return_value,&ssl_error);
        CheckSSLShutdown(return_value,ssl_error);
        DequeTempWriteQueue();
    }
    HandlePerformSSLResult(return_value,ssl_error,op);
}

void AsyncSSL::PerformSSLConn( bool server , std::function<void(SystemError::ErrorCode)>&& callback ) {
    AsyncSSLHandshake* handshake = new AsyncSSLHandshake(server,std::move(callback),ssl_,this);
    std::shared_ptr<AsyncSSLOperation> op(handshake);
    std::lock_guard<std::mutex> lock(ssl_mutex_);
    int return_value,ssl_error;
    current_ssl_operation_ = op;
    op->Perform(&return_value,&ssl_error);
    HandlePerformSSLResult(return_value,ssl_error,op);
}

bool AsyncSSL::AsyncRecv( nx::Buffer* data, std::function<void(SystemError::ErrorCode,std::size_t)> && callback ) {
    std::shared_ptr<AsyncSSLOperation> op( new AsyncSSLRead(data,std::move(callback),ssl_,this) );
    if( !SSL_is_init_finished(ssl_) ) {
        PerformSSLConn(
            server_,
            std::bind(
            &AsyncSSL::OnHandshakeDone,
            this,
            std::placeholders::_1,
            op,
            [this](std::shared_ptr<AsyncSSLOperation> op) {
                PerformSSLRead(op);
         }));
    } else {
        PerformSSLRead(op);
    }
    return true;
}

bool AsyncSSL::AsyncSend( const nx::Buffer& data, std::function<void(SystemError::ErrorCode,std::size_t)> && callback ) {
    std::shared_ptr<AsyncSSLOperation> op( new AsyncSSLWrite(data,std::move(callback),ssl_,this) );
    if( !SSL_is_init_finished(ssl_) ) {
        PerformSSLConn(
            server_,
            std::bind(
            &AsyncSSL::OnHandshakeDone,
            this,
            std::placeholders::_1,
            op,
            [this](std::shared_ptr<AsyncSSLOperation> op) {
                PerformSSLWrite(op);
            }));
    } else {
        PerformSSLWrite(op);
    }
    return true;
}


class MixedAsyncSSL : public AsyncSSL {
public:

    struct sniffer_data_t {
        std::function<void(SystemError::ErrorCode,std::size_t)> ssl_cb;
        std::function<void(SystemError::ErrorCode,std::size_t)> other_cb;
        nx::Buffer* buffer;
        sniffer_data_t( const std::function<void(SystemError::ErrorCode,std::size_t)>& ssl , 
            const std::function<void(SystemError::ErrorCode,std::size_t)>& other,
            nx::Buffer* buf ) :
        ssl_cb(ssl),
            other_cb(other),
            buffer(buf){}
    };

public:
    static const int kSnifferDataHeaderLength = 2;

    MixedAsyncSSL( SSL* ssl , AbstractStreamSocket* socket )
        :AsyncSSL(ssl,true,socket),
        is_initialized_(false),
        is_ssl_(false){}

    // User could not issue a async_send before a async_recv . Otherwise
    // we are in trouble since we don't know the type of the underly socking
    void AsyncSend(
        const nx::Buffer& buffer, const std::function<void(SystemError::ErrorCode,std::size_t)>& op ) {
            // When you see this, it means you screw up since the very first call should 
            // be a async_recv instead of async_send here . 
            Q_ASSERT(is_initialized_);
            Q_ASSERT(is_ssl_);
            AsyncSSL::AsyncSend(buffer,op);
    }

    void AsyncRecv( nx::Buffer* buffer, 
        const std::function<void(SystemError::ErrorCode,std::size_t)>& ssl ,
        const std::function<void(SystemError::ErrorCode,std::size_t)>& other ) {
            if( !is_initialized_ ) {
                // We need to sniffer the buffer here
                sniffer_buffer_.reserve(kSnifferDataHeaderLength);
                socket()->
                    readSomeAsync(
                    &sniffer_buffer_,
                    std::bind(
                    &mixed_async_ssl::sniffer,
                    this,
                    std::placeholders::_1,
                    std::placeholders::_2,
                    sniffer_data_t(ssl,other,buffer)));
                return;
            } else {
                Q_ASSERT(is_ssl_);
                async_ssl::async_recv(buffer,ssl);
            }
    }

    // When blocking version detects it is an SSL, it has to notify me
    void set_ssl( bool ssl ) {
        is_initialized_ = true;
        is_ssl_ = ssl;
    }


    bool is_initialized() const {
        return is_initialized_;
    }

    bool is_ssl() const {
        return is_ssl_;
    }

private:
    void sniffer( SystemError::ErrorCode ec , std::size_t bytes_transferred , 
        const sniffer_data_t& data ) {
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
                        &mixed_async_ssl::sniffer,
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
                Q_ASSERT( mutable_bio_input_buffer()->isEmpty() );
                mutable_bio_input_buffer()->append( sniffer_buffer_ );
                // If it is an SSL we still need to continue our async operation
                // otherwise we call the failed callback for the upper usage class
                // it should be QnMixedSSLSocket class
                if( is_ssl_ ) {
                    // request a SSL async recv
                    async_ssl::async_recv(data.buffer,data.ssl_cb);
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
    int mode;
    std::shared_ptr<async_ssl> async_ssl_ptr;

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
}

QnSSLSocket::QnSSLSocket(QnSSLSocketPrivate* priv, AbstractStreamSocket* wrappedSocket, bool isServerSide):
    d_ptr(priv)
{
    Q_D(QnSSLSocket);
    d->wrappedSocket = wrappedSocket;
    d->isServerSide = isServerSide;
    d->extraBufferLen = 0;

    init();
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

    if (d->ssl)
        SSL_free(d->ssl);
    if(d->mode == ASYNC ) {
        if(d->async_ssl_ptr)
            d->async_ssl_ptr->clear();
        d->wrappedSocket->cancelAsyncIO();
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

    d->mode = QnSSLSocket::SYNC;
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

    d->mode = QnSSLSocket::SYNC;
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
    d->mode = QnSSLSocket::ASYNC;
    if(!(d->async_ssl_ptr))
        d->async_ssl_ptr.reset( new async_ssl(d->ssl,d->isServerSide,d->wrappedSocket) );
    d->async_ssl_ptr->async_recv( buffer, handler );
    return true;
}

bool QnSSLSocket::sendAsyncImpl( const nx::Buffer& buffer , std::function<void( SystemError::ErrorCode, std::size_t )>&& handler )
{
    Q_D(QnSSLSocket);
    d->mode = QnSSLSocket::ASYNC;
    if(!(d->async_ssl_ptr))
        d->async_ssl_ptr.reset( new async_ssl(d->ssl,d->isServerSide,d->wrappedSocket) );
    d->async_ssl_ptr->async_send( buffer, handler );
    return true;
}

int QnSSLSocket::asyncRecvInternal( void* buffer , unsigned int bufferLen ) {
    // For async operation here
    Q_D(QnSSLSocket);
    Q_ASSERT(d->mode == ASYNC);
    Q_ASSERT(d->async_ssl_ptr != NULL);
    if(d->async_ssl_ptr->eof())
        return 0;
    int ret = d->async_ssl_ptr->bio_read( buffer , bufferLen );
    return ret == 0 ? -1 : ret;
}

int QnSSLSocket::asyncSendInternal( const void* buffer , unsigned int bufferLen ) {
    Q_D(QnSSLSocket);
    Q_ASSERT(d->mode == ASYNC);
    Q_ASSERT(d->async_ssl_ptr != NULL);
    return d->async_ssl_ptr->bio_write(buffer,bufferLen);
}

bool QnSSLSocket::registerTimerImpl( unsigned int timeoutMs, std::function<void()>&& handler ) {
    Q_D(QnSSLSocket);
    return d->wrappedSocket->registerTimer( timeoutMs, std::move(handler) );
}

int QnSSLSocket::mode() const {
    Q_D(const QnSSLSocket);
    return d->mode;
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
    d->mode = QnSSLSocket::ASYNC;
    if( d->async_ssl_ptr == NULL ) {
        d->async_ssl_ptr.reset( new mixed_async_ssl(d->ssl,d->wrappedSocket) );
    }
    mixed_async_ssl* ssl_ptr = 
        static_cast< mixed_async_ssl* >( d->async_ssl_ptr.get() );
    if( !d->initState )
        ssl_ptr->set_ssl(d->useSSL);
    if( ssl_ptr->is_initialized() && !ssl_ptr->is_ssl() ) {
        d->wrappedSocket->readSomeAsync( buffer, handler );
    } else {
        ssl_ptr->async_recv(buffer,handler,handler);
    }
    return true;
}

//!Implementation of AbstractCommunicatingSocket::sendAsyncImpl
bool QnMixedSSLSocket::sendAsyncImpl( const nx::Buffer& buffer, std::function<void( SystemError::ErrorCode , std::size_t )>&&  handler )
{
    Q_D(QnMixedSSLSocket);
    d->mode = QnSSLSocket::ASYNC;
    if( d->async_ssl_ptr == NULL ) {
        d->async_ssl_ptr.reset( new mixed_async_ssl(d->ssl,d->wrappedSocket) );
    }
    mixed_async_ssl* ssl_ptr = 
        static_cast< mixed_async_ssl* >( d->async_ssl_ptr.get() );
    if( !d->initState )
        ssl_ptr->set_ssl(d->useSSL);
    if( ssl_ptr->is_initialized() && !ssl_ptr->is_ssl() ) 
        d->wrappedSocket->sendAsync(buffer, handler );
    else {
        ssl_ptr->async_send( buffer, handler );
    }
    return true;
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
