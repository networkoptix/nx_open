#include "ssl_socket.h"

#ifdef ENABLE_SSL
#include <mutex>
#include <errno.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <utils/common/log.h>

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

// ==================================== SSL Async Socket Implementation ====================================
namespace {

struct buffer_t {
    const nx::Buffer* read_buffer;
    nx::Buffer* write_buffer;
    buffer_t(): read_buffer(NULL),write_buffer(NULL){}
    buffer_t( const nx::Buffer* rb , nx::Buffer* wb ) :
        read_buffer(rb),write_buffer(wb){}
};

class async_ssl;
class mixed_async_ssl;
class ssl_async_recv_op;
class ssl_async_send_op;
class ssl_async_handshake_op;

class ssl_async_op {
public:
    virtual void init() = 0;
    virtual int perform( int* ssl_err ) = 0;
    virtual void notify( SystemError::ErrorCode ) =0;
    virtual void error( SystemError::ErrorCode ec ) = 0;
    virtual void end_of_stream( SystemError::ErrorCode ) = 0;
    ssl_async_op( SSL* ssl ) : ssl_(ssl) {}
    SSL* ssl() const {
        return ssl_;
    }
    // no need for virtual dtor, we delete from original pointer
private:
    SSL* ssl_;
};

// Notes for async operation for SSL.
// The assumption :
// 1) Although we have 2 version of recv/send, user will only use
// one of them , either async or sync. So a blocking API call with
// a none blocking call is misuse and undefined !
// 2) The SSL_read/write will issue BIO read/write internally, we
// need to take care of it. It means after each SSL_read/write we
// may also issue a async read/write for the renegotiation. This
// should be properly handled. But the thing is the async read
// and write will only happened after one completion. I hope so here.
// In theory, as long as we have a well behavior reactor, we have no
// trouble.
// 3) The implementation is simple and tight. But I have to add buffer
// here since the read/write is issued from different place : 1) User
// issue an async operation by API of QnSSLSocket ; 2) SSL issue async
// operation internally from BIO interface. But we don't want it work
// like that. So I internally add a buffer to buffer all the data that
// request from SSL itself. 
// 4) Concurrent issue async operation leads to undefined behavior. So
// lucky, no lock is here :)

static const std::size_t bio_input_buffer_reserve = 1024;
static const std::size_t bio_output_buffer_reserve= 2048;
static const std::size_t async_buffer_default_size= 1024;
static const SystemError::ErrorCode ssl_internal_error = 100;

class async_ssl {
public:
    async_ssl( SSL* ssl , bool server , AbstractStreamSocket* socket ) ;

    // This function is invoked by the BIO function
    int bio_write( const void* buffer , std::size_t len ) {
        bio_out_buffer_.append( 
            static_cast<const char*>(buffer),static_cast<int>(len));
        return static_cast<int>(len);
    }

    int bio_read ( void* buffer , std::size_t len ) {
        int digest_size = static_cast<int>(std::min(len, bio_in_buffer_.size() - recv_buffer_used_pos_));
        if( digest_size == 0 )
            return 0;
        memcpy( buffer , 
            bio_in_buffer_.constData() + recv_buffer_used_pos_ , digest_size );
        recv_buffer_used_pos_ += digest_size;
        if( recv_buffer_used_pos_ == bio_in_buffer_.size() ) {
            recv_buffer_used_pos_ = 0;
            bio_in_buffer_.clear();
        }
        return digest_size;
    }

    // Interface that will be used by QnSSLSocket there
    void async_recv( nx::Buffer* buffer, const std::function<void(SystemError::ErrorCode,std::size_t)>& op ) ;
    void async_send( const nx::Buffer& buffer, const std::function<void(SystemError::ErrorCode,std::size_t)>& op );

    bool shutdown() const {
        return ssl_shutdown_ ;
    }

    bool eof() const {
        return eof_;
    }

    const nx::Buffer& bio_input_buffer() const {
        return bio_in_buffer_;
    }
    
protected:

    nx::Buffer* mutable_bio_input_buffer() const {
        return &bio_in_buffer_;
    }

    AbstractStreamSocket* socket() const {
        return socket_;
    }

private:
    static const char* ssl_error_str( int ssl_error ) {
        switch(ssl_error) {
        case SSL_ERROR_SSL: 
            return "SSL_ERROR_SSL";
        case SSL_ERROR_SYSCALL:
            return "SSL_ERROR_SYSCALL";
        default:
            return "<ssl not a real error>";
        }
    }
    // SSL typically spew wired error definition. It is very hard to
    // handle the error throw by the SSL itself. For those error that
    // we cannot understand, we just record them and hopefully later 
    // we will take advantage of these errors.

