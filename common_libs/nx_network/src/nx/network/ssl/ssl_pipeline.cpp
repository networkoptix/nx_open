#include "ssl_pipeline.h"

#include <nx/utils/log/assert.h>

#include "ssl_static_data.h"

namespace nx {
namespace network {
namespace ssl {

//-------------------------------------------------------------------------------------------------
// Stand-alone BIO functions

static int bioRead(BIO* b, char* out, int outl)
{
    Pipeline* sslSock = static_cast<Pipeline*>(BIO_get_app_data(b));
    int ret = sslSock->bioRead(out, outl);
    if (ret == -1)
    {
        BIO_clear_retry_flags(b);
        BIO_set_retry_read(b);
    }

    return ret;
}

static int bioWrite(BIO* b, const char* in, int inl)
{
    Pipeline* sslSock = static_cast<Pipeline*>(BIO_get_app_data(b));
    int ret = sslSock->bioWrite(in, inl);
    if (ret == -1)
    {
        BIO_clear_retry_flags(b);
        BIO_set_retry_write(b);
    }

    return ret;
}

static int bioPuts(BIO* bio, const char* str)
{
    return bioWrite(bio, str, strlen(str));
}

static long bioCtrl(BIO* bio, int cmd, long num, void* /*ptr*/)
{
    long ret = 1;

    switch (cmd)
    {
        case BIO_C_SET_FD:
            NX_ASSERT(false, "Invalid proxy socket use!");
            break;
        case BIO_C_GET_FD:
            NX_ASSERT(false, "Invalid proxy socket use!");
            break;
        case BIO_CTRL_GET_CLOSE:
            ret = bio->shutdown;
            break;
        case BIO_CTRL_SET_CLOSE:
            bio->shutdown = (int)num;
            break;
        case BIO_CTRL_DUP:
        case BIO_CTRL_FLUSH:
            ret = 1;
            break;
        default:
            ret = 0;
            break;
    }
    return(ret);
}

static int bioNew(BIO* bio)
{
    bio->init = 1;
    bio->num = 0;
    bio->ptr = NULL;
    bio->flags = 0;
    return 1;
}

static int bioFree(BIO* bio)
{
    if (bio == NULL) return(0);
    if (bio->shutdown)
    {
        if (bio->init)
        {
            Pipeline* sslSock = static_cast<Pipeline*>(BIO_get_app_data(bio));
            if (sslSock)
                sslSock->close();
        }

        bio->init = 0;
        bio->flags = 0;
    }

    return 1;
}

//-------------------------------------------------------------------------------------------------
// Pipeline

Pipeline::Pipeline(SSL_CTX* sslContext):
    m_state(State::init),
    m_ssl(nullptr, &SSL_free)
{
    initSslBio(sslContext);
    initOpenSSLGlobalLock();
}

Pipeline::~Pipeline()
{
}

void Pipeline::setInput(AbstractInput* inputStream)
{
    m_inputStream = inputStream;
}

void Pipeline::setOutput(AbstractOutput* outputStream)
{
    m_outputStream = outputStream;
}

int Pipeline::write(const void* data, size_t count)
{
    if (m_state < State::handshakeDone)
        doHandshake();
    if (m_state < State::handshakeDone)
        return StreamIoError::wouldBlock;

    return SSL_write(m_ssl.get(), data, count);
}

int Pipeline::read(void* data, size_t count)
{
    if (m_state < State::handshakeDone)
        doHandshake();
    if (m_state < State::handshakeDone)
        return StreamIoError::wouldBlock;

    return SSL_read(m_ssl.get(), data, count);
}

int Pipeline::bioRead(void* buffer, unsigned int bufferLen)
{
    return m_inputStream->read(buffer, bufferLen);
}

int Pipeline::bioWrite(const void* buffer, unsigned int bufferLen)
{
    return m_outputStream->write(buffer, bufferLen);
}

void Pipeline::close()
{
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
        ssl::bioWrite,
        ssl::bioRead,
        bioPuts,
        NULL, // bgets
        bioCtrl,
        bioNew,
        bioFree,
        NULL, // callback_ctrl
    };

    BIO* rbio = BIO_new(&kBioMethods);
    BIO_set_nbio(rbio, 1);
    BIO_set_app_data(rbio, this);

    BIO* wbio = BIO_new(&kBioMethods);
    BIO_set_app_data(wbio, this);
    BIO_set_nbio(wbio, 1);

    NX_ASSERT(context);
    m_ssl.reset(SSL_new(context)); // get new SSL state with context

    SSL_set_verify(m_ssl.get(), SSL_VERIFY_NONE, NULL);
    SSL_set_session_id_context(
        m_ssl.get(),
        reinterpret_cast<const unsigned char*>(
            ssl::SslStaticData::instance()->sslSessionId().constData()),
        ssl::SslStaticData::instance()->sslSessionId().size());

    SSL_set_bio(m_ssl.get(), rbio, wbio);  //ssl will free bio when freed
}

int Pipeline::doHandshake()
{
    if (SSL_do_handshake(m_ssl.get()) == 0)
        m_state = State::handshakeDone;
    return 0;
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
