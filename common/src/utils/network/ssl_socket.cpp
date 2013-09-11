#include "ssl_socket.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

static const int BUFFER_SIZE = 1024;
const unsigned char sid[] = "Network Optix SSL socket";
static SSL_CTX *sslCTX = 0;

static int sock_read(BIO *b, char *out, int outl)
{
    AbstractStreamSocket* wrappedSocket = (AbstractStreamSocket*) BIO_get_app_data(b, wrappedSocket);
    int ret=0;

    if (out != NULL)
    {
        //clear_socket_error();
        ret = wrappedSocket->recv(out, outl);
        BIO_clear_retry_flags(b);
        if (ret <= 0)
        {
            if (BIO_sock_should_retry(ret))
                BIO_set_retry_read(b);
        }
    }
    return ret;
}

static int sock_write(BIO *b, const char *in, int inl)
{
    AbstractStreamSocket* wrappedSocket = (AbstractStreamSocket*) BIO_get_app_data(b, wrappedSocket);
    //clear_socket_error();
    int ret = wrappedSocket->send(in, inl);
    BIO_clear_retry_flags(b);
    if (ret <= 0)
    {
        if (BIO_sock_should_retry(ret))
            BIO_set_retry_write(b);
    }
    return ret;
}

static int sock_puts(BIO *bp, const char *str)
{
    int n = strlen(str);
    int ret = sock_write(bp,str,n);
    return(ret);
}

static long sock_ctrl(BIO *b, int cmd, long num, void *ptr)
{
    long ret=1;
    int *ip;

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
            AbstractStreamSocket* wrappedSocket = (AbstractStreamSocket*) BIO_get_app_data(a, wrappedSocket);
            if (wrappedSocket)
                wrappedSocket->close();
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

// ---------------------------- QnSSLSocket -----------------------------------

void QnSSLSocket::initSSLEngine(const QByteArray& certData)
{
    Q_ASSERT(sslCTX == 0);

    SSL_library_init();
    OpenSSL_add_all_algorithms();   
    SSL_load_error_strings(); 
    sslCTX = SSL_CTX_new(SSLv23_server_method());

    BIO *bufio = BIO_new_mem_buf((void*) certData.data(), certData.size());
    X509 *x = PEM_read_bio_X509_AUX(bufio, NULL, sslCTX->default_passwd_callback, sslCTX->default_passwd_callback_userdata);
    SSL_CTX_use_certificate(sslCTX, x);
    BIO_free(bufio);

    bufio = BIO_new_mem_buf((void*) certData.data(), certData.size());
    EVP_PKEY *pkey = PEM_read_bio_PrivateKey(bufio, NULL, sslCTX->default_passwd_callback, sslCTX->default_passwd_callback_userdata);
    SSL_CTX_use_PrivateKey(sslCTX, pkey);
    BIO_free(bufio);

    SSL_CTX_set_options(sslCTX, SSL_OP_SINGLE_DH_USE);
    SSL_CTX_set_session_id_context(sslCTX, sid, 4);
}

void QnSSLSocket::releaseSSLEngine()
{
    if (sslCTX) {
        SSL_CTX_free(sslCTX);
        sslCTX = 0;
    }
}

class QnSSLSocketPrivate
{
public:
    AbstractStreamSocket* wrappedSocket;
    SSL* ssl;
    BIO* read;
    BIO* write;
};

QnSSLSocket::QnSSLSocket(AbstractStreamSocket* wrappedSocket):
    d_ptr(new QnSSLSocketPrivate())
{
    Q_D(QnSSLSocket);

    d->wrappedSocket = wrappedSocket;

    d->read = BIO_new(&Proxy_server_socket);
    BIO_set_nbio(d->read, 1);
    BIO_set_app_data(d->read, wrappedSocket);

    d->write = BIO_new(&Proxy_server_socket);
    BIO_set_app_data(d->write, wrappedSocket);
    BIO_set_nbio(d->write, 1);

    d->ssl = SSL_new(sslCTX);  // get new SSL state with context 
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

    return SSL_do_handshake(d->ssl) == 1;
}

int QnSSLSocket::recv( void* buffer, unsigned int bufferLen, int flags)
{
    Q_D(QnSSLSocket);
    return SSL_read(d->ssl, (char*) buffer, bufferLen);
}

int QnSSLSocket::send( const void* buffer, unsigned int bufferLen )
{
    Q_D(QnSSLSocket);
    return SSL_write(d->ssl, buffer, bufferLen);
}

bool QnSSLSocket::reopen()
{
    Q_D(QnSSLSocket);
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

bool QnSSLSocket::connect(
                     const QString& foreignAddress,
                     unsigned short foreignPort,
                     unsigned int timeoutMillis)
{
    Q_D(QnSSLSocket);
    return d->wrappedSocket->connect(foreignAddress, foreignPort, timeoutMillis);
}

const SocketAddress QnSSLSocket::getForeignAddress()
{
    Q_D(QnSSLSocket);
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

bool QnSSLSocket::bindToInterface( const QnInterfaceAndAddr& iface )
{
    Q_D(const QnSSLSocket);
    return d->wrappedSocket->bindToInterface(iface);
}

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

AbstractSocket::SOCKET_HANDLE QnSSLSocket::handle() const
{
    Q_D(const QnSSLSocket);
    return d->wrappedSocket->handle();
}