    void handle_error( int ssl_error ) {
        // Record the SSL error here
        NX_LOG(
            lit("SSL error code:%1\n").arg(QLatin1String(ssl_error_str(ssl_error))),
            cl_logDEBUG1 );
        // Dump the error stack here
        int err;
        while( (err = ERR_get_error()) != 0 ) {
            char err_str[1024];
            ERR_error_string_n(err,err_str,1024);
            NX_LOG(
                lit("SSL error stack:%1\n").arg(QLatin1String(err_str)),cl_logDEBUG1);
        }
        // System error string
        NX_LOG(
            lit("System error code:%1\n").arg(SystemError::getLastOSErrorCode()),cl_logDEBUG1);
    }

    // The SSL can be terminated without notifying the peer. This can be detected by:
    // 1. SSL_read return 0
    // 2. SSL_get_error return SSL_ERROR_SYSCALL
    // 3. ERR_get_error return 0
    // This situation do happens in our code base based on my debugging. The thing is
    // the original blocking version does not call SSL_shutdown before calling the 
    // socket->close. But I will detect it as an error . Anyway, I will handle the eof

    bool check_shutdown( int ssl_error ) {
        if( SSL_get_shutdown(ssl_) == SSL_RECEIVED_SHUTDOWN || 
            ssl_error == SSL_ERROR_ZERO_RETURN ) {
                // This should be the normal shutdown which means the
                // peer at least call SSL_shutdown once
                ssl_shutdown_ = true;
                return true;
        } else if( ssl_error == SSL_ERROR_SYSCALL ) {
            // Brute shutdown for SSL connection
            if( ERR_get_error() == 0 ) {
                ssl_shutdown_ = true;
                return true;
            }
        }
        return eof_;
    }

    // asynchronous handshake for SSL socket
    void async_handshake( const std::function<void(SystemError::ErrorCode)>& op );

    // using this function to issue an async read , it accepts 
    // an ssl_perform entry routine and do the job once the task
    // finished there .
    void do_recv( buffer_t buf , std::function<void(SystemError::ErrorCode,std::size_t)>&& op ) {
        socket_->readSomeAsync(buf.write_buffer,op);
    }
    void do_recv( buffer_t buf , const std::function<void(SystemError::ErrorCode,std::size_t)>& op ) {
        socket_->readSomeAsync(buf.write_buffer,op);
    }
    void do_send( buffer_t buf , std::function<void(SystemError::ErrorCode,std::size_t)>&& op ) {
        socket_->sendAsync(*buf.read_buffer,op);
    }
    void do_send( buffer_t buf , const std::function<void(SystemError::ErrorCode,std::size_t)>& op ) {
        socket_->sendAsync(*buf.read_buffer,op);
    }

    // the main entry for our async routine 
    void ssl_perform( SystemError::ErrorCode ec , std::size_t bytes_transferred , 
                      ssl_async_op *op, buffer_t buffer ) {
        Q_UNUSED(bytes_transferred);
        Q_ASSERT(!ec);
        
        int ssl_error;
        // perform SSL operation and get the result
        int bytes = op->perform( &ssl_error );
        // checking the SSL stream is shutdown or not 
        if( bytes == 0 && check_shutdown(ssl_error) ) {
            ssl_finalize(op,buffer,true);
            return;
        }
        // check SSL_error and then perform the task
        switch( ssl_error ) {
        case SSL_ERROR_NONE: 
            // We are done, so we finalize the operation here
            ssl_finalize(op,buffer,false);
            return;
        case SSL_ERROR_WANT_READ:
            // Feed the ssl with async operation and push the op to the completion
            // handler once finish the feed operation in remote thread 
            ssl_recv(op,buffer);
            return;
        case SSL_ERROR_WANT_WRITE:
            // This is simple since it means that SSL just overflow the underlying
            ssl_send(op,buffer);
            return;
        default: 
            // Since I am not 100% sure the underlying behavior of SSL. Here we
            // will meet some unexceptional error and I want to log them and also
            // even with a valid error. We have no way to notify the user that  
            // there is a SSL error happened. Because the SystemError::ErrorCode 
            // doesn't comes with SSL error code. We may need to fix them or add
            // some there. Anyway, at least some verbose logging is needed here .
            handle_error( ssl_error );
            // Call user's callback function and passing a fake ssl_internal_error
            // number to let user get notification that we are in trouble here !
            op->error( ssl_internal_error );
            return;
        }
    }

