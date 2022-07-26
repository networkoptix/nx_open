// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <openssl/ssl.h>

#include <QObject>

#include <nx/network/abstract_socket.h>
#include <nx/network/socket_common.h>
#include <nx/network/socket_delegate.h>

namespace nx::network::ssl::test {

/**
 * Only synchronous mode is implemented!
 */
class SyncSslSocket:
    public StreamSocketDelegate
{
    using base_type = StreamSocketDelegate;

public:
    SyncSslSocket(
        std::unique_ptr<AbstractStreamSocket> delegate,
        bool isServerSide);
    virtual ~SyncSslSocket();

    virtual int recv(void* buffer, std::size_t bufferLen, int flags = 0) override;
    virtual int send(const void* buffer, std::size_t bufferLen) override;

    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        IoCompletionHandler handler) override;

    virtual void sendAsync(
        const nx::Buffer* buffer,
        IoCompletionHandler handler) override;

    bool performHandshake();

    SSL* ssl();

protected:
    quint8 m_extraBuffer[32];
    int m_extraBufferLen;

    friend int sock_read(BIO *b, char *out, int outl);
    friend int sock_write(BIO *b, const char *in, int inl);

    int recvInternal(void* buffer, unsigned int bufferLen, int flags);
    int sendInternal(const void* buffer, unsigned int bufferLen);

private:
    std::unique_ptr<AbstractStreamSocket> m_delegate;
    std::unique_ptr<BIO_METHOD, decltype(&BIO_meth_free)> m_bioMethods;
    std::unique_ptr<SSL, decltype(&SSL_free)> m_ssl;
    bool m_isServerSide;

    bool doServerHandshake();
    bool doClientHandshake();
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

} // namespace nx::network::ssl::test
