#include "sync_ssl_socket.h"

#include <openssl/err.h>

#include <nx/network/ssl/ssl_static_data.h>

namespace nx {
namespace network {
namespace ssl {
namespace test {

const unsigned char sid[] = "Network Optix SSL socket";

int sock_read(BIO *b, char *out, int outl)
{
    SyncSslSocket* sslSock = (SyncSslSocket*) BIO_get_app_data(b);
    int ret=0;

    if (out != NULL)
    {
        //clear_socket_error();
        ret = sslSock->recvInternal(out, outl, 0);
        BIO_clear_retry_flags(b);
        if (ret <= 0)
        {
            if (BIO_sock_should_retry(ret))
                BIO_set_retry_read(b);
        }
    }
    return ret;
}

int sock_write(BIO *b, const char *in, int inl)
{
    SyncSslSocket* sslSock = (SyncSslSocket*) BIO_get_app_data(b);
    //clear_socket_error();
    int ret = sslSock->sendInternal(in, inl);
    BIO_clear_retry_flags(b);
    if (ret <= 0)
    {
        if (BIO_sock_should_retry(ret))
            BIO_set_retry_write(b);
    }
    return ret;
}

static int sock_puts(BIO* bp, const char* str)
{
    return sock_write(bp, str, (int)strlen(str));
}

static long sock_ctrl(BIO* b, int cmd, long num, void* /*ptr*/)
{
    long ret = 1;

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
            SyncSslSocket* sslSock = (SyncSslSocket*) BIO_get_app_data(a);
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

//-------------------------------------------------------------------------------------------------
// SyncSslSocket

SyncSslSocket::SyncSslSocket(
    std::unique_ptr<AbstractStreamSocket> delegatee,
    bool isServerSide)
    :
    base_type(delegatee.get()),
    m_extraBufferLen(0),
    m_delegatee(std::move(delegatee)),
    m_ssl(nullptr),
    m_isServerSide(isServerSide)
{
    BIO* readBio = BIO_new(&Proxy_server_socket);
    BIO_set_nbio(readBio, 1);
    BIO_set_app_data(readBio, this);

    BIO* writeBio = BIO_new(&Proxy_server_socket);
    BIO_set_app_data(writeBio, this);
    BIO_set_nbio(writeBio, 1);

    m_ssl = SSL_new(
        m_isServerSide
        ? SslStaticData::instance()->serverContext()
        : SslStaticData::instance()->clientContext());
    SSL_set_verify(m_ssl, SSL_VERIFY_NONE, NULL);
    SSL_set_session_id_context(m_ssl, sid, 4);
    SSL_set_bio(m_ssl, readBio, writeBio);
}

SyncSslSocket::~SyncSslSocket()
{
    if (m_ssl)
    {
        SSL_free(m_ssl);
        m_ssl = nullptr;
    }
}

int SyncSslSocket::recv(void* buffer, unsigned int bytesToRead, int flags)
{
    if (!SSL_is_init_finished(m_ssl))
    {
        if (m_isServerSide)
            doServerHandshake();
        else
            doClientHandshake();
    }

    if (flags & MSG_WAITALL)
    {
        int totalBytesRead = 0;
        for (;;)
        {
            const int bytesRead = SSL_read(m_ssl, (char*)buffer, bytesToRead);
            if (bytesRead <= 0)
                return totalBytesRead > 0 ? totalBytesRead : bytesRead;

            totalBytesRead += bytesRead;
            bytesToRead -= bytesRead;
            buffer = (char*)buffer + bytesRead;

            if (bytesToRead == 0)
                return totalBytesRead;
        }
    }
    else
    {
        return SSL_read(m_ssl, (char*) buffer, bytesToRead);
    }
}

int SyncSslSocket::send(const void* buffer, unsigned int bufferLen)
{
    if (!SSL_is_init_finished(m_ssl))
    {
        if (m_isServerSide)
            doServerHandshake();
        else
            doClientHandshake();
    }

    return SSL_write(m_ssl, buffer, bufferLen);
}

void SyncSslSocket::readSomeAsync(
    nx::Buffer* const /*buffer*/,
    IoCompletionHandler /*handler*/)
{
    NX_CRITICAL(false, "Not implemented and will never be. Use ssl::StreamSocket");
}

void SyncSslSocket::sendAsync(
    const nx::Buffer& /*buffer*/,
    IoCompletionHandler /*handler*/)
{
    NX_CRITICAL(false, "Not implemented and will never be. Use ssl::StreamSocket");
}

int SyncSslSocket::recvInternal(void* buffer, unsigned int bufferLen, int /*flags*/)
{
    if (m_extraBufferLen > 0)
    {
        int toReadLen = qMin((int)bufferLen, m_extraBufferLen);
        memcpy(buffer, m_extraBuffer, toReadLen);
        memmove(m_extraBuffer, m_extraBuffer + toReadLen, m_extraBufferLen - toReadLen);
        m_extraBufferLen -= toReadLen;
        int readRest = bufferLen - toReadLen;
        if (toReadLen > 0)
        {
            const int bytesRead = m_target->recv((char*) buffer + toReadLen, readRest);
            if (bytesRead > 0)
                toReadLen += bytesRead;
        }
        return toReadLen;
    }

    int bytesRead = m_target->recv(buffer, bufferLen);
    return bytesRead;
}

int SyncSslSocket::sendInternal(const void* buffer, unsigned int bufferLen)
{
    return m_target->send(buffer, bufferLen);
}

bool SyncSslSocket::doServerHandshake()
{
    SSL_set_accept_state(m_ssl);

    return SSL_do_handshake(m_ssl) == 1;
}

bool SyncSslSocket::doClientHandshake()
{
    SSL_set_connect_state(m_ssl);

    const int ret = SSL_do_handshake(m_ssl);
    return ret == 1;
}

ClientSyncSslSocket::ClientSyncSslSocket(
    std::unique_ptr<AbstractStreamSocket> wrappedSocket)
    :
    SyncSslSocket(std::move(wrappedSocket), false)
{
}

AcceptedSyncSslSocket::AcceptedSyncSslSocket(
    std::unique_ptr<AbstractStreamSocket> wrappedSocket)
    :
    SyncSslSocket(std::move(wrappedSocket), true)
{
}

//-------------------------------------------------------------------------------------------------
// QnMixedSSLSocket

#if 0

static const int TEST_DATA_LEN = 3;

QnMixedSSLSocket::QnMixedSSLSocket(
    std::unique_ptr<AbstractStreamSocket> wrappedSocket)
    :
    SyncSslSocket(std::move(wrappedSocket), true),
    m_initState(true),
    m_useSSL(false)
{
}

int QnMixedSSLSocket::recv(void* buffer, unsigned int bufferLen, int flags)
{
    // check for SSL pattern 0x80 (v2) or 0x16 03 (v3)
    if (m_initState)
    {
        if (m_extraBufferLen == 0)
        {
            int bytesRead = m_target->recv(m_extraBuffer, 1);
            if (bytesRead < 1)
                return bytesRead;
            m_extraBufferLen += bytesRead;
        }

        if (m_extraBuffer[0] == 0x80)
        {
            m_useSSL = true;
            m_initState = false;
        }
        else if (m_extraBuffer[0] == 0x16)
        {
            int bytesRead = m_target->recv(m_extraBuffer+1, 1);
            if (bytesRead < 1)
                return bytesRead;
            m_extraBufferLen += bytesRead;

            if (m_extraBuffer[1] == 0x03)
                m_useSSL = true;
        }
        m_initState = false;
    }

    if (m_useSSL)
        return SyncSslSocket::recv((char*) buffer, bufferLen, flags);
    else
        return recvInternal(buffer, bufferLen, flags);
}

int QnMixedSSLSocket::send(const void* buffer, unsigned int bufferLen)
{
    if (m_useSSL)
        return SyncSslSocket::send((char*) buffer, bufferLen);
    else
        return m_target->send(buffer, bufferLen);
}

#endif

} // namespace test
} // namespace ssl
} // namespace network
} // namespace nx