    // The reason why I need read after write is that we have potential deadlock
    // with remote socket for SSL. Suppose a renegotiation happened, and the socket
    // want to send something and then get reply , so indeed is that if the sending
    // content will not reach the peer, no reply will generate. However we cache all
    // the SSL output, so it make SSL assumes that SSL itself have already sent the
    // data while we don't get any SSL_ERROR as notification here. So for safety, every
    // async read that is issued by SSL_ERROR_WANT_READ must check the output buffer for
    // SSL and if there does exist data , we need to perform the send as well. But in 
    // order to avoid undefined behavior for multiple send/recv , we need to chain the
    // operation here . 

    void ssl_recv( ssl_async_op *op, buffer_t buffer ) {
            // We issue the read and write at same time if we needed
            if( bio_out_buffer_.isEmpty() ) {
                // Here we just issue a recv operation since the bio_out_buffer is empty
                // right now. So SSL doesn't produce renegotiation during the transaction
                do_recv( buffer, [this,buffer,op](SystemError::ErrorCode ec,std::size_t bytes_transferred){
                    if( ec ) {
                        op->error(ec);
                        return;
                    }
                    // Checking for EOF 
                    if( bytes_transferred == 0 ) {
                        // EOF stream here, this marking is at once
                        // however, the SSL will find out only when 
                        // the buffered data has been consumed 
                        eof_ = true;
                    } else {
                        Q_ASSERT(bytes_transferred == (std::size_t)buffer.write_buffer->size());
                        // append the buffer that has been written into 
                        // to the bio_in_buffer_ and later on SSL will use
                        bio_in_buffer_.append(*buffer.write_buffer);
                        buffer.write_buffer->resize(0);
                    }
                    // Then we need to call our self here 
                    ssl_perform(ec,bytes_transferred,op,buffer);
                });
            } else {

                // We need to output the BIO buffer and ONLY after that send operation finished
                // we can issue a recv operation. That's what I called chaining the operation
                // And another thing is, SSL is a state machine, so if I doesn't feed the SSL
                // buffer , it will continue returning SSL_ERROR_WANT_READ . So after a send we
                // issue another ssl_perform will work out .
                do_send( buffer, [this,buffer,op](SystemError::ErrorCode ec,std::size_t bytes_transferred){
                    if( ec ) {
                        op->error(ec);
                        return;
                    }
                    Q_ASSERT( bytes_transferred == (std::size_t)buffer.read_buffer->size() && 
                              buffer.read_buffer == &bio_out_buffer_  );
                    // since we have already sent the buffer there
                    bio_out_buffer_.resize(0); 
                    // This ssl_perform will cause SSL_ERROR_WANT_READ again
                    // and then it goes into the ssl_perform_read_and_write with
                    // an empty bio_out_buffer_, so it will go to another branch
                    ssl_perform(ec,bytes_transferred,op,buffer);
                });
            }
    }

    void ssl_send( ssl_async_op *op, buffer_t buffer ) {
        do_send( buffer , [this,buffer,op](SystemError::ErrorCode ec,std::size_t bytes_transferred) {
            if( ec ) {
                op->error(ec);
                return;
            }
            Q_ASSERT( bytes_transferred == (std::size_t)buffer.read_buffer->size() && 
                      buffer.read_buffer == &bio_out_buffer_ );
            // since we have already sent the buffer there
            bio_out_buffer_.resize(0); 
            // we may want to extend more buffer for SSL
            bio_out_buffer_.reserve( 2*bio_out_buffer_.capacity() );
            // Then we need to call our self here 
            ssl_perform(ec,bytes_transferred,op,buffer);
        });
    }

    void ssl_finalize( ssl_async_op* op, buffer_t buffer , bool eof ) {
        if( !bio_out_buffer_.isEmpty() ) {
            do_send( 
            buffer,
            [this,op,eof](SystemError::ErrorCode ec,std::size_t ) {
                // Until now , we should have been sent the data out and
                // what we need to do is just calling the callback function
                if( ec ) {
                    op->error(ec);
                    return;
                }
                // Clear the buffer here
                bio_out_buffer_.resize(0);
                // Invoke the callback, the final output has been stored inside
                // of user data instead of buffer_t object. This is transient data
                // object and we should not own this buffer object in any sense
                if(eof) {
                    op->end_of_stream(SystemError::noError);
                } else {
                    op->notify(SystemError::noError);
                }
            });
        } else {
            if(eof) {
                op->end_of_stream(SystemError::noError);
            } else {
                op->notify(SystemError::noError);
            }
        }
    }

