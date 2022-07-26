// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "sync_ssl_socket.h"

#include <openssl/err.h>

#include <nx/network/ssl/context.h>
#include <nx/utils/type_utils.h>

namespace nx::network::ssl::test {

const unsigned char sid[] = "Test SSL socket";

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
        ret = BIO_get_shutdown(b);
        break;
    case BIO_CTRL_SET_CLOSE:
        BIO_set_shutdown(b, (int) num);
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

static int sock_new(BIO *bio)
{
    BIO_set_init(bio, 1);
    BIO_set_data(bio, nullptr);
    BIO_clear_flags(bio, ~0);
    return 1;
}

static int sock_free(BIO *bio)
{
    if (!bio)
        return 0;

    if (BIO_get_shutdown(bio))
    {
        if (BIO_get_init(bio))
        {
            SyncSslSocket* sslSock = (SyncSslSocket*) BIO_get_app_data(bio);
            if (sslSock)
                sslSock->close();
        }

        BIO_set_init(bio, 0);
        BIO_clear_flags(bio, ~0);
    }
    return 1;
}

static std::unique_ptr<BIO_METHOD, decltype(&BIO_meth_free)> createBioMethod()
{
    auto bioMethod = utils::wrapUnique(
        BIO_meth_new(BIO_TYPE_SOCKET, "SyncSslSocket"),
        &BIO_meth_free);
    BIO_meth_set_write(bioMethod.get(), &sock_write);
    BIO_meth_set_read(bioMethod.get(), &sock_read);
    BIO_meth_set_puts(bioMethod.get(), &sock_puts);
    BIO_meth_set_ctrl(bioMethod.get(), &sock_ctrl);
    BIO_meth_set_create(bioMethod.get(), &sock_new);
    BIO_meth_set_destroy(bioMethod.get(), &sock_free);

    return bioMethod;
};

//-------------------------------------------------------------------------------------------------
// SyncSslSocket

SyncSslSocket::SyncSslSocket(
    std::unique_ptr<AbstractStreamSocket> delegate,
    bool isServerSide)
    :
    base_type(delegate.get()),
    m_extraBufferLen(0),
    m_delegate(std::move(delegate)),
    m_bioMethods(nullptr, &BIO_meth_free),
    m_ssl(nullptr, &SSL_free),
    m_isServerSide(isServerSide)
{
    m_bioMethods = createBioMethod();

    BIO* readBio = BIO_new(m_bioMethods.get());
    BIO_set_nbio(readBio, 1);
    BIO_set_app_data(readBio, this);

    BIO* writeBio = BIO_new(m_bioMethods.get());
    BIO_set_app_data(writeBio, this);
    BIO_set_nbio(writeBio, 1);

    m_ssl = utils::wrapUnique(
        SSL_new(m_isServerSide
            ? Context::instance()->defaultServerContext().get()
            : Context::instance()->clientContext().get()),
        &SSL_free);
    SSL_set_verify(m_ssl.get(), SSL_VERIFY_NONE, nullptr);
    SSL_set_session_id_context(m_ssl.get(), sid, 4);
    SSL_set_bio(m_ssl.get(), readBio, writeBio);
}

SyncSslSocket::~SyncSslSocket() = default;

int SyncSslSocket::recv(void* buffer, std::size_t bytesToRead, int flags)
{
    if (!SSL_is_init_finished(m_ssl.get()))
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
            const int bytesRead = SSL_read(m_ssl.get(), (char*)buffer, (int) bytesToRead);
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
        return SSL_read(m_ssl.get(), (char*) buffer, (int) bytesToRead);
    }
}

int SyncSslSocket::send(const void* buffer, std::size_t bufferLen)
{
    if (!SSL_is_init_finished(m_ssl.get()))
    {
        if (m_isServerSide)
            doServerHandshake();
        else
            doClientHandshake();
    }

    return SSL_write(m_ssl.get(), buffer, (int) bufferLen);
}

void SyncSslSocket::readSomeAsync(
    nx::Buffer* const /*buffer*/,
    IoCompletionHandler /*handler*/)
{
    NX_CRITICAL(false, "Not implemented and will never be. Use ssl::StreamSocket");
}

void SyncSslSocket::sendAsync(
    const nx::Buffer* /*buffer*/,
    IoCompletionHandler /*handler*/)
{
    NX_CRITICAL(false, "Not implemented and will never be. Use ssl::StreamSocket");
}

bool SyncSslSocket::performHandshake()
{
    if (SSL_is_init_finished(m_ssl.get()))
        return true;

    if (m_isServerSide)
        return doServerHandshake();
    else
        return doClientHandshake();
}

SSL* SyncSslSocket::ssl()
{
    return m_ssl.get();
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
    SSL_set_accept_state(m_ssl.get());

    return SSL_do_handshake(m_ssl.get()) == 1;
}

bool SyncSslSocket::doClientHandshake()
{
    SSL_set_connect_state(m_ssl.get());

    const int ret = SSL_do_handshake(m_ssl.get());
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

} // namespace nx::network::ssl::test
