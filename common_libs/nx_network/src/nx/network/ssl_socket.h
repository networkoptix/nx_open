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

namespace nx {
namespace network {

class QnSSLSocketPrivate;
class QnMixedSSLSocketPrivate;

class NX_NETWORK_API QnSSLSocket
:
    public AbstractSocketImplementationDelegate<AbstractEncryptedStreamSocket, std::function<AbstractStreamSocket*()>>
{
    typedef AbstractSocketImplementationDelegate<AbstractEncryptedStreamSocket, std::function<AbstractStreamSocket*()>> base_type;

public:
    QnSSLSocket(AbstractStreamSocket* wrappedSocket, bool isServerSide);
    virtual ~QnSSLSocket();

    static void initSSLEngine(const QByteArray& certData);
    static void releaseSSLEngine();

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

    //!Implementation of \a AbstractStreamSocket::setKeepAlive
    virtual bool setKeepAlive( boost::optional< KeepAliveOptions > info ) override;
    //!Implementation of \a AbstractStreamSocket::getKeepAlive
    virtual bool getKeepAlive( boost::optional< KeepAliveOptions >* result ) const override;

    //!Implementation of \a AbstractEncryptedStreamSocket::connectWithoutEncryption
    virtual bool connectWithoutEncryption(
        const QString& foreignAddress,
        unsigned short foreignPort,
        unsigned int timeoutMillis = DEFAULT_TIMEOUT_MILLIS ) override;
    //!Implementation of \a AbstractEncryptedStreamSocket::enableClientEncryption
    virtual bool enableClientEncryption() override;

    bool doServerHandshake();
    bool doClientHandshake();

    virtual void cancelIOAsync(
        nx::network::aio::EventType eventType,
        nx::utils::MoveOnlyFunc<void()> cancellationDoneHandler) override;
    virtual void cancelIOSync(nx::network::aio::EventType eventType) override;

protected:
    enum IOMode
    {
        ASYNC,
        SYNC
    };

    Q_DECLARE_PRIVATE(QnSSLSocket);
    QnSSLSocketPrivate *d_ptr;

    friend int sock_read(BIO *b, char *out, int outl);
    friend int sock_write(BIO *b, const char *in, int inl);

    QnSSLSocket(QnSSLSocketPrivate* priv, AbstractStreamSocket* wrappedSocket, bool isServerSide);
    int recvInternal(void* buffer, unsigned int bufferLen, int flags);
    int sendInternal( const void* buffer, unsigned int bufferLen );

    //!Implementation of AbstractCommunicatingSocket::connectAsync
    virtual void connectAsync(
        const SocketAddress& addr,
        nx::utils::MoveOnlyFunc<void( SystemError::ErrorCode )> handler ) override;
    //!Implementation of AbstractCommunicatingSocket::readSomeAsync
    virtual void readSomeAsync(
        nx::Buffer* const buf,
        std::function<void( SystemError::ErrorCode, size_t )> handler ) override;
    //!Implementation of AbstractCommunicatingSocket::sendAsync
    virtual void sendAsync(
        const nx::Buffer& buf,
        std::function<void( SystemError::ErrorCode, size_t )> handler ) override;
    //!Implementation of AbstractCommunicatingSocket::registerTimer
    virtual void registerTimer(
        std::chrono::milliseconds timeoutMs,
        nx::utils::MoveOnlyFunc<void()> handler ) override;

private:
    // Async version
    int asyncRecvInternal( void* buffer , unsigned int bufferLen );
    int asyncSendInternal( const void* buffer , unsigned int bufferLen );
    IOMode readMode() const;
    IOMode writeMode() const;
    void init();
};

//!Can be used to accept both SSL and non-SSL connections on single port
/*!
    Auto detects whether remote side uses SSL and delegates calls to \a QnSSLSocket or to system socket
    \note Can only be used on server side for accepting connections
*/
class NX_NETWORK_API QnMixedSSLSocket: public QnSSLSocket
{
public:
    QnMixedSSLSocket(AbstractStreamSocket* wrappedSocket);
    virtual int recv( void* buffer, unsigned int bufferLen, int flags) override;
    virtual int send( const void* buffer, unsigned int bufferLen ) override;

    virtual void cancelIOAsync(
        nx::network::aio::EventType eventType,
        nx::utils::MoveOnlyFunc<void()> cancellationDoneHandler) override;

protected:
    //!Implementation of AbstractCommunicatingSocket::connectAsync
    virtual void connectAsync(
        const SocketAddress& addr,
        nx::utils::MoveOnlyFunc<void( SystemError::ErrorCode )> handler ) override;
    //!Implementation of AbstractCommunicatingSocket::readSomeAsync
    virtual void readSomeAsync(
        nx::Buffer* const buf,
        std::function<void( SystemError::ErrorCode, size_t )> handler ) override;
    //!Implementation of AbstractCommunicatingSocket::sendAsync
    virtual void sendAsync(
        const nx::Buffer& buf,
        std::function<void( SystemError::ErrorCode, size_t )> handler ) override;
    //!Implementation of AbstractCommunicatingSocket::registerTimer
    virtual void registerTimer(
        std::chrono::milliseconds timeoutMs,
        nx::utils::MoveOnlyFunc<void()> handler ) override;

private:
    Q_DECLARE_PRIVATE(QnMixedSSLSocket);
};


class NX_NETWORK_API SSLServerSocket
:
    public AbstractSocketImplementationDelegate<AbstractStreamServerSocket, std::function<AbstractStreamServerSocket*()>>
{
    typedef AbstractSocketImplementationDelegate<AbstractStreamServerSocket, std::function<AbstractStreamServerSocket*()>> base_type;

public:
    /*!
        \param delegateSocket Ownership is passed to this class
    */
    SSLServerSocket( AbstractStreamServerSocket* delegateSocket, bool allowNonSecureConnect );

    //////////////////////////////////////////////////////////////////////
    ///////// Implementation of AbstractStreamServerSocket methods
    //////////////////////////////////////////////////////////////////////

    //!Implementation of AbstractStreamServerSocket::listen
    virtual bool listen( int queueLen ) override;
    //!Implementation of AbstractStreamServerSocket::accept
    virtual AbstractStreamSocket* accept() override;
    //!Implementation of QnStoppable::pleaseStop
    virtual void pleaseStop(nx::utils::MoveOnlyFunc< void() > handler) override;

    //!Implementation of AbstractStreamServerSocket::acceptAsync
    virtual void acceptAsync(
        nx::utils::MoveOnlyFunc<void(
            SystemError::ErrorCode,
            AbstractStreamSocket*)> handler) override;
    //!Implementation of AbstractStreamServerSocket::cancelIOAsync
    virtual void cancelIOAsync(nx::utils::MoveOnlyFunc<void()> handler) override;
    //!Implementation of AbstractStreamServerSocket::cancelIOSync
    virtual void cancelIOSync() override;

private:
    const bool m_allowNonSecureConnect;
    std::unique_ptr<AbstractStreamServerSocket> m_delegateSocket;
    nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode,
        AbstractStreamSocket*)> m_acceptHandler;

    void connectionAccepted(SystemError::ErrorCode errorCode, AbstractStreamSocket* newSocket);
};

}   //network
}   //nx

#endif // ENABLE_SSL

#endif // __SSL_SOCKET_H_