    buffer_t make_read_write_buffer( std::size_t rcap, std::size_t wcap ) const {
        recv_buffer_.resize(0);
        recv_buffer_.reserve( static_cast<int>(
            std::max(wcap,async_buffer_default_size)) );
        bio_out_buffer_.reserve(static_cast<int>(rcap));
        // The bio_out_buffer_ is always the output buffer and recv_buffer_
        // is the buffer full of encrypted text which is not the user buffer
        return buffer_t(&bio_out_buffer_,&recv_buffer_);
    }

private:
    bool eof_;
    bool ssl_shutdown_;
    bool server_; 
    SSL* ssl_;
    AbstractStreamSocket* socket_;

    mutable nx::Buffer bio_in_buffer_;
    mutable nx::Buffer bio_out_buffer_;
    mutable nx::Buffer recv_buffer_; // This is used to store the data 
                                     // that comes from async operations
    std::size_t recv_buffer_used_pos_; // The position that recv buffer is used
    std::unique_ptr<ssl_async_recv_op> recv_op_;
    std::unique_ptr<ssl_async_send_op> send_op_;
    std::unique_ptr<ssl_async_handshake_op> handshake_op_;
};

// read operation 
class ssl_async_recv_op : public ssl_async_op {
public:
    ssl_async_recv_op( async_ssl* async_ssl , SSL* ssl ) : ssl_async_op(ssl),read_bytes_(0),async_ssl_(async_ssl){}
    virtual void init() {
        read_bytes_ = 0;
    }
    virtual int perform( int* ssl_err ) {
        // A bug here is just that I misunderstand the buffer usage.
        // The user suppose I will not touch the data that originally
        // reside inside of the buffer. I suppose this is not true !
        std::size_t old_size = user_buffer_->size();
        std::size_t target_num = user_buffer_->capacity()-old_size;
        user_buffer_->resize( user_buffer_->capacity() );
        read_bytes_ = SSL_read(ssl(),
            user_buffer_->data() + old_size ,
            static_cast<int>(target_num) );
        // Modify the buffer size if we really need to do so
        if( read_bytes_ >0 )
            user_buffer_->resize(static_cast<int>(old_size)+read_bytes_);
        else 
            user_buffer_->resize(static_cast<int>(old_size));

        *ssl_err = SSL_get_error(ssl(),read_bytes_);
        return read_bytes_;
    }
    virtual void notify( SystemError::ErrorCode ec ) {
        drain_ssl_buffer();
        callback_(ec,read_bytes_);
    }
    virtual void error( SystemError::ErrorCode ec ) {
        callback_(ec,static_cast<std::size_t>(-1));
    }
    virtual void end_of_stream( SystemError::ErrorCode ec ) {
        // For recv, the end of stream is just a normal read with a zero bytes return
        // But, we don't know how many bytes we have already read so just call notify
        // which will ensure that the bytes_transferred to be set correctly. And also
        // if user issue a recv again on this socket, it will goes directly to the notify
        // which ensure that the read_bytes_ is zero there.
        notify(ec);
    }
    void set_callback( const std::function<void(SystemError::ErrorCode,std::size_t)>& cb ) {
        callback_ = cb;
    }
    void set_user_buffer( nx::Buffer* ubuf ) {
        Q_ASSERT(ubuf->capacity() >0);
        user_buffer_ = ubuf;
    }

    // This is a revised version of drain_ssl_buffer. It uses assumption that the
    // SSL's internal record is 16KB, which means the padding data cannot be exceed
    // beyond 16KB .Otherwise it make no sense. 16KB = 1<< 14, so this allow us to 
    // drain the SSL buffer in at most 14 SSL_read . Indeed , a hand crafted binary
    // probing method will lead the average invoking times in log(14) , however it 
    // is too complicated to put here . This version is way more better than the
    // original conservative method to drain the buffer which requires at most 
    // 16KB - 1 times SSL_read invocation !
    // Original version:
    // void drain_ssl_buffer() {
    // char bytes_buffer;
    // int ssl_return;
    // do {
    //    ssl_return = SSL_read(ssl(),&bytes_buffer,1);
    //    if( ssl_return >0 ) {
    //        user_buffer_->push_back(bytes_buffer);
    //        ++read_bytes_;
    //    } else {
    //         return;
    //     }
    //  } while( true );
    // }

