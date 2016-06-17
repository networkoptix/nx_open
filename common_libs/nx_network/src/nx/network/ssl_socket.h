#ifndef __SSL_SOCKET_H_
#define __SSL_SOCKET_H_

#ifdef ENABLE_SSL

#include <openssl/ssl.h>

#include <QObject>

#include "abstract_socket.h"
#include "socket_common.h"
#include "socket_impl_helper.h"

// Forward
struct bio_st;
typedef struct bio_st BIO;

namespace nx {
namespace network {

class SslSocketPrivate;
class MixedSslSocketPrivate;

typedef AbstractSocketImplementationDelegate<
        AbstractEncryptedStreamSocket,
        std::function<AbstractStreamSocket*()>
    > SslSocketImplementationDelegate;

typedef AbstractSocketImplementationDelegate<
        AbstractStreamServerSocket,
        std::function<AbstractStreamServerSocket*()>
    > SslSocketServerImplementationDelegate;

class NX_NETWORK_API SslEngine
{
    static const size_t kBufferSize;
    static const int kRsaLength;
    static const std::chrono::seconds kCertExpiration;

public:
    static String makeCertificateAndKey(
        const String& name, const String& country, const String& company);

    static bool useCertificateAndPkey(const String& certData);
};

class NX_NETWORK_API SslSocket
:
    public SslSocketImplementationDelegate
{
    typedef SslSocketImplementationDelegate base_type;

public:
    SslSocket(
        AbstractStreamSocket* wrappedSocket,
        bool isServerSide, bool encriptionEnforced = false);
    virtual ~SslSocket();

    virtual bool reopen() override;
    virtual bool setNoDelay(bool value) override;
    virtual bool getNoDelay(bool* value) const override;
    virtual bool toggleStatisticsCollection(bool val) override;
    virtual bool getConnectionStatistics(StreamSocketInfo* info) override;

    virtual bool connect(
        const SocketAddress& remoteAddress,
        unsigned int timeoutMillis = kDefaultTimeoutMillis) override;
    virtual int recv(void* buffer, unsigned int bufferLen, int flags) override;
    virtual int send(const void* buffer, unsigned int bufferLen) override;

    virtual SocketAddress getForeignAddress() const override;
    virtual bool isConnected() const override;

    virtual bool setKeepAlive(boost::optional< KeepAliveOptions > info) override;
    virtual bool getKeepAlive(boost::optional< KeepAliveOptions >* result) const override;

    virtual bool connectWithoutEncryption(
        const QString& foreignAddress,
        unsigned short foreignPort,
        unsigned int timeoutMillis = kDefaultTimeoutMillis) override;
    virtual bool enableClientEncryption() override;

    virtual void cancelIOAsync(
        nx::network::aio::EventType eventType,
        nx::utils::MoveOnlyFunc<void()> cancellationDoneHandler) override;
    virtual void cancelIOSync(nx::network::aio::EventType eventType) override;

    virtual bool setNonBlockingMode(bool val) override;
    virtual bool getNonBlockingMode(bool* val) const override;
    virtual bool shutdown() override;

    enum IOMode { ASYNC, SYNC };

protected:
    Q_DECLARE_PRIVATE(SslSocket);
    SslSocketPrivate *d_ptr;

    SslSocket(
        SslSocketPrivate* priv, AbstractStreamSocket* wrappedSocket,
        bool isServerSide, bool encriptionEnforced);

    int recvInternal(void* buffer, unsigned int bufferLen, int flags);
    int sendInternal(const void* buffer, unsigned int bufferLen);

public:
    virtual void connectAsync(
        const SocketAddress& addr,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override;

    virtual void readSomeAsync(
        nx::Buffer* const buf,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;

    virtual void sendAsync(
        const nx::Buffer& buf,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;

    virtual void registerTimer(
        std::chrono::milliseconds timeoutMs,
        nx::utils::MoveOnlyFunc<void()> handler) override;

private:
    bool doHandshake();
    int asyncRecvInternal(void* buffer , unsigned int bufferLen);
    int asyncSendInternal(const void* buffer , unsigned int bufferLen);
    IOMode ioMode() const;
    void init();

    static int bioRead(BIO* bio, char* out, int outl);
    static int bioWrite(BIO* bio, const char* in, int inl);
    static int bioPuts(BIO* bio, const char* str);
    static long bioCtrl(BIO* bio, int cmd, long num, void* ptr);
    static int bioNew(BIO* bio);
    static int bioFree(BIO* bio);
};

/**
 *  Can be used to accept both SSL and non-SSL connections on single port
 *
 *  Auto detects whether remote side uses SSL and delegates calls to SslSocket
 *      or to system socket directly.
 *  @note Can only be used on server side for accepting connections
*/
class NX_NETWORK_API MixedSslSocket: public SslSocket
{
public:
    MixedSslSocket(AbstractStreamSocket* wrappedSocket);
    virtual int recv(void* buffer, unsigned int bufferLen, int flags) override;
    virtual int send(const void* buffer, unsigned int bufferLen) override;

    virtual void cancelIOAsync(
        nx::network::aio::EventType eventType,
        nx::utils::MoveOnlyFunc<void()> cancellationDoneHandler) override;

    virtual void connectAsync(
        const SocketAddress& addr,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override;

    virtual void readSomeAsync(
        nx::Buffer* const buf,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;

    virtual void sendAsync(
        const nx::Buffer& buf,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;

private:
    Q_DECLARE_PRIVATE(MixedSslSocket);
};

class NX_NETWORK_API SslServerSocket
:
    public SslSocketServerImplementationDelegate
{
    typedef SslSocketServerImplementationDelegate base_type;

public:
    SslServerSocket(
        AbstractStreamServerSocket* delegateSocket,
        bool allowNonSecureConnect);

    virtual bool listen(int queueLen) override;
    virtual AbstractStreamSocket* accept() override;
    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

    virtual void acceptAsync(
        nx::utils::MoveOnlyFunc<void(
            SystemError::ErrorCode,
            AbstractStreamSocket*)> handler) override;

    virtual void cancelIOAsync(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void cancelIOSync() override;

private:
    const bool m_allowNonSecureConnect;
    std::unique_ptr<AbstractStreamServerSocket> m_delegateSocket;
    nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode,
        AbstractStreamSocket*)> m_acceptHandler;

    void connectionAccepted(
            SystemError::ErrorCode errorCode,
            AbstractStreamSocket* newSocket);
};

} // namespace network
} // namespace nx

#endif // ENABLE_SSL

#endif // __SSL_SOCKET_H_
