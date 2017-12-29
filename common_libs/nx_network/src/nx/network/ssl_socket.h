#pragma once

#ifdef ENABLE_SSL

#include <openssl/ssl.h>

#include <QObject>

#include "abstract_socket.h"
#include "socket_common.h"
#include "deprecated/socket_impl_helper.h"
#include "ssl/ssl_engine.h"

// Forward
struct bio_st;
typedef struct bio_st BIO;

namespace nx {
namespace network {
namespace deprecated {

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

class NX_NETWORK_API SslSocket:
    public SslSocketImplementationDelegate
{
    typedef SslSocketImplementationDelegate base_type;

public:
    SslSocket(
        std::unique_ptr<AbstractStreamSocket> wrappedSocket,
        bool isServerSide,
        bool encriptionEnforced = false);
    virtual ~SslSocket();

    virtual bool reopen() override;
    virtual bool setNoDelay(bool value) override;
    virtual bool getNoDelay(bool* value) const override;
    virtual bool toggleStatisticsCollection(bool val) override;
    virtual bool getConnectionStatistics(StreamSocketInfo* info) override;

    virtual bool connect(
        const SocketAddress& remoteAddress,
        std::chrono::milliseconds timeout) override;

    virtual int recv(void* buffer, unsigned int bufferLen, int flags) override;
    virtual int send(const void* buffer, unsigned int bufferLen) override;

    virtual SocketAddress getForeignAddress() const override;
    virtual QString getForeignHostName() const override;
    virtual bool isConnected() const override;

    virtual bool setKeepAlive(boost::optional< KeepAliveOptions > info) override;
    virtual bool getKeepAlive(boost::optional< KeepAliveOptions >* result) const override;

    virtual void cancelIOAsync(aio::EventType eventType, utils::MoveOnlyFunc<void()> handler) override;
    virtual void cancelIOSync(nx::network::aio::EventType eventType) override;

    virtual bool setNonBlockingMode(bool val) override;
    virtual bool getNonBlockingMode(bool* val) const override;
    virtual bool shutdown() override;

    virtual void connectAsync(
        const SocketAddress& addr,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override;

    virtual void readSomeAsync(
        nx::Buffer* const buf,
        IoCompletionHandler handler) override;

    virtual void sendAsync(
        const nx::Buffer& buf,
        IoCompletionHandler handler) override;

    virtual void registerTimer(
        std::chrono::milliseconds timeoutMs,
        nx::utils::MoveOnlyFunc<void()> handler) override;

    virtual bool isEncryptionEnabled() const override;

    virtual QString idForToStringFromPtr() const override;

protected:
    Q_DECLARE_PRIVATE(SslSocket);
    SslSocketPrivate *d_ptr;

    SslSocket(
        SslSocketPrivate* priv,
        std::unique_ptr<AbstractStreamSocket> wrappedSocket,
        bool isServerSide,
        bool encriptionEnforced);

    int recvInternal(void* buffer, unsigned int bufferLen, int flags);
    int sendInternal(const void* buffer, unsigned int bufferLen);

private:
    bool m_isUnderlyingSocketInitialized = false;

    bool doHandshake();
    int asyncRecvInternal(void* buffer , unsigned int bufferLen);
    int asyncSendInternal(const void* buffer , unsigned int bufferLen);
    void initSsl();
    bool initializeUnderlyingSocketIfNeeded();

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
 *  NOTE: Can only be used on server side for accepting connections
*/
class NX_NETWORK_API MixedSslSocket:
    public SslSocket
{
public:
    MixedSslSocket(std::unique_ptr<AbstractStreamSocket> wrappedSocket);

    virtual int recv(void* buffer, unsigned int bufferLen, int flags) override;
    virtual int send(const void* buffer, unsigned int bufferLen) override;
    virtual bool setNonBlockingMode(bool val) override;

    virtual void cancelIOAsync(
        nx::network::aio::EventType eventType,
        nx::utils::MoveOnlyFunc<void()> cancellationDoneHandler) override;

    virtual void connectAsync(
        const SocketAddress& addr,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override;

    virtual void readSomeAsync(
        nx::Buffer* const buf,
        IoCompletionHandler handler) override;

    virtual void sendAsync(
        const nx::Buffer& buf,
        IoCompletionHandler handler) override;

private:
    bool updateInternalBlockingMode();

    Q_DECLARE_PRIVATE(MixedSslSocket);
};

class NX_NETWORK_API SslServerSocket:
    public SslSocketServerImplementationDelegate
{
    typedef SslSocketServerImplementationDelegate base_type;

public:
    SslServerSocket(
        std::unique_ptr<AbstractStreamServerSocket> delegateSocket,
        bool allowNonSecureConnect);

    virtual bool listen(int queueLen) override;
    virtual AbstractStreamSocket* accept() override;
    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void pleaseStopSync(bool assertIfCalledUnderLock = true) override;

    virtual void acceptAsync(AcceptCompletionHandler handler) override;

    virtual void cancelIOAsync(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void cancelIOSync() override;

private:
    const bool m_allowNonSecureConnect;
    std::unique_ptr<AbstractStreamServerSocket> m_delegateSocket;
    AcceptCompletionHandler m_acceptHandler;

    void connectionAccepted(
        SystemError::ErrorCode errorCode,
        std::unique_ptr<AbstractStreamSocket> newSocket);
};

} // namespace deprecated
} // namespace network
} // namespace nx

#endif // ENABLE_SSL