    void drain_ssl_buffer_component( int bit_pos ) {
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

    void drain_ssl_buffer() {
        char byte;
        while( SSL_read(ssl(),&byte,1) == 1 ) {
            // We need to push back the lead byte here
            user_buffer_->push_back(byte);
            read_bytes_ += byte;
            for( int i = 0 ; i < 14 ; ++i )
                drain_ssl_buffer_component(i);
        }
        Q_ASSERT(SSL_read(ssl(),&byte,1) <=0);
    }

private:
    std::function<void(SystemError::ErrorCode,std::size_t)> callback_;
    nx::Buffer* user_buffer_;
    int read_bytes_;
    async_ssl* async_ssl_;
};

// write operation
class ssl_async_send_op : public ssl_async_op {
public:
    ssl_async_send_op( SSL* ssl ) : ssl_async_op(ssl){}
    virtual void init() {
        send_bytes_ = 0;
    }
    virtual int perform( int* ssl_err ) {
        if( user_buffer_->isEmpty() ) {
            *ssl_err = SSL_ERROR_NONE;
            return 0;
        }
        int bytes = SSL_write( ssl() , 
            user_buffer_->constData() , static_cast<int>(user_buffer_->size()) );
        *ssl_err = SSL_get_error(ssl(),bytes);
        if( bytes > 0 ) {
            send_bytes_ += bytes;
        }
        return bytes;
    }
    virtual void notify( SystemError::ErrorCode ec ) {
        callback_(ec,user_buffer_->size());
    }
    virtual void error( SystemError::ErrorCode ec ) {
        callback_(ec,static_cast<std::size_t>(-1));
    }
    virtual void end_of_stream( SystemError::ErrorCode ec ) {
        Q_UNUSED(ec);
        // We may have already sent out the whole buffer here , so we should not issue
        // an error on send but just normally called notify 
        if( send_bytes_ == user_buffer_->size() ) {
            notify(0);
        } else {
            // This means we receive end of the stream during the middle
            // of transmission , so we haven't sent out all the data
            // and it should be an error here .
            error( ssl_internal_error );
        }
    }
    void set_callback( const std::function<void(SystemError::ErrorCode,std::size_t)>& cb ) {
        callback_ = cb;
    }
    void set_user_buffer( const nx::Buffer* ubuf ) {
        user_buffer_ = ubuf;
    }
private:
    std::function<void(SystemError::ErrorCode,std::size_t)> callback_;
    const nx::Buffer* user_buffer_;
    std::size_t send_bytes_;
};

// handshake operation
class ssl_async_handshake_op : public ssl_async_op {
public:
    ssl_async_handshake_op( SSL* ssl ) : ssl_async_op(ssl){}
    void set_model( bool server ) {
        if( server )
            SSL_set_accept_state(ssl());
        else 
            SSL_set_connect_state(ssl());
    }
    virtual void init() {}
    virtual int perform( int* ssl_err ) {
        int bytes = SSL_do_handshake(ssl());
        *ssl_err = SSL_get_error(ssl(),bytes);
        return bytes;
    }
    virtual void notify( SystemError::ErrorCode ec ) {
        callback_(ec);
    }
    virtual void error( SystemError::ErrorCode ec ) {
        callback_(ec);
    }
    virtual void end_of_stream( SystemError::ErrorCode ec ) {
        Q_UNUSED(ec);
        // For handshake, it will never receive end of stream
        // otherwise, the handshake will fail here. So we notify
        // user that we have an error during the handshake
        error( ssl_internal_error );
    }
    void set_callback( const std::function<void(SystemError::ErrorCode)>& callback ) {
        callback_ = callback;
    }
private:
    std::function<void(SystemError::ErrorCode)> callback_;
};

// async_ssl implementation
void async_ssl::async_recv(
    nx::Buffer* buffer, const std::function<void(SystemError::ErrorCode,std::size_t)>& op ) {
        if( !SSL_is_init_finished(ssl_) ) {
            if( server_ ) {
                SSL_set_accept_state(ssl_);
            } else {
                SSL_set_connect_state(ssl_);
            }
            // perform async handshake here
            async_handshake(
                [this,buffer,op]( SystemError::ErrorCode ec ) {
                    if(ec) {
                        op(ec,static_cast<std::size_t>(-1));
                        return;
                    }
                    // We delay the async_recv operation until the async handshake finished
                    async_recv(buffer,op);
                });
        } else {
            recv_op_->init();
            recv_op_->set_user_buffer(buffer);
            recv_op_->set_callback(op);
            ssl_perform(SystemError::noError,0,recv_op_.get(),
                make_read_write_buffer(async_buffer_default_size,
                buffer->capacity()));
        }
}

void async_ssl::async_send( 
    const nx::Buffer& buffer, const std::function<void(SystemError::ErrorCode,std::size_t)>& op ) {
        if( !SSL_is_init_finished(ssl_) ) {
            if( server_ ) {
                SSL_set_accept_state(ssl_);
            } else {
                SSL_set_connect_state(ssl_);
            }
            // perform async handshake here
            async_handshake(
                [this,buffer,op]( SystemError::ErrorCode ec ) {
                    if(ec) {
                        op(ec,static_cast<std::size_t>(-1));
                        return;
                    }
                    // We delay the async_recv operation until the async handshake finished
                    // We will have potential stack overflow here. Suppose a async_handshake
                    // failed. It will 
                    async_send(buffer,op);
            });
        }  else {
            send_op_->init();
            // For sending operation, we need to issue SSL operation at first
            send_op_->set_user_buffer(&buffer);
            send_op_->set_callback(op);
            buffer_t buf = make_read_write_buffer(async_buffer_default_size,buffer.size());
            // Perform the SSL operation here. The ssl_perform will issue
            // SSL operation first and based on the result to perform the
            // task. So it is safe to call this function here .
            ssl_perform(SystemError::noError,0,send_op_.get(),buf);
        }
}

void async_ssl::async_handshake( const std::function<void(SystemError::ErrorCode)>&op ) {
    Q_ASSERT(!SSL_is_init_finished(ssl_));
    buffer_t buf = make_read_write_buffer(async_buffer_default_size,
                                          async_buffer_default_size);
    handshake_op_->init();
    handshake_op_->set_callback(op);
    handshake_op_->set_model(server_);
    // Then we trigger our async handshake by using ssl_perform operation
    ssl_perform(SystemError::noError,0,handshake_op_.get(),buf);
}

async_ssl::async_ssl( SSL* ssl , bool server , AbstractStreamSocket* socket ) :
    eof_(false),
    ssl_shutdown_(false),
    server_(server),
    ssl_(ssl),
    socket_(socket),
    recv_buffer_used_pos_(0),
    recv_op_( new ssl_async_recv_op(this,ssl) ),
    send_op_( new ssl_async_send_op(ssl) ),
    handshake_op_( new ssl_async_handshake_op(ssl) ) {
        bio_in_buffer_.reserve( bio_input_buffer_reserve );
        bio_out_buffer_.reserve( bio_output_buffer_reserve );
}


// mixed ssl.
// Simply puts it, before performing any operation, the socket will
// sniffer the data of the socket and then try to figure out the type.
// For async version, it is still simple, for any ssl socket doesn't 
// figure out the point, it will try to figure out at first. And then
// it can perform the task.

// Based on the implementation of the blocking version. For this mixed
// one, if the user issue a send first, I think this will cause problem.
// Since for that send function, it directly sends the packet on the that
// socket , but at that time, user doesn't have any information for the
// remote peer. 
class mixed_async_ssl : public async_ssl {

