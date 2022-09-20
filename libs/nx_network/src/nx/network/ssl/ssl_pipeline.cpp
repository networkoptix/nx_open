// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ssl_pipeline.h"

#include <typeinfo>

#include <openssl/err.h>

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

#include "context.h"

namespace nx::network::ssl {

static constexpr int kSslExDataThisIndex = 0;

Pipeline::Pipeline(
    Context* context,
    std::shared_ptr<SSL_CTX> sslContext)
    :
    m_context(context),
    m_bioMethods(nullptr, &BIO_meth_free),
    m_ssl(nullptr, &SSL_free)
{
    initSslBio(sslContext);
}

void Pipeline::setServerName(const std::string& serverName)
{
    SSL_set_tlsext_host_name(m_ssl.get(), serverName.c_str());
}

std::string Pipeline::serverNameFromClientHello() const
{
    const auto serverName = SSL_get_servername(m_ssl.get(), TLSEXT_NAMETYPE_host_name);
    return serverName ? std::string(serverName) : std::string();
}

int Pipeline::write(const void* data, size_t size)
{
    NX_TRACE(this, "Write %1 bytes", size);

    if (m_failed)
    {
        SystemError::setLastErrorCode(SystemError::invalidData);
        return utils::bstream::StreamIoError::nonRecoverableError;
    }

    const auto resultCode = performSslIoOperation(&SSL_write, data, size);
    if (m_state >= State::handshakeDone && !m_isPaused && resultCode < 0)
        m_failed = true;

    NX_TRACE(this, "Written %1 bytes out of %2 requested", resultCode, size);

    return resultCode;
}

int Pipeline::read(void* data, size_t size)
{
    NX_TRACE(this, "Read %1 bytes", size);

    // NOTE: Not checking m_failed similar to Pipeline::write because openssl supports
    // recovering from read errors. It may be useful to read already-received bytes even
    // when write has failed.
    // So, input and write channels of the pipeline are independent here.

    const auto resultCode = performSslIoOperation(&SSL_read, data, size);
    if (resultCode == 0)
        m_eof = true;

    NX_TRACE(this, "Read %1 bytes out of %2 requested", resultCode, size);

    return resultCode;
}

bool Pipeline::performHandshake()
{
    int resultCode = performHandshakeInternal();
    if (resultCode <= 0)
    {
        handleSslIoResult(resultCode);
        NX_VERBOSE(this, nx::format("Handshake failed. %1").args(resultCode));
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

void Pipeline::setFailed(bool val)
{
    m_failed = val;
}

void Pipeline::setVerifyCertificateChainCallback(VerifyCertificateChainCallback func)
{
    m_verifyCertificateChainCallback = std::move(func);

    if (m_verifyCertificateChainCallback)
        SSL_set_verify(m_ssl.get(), SSL_VERIFY_PEER, &Pipeline::verifyServerCertificateCallback);
    else
        SSL_set_verify(m_ssl.get(), SSL_VERIFY_NONE, nullptr);
}

void Pipeline::pauseDataProcessingAfterHandshake(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    if (m_state >= State::handshakeDone)
    {
        m_isPausePending = false;
        m_isPaused = true;
        completionHandler();
        return;
    }
    m_onIoPausedAfterHandshake = std::move(completionHandler);
    m_isPausePending = true;
}

void Pipeline::resumeDataProcessing()
{
    bool expected = true;
    if (m_isPausePending.compare_exchange_strong(expected, false))
    {
        m_onIoPausedAfterHandshake = nullptr;
    }
    m_isPaused = false;
}

SSL* Pipeline::ssl()
{
    return m_ssl.get();
}

void Pipeline::initSslBio(std::shared_ptr<SSL_CTX> context)
{
    m_bioMethods = utils::wrapUnique(BIO_meth_new(BIO_TYPE_SOCKET, typeid(*this).name()), &BIO_meth_free);
    BIO_meth_set_write(m_bioMethods.get(), &Pipeline::bioWrite);
    BIO_meth_set_read(m_bioMethods.get(), &Pipeline::bioRead);
    BIO_meth_set_puts(m_bioMethods.get(), &Pipeline::bioPuts);
    BIO_meth_set_ctrl(m_bioMethods.get(), &Pipeline::bioCtrl);
    BIO_meth_set_create(m_bioMethods.get(), &Pipeline::bioNew);
    BIO_meth_set_destroy(m_bioMethods.get(), &Pipeline::bioFree);

    BIO* rbio = BIO_new(m_bioMethods.get());
    BIO_up_ref(rbio);
    BIO_set_nbio(rbio, /*nonblocking I/O*/ 1);
    BIO_set_app_data(rbio, this);

    BIO* wbio = BIO_new(m_bioMethods.get());
    BIO_up_ref(wbio);
    BIO_set_nbio(wbio, /*nonblocking I/O*/ 1);
    BIO_set_app_data(wbio, this);

    NX_ASSERT(context);
    m_ssl.reset(SSL_new(context.get()));

    m_context->configure(m_ssl.get());

    SSL_set_ex_data(m_ssl.get(), kSslExDataThisIndex, this);

    SSL_set0_rbio(m_ssl.get(), rbio); //< SSL will free bio when freed.
    SSL_set0_wbio(m_ssl.get(), wbio); //< SSL will free bio when freed.

    SSL_set_allow_early_data_cb(
        m_ssl.get(),
        &Pipeline::verifyEarlyData,
        this);
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
    if (m_state < State::handshakeDone || m_isPaused)
        return utils::bstream::StreamIoError::wouldBlock;

    ERR_clear_error();

    const int resultCode = sslFunc(m_ssl.get(), data, static_cast<int>(size));
    return handleSslIoResult(resultCode);
}

int Pipeline::performHandshakeInternal()
{
    ERR_clear_error();

    const int resultCode = SSL_do_handshake(m_ssl.get());
    if (resultCode == 1)
    {
        m_state = State::handshakeDone;
        bool expected = true;
        if (m_isPausePending.compare_exchange_strong(expected, false))
        {
            m_isPaused = true;
            nx::utils::swapAndCall(m_onIoPausedAfterHandshake);
        }
    }

    // Zero is a "controlled" TLS handshake shutdown. To make life simpler, handling it as an error.
    return resultCode == 0 ? -1 : resultCode;
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
    if (sslErrorCode == SSL_ERROR_WANT_READ || sslErrorCode == SSL_ERROR_WANT_WRITE)
        NX_TRACE(this, "SSL error %1", sslErrorCodeToString(sslErrorCode));
    else
        NX_VERBOSE(this, "SSL error %1", sslErrorCodeToString(sslErrorCode));

    switch (sslErrorCode)
    {
        case SSL_ERROR_NONE:
            return resultCode;

        case SSL_ERROR_ZERO_RETURN:
            m_eof = true;
            return 0;

        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            return utils::bstream::StreamIoError::wouldBlock;

        case SSL_ERROR_SSL:
        {
            bool fatalErrorFound = false;
            analyzeSslErrorQueue(&fatalErrorFound);
            if (fatalErrorFound)
            {
                m_eof = true;
                m_failed = true;
                SystemError::setLastErrorCode(
                    m_state < State::handshakeDone
                        ? SystemError::sslHandshakeError
                        : SystemError::connectionReset);

                return utils::bstream::StreamIoError::nonRecoverableError;
            }

            // NOTE: Socket API does not provide a way to report logical error.
            // So, have to emulate some recoverable OS error here.
            SystemError::setLastErrorCode(SystemError::interrupted);
            return utils::bstream::StreamIoError::osError;
        }

        case SSL_ERROR_SYSCALL:
        {
            NX_VERBOSE(this, "SSL_ERROR_SYSCALL. System error %1, read thirsty %2, write thirsty %3",
                SystemError::getLastOSErrorText(), m_readThirsty, m_writeThirsty);

            if (m_readThirsty || m_writeThirsty)
                return utils::bstream::StreamIoError::wouldBlock;

            m_eof = true;
            m_failed = true;
            return utils::bstream::StreamIoError::osError;
        }
    }

    return resultCode;
}

std::string Pipeline::sslErrorCodeToString(int errorCode)
{
    switch (errorCode)
    {
        case SSL_ERROR_NONE:
            return "SSL_ERROR_NONE";
        case SSL_ERROR_ZERO_RETURN:
            return "SSL_ERROR_ZERO_RETURN";
        case SSL_ERROR_WANT_READ:
            return "SSL_ERROR_WANT_READ";
        case SSL_ERROR_WANT_WRITE:
            return "SSL_ERROR_WANT_WRITE";
        case SSL_ERROR_WANT_CONNECT:
            return "SSL_ERROR_WANT_CONNECT";
        case SSL_ERROR_WANT_ACCEPT:
            return "SSL_ERROR_WANT_ACCEPT";
        case SSL_ERROR_WANT_X509_LOOKUP:
            return "SSL_ERROR_WANT_X509_LOOKUP";
        case SSL_ERROR_SYSCALL:
            return "SSL_ERROR_SYSCALL";
        case SSL_ERROR_SSL:
            return "SSL_ERROR_SSL";
        default:
            return nx::format("Unknown error code %1").args(errorCode).toStdString();
    }
}

void Pipeline::analyzeSslErrorQueue(bool* fatalErrorFound)
{
    char errText[1024];
    *fatalErrorFound = false;

    while (int errCode = ERR_get_error())
    {
        ERR_error_string_n(errCode, errText, sizeof(errText));

        // NOTE: During handshake considering every error to be fatal since re-handshake is not
        // implemented and is pretty much useless.
        if (ERR_FATAL_ERROR(errCode) || m_state < State::handshakeDone)
        {
            NX_DEBUG(this, "SSL fatal error %1. %2", errCode, errText);
            *fatalErrorFound = true;
        }
        else
        {
            NX_VERBOSE(this, "SSL non fatal error %1. %2", errCode, errText);
        }
    }
}

//-------------------------------------------------------------------------------------------------
// Stand-alone SSL BIO functions

int Pipeline::bioRead(BIO* b, char* out, int outl)
{
    Pipeline* sslSock = static_cast<Pipeline*>(BIO_get_app_data(b));
    int resultCode = sslSock->bioRead(out, outl);
    if (resultCode >= 0)
        return resultCode;

    if (resultCode == utils::bstream::StreamIoError::wouldBlock)
        BIO_set_retry_read(b);
    else if (resultCode < 0)
        BIO_clear_retry_flags(b);

    return -1;
}

int Pipeline::bioWrite(BIO* b, const char* in, int inl)
{
    Pipeline* sslSock = static_cast<Pipeline*>(BIO_get_app_data(b));
    int resultCode = sslSock->bioWrite(in, inl);
    if (resultCode >= 0)
        return resultCode;

    if (resultCode == utils::bstream::StreamIoError::wouldBlock)
        BIO_set_retry_write(b);
    else if (resultCode < 0)
        BIO_clear_retry_flags(b);

    return -1;
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
            resultCode = BIO_get_shutdown(bio);
            break;
        case BIO_CTRL_SET_CLOSE:
            BIO_set_shutdown(bio, (int) num);
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
    BIO_set_init(bio, 1);
    // bio->num = 0
    BIO_set_data(bio, nullptr);
    BIO_clear_flags(bio, ~0);
    return 1;
}

int Pipeline::bioFree(BIO* bio)
{
    if (!bio)
        return 0;

    if (BIO_get_shutdown(bio))
    {
        BIO_set_init(bio, 0);
        BIO_clear_flags(bio, ~0);
    }

    return 1;
}

int Pipeline::verifyServerChainCallback(X509_STORE_CTX* x509Ctx)
{
    auto ssl = static_cast<SSL*>(X509_STORE_CTX_get_ex_data(
        x509Ctx,
        SSL_get_ex_data_X509_STORE_CTX_idx()));

    auto pipeline = static_cast<Pipeline*>(SSL_get_ex_data(ssl, kSslExDataThisIndex));

    CertificateChainView chain;
    STACK_OF(X509)* osslChain = X509_STORE_CTX_get0_chain(x509Ctx);
    const int certCount = sk_X509_num(osslChain);
    for (int i = 0; i < certCount; i++)
        chain.push_back(sk_X509_value(osslChain, i));

    return pipeline->m_verifyCertificateChainCallback(std::move(chain)) ? 1 : 0;
}

int Pipeline::verifyServerCertificateCallback(int /*preverifyOk*/, X509_STORE_CTX* x509Ctx)
{
    X509_STORE_CTX_set_verify(x509Ctx, &Pipeline::verifyServerChainCallback);
    return 1;
}

int Pipeline::verifyEarlyData(SSL* /*s*/, void* /*arg*/)
{
    // TLSv1.3 0-RTT is disabled for now.
    // Anyway, TLSv1.3 introduces faster handshake than TLSv1.2.
    return 0;
}

//-------------------------------------------------------------------------------------------------
// ConnectingPipeline

ConnectingPipeline::ConnectingPipeline(Context* context)
    :
    Pipeline(
        context ? context : Context::instance(),
        (context ? context : Context::instance())->clientContext())
{
    SSL_set_connect_state(ssl());
}

//-------------------------------------------------------------------------------------------------
// AcceptingPipeline

AcceptingPipeline::AcceptingPipeline(Context* context):
    Pipeline(
        context ? context : Context::instance(),
        (context ? context : Context::instance())->defaultServerContext())
{
    SSL_set_accept_state(ssl());
}

} // namespace nx::network::ssl
