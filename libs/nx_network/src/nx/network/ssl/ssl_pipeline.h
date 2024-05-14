// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <string>

#include <openssl/ssl.h>

#include <nx/utils/byte_stream/pipeline.h>
#include <nx/utils/move_only_func.h>

#include "certificate.h"
#include "helpers.h"

namespace nx::network::aio { class BasicPollable; }

namespace nx::network::ssl {

class Context;
class Pipeline;

/**
 * Type of function being called on synchronous certificate chain verification.
 * @param chain The chain that must be verified.
 * @return true if the chain has been verified, false otherwise.
 */
using VerifyCertificateChainCallback = nx::utils::MoveOnlyFunc<
    bool(CertificateChainView /*chain*/)>;

/**
 * NOTE: Not thread-safe.
 */
class NX_NETWORK_API Pipeline:
    public nx::utils::bstream::Converter
{
public:
    /**
     * @param context
     * @param sslContext Caller may use different contexts on client & server sides.
     * TODO: #akolesnikov remove sslContext
     */
    Pipeline(
        Context* context,
        std::shared_ptr<SSL_CTX> sslContext);

    ~Pipeline() = default;

    /**
     * If not empty, ClientHello will contain "servername" attribute.
     */
    void setServerName(const std::string& serverName);

    std::string serverNameFromClientHello() const;

    /**
     * NOTE: SSL pipeline does not recover from any I/O error because openssl supports
     * retrying write only with the same data. That does not conform to
     * nx::utils::bstream::AbstractOutput.
     * So, even after wouldBlock write error, next write will always produce non-recoverable
     * failure. Though, it conforms to nx::utils::bstream::AbstractOutput.
     */
    virtual int write(const void* data, size_t size) override;
    virtual int read(void* data, size_t size) override;

    virtual bool eof() const override;
    virtual bool failed() const override;

    bool performHandshake();
    bool isHandshakeCompleted() const;

    bool isReadThirsty() const;
    bool isWriteThirsty() const;

    void shutdown();

    void setFailed(bool val);
    void setVerifyCertificateChainCallback(VerifyCertificateChainCallback func);

    /**
     * Blocks any I/O with user data. It means read() and write() functions will return wouldBlock
     * even if some data is available. User I/O can only be paused after the handshake is done.
     * If it's already done, completionHandler will be called immediately. Otherwise, it will be
     * called upon completion of the handshake.
     * Note that:
     *     - function can be invoked concurrently with I/O
     *     - completionHandler will be invoked in the context of the I/O thread, which drives
     *         its I/O
     *     - completionHandler may never be called if the handshake fails or if
     *         resumeDataProcessing() was invoked just after pauseDataProcessingAfterHandshake().
     */
    void pauseDataProcessingAfterHandshake(nx::utils::MoveOnlyFunc<void()> completionHandler);

    /**
     * Resumes previously paused user I/O. I/O can be resumed even if it wasn't fully paused,
     * i.e. completionHandler of pauseDataProcessingAfterHandshake() is not called yet. In this
     * case, the callback will be never called and will be destroyed by this function.
     */
    void resumeDataProcessing();

protected:
    SSL* ssl();

private:
    enum class State
    {
        init,
        handshakeDone,
    };

    Context* m_context = nullptr;
    State m_state = State::init;
    std::unique_ptr<BIO_METHOD, decltype(&BIO_meth_free)> m_bioMethods;
    std::unique_ptr<SSL, decltype(&SSL_free)> m_ssl;
    bool m_readThirsty = false;
    bool m_writeThirsty = false;
    bool m_eof = false;
    bool m_failed = false;
    VerifyCertificateChainCallback m_verifyCertificateChainCallback;
    std::atomic<bool> m_isPausePending = false;
    std::atomic<bool> m_isPaused = false;
    nx::utils::MoveOnlyFunc<void()> m_onIoPausedAfterHandshake;

    void initSslBio(std::shared_ptr<SSL_CTX> sslContext);

    template<typename Func, typename Data>
    int performSslIoOperation(Func sslFunc, Data* data, size_t size);
    int performHandshakeInternal();
    int bioRead(void* buffer, unsigned int bufferLen);
    int bioWrite(const void* buffer, unsigned int bufferLen);

    int handleSslIoResult(int result);
    std::string sslErrorCodeToString(int errorCode);
    void analyzeSslErrorQueue(bool* fatalErrorFound);

    static int bioRead(BIO* b, char* out, int outl);
    static int bioWrite(BIO* b, const char* in, int inl);
    static int bioPuts(BIO* bio, const char* str);
    static long bioCtrl(BIO* bio, int cmd, long num, void* /*ptr*/);
    static int bioNew(BIO* bio);
    static int bioFree(BIO* bio);
    static int verifyServerCertificateCallback(int preverify_ok, X509_STORE_CTX* x509_ctx);
    static int verifyServerChainCallback(X509_STORE_CTX* x509_ctx);

    /** SSL_allow_early_data_cb_fn. */
    static int verifyEarlyData(SSL* s, void* arg);
};

class NX_NETWORK_API ConnectingPipeline:
    public Pipeline
{
public:
    /**
     * @param context If null, global context (Context::instance()) is used.
     */
    ConnectingPipeline(Context* context = nullptr);
};

class NX_NETWORK_API AcceptingPipeline:
    public Pipeline
{
public:
    /**
     * @param context If null, global context (Context::instance()) is used.
     */
    AcceptingPipeline(Context* context = nullptr);
};

} // namespace nx::network::ssl