    // Why I need this ? It is because MSVC 2012 default supports _ONLY 5_ 
    // parameters for variadic template parameter. Which in turn, make std::bind
    // in default state , only support 5 input parameter. For our sniffer if
    // I unpack the data, I need it to support 7 parameter in std::bind which
    // requires us to define a macro called _VARIADIC_MAX = 7 . This may make
    // other user feel confused. So I wrap all the data as one to make the 
    // default MSVC 2012 work .

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
    static const int sniffer_header_length = 2;

    mixed_async_ssl( SSL* ssl , AbstractStreamSocket* socket )
        :async_ssl(ssl,true,socket),
        is_initialized_(false),
        is_ssl_(false){}

    // User could not issue a async_send before a async_recv . Otherwise
    // we are in trouble since we don't know the type of the underly socking
    void async_send(
        const nx::Buffer& buffer, const std::function<void(SystemError::ErrorCode,std::size_t)>& op ) {
            // When you see this, it means you screw up since the very first call should 
            // be a async_recv instead of async_send here . 
            Q_ASSERT(is_initialized_);
            Q_ASSERT(is_ssl_);
            async_ssl::async_send(buffer,op);
    }

    void async_recv( nx::Buffer* buffer, 
        const std::function<void(SystemError::ErrorCode,std::size_t)>& ssl ,
        const std::function<void(SystemError::ErrorCode,std::size_t)>& other ) {
        if( !is_initialized_ ) {
            // We need to sniffer the buffer here
            sniffer_buffer_.reserve(sniffer_header_length);
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
             Q_ASSERT( bytes_transferred == sniffer_header_length );
             // No idea whether that socket implementation set this length
             sniffer_buffer_.resize(sniffer_header_length); 
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
    std::unique_ptr<async_ssl> async_ssl_ptr;

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

QnSSLSocket::QnSSLSocket(AbstractStreamSocket* wrappedSocket, bool isServerSide)
:
    base_type([wrappedSocket](){ return wrappedSocket; }),
    d_ptr(new QnSSLSocketPrivate())
{
    Q_D(QnSSLSocket);
    d->wrappedSocket = wrappedSocket;
    d->isServerSide = isServerSide;
    d->extraBufferLen = 0;

    init();
}

QnSSLSocket::QnSSLSocket(QnSSLSocketPrivate* priv, AbstractStreamSocket* wrappedSocket, bool isServerSide)
:
    base_type([wrappedSocket](){ return wrappedSocket; }),
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

bool QnSSLSocket::getNoDelay( bool* value ) const
{
    Q_D(const QnSSLSocket);
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
    if(d->async_ssl_ptr == NULL)
        d->async_ssl_ptr.reset( new async_ssl(d->ssl,d->isServerSide,d->wrappedSocket) );
    d->async_ssl_ptr->async_recv( buffer, handler );
    return true;
}

bool QnSSLSocket::sendAsyncImpl( const nx::Buffer& buffer , std::function<void( SystemError::ErrorCode, std::size_t )>&& handler )
{
    Q_D(QnSSLSocket);
    d->mode = QnSSLSocket::ASYNC;
    if(d->async_ssl_ptr == NULL)
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
////////////// class SSLServerSocket
//////////////////////////////////////////////////////////

SSLServerSocket::SSLServerSocket(AbstractStreamServerSocket* delegateSocket, bool allowNonSecureConnect)
:
    base_type([delegateSocket](){ return delegateSocket; }),
    m_allowNonSecureConnect( allowNonSecureConnect ),
    m_delegateSocket( delegateSocket )
{
}

bool SSLServerSocket::listen(int queueLen)
{
    return m_delegateSocket->listen(queueLen);
}

AbstractStreamSocket* SSLServerSocket::accept()
{
    AbstractStreamSocket* acceptedSock = m_delegateSocket->accept();
    if( !acceptedSock )
        return nullptr;
    if( m_allowNonSecureConnect )
        return new QnMixedSSLSocket(acceptedSock);
    else
        return new QnSSLSocket(acceptedSock, true);
}

void SSLServerSocket::cancelAsyncIO(bool waitForRunningHandlerCompletion)
{
    return m_delegateSocket->cancelAsyncIO(waitForRunningHandlerCompletion);
}

bool SSLServerSocket::acceptAsyncImpl(std::function<void(SystemError::ErrorCode, AbstractStreamSocket*)>&& handler)
{
    using namespace std::placeholders;
    m_acceptHandler = std::move(handler);
    if( !m_delegateSocket->acceptAsync(std::bind(&SSLServerSocket::connectionAccepted, this, _1, _2)) )
    {
        m_acceptHandler = std::function<void(SystemError::ErrorCode, AbstractStreamSocket*)>();
        return false;
    }
    return true;
}

void SSLServerSocket::connectionAccepted(SystemError::ErrorCode errorCode, AbstractStreamSocket* newSocket)
{
    if( newSocket )
        if( m_allowNonSecureConnect )
            newSocket = new QnMixedSSLSocket(newSocket);
        else
            newSocket = new QnSSLSocket(newSocket, true);
    auto handler = std::move(m_acceptHandler);
    handler(errorCode, newSocket);
}

#endif // ENABLE_SSL
