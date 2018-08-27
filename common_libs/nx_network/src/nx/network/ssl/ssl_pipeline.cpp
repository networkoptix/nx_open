#include "ssl_pipeline.h"

#include <nx/utils/log/assert.h>

#include "ssl_static_data.h"

namespace nx {
namespace network {

//-------------------------------------------------------------------------------------------------
// ssl::Pipeline

namespace ssl {

Pipeline::Pipeline(SSL_CTX* sslContext):
    m_state(State::init),
    m_ssl(nullptr, &SSL_free),
    m_readThirsty(false),
    m_writeThirsty(false),
    m_eof(false),
    m_failed(false)
{
    initSslBio(sslContext);
}

int Pipeline::write(const void* data, size_t size)
{
    return performSslIoOperation(&SSL_write, data, size);
}

int Pipeline::read(void* data, size_t size)
{
    const auto resultCode = performSslIoOperation(&SSL_read, data, size);
    if (resultCode == 0)
        m_eof = true;
    return resultCode;
}

bool Pipeline::performHandshake()
{
    int resultCode = performHandshakeInternal();
    if (resultCode <= 0)
    {
        handleSslIoResult(resultCode);
        return false;
    }

    return true;
}

bool Pipeline::isHandshakeCompleted() const
{
    return m_state == State::handshakeDone;
}

bool Pipeline::isReadThirsty() const
{
    return m_readThirsty;
}

bool Pipeline::isWriteThirsty() const
{
    return m_writeThirsty;
}

bool Pipeline::eof() const
{
    return m_eof;
}

bool Pipeline::failed() const
{
    return m_failed;
}

void Pipeline::shutdown()
{
    SSL_shutdown(ssl());
}

SSL* Pipeline::ssl()
{
    return m_ssl.get();
}

void Pipeline::initSslBio(SSL_CTX* context)
{
    static BIO_METHOD kBioMethods =
    {
        BIO_TYPE_SOCKET,
        "proxy server socket",
        &Pipeline::bioWrite,
        &Pipeline::bioRead,
        &Pipeline::bioPuts,
        NULL, //<< bgets
        &Pipeline::bioCtrl,
        &Pipeline::bioNew,
        &Pipeline::bioFree,
        NULL, //<< callback_ctrl
    };

    BIO* rbio = BIO_new(&kBioMethods);
    BIO_set_nbio(rbio, 1);
    BIO_set_app_data(rbio, this);

    BIO* wbio = BIO_new(&kBioMethods);
    BIO_set_app_data(wbio, this);
    BIO_set_nbio(wbio, 1);

    NX_ASSERT(context);
    m_ssl.reset(SSL_new(context)); //<< Get new SSL state with context.

    SSL_set_verify(m_ssl.get(), SSL_VERIFY_NONE, NULL);
    SSL_set_session_id_context(
        m_ssl.get(),
        reinterpret_cast<const unsigned char*>(
            ssl::SslStaticData::instance()->sslSessionId().constData()),
        ssl::SslStaticData::instance()->sslSessionId().size());

    SSL_set_bio(m_ssl.get(), rbio, wbio);  //< SSL will free bio when freed.
}

template<typename Func, typename Data>
int Pipeline::performSslIoOperation(Func sslFunc, Data* data, size_t size)
{
    if (m_state < State::handshakeDone)
    {
        int resultCode = performHandshakeInternal();
        if (resultCode <= 0)
            return handleSslIoResult(resultCode);
    }
    if (m_state < State::handshakeDone)
        return utils::bstream::StreamIoError::wouldBlock;

    const int resultCode = sslFunc(m_ssl.get(), data, static_cast<int>(size));
    return handleSslIoResult(resultCode);
}

int Pipeline::performHandshakeInternal()
{
    const int resultCode = SSL_do_handshake(m_ssl.get());
    if (resultCode == 1)
        m_state = State::handshakeDone;

    return resultCode;
}

int Pipeline::bioRead(void* buffer, unsigned int bufferLen)
{
    const auto resultCode = m_inputStream->read(buffer, bufferLen);
    m_readThirsty = (resultCode == utils::bstream::StreamIoError::wouldBlock) || (resultCode == 0);
    return resultCode;
}

int Pipeline::bioWrite(const void* buffer, unsigned int bufferLen)
{
    const auto resultCode = m_outputStream->write(buffer, bufferLen);
    m_writeThirsty = (resultCode == utils::bstream::StreamIoError::wouldBlock) || (resultCode == 0);
    return resultCode;
}

int Pipeline::handleSslIoResult(int resultCode)
{
    if (resultCode >= 0)
        return resultCode;

    const auto sslErrorCode = SSL_get_error(m_ssl.get(), resultCode);
    switch (sslErrorCode)
    {
        case SSL_ERROR_NONE:
            return resultCode;

        case SSL_ERROR_ZERO_RETURN:
            m_eof = true;
            return 0;

        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
        {
            // TODO
            break;
        }

        case SSL_ERROR_SSL:
            m_eof = true;
            m_failed = true;
            return utils::bstream::StreamIoError::nonRecoverableError;

        case SSL_ERROR_SYSCALL:
            if (m_readThirsty || m_writeThirsty)
                return utils::bstream::StreamIoError::wouldBlock;
            return utils::bstream::StreamIoError::osError;

        default:
            break;
    }

    return resultCode;
}

//-------------------------------------------------------------------------------------------------
// Stand-alone SSL BIO functions

int Pipeline::bioRead(BIO* b, char* out, int outl)
{
    Pipeline* sslSock = static_cast<Pipeline*>(BIO_get_app_data(b));
    int resultCode = sslSock->bioRead(out, outl);
    if (resultCode == utils::bstream::StreamIoError::osError)
    {
        BIO_clear_retry_flags(b);
        BIO_set_retry_read(b);
    }

    return resultCode;
}

int Pipeline::bioWrite(BIO* b, const char* in, int inl)
{
    Pipeline* sslSock = static_cast<Pipeline*>(BIO_get_app_data(b));
    int resultCode = sslSock->bioWrite(in, inl);
    if (resultCode == utils::bstream::StreamIoError::osError)
    {
        BIO_clear_retry_flags(b);
        BIO_set_retry_write(b);
    }

    return resultCode;
}

int Pipeline::bioPuts(BIO* bio, const char* str)
{
    return bioWrite(bio, str, static_cast<int>(strlen(str)));
}

long Pipeline::bioCtrl(BIO* bio, int cmd, long num, void* /*ptr*/)
{
    long resultCode = 1;

    switch (cmd)
    {
        case BIO_C_SET_FD:
            NX_ASSERT(false, "Invalid proxy socket use!");
            break;
        case BIO_C_GET_FD:
            NX_ASSERT(false, "Invalid proxy socket use!");
            break;
        case BIO_CTRL_GET_CLOSE:
            resultCode = bio->shutdown;
            break;
        case BIO_CTRL_SET_CLOSE:
            bio->shutdown = (int)num;
            break;
        case BIO_CTRL_DUP:
        case BIO_CTRL_FLUSH:
            resultCode = 1;
            break;
        default:
            resultCode = 0;
            break;
    }
    return(resultCode);
}

int Pipeline::bioNew(BIO* bio)
{
    bio->init = 1;
    bio->num = 0;
    bio->ptr = NULL;
    bio->flags = 0;
    return 1;
}

int Pipeline::bioFree(BIO* bio)
{
    if (bio == NULL) return(0);
    if (bio->shutdown)
    {
        bio->init = 0;
        bio->flags = 0;
    }

    return 1;
}

//-------------------------------------------------------------------------------------------------
// ConnectingPipeline

ConnectingPipeline::ConnectingPipeline():
    Pipeline(SslStaticData::instance()->clientContext())
{
    SSL_set_connect_state(ssl());
}

//-------------------------------------------------------------------------------------------------
// AcceptingPipeline

AcceptingPipeline::AcceptingPipeline():
    Pipeline(SslStaticData::instance()->serverContext())
{
    SSL_set_accept_state(ssl());
}

} // namespace ssl
} // namespace network
} // namespace nx
