#ifndef __SSL_SOCKET_H_
#define __SSL_SOCKET_H_

#ifdef ENABLE_SSL

#include <openssl/ssl.h>

#include <QObject>

#include "abstract_socket.h"
#include "socket_common.h"
#include "socket_impl_helper.h"


struct bio_st;
typedef struct bio_st BIO; /* This one is from OpenSSL, which we don't want to include in this header. */

class QnSSLSocketPrivate;
class QnMixedSSLSocketPrivate;

class QnSSLSocket
:
    public AbstractSocketImplementationDelegate<AbstractEncryptedStreamSocket, std::function<AbstractStreamSocket*()>>
{
    typedef AbstractSocketImplementationDelegate<AbstractEncryptedStreamSocket, std::function<AbstractStreamSocket*()>> base_type;

public:
    QnSSLSocket(AbstractStreamSocket* wrappedSocket, bool isServerSide);
    virtual ~QnSSLSocket();

    static void initSSLEngine(const QByteArray& certData);
    static void releaseSSLEngine();

    //!Implementation of AbstractSocket::terminateAsyncIO
    virtual void terminateAsyncIO( bool waitForRunningHandlerCompletion ) override;

    virtual bool reopen() override;
    virtual bool setNoDelay( bool value ) override;
    virtual bool getNoDelay( bool* value ) const override;
    //!Implementation of \a AbstractStreamSocket::toggleStatisticsCollection
    virtual bool toggleStatisticsCollection( bool val ) override;
    //!Implementation of \a AbstractStreamSocket::getConnectionStatistics
    virtual bool getConnectionStatistics( StreamSocketInfo* info ) override;
    //!Implementation of \a AbstractStreamSocket::connect
    virtual bool connect(
        const SocketAddress& remoteAddress,
        unsigned int timeoutMillis = DEFAULT_TIMEOUT_MILLIS ) override;
    //!Implementation of \a AbstractStreamSocket::recv
    virtual int recv(void* buffer, unsigned int bufferLen, int flags) override;
    //!Implementation of \a AbstractStreamSocket::send
    virtual int send(const void* buffer, unsigned int bufferLen) override;

    virtual SocketAddress getForeignAddress() const override;
    virtual bool isConnected() const override;

    //!Implementation of \a AbstractEncryptedStreamSocket::connectWithoutEncryption
    virtual bool connectWithoutEncryption(
        const QString& foreignAddress,
        unsigned short foreignPort,
        unsigned int timeoutMillis = DEFAULT_TIMEOUT_MILLIS ) override;
    //!Implementation of \a AbstractEncryptedStreamSocket::enableClientEncryption
    virtual bool enableClientEncryption() override;

    bool doServerHandshake();
    bool doClientHandshake();

    //!Implementation of \a AbstractCommunicatingSocket::cancelAsyncIO
    virtual void cancelAsyncIO( aio::EventType eventType, bool waitForRunningHandlerCompletion ) override;

    enum
    {
        ASYNC,
        SYNC
    };

protected:
    Q_DECLARE_PRIVATE(QnSSLSocket);
    QnSSLSocketPrivate *d_ptr;

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


class SSLServerSocket
:
    public AbstractSocketImplementationDelegate<AbstractStreamServerSocket, std::function<AbstractStreamServerSocket*()>>
{
    typedef AbstractSocketImplementationDelegate<AbstractStreamServerSocket, std::function<AbstractStreamServerSocket*()>> base_type;

public:
    /*!
        \param delegateSocket Ownership is passed to this class
    */
    SSLServerSocket( AbstractStreamServerSocket* delegateSocket, bool allowNonSecureConnect );

    //!Implementation of AbstractSocket::terminateAsyncIO
    virtual void terminateAsyncIO( bool waitForRunningHandlerCompletion ) override;

    //////////////////////////////////////////////////////////////////////
    ///////// Implementation of AbstractStreamServerSocket methods
    //////////////////////////////////////////////////////////////////////

    //!Implementation of SSLServerSocket::listen
    virtual bool listen( int queueLen ) override;
    //!Implementation of SSLServerSocket::accept
    virtual AbstractStreamSocket* accept() override;
    //!Implementation of SSLServerSocket::cancelAsyncIO
    virtual void cancelAsyncIO(bool waitForRunningHandlerCompletion = true) override;

protected:
    //!Implementation of SSLServerSocket::acceptAsyncImpl
    virtual bool acceptAsyncImpl(std::function<void(SystemError::ErrorCode, AbstractStreamSocket*)>&& handler) override;

private:
    const bool m_allowNonSecureConnect;
    std::unique_ptr<AbstractStreamServerSocket> m_delegateSocket;
    std::function<void(SystemError::ErrorCode, AbstractStreamSocket*)> m_acceptHandler;

    void connectionAccepted(SystemError::ErrorCode errorCode, AbstractStreamSocket* newSocket);
};


#endif // ENABLE_SSL

#endif // __SSL_SOCKET_H_
