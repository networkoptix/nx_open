#pragma once

#include <openssl/ssl.h>

#include <QObject>

#include <nx/network/abstract_socket.h>
#include <nx/network/socket_common.h>
#include <nx/network/socket_delegate.h>

namespace nx {
namespace network {
namespace ssl {
namespace test {

/**
 * Only synchronous mode is implemented!
 */
class SyncSslSocket:
    public StreamSocketDelegate
{
    using base_type = StreamSocketDelegate;

public:
    SyncSslSocket(
        std::unique_ptr<AbstractStreamSocket> wrappedSocket,
        bool isServerSide);
    virtual ~SyncSslSocket();

    virtual int recv(void* buffer, unsigned int bufferLen, int flags = 0) override;
    virtual int send(const void* buffer, unsigned int bufferLen) override;

protected:
    friend int sock_read(BIO *b, char *out, int outl);
    friend int sock_write(BIO *b, const char *in, int inl);

    int recvInternal(void* buffer, unsigned int bufferLen, int flags);
    int sendInternal(const void* buffer, unsigned int bufferLen);

private:
    SSL* m_ssl;
    bool m_isServerSide;

    bool doServerHandshake();
    bool doClientHandshake();

protected:
    quint8 m_extraBuffer[32];
    int m_extraBufferLen;
};

class ClientSyncSslSocket:
    public SyncSslSocket
{
public:
    ClientSyncSslSocket(std::unique_ptr<AbstractStreamSocket> wrappedSocket);
};

class AcceptedSyncSslSocket:
    public SyncSslSocket
{
public:
    AcceptedSyncSslSocket(std::unique_ptr<AbstractStreamSocket> wrappedSocket);
};

//-------------------------------------------------------------------------------------------------
// QnMixedSSLSocket

#if 0

class QnMixedSSLSocket:
    public SyncSslSocket
{
public:
    QnMixedSSLSocket(std::unique_ptr<AbstractStreamSocket> wrappedSocket);

    virtual int recv(void* buffer, unsigned int bufferLen, int flags) override;
    virtual int send(const void* buffer, unsigned int bufferLen) override;

private:
    bool m_initState;
    bool m_useSSL;
};

#endif

} // namespace test
} // namespace ssl
} // namespace network
} // namespace nx
