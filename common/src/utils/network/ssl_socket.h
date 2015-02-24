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

class QnSSLSocket : public AbstractEncryptedStreamSocket
{
public:
    QnSSLSocket(AbstractStreamSocket* wrappedSocket, bool isServerSide);
    virtual ~QnSSLSocket();

    static void initSSLEngine(const QByteArray& certData);
    static void releaseSSLEngine();

    virtual bool reopen() override;
    virtual bool setNoDelay( bool value ) override;
    virtual bool getNoDelay( bool* value ) override;
    //!Implementation of AbstractStreamSocket::toggleStatisticsCollection
    virtual bool toggleStatisticsCollection( bool val ) override;
    //!Implementation of AbstractStreamSocket::getConnectionStatistics
    virtual bool getConnectionStatistics( StreamSocketInfo* info ) override;

    virtual bool connect(
        const SocketAddress& remoteAddress,
        unsigned int timeoutMillis = DEFAULT_TIMEOUT_MILLIS ) override;
    virtual int recv( void* buffer, unsigned int bufferLen, int flags = 0 ) override;
    virtual int send( const void* buffer, unsigned int bufferLen ) override;

    virtual SocketAddress getForeignAddress() const override;
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
    //!Implementation of AbstractSocket::postImpl
    virtual bool postImpl( std::function<void()>&& handler ) override;
    //!Implementation of AbstractSocket::dispatchImpl
    virtual bool dispatchImpl( std::function<void()>&& handler ) override;
    //!Implementation of AbstractSocket::terminateAsyncIO
    virtual void terminateAsyncIO( bool waitForRunningHandlerCompletion ) override;

    //!Implementation of AbstractEncryptedStreamSocket::connectWithoutEncryption
    virtual bool connectWithoutEncryption(
        const QString& foreignAddress,
        unsigned short foreignPort,
        unsigned int timeoutMillis = DEFAULT_TIMEOUT_MILLIS ) override;
    //!Implementation of AbstractEncryptedStreamSocket::enableClientEncryption
    virtual bool enableClientEncryption() override;

    bool doServerHandshake();
    bool doClientHandshake();

    //!Implementation of AbstractCommunicatingSocket::cancelAsyncIO
    virtual void cancelAsyncIO( aio::EventType eventType, bool waitForRunningHandlerCompletion ) override;

    enum {
        ASYNC,
        SYNC
    };

protected:
    friend int sock_read(BIO *b, char *out, int outl);
    friend int sock_write(BIO *b, const char *in, int inl);

    QnSSLSocket(QnSSLSocketPrivate* priv, AbstractStreamSocket* wrappedSocket, bool isServerSide);
    int recvInternal(void* buffer, unsigned int bufferLen, int flags);
    int sendInternal( const void* buffer, unsigned int bufferLen );

    //!Implementation of AbstractCommunicatingSocket::connectAsyncImpl
    virtual bool connectAsyncImpl( const SocketAddress& addr, std::function<void( SystemError::ErrorCode )>&& handler ) override;
    //!Implementation of AbstractCommunicatingSocket::recvAsyncImpl
    virtual bool recvAsyncImpl( nx::Buffer* const buf, std::function<void( SystemError::ErrorCode, size_t )>&& handler ) override;
    //!Implementation of AbstractCommunicatingSocket::sendAsyncImpl
    virtual bool sendAsyncImpl( const nx::Buffer& buf, std::function<void( SystemError::ErrorCode, size_t )>&& handler ) override;
    //!Implementation of AbstractCommunicatingSocket::registerTimerImpl
    virtual bool registerTimerImpl( unsigned int timeoutMs, std::function<void()>&& handler ) override;

private:
    // Async version
    int asyncRecvInternal( void* buffer , unsigned int bufferLen );
    int asyncSendInternal( const void* buffer , unsigned int bufferLen );
    int mode() const;
    void init();
protected:
    Q_DECLARE_PRIVATE(QnSSLSocket);
    QnSSLSocketPrivate *d_ptr;
};

//!Can be used to accept both SSL and non-SSL connections on single port
/*!
    Auto detects whether remote side uses SSL and delegates calls to \a QnSSLSocket or to system socket
    \note Can only be used on server side for accepting connections
*/
class QnMixedSSLSocket: public QnSSLSocket
{
public:
    QnMixedSSLSocket(AbstractStreamSocket* wrappedSocket);
    virtual int recv( void* buffer, unsigned int bufferLen, int flags) override;
    virtual int send( const void* buffer, unsigned int bufferLen ) override;

    //!Implementation of AbstractCommunicatingSocket::cancelAsyncIO
    virtual void cancelAsyncIO( aio::EventType eventType, bool waitForRunningHandlerCompletion ) override;

protected:
    //!Implementation of AbstractCommunicatingSocket::connectAsyncImpl
    virtual bool connectAsyncImpl( const SocketAddress& addr, std::function<void( SystemError::ErrorCode )>&& handler ) override;
    //!Implementation of AbstractCommunicatingSocket::recvAsyncImpl
    virtual bool recvAsyncImpl( nx::Buffer* const buf, std::function<void( SystemError::ErrorCode, size_t )>&& handler ) override;
    //!Implementation of AbstractCommunicatingSocket::sendAsyncImpl
    virtual bool sendAsyncImpl( const nx::Buffer& buf, std::function<void( SystemError::ErrorCode, size_t )>&& handler ) override;
    //!Implementation of AbstractCommunicatingSocket::registerTimerImpl
    virtual bool registerTimerImpl( unsigned int timeoutMs, std::function<void()>&& handler ) override;

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
