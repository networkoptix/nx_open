#ifndef __SSL_SOCKET_H_
#define __SSL_SOCKET_H_

#ifdef ENABLE_SSL
#include <openssl/ssl.h>

#include <QObject>


#include "abstract_socket.h"
#include "socket_common.h"
#include "system_socket.h"

struct bio_st;
typedef struct bio_st BIO; /* This one is from OpenSSL, which we don't want to include in this header. */

class QnSSLSocketPrivate;
class QnMixedSSLSocketPrivate;

class QnSSLSocket: public AbstractStreamSocket
{
public:
    QnSSLSocket(AbstractStreamSocket* wrappedSocket, bool isServerSide);
    virtual ~QnSSLSocket();

    static void initSSLEngine(const QByteArray& certData);
    static void releaseSSLEngine();

    virtual bool reopen() override;
    virtual bool setNoDelay( bool value ) override;
    virtual bool getNoDelay( bool* value ) override;

    virtual bool connect(
        const QString& foreignAddress,
        unsigned short foreignPort,
        unsigned int timeoutMillis = DEFAULT_TIMEOUT_MILLIS ) override;
    virtual int recv( void* buffer, unsigned int bufferLen, int flags = 0 ) override;
    virtual int send( const void* buffer, unsigned int bufferLen ) override;

    virtual const SocketAddress getForeignAddress() override;
    virtual bool isConnected() const override;

    virtual bool bind( const SocketAddress& localAddress ) override;
    //virtual bool bindToInterface( const QnInterfaceAndAddr& iface ) override;
    virtual SocketAddress getLocalAddress() const override;
    virtual SocketAddress getPeerAddress() const override;
    virtual void close() override;
    virtual bool isClosed() const override;
    virtual bool setReuseAddrFlag( bool reuseAddr ) override;
    virtual bool getReuseAddrFlag( bool* val ) override;
    virtual bool setNonBlockingMode( bool val ) override;
    virtual bool getNonBlockingMode( bool* val ) const override;
    virtual bool getMtu( unsigned int* mtuValue ) override;
    virtual bool setSendBufferSize( unsigned int buffSize ) override;
    virtual bool getSendBufferSize( unsigned int* buffSize ) override;
    virtual bool setRecvBufferSize( unsigned int buffSize ) override;
    virtual bool getRecvBufferSize( unsigned int* buffSize ) override;
    virtual bool setRecvTimeout( unsigned int millis ) override;
    virtual bool getRecvTimeout( unsigned int* millis ) override;
    virtual bool setSendTimeout( unsigned int ms ) override;
    virtual bool getSendTimeout( unsigned int* millis ) override;
    //!Implementation of AbstractSocket::getLastError
    virtual bool getLastError(SystemError::ErrorCode* errorCode) override;
    virtual SOCKET_HANDLE handle() const override;

    bool doServerHandshake();
    bool doClientHandshake();
protected:
    friend int sock_read(BIO *b, char *out, int outl);
    friend int sock_write(BIO *b, const char *in, int inl);

    QnSSLSocket(QnSSLSocketPrivate* priv, AbstractStreamSocket* wrappedSocket, bool isServerSide);
    int recvInternal(void* buffer, unsigned int bufferLen, int flags);
    int sendInternal( const void* buffer, unsigned int bufferLen );

    //!Implementation of AbstractCommunicatingSocket::recvAsyncImpl
    virtual bool recvAsyncImpl( nx::Buffer* const buf, std::function<void( SystemError::ErrorCode, size_t )> handler ) override;
    //!Implementation of AbstractCommunicatingSocket::sendAsyncImpl
    virtual bool sendAsyncImpl( const nx::Buffer& buf, std::function<void( SystemError::ErrorCode, size_t )> handler ) override;

private:
    void init();
protected:
    Q_DECLARE_PRIVATE(QnSSLSocket);
    QnSSLSocketPrivate *d_ptr;
};

class QnMixedSSLSocket: public QnSSLSocket
{
public:
    QnMixedSSLSocket(AbstractStreamSocket* wrappedSocket);
    virtual int recv( void* buffer, unsigned int bufferLen, int flags) override;
    virtual int send( const void* buffer, unsigned int bufferLen ) override;

protected:
    //!Implementation of AbstractCommunicatingSocket::recvAsyncImpl
    virtual bool recvAsyncImpl( nx::Buffer* const buf, std::function<void( SystemError::ErrorCode, size_t )> handler ) override;
    //!Implementation of AbstractCommunicatingSocket::sendAsyncImpl
    virtual bool sendAsyncImpl( const nx::Buffer& buf, std::function<void( SystemError::ErrorCode, size_t )> handler ) override;

private:
    Q_DECLARE_PRIVATE(QnMixedSSLSocket);
};

class TCPSslServerSocket: public TCPServerSocket
{
public:
    /*
    *   allowNonSecureConnect - allow mixed ssl and non ssl connect for socket
    */
    TCPSslServerSocket(bool allowNonSecureConnect = true);

    virtual AbstractStreamSocket* accept() override;

private:
    bool m_allowNonSecureConnect;
};
#endif // ENABLE_SSL

#endif // __SSL_SOCKET_H_
